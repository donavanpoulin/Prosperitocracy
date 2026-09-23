// Copyright Prosperitocracy. All Rights Reserved.

#include "Weapons/ProsperitocracyBloodBlade.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "ProsperitocracyGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/ProsperitocracyCharacter.h"
#include "CollisionQueryParams.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyBloodBlade)

/**
 * The combo's own vocabulary: its sections, and the rule their order carries.
 *
 * These are the montage's OWN section names, and they are asked for BY NAME and never by seconds —
 * which is what keeps a press-window safe when Rate moves, because Rate re-times every part of the
 * montage and would move every second-based window with it.
 *
 * The pairing IS the rule: the sections alternate, attack then recovery, so attack n sits at section
 * 2n and its recovery at 2n + 1. A press inside recovery n takes the combo to attack n + 1, and the
 * last recovery leads back to `a` — the combo is a loop.
 */
namespace ProsperitocracyBladeCombo
{
	/** The attack sections, in the order they are swung. A press starts at the first one. */
	const TCHAR* const Attacks[] = { TEXT("a"), TEXT("b"), TEXT("c") };

	/** The recovery sections, in order: recovery n belongs to attack n and leads to attack n + 1. */
	const TCHAR* const Recoveries[] = { TEXT("rec_a"), TEXT("rec_b"), TEXT("rec_c") };

	int32 NumAttacks() { return UE_ARRAY_COUNT(Attacks); }
	int32 NumRecoveries() { return UE_ARRAY_COUNT(Recoveries); }

	/** The montage section an attack's own piece lives at. */
	int32 AttackSection(int32 AttackIndex) { return AttackIndex * 2; }
}

AProsperitocracyBloodBlade::AProsperitocracyBloodBlade()
{
	// A SWORD IS SIMPLY IN THE HAND. It is not drawn like a gun: there is no equip animation to play,
	// no sound to play with it, and no stance for the body to take — the sword appears in the hand it
	// is put in, it follows that hand, and the body goes on running the anims it already runs. The rig
	// asks this of the thing it is bringing out, so the sword is asked and answers for itself.
	bDrawnWithItsOwnAnimation = false;
}

bool AProsperitocracyBloodBlade::IsInHand() const
{
	// The rig decides which weapon is out; this blade only asks. Merely being carried is not being
	// held, and a blade that is stowed reaches nothing and swings nothing.
	return IsHeldInRig();
}

float AProsperitocracyBloodBlade::GetRangeMeters() const
{
	// Range's own unit, read FINAL through this blade's own GAS home, so a Range perk moves the
	// carry, the reach and the width together — they are all this one number.
	return GetWeaponStat(EProsperitocracyStat::Range);
}

float AProsperitocracyBloodBlade::GetRangeCm() const
{
	return GetRangeMeters() * ProsperitocracyBladeHandling::CentimetersPerMeter;
}

float AProsperitocracyBloodBlade::GetComboPlayRate() const
{
	const UProsperitocracyStatTable* Block = GetStatBlock();

	// The base is the block's own authored Rate and the value is what the one evaluator answers, so
	// the ratio is exactly "what the stat says over what it shipped as".
	const float BaseRate = Block ? Block->GetBaseValue(EProsperitocracyStat::Rate) : 0.0f;
	const float FinalRate = GetWeaponStat(EProsperitocracyStat::Rate);

	// Nothing to scale by — no block, or a block with no Rate. The combo plays at its own speed
	// rather than at a guess.
	if (BaseRate <= 0.0f || FinalRate <= 0.0f)
	{
		return 1.0f;
	}

	return FinalRate / BaseRate;
}

float AProsperitocracyBloodBlade::GetAttackSeconds(int32 AttackIndex) const
{
	// An attack's length is ITS OWN piece of the combo and never the whole clip its piece lives in:
	// the section's start to the NEXT section's start. Played at the Rate the player reads, so its
	// length in the world is its own length over that rate — the same ratio the animation runs at.
	if (!ComboMontage || AttackIndex < 0 || AttackIndex >= ProsperitocracyBladeCombo::NumAttacks())
	{
		return 0.0f;
	}

	const int32 Section = ProsperitocracyBladeCombo::AttackSection(AttackIndex);
	if (!ComboMontage->IsValidSectionIndex(Section))
	{
		return 0.0f;
	}

	return ComboMontage->GetSectionLength(Section) / FMath::Max(GetComboPlayRate(), KINDA_SMALL_NUMBER);
}

