// Copyright Prosperitocracy. All Rights Reserved.

#include "Weapons/ProsperitocracyBloodBlade.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/ProsperitocracyCharacter.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyBloodBlade)

namespace
{
	/** Range is in meters (the stat's own unit); a trace is in cm. */
	constexpr float CentimetersPerMeter = 100.0f;
}

/**
 * The combo's own vocabulary: its sections, and the rule their order carries.
 *
 * These are the montage's OWN section names — the plan's, his word — and the pairing is the rule: a
 * press inside recovery n takes the combo to attack n + 1. By NAME and never by seconds, so moving Rate
 * (which re-times every part of the montage) cannot break a window.
 */
namespace ProsperitocracyBladeCombo
{
	/** The attack sections, in the order they are swung. A press starts at the first one. */
	const TCHAR* const Attacks[] = { TEXT("A"), TEXT("B"), TEXT("C") };

	/** The recovery sections, in order: recovery n belongs to attack n and leads to attack n + 1. */
	const TCHAR* const Recoveries[] = { TEXT("A rec"), TEXT("B Rec") };

	int32 NumAttacks() { return UE_ARRAY_COUNT(Attacks); }
	int32 NumRecoveries() { return UE_ARRAY_COUNT(Recoveries); }
}

int32 AProsperitocracyBloodBlade::GetBloodPoolSize() const
{
	// The pool is the blade's own MagSize, read FINAL through the blade's own GAS home — so a perk on
	// the pool is already in this number, and the swing's cost and the drain (both derived from it)
	// move with it. One number, three readers.
	return FMath::Max(0, FMath::RoundToInt(GetWeaponStat(EProsperitocracyStat::MagSize)));
}

int32 AProsperitocracyBloodBlade::GetSwingCost() const
{
	// A share of the pool, with a floor so a pool too small to share out still costs a swing something.
	// Both numbers are universal constants; the pool is the only authored one, and it is a row.
	return FMath::Max(ProsperitocracyBloodHandling::SwingCostFloor,
		FMath::RoundToInt(GetBloodPoolSize() * ProsperitocracyBloodHandling::SwingCostShareOfPool));
}

float AProsperitocracyBloodBlade::GetDrainPerSecond() const
{
	if (ProsperitocracyBloodHandling::DrainWindowSeconds <= 0.0f)
	{
		return 0.0f;
	}

	// The pool over its window — the same shape as contested health's fade, so the number the player
	// reads (the pool) is the only thing there is to move.
	return static_cast<float>(GetBloodPoolSize()) / ProsperitocracyBloodHandling::DrainWindowSeconds;
}

FProsperitocracyWeaponAmmo* AProsperitocracyBloodBlade::GetBloodStore()
{
	// The blood lives where a gun's magazine lives: the slot's store on the carrier, filled from this
	// thing's own numbers the first time it is asked for. A blade has NO Capacity row, and an absent
	// Capacity is worth no spare magazines — so the pool is the whole of the blood, which is exactly
	// "the ammo, minus the spare".
	if (!EnsureInitialized() || !OwnerLoadout || !Slot.IsValid())
	{
		return nullptr;
	}

	const int32 Capacity = FMath::Max(0, FMath::RoundToInt(GetWeaponStat(EProsperitocracyStat::Capacity)));
	return &OwnerLoadout->GetOrCreateAmmoForSlot(Slot, GetBloodPoolSize(), Capacity);
}

int32 AProsperitocracyBloodBlade::GetBloodLeft() const
{
	// Read through the weapon's own public answer for its magazine, so the pool is read the same way a
	// gun's rounds are — it IS the same number in the same store.
	return GetMagazineAmmo();
}

bool AProsperitocracyBloodBlade::IsInHand() const
{
	// The rig decides which weapon is out, and the rig now BUILDS what the loadout carries: for a class
	// whose primary is its melee, the primary channel spawns this blade, so the rig's own answer is the
	// truth — a blade is in hand exactly when the rig says this blade is what it is holding. (Merely
	// being dressed is not enough: a weapon in a slot that is not up is carried, not held.)
	return IsHeldInRig();
}

bool AProsperitocracyBloodBlade::SpendSwingCost()
{
	FProsperitocracyWeaponAmmo* Blood = GetBloodStore();
	const int32 Cost = GetSwingCost();

	// No pool to take from, or not enough in it: the swing is refused rather than going into debt. A
	// swing is not a shot — there is no cadence here, because the combo's own parts are its cadence
	// and the animation is what decides when the next one may come.
	if (!Blood || Blood->Magazine < Cost)
	{
		return false;
	}

	Blood->Magazine -= Cost;
	return true;
}

bool AProsperitocracyBloodBlade::Swing()
{
	// Only the blade in the player's hand swings: a stowed blade neither reaches anything nor spends
	// anything.
	if (!IsInHand())
	{
		return false;
	}

	// A swing costs whether it hits or not, so a pool that cannot cover one refuses it — and it refuses
	// BEFORE the animation starts, so a refused press is not felt as a swing at all.
	if (GetBloodLeft() < GetSwingCost())
	{
		return false;
	}

	// The animation half decides whether this press IS an attack: a press inside a recovery takes the
	// combo to its next attack, a press inside an attack does nothing, and a press with nothing playing
	// starts the combo at its first attack.
	if (!PlayOrAdvanceCombo())
	{
		return false;
	}

	// The numbers half: the blood, then the ball.
	SpendSwingCost();
	LandTheHit();

	// The run's own account of a swing: what the combo is on, and what is left in the pool afterwards.
	// DEBUG-level noise in the log is why this is one line and not three.
	UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: swung — blood %d of %d | swing cost %d | drain %.2f/s while out"),
		*GetName(), GetBloodLeft(), GetBloodPoolSize(), GetSwingCost(), GetDrainPerSecond());

	return true;
}

bool AProsperitocracyBloodBlade::PlayOrAdvanceCombo()
{
	UAnimInstance* Anim = GetBodyAnimInstance();

	// WHICH attack this press is: the combo's first for a press with nothing playing, and the NEXT one for a
	// press that passes through a recovery. Taken from the section the press was MADE in, not read back off
	// the montage afterwards, so the answer cannot depend on when the engine applies a section jump. An
	// attack with no animation behind it (below) has no clock either, and INDEX_NONE is how that is said.
	int32 AttackIndex = INDEX_NONE;

	// No combo to play — a body with no anim instance, or no montage set on this blade. The press is
	// still a swing: the numbers must never depend on the animation being there. Said out loud, because
	// a swing whose animation never plays is a swing nobody can see.
	if (!Anim || !ComboMontage)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Blade] %s: the swing has no animation to play — %s"),
			*GetName(), !Anim ? TEXT("the body has no anim instance") : TEXT("ComboMontage is not set on this blade"));
		return true;
	}

	if (!Anim->Montage_IsPlaying(ComboMontage))
	{
		// Nothing is playing: the combo starts at its FIRST attack, at the rate the player's Rate row
		// says (final ÷ base), so the whole animation moves with that one number.
		const float Length = Anim->Montage_Play(ComboMontage, GetComboPlayRate(), EMontagePlayReturnType::MontageLength,
			/*InTimeToStartMontageAt=*/ 0.0f, /*bStopAllMontages=*/ false);
		Anim->Montage_JumpToSection(FName(ProsperitocracyBladeCombo::Attacks[0]), ComboMontage);
		AttackIndex = 0;

		UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: combo starts at %s on %s at x%.2f (montage length %.2fs)"),
			*GetName(), ProsperitocracyBladeCombo::Attacks[0], *GetNameSafe(Anim), GetComboPlayRate(), Length);
	}
	else
	{
		// An attack is already playing, so the ONLY thing a press may do is pass through a recovery:
		// from recovery n straight to attack n + 1. Inside an attack, or in the last attack, it does
		// nothing at all — a press never cancels a swing and never stacks one up.
		const FName Current = Anim->Montage_GetCurrentSection(ComboMontage);

		bool bAdvanced = false;
		for (int32 Index = 0; Index < ProsperitocracyBladeCombo::NumRecoveries(); ++Index)
		{
			if (Current != FName(ProsperitocracyBladeCombo::Recoveries[Index]))
			{
				continue;
			}

			if (Index + 1 >= ProsperitocracyBladeCombo::NumAttacks())
			{
				break;
			}

			Anim->Montage_JumpToSection(FName(ProsperitocracyBladeCombo::Attacks[Index + 1]), ComboMontage);
			AttackIndex = Index + 1;
			UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: press inside %s — straight to %s"),
				*GetName(), *Current.ToString(), ProsperitocracyBladeCombo::Attacks[Index + 1]);
			bAdvanced = true;
			break;
		}

		if (!bAdvanced)
		{
			UE_LOG(LogProsperitocracy, Verbose, TEXT("[Blade] %s: press inside %s — nothing happens"),
				*GetName(), *Current.ToString());
			return false;
		}
	}

	bSwinging = true;

	// The BODY's half: an attack belongs to the body from here. It holds the line, travels it, faces it and
	// refuses the player's own movement for as long as the attack lasts, and it is the body's own clock that
	// ends it. The blade hands over the two numbers it owns — its Range and this attack's own piece of the
	// combo — and then holds nothing: it never writes the body's speed, its velocity or its facing.
	BeginTheBodyAttack(AttackIndex);

	return true;
}