int32 AProsperitocracyBloodBlade::FindRecoveryIndex(const FName& Section) const
{
	for (int32 Index = 0; Index < ProsperitocracyBladeCombo::NumRecoveries(); ++Index)
	{
		if (Section == FName(ProsperitocracyBladeCombo::Recoveries[Index]))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

UAnimInstance* AProsperitocracyBloodBlade::GetBodyAnimInstance() const
{
	// The body's own anim instance, which is where a montage plays. The visible body is a second mesh
	// that retargets this one, so this is where the player sees the combo from.
	const AProsperitocracyCharacter* Character = Cast<AProsperitocracyCharacter>(OwningPawn.Get());
	if (!Character)
	{
		return nullptr;
	}

	const USkeletalMeshComponent* Body = Character->GetMesh();
	return Body ? Body->GetAnimInstance() : nullptr;
}

bool AProsperitocracyBloodBlade::IsInARecovery() const
{
	UAnimInstance* Anim = GetBodyAnimInstance();
	if (!Anim || !ComboMontage || !Anim->Montage_IsPlaying(ComboMontage))
	{
		return false;
	}

	return FindRecoveryIndex(Anim->Montage_GetCurrentSection(ComboMontage)) != INDEX_NONE;
}

bool AProsperitocracyBloodBlade::HasMovementInput() const
{
	// MOVEMENT INPUT, not speed: the rule is about the player asking the body to move, so a body that
	// is being pushed around by something else does not lose its recovery and a player holding a key
	// against a wall does.
	const APawn* Pawn = Cast<APawn>(OwningPawn.Get());
	return Pawn && !Pawn->GetLastMovementInputVector().IsNearlyZero();
}

FVector AProsperitocracyBloodBlade::GetSwingDirection() const
{
	// What the player is LOOKING at, taken FLAT — the swing goes along the ground he is standing on,
	// whatever the camera is doing with its pitch, and it is taken once, at the press.
	const APawn* Pawn = Cast<APawn>(OwningPawn.Get());
	if (!Pawn)
	{
		return FVector::ZeroVector;
	}

	return Pawn->GetControlRotation().Vector().GetSafeNormal2D();
}

void AProsperitocracyBloodBlade::ReleaseTheBodyFacing()
{
	if (AProsperitocracyCharacter* Character = Cast<AProsperitocracyCharacter>(OwningPawn.Get()))
	{
		Character->ReleaseFacing();
	}
}

bool AProsperitocracyBloodBlade::PlayOrAdvanceCombo()
{
	UAnimInstance* Anim = GetBodyAnimInstance();

	// WHICH attack this press is. Asked of the BLADE'S OWN CLOCK, never of the montage's position:
	// whether an attack is still happening is a question about TIME, and the picture may already have
	// been cut (the player walked out of the recovery) while the combo is still open. Reading this off
	// the montage is exactly what used to make the window die with the picture.
	int32 AttackIndex = INDEX_NONE;

	// No combo to play: no anim instance, or no montage set on this blade. The press is STILL a
	// swing — the numbers must never depend on the animation being there — and it is said out loud,
	// because a swing whose animation never plays is a swing nobody can see.
	if (!Anim || !ComboMontage)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Blade] %s: the swing has no animation to play — %s"),
			*GetName(), !Anim ? TEXT("the body has no anim instance") : TEXT("ComboMontage is not set on this blade"));
		return true;
	}

	// AN ATTACK THE PLAYER HAS COMMITTED TO IS NEVER OVERWRITTEN — EITHER MOVE'S. The body is the one
	// authority on that: it holds the attack, and it is LOCKED for the whole of one, whichever move
	// started it. So a press inside the DASH's own attack does nothing at all, and the only place the
	// combo takes over is the dash's RECOVERY — where the body is unlocked and the player is choosing.
	if (const AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(OwningPawn.Get()))
	{
		if (Body->IsSwingLocked())
		{
			UE_LOG(LogProsperitocracy, Verbose, TEXT("[Blade] %s: an attack is live — nothing happens"), *GetName());
			return false;
		}
	}

	// THE SWING COSTS BLOOD, and it is paid BEFORE the move that is running is stood down: a pool that
	// cannot cover it means the press was never an attack at all — nothing spent, nothing swung, and
	// whatever is playing keeps playing.
	if (!SpendBlood(GetBloodCostPerUse()))
	{
		return false;
	}

	// A NEW ATTACK TAKES THE STAGE, so the MOVE THAT WAS ALREADY GOING ENDS FIRST — the one the right
	// button runs is an ability, and the blade ends it by the name its own slot holds. Nothing of it
	// keeps running underneath this: not its clock, not its window, not its sweep, not its facing.
	EndTheOtherMove();

	if (!bSwinging)
	{
		// NOTHING IS LIVE, so the combo starts at its FIRST attack, and its picture goes on through the
		// BODY's own door: the body takes the last move's picture off with its own blend-out first, so
		// two of our animations can never blend into a pose that is half of each.
		AttackIndex = 0;

		if (AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(OwningPawn.Get()))
		{
			Body->BeginMovePicture(ComboMontage, FName(ProsperitocracyBladeCombo::Attacks[0]), GetComboPlayRate());
		}

		const float Length = ComboMontage ? ComboMontage->GetPlayLength() : 0.0f;

		// The combo's own numbers, said out loud once per start: what each attack is worth in the
		// world at its base speed, and the Rate number the row SHOULD hold for base to mean base.
		++ComboStarts;
		UE_LOG(LogProsperitocracy, Log,
			TEXT("[Blade] %s: combo #%d starts at %s at x%.3f (montage %.2fs) — a %.4fs, b %.4fs, c %.4fs | Rate base %.4f, final %.4f | the row should hold %.6f"),
			*GetName(), ComboStarts, ProsperitocracyBladeCombo::Attacks[0], GetComboPlayRate(), Length,
			GetAttackSeconds(0), GetAttackSeconds(1), GetAttackSeconds(2),
			GetStatBlock() ? GetStatBlock()->GetBaseValue(EProsperitocracyStat::Rate) : 0.0f,
			GetWeaponStat(EProsperitocracyStat::Rate), GetTrueAverageAttackRate());
	}
	else if (AttackElapsed < AttackSeconds)
	{
		// AN ATTACK IS PLAYING, so a press does nothing at all — it never cancels a swing and never
		// stacks one up.
		UE_LOG(LogProsperitocracy, Verbose, TEXT("[Blade] %s: press inside an attack — nothing happens"), *GetName());
		return false;
	}
	else
	{
		// THE ATTACK IS OVER AND THE WINDOW IS OPEN, so this press IS the next attack — from the last
		// one back round to the first, because the combo is a loop. The picture is put on that attack's
		// section whether or not the recovery was still being shown, so a player who walked out of the
		// recovery and kept the combo going gets the attack, not a restart.
		AttackIndex = (CurrentAttack + 1) % ProsperitocracyBladeCombo::NumAttacks();

		// The next attack's picture, through the body's door — the same file, so this is a jump within
		// the montage already playing, and nothing else is left blending beside it.
		if (AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(OwningPawn.Get()))
		{
			Body->BeginMovePicture(ComboMontage, FName(ProsperitocracyBladeCombo::Attacks[AttackIndex]), GetComboPlayRate());
		}

		UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: the window is open — straight to %s"),
			*GetName(), ProsperitocracyBladeCombo::Attacks[AttackIndex]);
	}

	// A NEW ATTACK, so its bite starts over, the window closes behind it, and a body cut by the last
	// attack may be cut by this one.
	CurrentAttack = AttackIndex;
	AttackSeconds = GetAttackSeconds(AttackIndex);
	AttackElapsed = 0.0f;
	WindowRemaining = 0.0f;
	CutThisSwing.Reset();
	bSwinging = true;

	// The BODY's half, and then the blade holds nothing: the line, the distance and the time are the
	// body's from here (it faces the line, travels it, refuses the player's own movement and ends it
	// on its own clock). This class never writes the body's speed, its velocity or its facing.
	BeginTheBodyAttack(AttackIndex);

	return true;
}