float AProsperitocracyBloodBlade::GetRangeCm() const
{
	// Range's own unit is meters (Design/stats.md) and the body is moved in centimeters. One conversion, in
	// one place, so nothing else in this class has to remember which unit it is holding.
	return GetWeaponStat(EProsperitocracyStat::Range) * CentimetersPerMeter;
}

float AProsperitocracyBloodBlade::GetAttackSeconds(int32 AttackIndex) const
{
	// The attack's length is ITS OWN piece of the combo, at the rate the player reads. An attack is every
	// OTHER piece — A, its recovery, B, its recovery, C — so the piece that times attack n is segment 2n.
	// No montage means no clock, and a body given no clock is given no attack (see BeginTheBodyAttack).
	if (!ComboMontage || !ComboMontage->SlotAnimTracks.IsValidIndex(0) || AttackIndex < 0)
	{
		return 0.0f;
	}

	const TArray<FAnimSegment>& Pieces = ComboMontage->SlotAnimTracks[0].AnimTrack.AnimSegments;
	const int32 PieceIndex = AttackIndex * 2;
	if (!Pieces.IsValidIndex(PieceIndex))
	{
		return 0.0f;
	}

	// Played at the Rate the player reads, so the attack's length in the world is its own length over that
	// rate — the same ratio the animation itself runs at.
	return Pieces[PieceIndex].AnimEndTime / FMath::Max(GetComboPlayRate(), KINDA_SMALL_NUMBER);
}

void AProsperitocracyBloodBlade::BeginTheBodyAttack(int32 AttackIndex)
{
	APawn* Pawn = Cast<APawn>(OwningPawn.Get());
	AProsperitocracyCharacter* Character = Cast<AProsperitocracyCharacter>(OwningPawn.Get());
	if (!Pawn || !Character)
	{
		return;
	}

	// The three things an attack needs, all of which belong to the BODY once it has them: the LINE is what
	// the player was looking at when they pressed (taken flat, and held for the whole attack, which is what
	// keeps an attack straight while they are still free to turn for the next one), the DISTANCE is Range —
	// the same one number the sweep's reach and the swing's width come from — and the TIME is this attack's
	// own piece of the combo. A missing clock or a missing number is handed over as nothing at all, and the
	// body says no to that honestly instead of inventing an attack.
	Character->BeginAttack(Pawn->GetControlRotation().Vector(), GetRangeCm(), GetAttackSeconds(AttackIndex));
}

void AProsperitocracyBloodBlade::LandTheHit()
{
	APawn* Pawn = Cast<APawn>(OwningPawn.Get());
	UWorld* World = GetWorld();

	// THE REACH IS THE RANGE STAT: meters, this blade's own row read through its own GAS home, so a
	// Range perk extends the swing with no code change anywhere.
	const float ReachMeters = GetWeaponStat(EProsperitocracyStat::Range);
	if (!Pawn || !World || ReachMeters <= 0.0f)
	{
		return;
	}

	// From the player's EYE, along what they were looking at when they pressed. The camera sits a boom
	// length behind the character, so a camera-origin sweep would start inside the character's own back.
	const FVector SwingStart = Pawn->GetPawnViewLocation();
	const FVector SwingEnd = SwingStart + Pawn->GetControlRotation().Vector() * (ReachMeters * CentimetersPerMeter);

	// The same channel and the same ignores as a shot: never the swinger, never what hangs off them (the
	// blade itself is attached to this body).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BladeSwing), /*bTraceComplex=*/ true, Pawn);
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	// One universal thickness for every melee swing, never a per-thing number: the same ball the gun's
	// bash sweeps.
	const FCollisionShape Ball = FCollisionShape::MakeSphere(ProsperitocracyMeleeHandling::SwingRadius);

	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, SwingStart, SwingEnd, FQuat::Identity, ECC_Visibility, Ball, Params)
		&& Hit.bBlockingHit)
	{
		// The ONE damage path: the blade's own block is the ability source, so its Piercing Damage and
		// its Penetration travel the same effect and the same pen-gate -> resist execution a shot does.
		ApplyShotDamage(Hit);
	}
}