void AProsperitocracyBloodBlade::BeginTheBodyAttack(int32 AttackIndex)
{
	AProsperitocracyCharacter* Character = Cast<AProsperitocracyCharacter>(OwningPawn.Get());
	if (!Character)
	{
		return;
	}

	// The numbers half, handed to the BODY: the LINE (what the player was looking at when he pressed,
	// taken flat), the DISTANCE (Range), and TWO times — the DASH and the ATTACK — because they are
	// not the same thing. The body covers its Range over the part of the attack up to the BITE, so the
	// dash and the cut land together, and it is then refused for the whole attack.
	//
	// One number decides both, and it is the same half the blade starts cutting at: the dash ends
	// exactly where the cutting begins, which is the whole point of it. A missing clock or a missing
	// number is handed over as nothing at all, and the body says no to that honestly rather than
	// inventing an attack.
	const float TheAttack = GetAttackSeconds(AttackIndex);
	const float TheDash = TheAttack * ProsperitocracyBladeHandling::BiteStartsAtFraction;

	Character->BeginAttack(GetSwingDirection(), GetRangeCm(), TheDash, TheAttack);
}

bool AProsperitocracyBloodBlade::HasAlreadyBeenCut(const AActor* Actor) const
{
	for (const TWeakObjectPtr<AActor>& Cut : CutThisSwing)
	{
		if (Cut.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

void AProsperitocracyBloodBlade::CutWhatTheSwingIsThrough()
{
	APawn* Pawn = Cast<APawn>(OwningPawn.Get());
	UWorld* World = GetWorld();

	const float RangeCm = GetRangeCm();
	const FVector Forward = GetSwingDirection();
	if (!Pawn || !World || RangeCm <= 0.0f || Forward.IsNearlyZero())
	{
		return;
	}

	// THE SHAPE IS THE RANGE, and there is nothing else to it. A cube one Range on a side sits with
	// its back face at the player's eye and its far face a Range in front of him: the swing reaches a
	// Range, it is a Range WIDE, and there is nothing behind him at all — because a swing is a swing
	// and not a circle. Move Range and all of it moves together.
	const FVector Eye = Pawn->GetPawnViewLocation();
	const FVector Centre = Eye + Forward * (RangeCm * 0.5f);
	const FQuat Facing = FRotator(0.0f, Forward.Rotation().Yaw, 0.0f).Quaternion();
	const FCollisionShape Swing = FCollisionShape::MakeBox(FVector(RangeCm * 0.5f));

	// The same channel and the same ignores as a shot: never the swinger, never what hangs off him
	// (this blade is attached to that body).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BladeSwing), /*bTraceComplex=*/ false, Pawn);
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Centre, Facing, ECC_Visibility, Swing, Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Through = Overlap.GetActor();

		// EACH ENEMY ONCE PER SWING: the swing remembers who it has already been through, so a body
		// sitting inside the blade for ten frames is cut one time, not ten.
		if (!Through || HasAlreadyBeenCut(Through))
		{
			continue;
		}

		CutThisSwing.Add(Through);

		// A wall has no ability system: the swing has been through it and that is the end of it.
		UAbilitySystemComponent* Target = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Through);
		if (!Target)
		{
			continue;
		}

		// The ONE damage path. This blade's own block is the ability source, so its Piercing Damage
		// and its Penetration travel the same effect and the same pen-gate -> resist execution a shot
		// does, and the part that was hit answers with its own armour and its own resist.
		const FHitResult Hit(Through, Overlap.GetComponent(), Eye, -Forward);
		ApplyShotDamage(Hit);

		UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: the swing caught %s (%s)"),
			*GetName(), *GetNameSafe(Through), *GetNameSafe(Overlap.GetComponent()));
	}
}

void AProsperitocracyBloodBlade::BeginPlay()
{
	Super::BeginPlay();

	// Looked at, never bumped into — see the header. Off on every component the blade has.
	SetActorEnableCollision(false);
}

void AProsperitocracyBloodBlade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// THE POOL BLEEDS WHILE THE BLADE IS OUT, and that happens whether or not anything is being swung —
	// so it comes before the combo's own tick, which only runs while a move is live. Standing here
	// holding the sword is what costs you.
	BleedThePool(DeltaSeconds);

	if (!bSwinging)
	{
		return;
	}

	UAnimInstance* Anim = GetBodyAnimInstance();
	const bool bPlaying = Anim && ComboMontage && Anim->Montage_IsPlaying(ComboMontage);

	// THE ATTACK'S OWN CLOCK, kept here and never read off the montage. Whether the attack is still
	// happening is a question about TIME — and that is what makes the window below possible at all:
	// the picture can be cut without the attack's own state being cut with it.
	const bool bAttackLive = AttackElapsed < AttackSeconds;
	if (bAttackLive)
	{
		AttackElapsed += DeltaSeconds;
	}

	// THE ATTACK IS OVER, SO THE WINDOW OPENS — for the RECOVERY'S OWN LENGTH, taken at its own speed
	// and never at the rate the attack played at. A window that scaled with Rate would let attack
	// speed quietly buy recovery speed as well.
	if (!bAttackLive && WindowRemaining <= 0.0f)
	{
		WindowRemaining = GetRecoverySeconds(CurrentAttack);
	}

	// THE PICTURE'S RATE FOLLOWS WHATEVER IS PLAYING: an attack runs at the rate the player reads off
	// Rate, and a recovery always runs at its own speed. One rule, asked every frame, so no section
	// jump can leave the wrong rate behind on the next one.
	if (bPlaying)
	{
		Anim->Montage_SetPlayRate(ComboMontage, IsInARecovery() ? 1.0f : GetComboPlayRate());
	}

	// A RECOVERY IS THE PLAYER'S OWN TIME, and MOVING ENDS THE PICTURE — with the blend the montage
	// itself declares, and never with a zero-second cut. The combo carries its own blend-out (a
	// quarter of a second, eased): forcing zero here threw that away and jumped the body straight from
	// the sword's pose into running, which is a thing the player sees and hates. The animation says how
	// it leaves; this asks it to leave that way.
	//
	// IT DOES NOT END THE WINDOW. The window is time, and time does not stop because a man walked —
	// which is the whole point: it is what lets him hold a direction and keep the combo going.
	if (bPlaying && IsInARecovery() && HasMovementInput())
	{
		Anim->Montage_StopWithBlendOut(ComboMontage->GetBlendOutArgs(), ComboMontage);
		ReleaseTheBodyFacing();

		UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: the player moved — the recovery's picture is dropped, the window stays open"), *GetName());
	}

	// THE BITE. From the second half of the attack to its end, and no longer: the wind-up is cocked
	// back and hurts nothing, and by the halfway point the blade is through the arc.
	if (CurrentAttack != INDEX_NONE
		&& AttackElapsed >= AttackSeconds * ProsperitocracyBladeHandling::BiteStartsAtFraction
		&& AttackElapsed <= AttackSeconds)
	{
		CutWhatTheSwingIsThrough();
	}

	// THE COMBO ENDS in one of two ways, and both mean the same thing: the window has run out, or the
	// picture has stopped and there is no window left behind it. Either way the facing goes with it.
	if (WindowRemaining > 0.0f)
	{
		WindowRemaining -= DeltaSeconds;
		if (WindowRemaining <= 0.0f)
		{
			EndTheCombo();
		}
	}
	else if (!bPlaying)
	{
		EndTheCombo();
	}
}