UAnimInstance* AProsperitocracyBloodBlade::GetBodyAnimInstance() const
{
	// The body's own anim instance, which is where a montage plays and where the combo's slots live. The
	// visible body is a second mesh that retargets this one, so this is where the player sees it from.
	const AProsperitocracyCharacter* Character = Cast<AProsperitocracyCharacter>(OwningPawn.Get());
	if (!Character)
	{
		return nullptr;
	}

	const USkeletalMeshComponent* Body = Character->GetMesh();
	return Body ? Body->GetAnimInstance() : nullptr;
}

float AProsperitocracyBloodBlade::GetComboPlayRate() const
{
	const UProsperitocracyStatTable* Block = GetStatBlock();

	// The base is the block's own authored Rate and the value is what the one evaluator answers, so the
	// ratio is exactly "what the stat says over what it shipped as" — the swing plays the combo at the
	// number the player is reading.
	const float BaseRate = Block ? Block->GetBaseValue(EProsperitocracyStat::Rate) : 0.0f;
	const float FinalRate = GetWeaponStat(EProsperitocracyStat::Rate);

	// Nothing to scale by (an undressed blade, or a block with no Rate): the combo plays at its own
	// speed rather than at a guess.
	if (BaseRate <= 0.0f || FinalRate <= 0.0f)
	{
		return 1.0f;
	}

	return FinalRate / BaseRate;
}

void AProsperitocracyBloodBlade::BeginPlay()
{
	Super::BeginPlay();

	// The blade is looked at, never bumped into. Its damage is its own sweep (which ignores what hangs off
	// the swinger), and a collidable rod the length of an arm, hanging off a body, is a thing the world
	// would keep pushing back out of whatever it is inside — at the cost of the body it is attached to.
	// Off the moment the blade exists, on every component it has.
	SetActorEnableCollision(false);

	// Where it sits in the rig is the rig's own business and never this class's: the character blueprint's
	// equip path is what moves a weapon between the hand and the body, socket by socket, and it is what
	// animates and sounds the move. A weapon that moved its own channel would be a second writer of the
	// same socket — which is exactly how the pistol came to be un-equippable — so this thing only ASKS
	// which weapon is held (IsHeldInRig) and never moves itself.
}

void AProsperitocracyBloodBlade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Whether the player's combo is still running is the combo's own business: the montage IS its clock, and
	// this is the one line that notices the animation has stopped. The BODY's half of an attack is not here
	// at all — it ends on the body's own clock (see AProsperitocracyCharacter::BeginAttack) — so this class
	// holds no movement, no facing and no lock.
	if (bSwinging)
	{
		UAnimInstance* Anim = GetBodyAnimInstance();
		if (!IsInHand() || !Anim || !ComboMontage || !Anim->Montage_IsPlaying(ComboMontage))
		{
			bSwinging = false;
		}
	}

	// Only the blade that is OUT drains, and only what the pool can cover: the tick never goes into
	// debt. The pool only goes DOWN here — refuelling it on kills is the blood mechanic, and it is not
	// built yet.
	if (!IsInHand())
	{
		DrainRemainder = 0.0f;
		return;
	}

	FProsperitocracyWeaponAmmo* Blood = GetBloodStore();
	if (!Blood || Blood->Magazine <= 0)
	{
		return;
	}

	// The blood is spent in whole points and a drain is a fraction of one a second, so the fraction is
	// carried between frames — otherwise every frame's share would round away and nothing would ever
	// leave the pool.
	DrainRemainder += GetDrainPerSecond() * DeltaSeconds;

	const int32 Whole = FMath::FloorToInt(DrainRemainder);
	if (Whole > 0)
	{
		Blood->Magazine = FMath::Max(0, Blood->Magazine - Whole);
		DrainRemainder -= Whole;
	}
}