void AProsperitocracyBloodBlade::EndTheCombo()
{
	bSwinging = false;
	CurrentAttack = INDEX_NONE;
	WindowRemaining = 0.0f;

	// The facing the attack has been holding is let go, so the body turns back to where the player is
	// looking instead of snapping to it.
	ReleaseTheBodyFacing();
}

float AProsperitocracyBloodBlade::GetRecoverySeconds(int32 AttackIndex) const
{
	// The recovery's OWN length, at its OWN speed: this is a window in time, not a picture, so the
	// rate the attacks play at has nothing to do with it.
	if (!ComboMontage || AttackIndex < 0 || AttackIndex >= ProsperitocracyBladeCombo::NumAttacks())
	{
		return 0.0f;
	}

	const int32 Section = ProsperitocracyBladeCombo::AttackSection(AttackIndex) + 1;
	return ComboMontage->IsValidSectionIndex(Section) ? ComboMontage->GetSectionLength(Section) : 0.0f;
}

float AProsperitocracyBloodBlade::GetTrueAverageAttackRate() const
{
	// The three attacks over their own lengths, untouched by any rate: this is what Rate IS, so it is
	// the number the row holds. Everything the player reads off Rate is a ratio against this, which is
	// why the row has to hold the real thing and not an approximation of it.
	if (!ComboMontage)
	{
		return 0.0f;
	}

	const float Total = ComboMontage->GetSectionLength(ProsperitocracyBladeCombo::AttackSection(0))
		+ ComboMontage->GetSectionLength(ProsperitocracyBladeCombo::AttackSection(1))
		+ ComboMontage->GetSectionLength(ProsperitocracyBladeCombo::AttackSection(2));

	return Total > KINDA_SMALL_NUMBER
		? static_cast<float>(ProsperitocracyBladeCombo::NumAttacks()) / Total
		: 0.0f;
}

//~ The blood pool -----------------------------------------------------------------------------------
//
// The pool is how a blade that is LANDING ITS HITS pays for itself and a blade that is standing still
// starves: every use takes blood, holding the thing bleeds it away, and real damage dealt puts it back.

FProsperitocracyWeaponAmmo* AProsperitocracyBloodBlade::GetTheBloodStore()
{
	// One store per slot, on the carrier, exactly where a gun's magazine lives — and for the same
	// reason: the rig rebuilds weapon actors, so a pool kept on this actor would come back full every
	// time. No Capacity is asked for (a sword carries no spare magazine), so the round fields of the
	// store are never used for blood.
	return OwnerLoadout ? &OwnerLoadout->GetOrCreateAmmoForSlot(Slot, FMath::RoundToInt(GetMaxBlood()), 0) : nullptr;
}

float AProsperitocracyBloodBlade::GetMaxBlood() const
{
	// How big the pool is: this blade's own MagSize row, read FINAL — so "magsize upgrades" is exactly
	// what it sounds like for a sword. Nothing else says how much blood there is.
	return FMath::Max(0.0f, GetWeaponStat(EProsperitocracyStat::MagSize));
}

float AProsperitocracyBloodBlade::GetBlood()
{
	FProsperitocracyWeaponAmmo* Store = GetTheBloodStore();
	if (!Store)
	{
		return 0.0f;
	}

	// The first ask is what fills it: a blade comes out with a full pool, and nothing else fills it but
	// the damage its body deals.
	if (!bBloodSeeded)
	{
		Store->Blood = GetMaxBlood();
		bBloodSeeded = true;
	}

	return Store->Blood;
}

void AProsperitocracyBloodBlade::AddBlood(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	FProsperitocracyWeaponAmmo* Store = GetTheBloodStore();
	if (!Store)
	{
		return;
	}

	// Capped at the pool's own size: a pool is never overfilled, exactly as a magazine never holds more
	// rounds than it is worth.
	Store->Blood = FMath::Clamp(GetBlood() + Amount, 0.0f, GetMaxBlood());
}

float AProsperitocracyBloodBlade::GetBloodDrainPerSecond() const
{
	return FMath::Max(0.0f, GetWeaponStat(EProsperitocracyStat::BloodDrain));
}

float AProsperitocracyBloodBlade::GetBloodCostPerUse() const
{
	return FMath::Max(0.0f, GetWeaponStat(EProsperitocracyStat::BloodCost));
}

void AProsperitocracyBloodBlade::BleedThePool(float DeltaSeconds)
{
	const float Drain = GetBloodDrainPerSecond();
	if (Drain <= 0.0f || DeltaSeconds <= 0.0f || !IsInHand())
	{
		return;
	}

	FProsperitocracyWeaponAmmo* Store = GetTheBloodStore();
	if (!Store)
	{
		return;
	}

	// A RATE, taken every frame: the pool goes down smoothly rather than in units, which is what the
	// bar is drawn from. A stowed blade costs nothing — carrying it is free, holding it is not.
	const float Pool = GetBlood();
	const float Taken = Drain * DeltaSeconds;

	if (Pool >= Taken)
	{
		Store->Blood = Pool - Taken;
		return;
	}

	// AND WHEN THE POOL RUNS OUT THE BLEED KEEPS RUNNING: with the blood mode on it comes off the BODY,
	// because the mode is a promise that the sword is fed one way or the other. With it off, an empty
	// pool is simply empty and nothing further is owed.
	Store->Blood = 0.0f;
	if (bBloodMode)
	{
		DrainTheBody(Taken - Pool);
	}
}

void AProsperitocracyBloodBlade::ReloadAction_Implementation()
{
	// Only the blade in the player's hand answers the key: a stowed blade is not what it is about.
	if (!IsInHand())
	{
		return;
	}

	// THE BLOOD MODE. Blood is what a sword "reloads", and this key is the clean one to say it on: the
	// key asks the thing in hand, a gun's own answer owns its magazine swap AND its own reload
	// animation, so a sword's answer is the whole of what the press does — and the melee key is left
	// free for the sword's parry.
	SetBloodMode(!bBloodMode);
}

void AProsperitocracyBloodBlade::SetBloodMode(bool bOn)
{
	bBloodMode = bOn;

	UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: blood mode %s — the pool pays first and the body pays the rest"),
		*GetName(), bBloodMode ? TEXT("ON") : TEXT("OFF"));
}

bool AProsperitocracyBloodBlade::DrainTheBody(float Amount)
{
	if (Amount <= 0.0f)
	{
		return false;
	}

	AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(OwningPawn.Get());
	UProsperitocracyAbilitySystemComponent* AbilitySystemComponent = Body
		? Cast<UProsperitocracyAbilitySystemComponent>(Body->GetAbilitySystemComponent()) : nullptr;
	const UProsperitocracyHealthSet* HealthSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UProsperitocracyHealthSet>() : nullptr;

	if (!AbilitySystemComponent || !HealthSet)
	{
		// Nothing to take it out of — a body with no health of its own. Said out loud rather than
		// silently: the sword asked for blood and the body had none to give.
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Blade] %s: the pool is dry and the body has no health set to pay from."), *GetName());
		return false;
	}

	// THE PRICE IS REAL DAMAGE, and that is the whole reason it goes through here instead of being written
	// onto Health. Every other thing in this game that takes health off a player TELLS contested health
	// what it took, at the one place damage becomes real — and this was the one that did not, so the
	// sword's price was the only damage in the game that left nothing winnable behind it. Now it does:
	// what the body pays becomes CONTESTED, and the Reclaimer wins it back with the same damage-dealt
	// recovery every other hit feeds. Overspending stops being a flat tax and becomes the fight it should be.
	//
	// It carries the SWORD'S OWN TYPE, and a line IS its type — there is no untyped damage here. Piercing,
	// with NO PEN, which is exactly right against a player: a pen is meaningless on a body with no armour,
	// so a line carrying none reaches the body ITSELF and is answered by the body's own Piercing Resist.
	// That is the accepted catch of routing it through the one pipeline — a weave or a perk built against
	// Piercing makes the sword cheaper to run, which is a real Reclaimer advantage.
	//
	// AND THE CONTEXT IS BUILT BY HAND WITH NOBODY AS ITS INSTIGATOR. Deliberately, and this is the one
	// thing that must not be got wrong: the ability system's own MakeEffectContext stamps the body as the
	// instigator, and the pipeline credits the instigator with winning health back AND credits the body's
	// blade with refilling its pool. A self-inflicted cost credited to yourself would hand back an eighth
	// of the health it took and a sixth of the blood it spent — cancelling the cost outright. Nobody dealt
	// this damage, so nobody is paid for it.
	if (!GetDamageEffectClass())
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Blade] %s: the pool is dry and there is no damage effect to charge the body with — the cost is NOT taken. This is a bug, not a missing feature."),
			*GetName());
		return false;
	}

	FGameplayEffectContextHandle Context = FGameplayEffectContextHandle(
		new FProsperitocracyGameplayEffectContext(/*InInstigator=*/ nullptr, /*InEffectCauser=*/ nullptr));

	FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context);
	if (!TypedContext)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Blade] %s: the effect context is not ours, so the cost could not be charged. Check AbilitySystemGlobalsClassName in DefaultGame.ini."),
			*GetName());
		return false;
	}

	// This blade is the source, exactly as it is for a swing, so the one evaluator answers the falloff and
	// the line below is the damage. There is no hit result because nothing was struck — the body is being
	// charged by the thing it is holding.
	TypedContext->SetAbilitySource(StatHost, 1.0f);
	TypedContext->AddDamageLine(ProsperitocracyGameplayTags::Damage_Type_Piercing, /*InPenTier=*/ 0, Amount);

	// And then the ONE applier every hit in the game travels — the same pen-gate → resist path, the damage
	// onto Health, and the two halves of contested health told at the one place damage becomes real.
	UProsperitocracyDamageStatics::ApplyDamageEffectToHit(Context, Body, AbilitySystemComponent, GetDamageEffectClass());

	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Blade] %s: the pool is dry — the body is charged %.1f Piercing with no pen, so its own resist answers it and what it takes becomes contested"),
		*GetName(), Amount);

	return true;
}

bool AProsperitocracyBloodBlade::SpendBlood(float Cost)
{
	if (Cost <= 0.0f)
	{
		return true;
	}

	FProsperitocracyWeaponAmmo* Store = GetTheBloodStore();
	if (!Store)
	{
		return false;
	}

	const float Pool = GetBlood();
	if (Pool < Cost)
	{
		// THE BLOOD MODE is the one thing that answers a short pool: what blood there is goes in and the
		// BODY pays the rest — nothing is refused while the mode is on, which is the whole of what the
		// toggle buys. With it off, the pool pays for the whole thing or the thing does not happen at
		// all, said out loud, because a swing that does nothing and a swing that is broken look the same
		// from the outside.
		if (bBloodMode)
		{
			const float Shortfall = Cost - Pool;
			Store->Blood = 0.0f;
			DrainTheBody(Shortfall);
			return true;
		}

		UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: not enough blood — the use needs %.1f and the pool holds %.1f"),
			*GetName(), Cost, Pool);
		return false;
	}

	Store->Blood = Pool - Cost;
	return true;
}

void AProsperitocracyBloodBlade::NotifyDamageDealt(AActor* Dealer, float DamageDealt)
{
	if (!Dealer || DamageDealt <= 0.0f)
	{
		return;
	}

	AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(Dealer);
	if (!Body)
	{
		return;
	}

	// The blade this body CARRIES — not merely the thing in its hand — so a kill with the secondaries
	// feeds the pack exactly as a sword kill does (Design/classes/reclaimer.md: anything that dies to
	// him refuels it). It is found by asking each channel's weapon what it is: the same walk the equip
	// path and the dress use, so no list of component names is kept anywhere.
	TArray<UChildActorComponent*> Channels;
	Body->GetComponents(Channels);

	for (UChildActorComponent* Channel : Channels)
	{
		if (AProsperitocracyBloodBlade* Blade = Channel ? Cast<AProsperitocracyBloodBlade>(Channel->GetChildActor()) : nullptr)
		{
			Blade->AddBlood(DamageDealt * ProsperitocracyBladeHandling::BloodFromDamageDealt);
			return;
		}
	}
}

void AProsperitocracyBloodBlade::PrimaryAction_Implementation()
{
	// Only the blade in the player's hand swings: a stowed blade reaches nothing and spends nothing.
	if (!IsInHand())
	{
		return;
	}

	// The animation half decides whether this press IS an attack — nothing playing starts the combo,
	// a press inside a recovery takes it on, a press inside an attack does nothing at all.
	if (!PlayOrAdvanceCombo())
	{
		return;
	}

	// The numbers half is NOT here: the cut happens AS THE BLADE GOES THROUGH, over the second half
	// of the attack — see Tick. A swing that dealt its damage on the press would cut a body before
	// the blade had moved at all.
	//
	// The release half is not here either, and that is the truth about a sword: there is nothing a
	// swing owes the moment the button comes up. `ReloadAction`, inherited from the weapon, does
	// nothing — a melee never reloads.
}

void AProsperitocracyBloodBlade::SecondaryAction_Implementation()
{
	// THE BUTTON IS DOWN, recorded on the thing it was pressed on — for every weapon, melee included,
	// and BEFORE anything below can return. That is the whole of what a HOLD is: a move that runs while
	// the button is down (the heavy combo) asks this weapon every step whether it still is, so nothing
	// has to route a release back to a running ability.
	Super::SecondaryAction_Implementation();

	// Only the blade in the player's hand does anything on the right press: a stowed blade owes
	// nothing to a button.
	if (!IsInHand())
	{
		return;
	}

	// THE LOADOUT LENT IT THE ABILITY; THE ABILITY IS THE MOVE. The slot is the weapon's and it was
	// dressed from a loadout's four ability slots, so which move this is belongs to what the player took
	// in and never to this class. A null slot is a real answer — a loadout that lends this blade no move
	// means the right press does nothing — and it is said out loud, because a press that does nothing
	// and a press that is broken look the same from the outside.
	const TSubclassOf<UProsperitocracyGameplayAbility> SecondPress = GetSecondPressAbility();
	if (!SecondPress)
	{
		UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: no loadout lent this blade a right-click move, so the press does nothing."), *GetName());
		return;
	}

	// The body's ability system runs it, and everything about the move is the ability's own: whether
	// it may start at all, its numbers, its animation. This class never learns what it asked for.
	const UProsperitocracyAbilitySystemComponent* AbilitySystemComponent = Cast<UProsperitocracyAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn.Get()));
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Blade] %s: the body has no ability system, so its right-click ability cannot run."), *GetName());
		return;
	}

	const_cast<UProsperitocracyAbilitySystemComponent*>(AbilitySystemComponent)->TryActivateAbilityByClass(SecondPress);
}

void AProsperitocracyBloodBlade::StandDownForANewMove()
{
	// THE COMBO IS OVER: something else is taking the stage on this body. The picture is the body's to
	// swap (see BeginMovePicture) and the facing is taken by the move that is starting, so all this
	// owes is its own state — no press is an attack any more, nothing goes on biting, and the window
	// behind it closes.
	if (bSwinging)
	{
		UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: another move is starting — the combo stands down"), *GetName());
		EndTheCombo();
	}
}

void AProsperitocracyBloodBlade::EndTheOtherMove()
{
	const TSubclassOf<UProsperitocracyGameplayAbility> SecondPress = GetSecondPressAbility();
	if (!SecondPress)
	{
		return;
	}

	UProsperitocracyAbilitySystemComponent* AbilitySystemComponent = Cast<UProsperitocracyAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn.Get()));
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Cancelling it IS ending the move: the ability's own EndAbility stops its clock, drops its window
	// and lets the facing go — so a move the player is no longer in never keeps working beneath him.
	if (const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(SecondPress))
	{
		if (Spec->IsActive())
		{
			UE_LOG(LogProsperitocracy, Log, TEXT("[Blade] %s: the right-click move is standing down for the combo"), *GetName());
			AbilitySystemComponent->CancelAbilityHandle(Spec->Handle);
		}
	}
}
