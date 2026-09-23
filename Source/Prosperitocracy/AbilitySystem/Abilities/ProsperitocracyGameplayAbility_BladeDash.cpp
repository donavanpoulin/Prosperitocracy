// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility_BladeDash.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/ProsperitocracyCharacter.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyGameplayAbility_BladeDash)

/**
 * The dash's own vocabulary: its two sections, and the arc one Range buys.
 *
 * The sections sit in this ability's own montage in the same order the combo's do — the attack, then
 * its recovery — so the one rule that reads them (the attack's piece is its section, the window is
 * the recovery's own length) is the same rule, asked of a different file. Its own file because these
 * are two different moves: the combo is three attacks that loop, the dash is one that does not and
 * carries a cooldown. One rule, two montages.
 *
 * The arc's fractions are the whole of the shape, and every one of them is read off the ONE Range
 * row: a third of it forwards, half of it side to side, and as tall as it is long. Nothing here is a
 * second number the player can move — a Range perk moves every reading at once, which is the point.
 */
namespace ProsperitocracyBladeDash
{
	/** The attack section — the one attack a dash is. */
	const TCHAR* const Attack = TEXT("dash");

	/** The recovery section: its own window, straight after the attack. */
	const TCHAR* const Recovery = TEXT("dash_rec");

	/**
	 * How much of the attack the body spends covering its Range: the first half, so the dash and the
	 * cut land together and the rest of the attack is spent holding that ground.
	 */
	constexpr float TravelFraction = 0.5f;

	/** The arc's forward length, as a fraction of the Range. */
	constexpr float ReachFraction = 1.0f / 3.0f;

	/** The arc's width, as a fraction of the Range: a metre either side of a body is a metre. */
	constexpr float WidthFraction = 0.5f;

	/** The arc's height: the same third the reach is, so the height is never its own number. */
	constexpr float HeightFraction = ReachFraction;

	/** Range's own unit is meters (Design/stats.md); a body is moved and an arc is sized in cm. */
	constexpr float CentimetersPerMeter = 100.0f;
}

UProsperitocracyGameplayAbility_BladeDash::UProsperitocracyGameplayAbility_BladeDash()
{
	// The move's state has to live BETWEEN the steps of its own clock — which attack, how far into it,
	// the window — so this ability is instanced per actor rather than being a fresh object per
	// activation. No activation policy: nothing presses this by a tag. A WEAPON does, by name.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UProsperitocracyGameplayAbility_BladeDash::IsDashingOn(AActor* Body)
{
	if (!Body)
	{
		return false;
	}

	// The one answer to "is a dash running on that body": asked of the ability system that runs it,
	// never inferred from an animation playing or a body moving.
	const UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Body);
	if (!AbilitySystemComponent)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (const UProsperitocracyGameplayAbility_BladeDash* Dash = Cast<UProsperitocracyGameplayAbility_BladeDash>(Spec.GetPrimaryInstance()))
		{
			if (Dash->IsDashing())
			{
				return true;
			}
		}
	}

	return false;
}

float UProsperitocracyGameplayAbility_BladeDash::GetAttackSeconds() const
{
	if (!DashMontage || !DashMontage->IsValidSectionIndex(0))
	{
		return 0.0f;
	}

	// Its own section, at the rate the player reads off Rate: an attack's length in the world is its
	// own length over that rate, which is the same ratio its animation runs at.
	return DashMontage->GetSectionLength(0) / FMath::Max(GetPlayRate(), KINDA_SMALL_NUMBER);
}

float UProsperitocracyGameplayAbility_BladeDash::GetRecoverySeconds() const
{
	if (!DashMontage || !DashMontage->IsValidSectionIndex(1))
	{
		return 0.0f;
	}

	// The recovery's OWN length at its OWN speed: this is a window in time, not a picture, so the rate
	// the attack played at has nothing to do with it.
	return DashMontage->GetSectionLength(1);
}

float UProsperitocracyGameplayAbility_BladeDash::GetPlayRate() const
{
	// This ability's own Rate row, read the way every rate in the game is read: what the stat says over
	// what it shipped as. The dash's numbers are the dash's — the blade's rows are the combo's — so
	// moving this one row re-times this move and nothing else.
	const float BaseRate = StatBlock ? StatBlock->GetBaseValue(EProsperitocracyStat::Rate) : 0.0f;
	const float FinalRate = GetStatFinalValue(EProsperitocracyStat::Rate);

	if (BaseRate <= 0.0f || FinalRate <= 0.0f)
	{
		return 1.0f;
	}

	return FinalRate / BaseRate;
}

float UProsperitocracyGameplayAbility_BladeDash::GetCooldownSeconds() const
{
	// FINAL through this ability's own GAS home, so a Cooldown perk reaches it like any other stat.
	return GetStatFinalValue(EProsperitocracyStat::Cooldown);
}

float UProsperitocracyGameplayAbility_BladeDash::GetReachCm() const
{
	// One reading of the one Range row, in the unit a body is placed and an arc is sized in. Every
	// other reading comes off the same number, so there is no second Range anywhere to keep in step.
	return GetStatFinalValue(EProsperitocracyStat::Range) * ProsperitocracyBladeDash::CentimetersPerMeter;
}

UAnimInstance* UProsperitocracyGameplayAbility_BladeDash::GetBodyAnimInstance() const
{
	const AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(DashingOn.Get());
	if (!Body)
	{
		return nullptr;
	}

	const USkeletalMeshComponent* Mesh = Body->GetMesh();
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

bool UProsperitocracyGameplayAbility_BladeDash::HasMovementInput() const
{
	// MOVEMENT INPUT, not speed: the rule is about the player asking the body to move, so a body being
	// pushed around by something else keeps its recovery and a player holding a key against a wall does
	// not.
	const APawn* Pawn = Cast<APawn>(DashingOn.Get());
	return Pawn && !Pawn->GetLastMovementInputVector().IsNearlyZero();
}

void UProsperitocracyGameplayAbility_BladeDash::ReleaseTheBodyFacing()
{
	if (AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(DashingOn.Get()))
	{
		Body->ReleaseFacing();
	}
}

void UProsperitocracyGameplayAbility_BladeDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base class brings this ability's GAS home up, which is where every number it owns is
	// evaluated — its Range, its Rate, its Cooldown, its damage and its Pen.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(Avatar);
	const AProsperitocracyWeapon* Blade = Body ? Body->GetGunWeaponInHand() : nullptr;

	if (!Body || !Blade || !StatBlock)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Dash] %s: nothing dashes — %s. This is a bug or an unauthored ability, not a missing feature."),
			*GetPathName(),
			(!Body ? TEXT("there is no body")
				: (!Blade ? TEXT("there is no weapon in hand")
					: TEXT("the ability has no stat block, so it has no Range, no Cooldown and no damage"))));

		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	if (!StartTheDash(Body, Blade))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	// THE MOVE'S OWN CLOCK, one step at a time. The TRAVEL is not here — the body owns it, and it was
	// handed the line, the distance and the two times. What is here is the cut, the window and the
	// recovery's picture, which are this ability's own.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(StepTimerHandle, this, &UProsperitocracyGameplayAbility_BladeDash::DashStep, DashStepSeconds, /*bLoop=*/ true);
	}
}

bool UProsperitocracyGameplayAbility_BladeDash::StartTheDash(AProsperitocracyCharacter* Body, const AProsperitocracyWeapon* Blade)
{
	if (!Body || !Blade || bDashing)
	{
		return false;
	}

	// THE ONE QUESTION EVERY MOVE ASKS, and it covers BOTH of a blade's moves: an attack the player has
	// committed to is never overwritten, and a move's recovery is where he gets to choose the next one.
	// The BODY is the only thing that knows — it is the one holding the attack — so it is asked, rather
	// than any one move's clock being read.
	if (Body->IsSwingLocked())
	{
		UE_LOG(LogProsperitocracy, Verbose, TEXT("[Dash] %s: an attack is live — nothing happens"), *GetPathName());
		return false;
	}

	// THE COOLDOWN, asked before anything is spent, and its answer is a refusal rather than a silence.
	UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < ReadyAt)
	{
		UE_LOG(LogProsperitocracy, Log, TEXT("[Dash] %s: on cooldown — %.2fs of %.2fs left"),
			*GetPathName(), FMath::Max(0.0f, ReadyAt - World->GetTimeSeconds()), GetCooldownSeconds());
		return false;
	}

	if (!Blade->GetDamageEffectClass())
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Dash] %s: the blade in hand carries no damage effect — the dash will hit and hurt nothing."), *GetPathName());
	}

	// The clock he watches starts the moment he spends it: the cooldown belongs to the attack he asked
	// for, not to the animation finishing.
	if (World)
	{
		ReadyAt = World->GetTimeSeconds() + GetCooldownSeconds();
	}

	DashingOn = Body;

	// The LINE, taken once at the press and held: FLAT, so the dash goes along the ground the player is
	// standing on whatever the camera is doing with its pitch, and the body faces it to the end.
	AttackLine = Body->GetControlRotation().Vector().GetSafeNormal2D();

	AttackSeconds = GetAttackSeconds();
	AttackElapsed = 0.0f;
	WindowRemaining = 0.0f;
	bPictureDropped = false;
	CutThisDash.Reset();
	bDashing = true;

	// The picture, at the rate the player reads off Rate. The montage carries its own blend-in, so
	// coming out of the combo's recovery into this is the animation's business and never a hard cut.
	if (UAnimInstance* Anim = GetBodyAnimInstance())
	{
		if (DashMontage)
		{
			Anim->Montage_Play(DashMontage, GetPlayRate(), EMontagePlayReturnType::MontageLength,
				/*InTimeToStartMontageAt=*/ 0.0f, /*bStopAllMontages=*/ false);
			Anim->Montage_JumpToSection(FName(ProsperitocracyBladeDash::Attack), DashMontage);
		}
		else
		{
			UE_LOG(LogProsperitocracy, Warning, TEXT("[Dash] %s: the dash has no montage set — it runs with no picture at all."), *GetPathName());
		}
	}

	// THE BODY'S HALF, handed over and then let go: the line, the distance in the unit a body is moved
	// in, and TWO times — the dash and the attack — because they are not the same thing. The body
	// covers its Range over the part of the attack up to the cut, so the dash and the blade land
	// together, and its own movement is then refused for the whole attack.
	const float RangeCm = GetReachCm();
	const float TheDash = AttackSeconds * ProsperitocracyBladeDash::TravelFraction;

	Body->BeginAttack(AttackLine, RangeCm, TheDash, AttackSeconds);

	++DashCount;
	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Dash] %s: dash #%d — attack %.4fs at x%.3f, Range %.2fm | fan %.2fm forward, %.2fm wide, %.2fm tall | cooldown %.2fs"),
		*GetPathName(), DashCount, AttackSeconds, GetPlayRate(),
		GetStatFinalValue(EProsperitocracyStat::Range),
		RangeCm * ProsperitocracyBladeDash::ReachFraction / ProsperitocracyBladeDash::CentimetersPerMeter,
		RangeCm * ProsperitocracyBladeDash::WidthFraction / ProsperitocracyBladeDash::CentimetersPerMeter,
		RangeCm * ProsperitocracyBladeDash::HeightFraction / ProsperitocracyBladeDash::CentimetersPerMeter,
		GetCooldownSeconds());

	return true;
}

void UProsperitocracyGameplayAbility_BladeDash::DashStep()
{
	if (!bDashing || !DashingOn.IsValid())
	{
		StopTheStepClock();
		return;
	}

	UAnimInstance* Anim = GetBodyAnimInstance();
	const bool bPlaying = Anim && DashMontage && Anim->Montage_IsPlaying(DashMontage);

	// THE ATTACK'S OWN CLOCK, and never the montage's position: the picture can be cut while the move
	// is still running, and whether the attack is still happening is a question about TIME.
	const bool bAttackLive = AttackElapsed < AttackSeconds;
	if (bAttackLive)
	{
		AttackElapsed += DashStepSeconds;
	}

	// THE ATTACK IS OVER, SO THE WINDOW OPENS — for the recovery's own length, at its own speed.
	if (!bAttackLive && WindowRemaining <= 0.0f)
	{
		WindowRemaining = GetRecoverySeconds();
	}

	// The picture's rate follows whichever half of the move is playing: the attack at the rate the
	// player reads, and the recovery always at its own.
	if (bPlaying)
	{
		const bool bInRecovery = Anim->Montage_GetCurrentSection(DashMontage) == FName(ProsperitocracyBladeDash::Recovery);
		Anim->Montage_SetPlayRate(DashMontage, bInRecovery ? 1.0f : GetPlayRate());

		// A RECOVERY IS THE PLAYER'S OWN TIME: moving drops its PICTURE, with the blend the montage
		// itself declares — never a zero-second cut — and never the window.
		if (bInRecovery && !bPictureDropped && HasMovementInput())
		{
			Anim->Montage_StopWithBlendOut(DashMontage->GetBlendOutArgs(), DashMontage);
			ReleaseTheBodyFacing();
			bPictureDropped = true;

			UE_LOG(LogProsperitocracy, Log, TEXT("[Dash] %s: the player moved — the recovery's picture is dropped, the window stays open"), *GetPathName());
		}
	}

	// THE CUT, the whole attack long: the body is travelling THROUGH what it cuts, so the arc is swept
	// as it goes and a man stepping into it anywhere still gets cut.
	if (AttackElapsed <= AttackSeconds)
	{
		CutWhatTheDashIsThrough();
	}

	// THE MOVE ENDS: the window ran out, or nothing is playing with no window left behind it.
	const bool bWindowRanOut = (WindowRemaining > 0.0f) && ((WindowRemaining - DashStepSeconds) <= 0.0f);
	if (WindowRemaining > 0.0f)
	{
		WindowRemaining -= DashStepSeconds;
	}

	if (bWindowRanOut || (WindowRemaining <= 0.0f && !bPlaying))
	{
		ReleaseTheBodyFacing();
		bDashing = false;
		StopTheStepClock();

		// The move is over, so the ability is done with. Ending it here is what lets the same move be
		// pressed again rather than the ability latching on.
		if (IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		}
	}
}

void UProsperitocracyGameplayAbility_BladeDash::StopTheStepClock()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimerHandle);
	}
}

void UProsperitocracyGameplayAbility_BladeDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopTheStepClock();
	bDashing = false;

	// The facing the attack has been holding is let go, so the body turns back to where the player is
	// looking instead of snapping to it.
	ReleaseTheBodyFacing();

	DashingOn = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UProsperitocracyGameplayAbility_BladeDash::CutWhatTheDashIsThrough()
{
	APawn* Pawn = Cast<APawn>(DashingOn.Get());
	UWorld* World = GetWorld();

	const float RangeCm = GetReachCm();
	const FVector Forward = AttackLine;

	if (!Pawn || !World || RangeCm <= 0.0f || Forward.IsNearlyZero())
	{
		return;
	}

	// THE SHAPE IS THE RANGE — the same one row the body's travel comes off — and the arc is a FAN in
	// front of the body: a third of the Range forwards, half of it side to side, as tall as it is long.
	// Its back face sits at the player's eye and it reaches its reach in front of him, and there is
	// nothing at all behind him, because a swing is a swing and not a circle.
	const float LengthCm = RangeCm * ProsperitocracyBladeDash::ReachFraction;
	const float WidthCm = RangeCm * ProsperitocracyBladeDash::WidthFraction;
	const float HeightCm = RangeCm * ProsperitocracyBladeDash::HeightFraction;

	const FVector Eye = Pawn->GetPawnViewLocation();
	const FVector Centre = Eye + Forward * (LengthCm * 0.5f);
	const FQuat Facing = FRotator(0.0f, Forward.Rotation().Yaw, 0.0f).Quaternion();
	const FCollisionShape Fan = FCollisionShape::MakeBox(FVector(LengthCm * 0.5f, WidthCm * 0.5f, HeightCm * 0.5f));

	// The same channel and the same ignores as a shot: never the swinger, never what hangs off him.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BladeDash), /*bTraceComplex=*/ false, Pawn);
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Centre, Facing, ECC_Visibility, Fan, Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Through = Overlap.GetActor();

		// EACH ENEMY ONCE PER DASH: the move remembers who it has already been through, so a body
		// sitting in the fan for twenty steps is cut one time.
		if (!Through || HasAlreadyBeenCut(Through))
		{
			continue;
		}

		CutThisDash.Add(Through);

		// A wall has no ability system: the dash has been through it and that is the end of it.
		UAbilitySystemComponent* Target = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Through);
		if (!Target)
		{
			continue;
		}

		const AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(DashingOn.Get());
		const AProsperitocracyWeapon* Blade = Body ? Body->GetGunWeaponInHand() : nullptr;
		if (!Blade)
		{
			continue;
		}

		const FHitResult Hit(Through, Overlap.GetComponent(), Eye, -Forward);
		ApplyDashToHit(CurrentSpecHandle, CurrentActorInfo, Hit, *Blade);

		UE_LOG(LogProsperitocracy, Log, TEXT("[Dash] %s: the dash caught %s (%s)"),
			*GetPathName(), *GetNameSafe(Through), *GetNameSafe(Overlap.GetComponent()));
	}
}

bool UProsperitocracyGameplayAbility_BladeDash::HasAlreadyBeenCut(const AActor* Actor) const
{
	for (const TWeakObjectPtr<AActor>& Cut : CutThisDash)
	{
		if (Cut.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

void UProsperitocracyGameplayAbility_BladeDash::ApplyDashToHit(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, const FHitResult& Hit, const AProsperitocracyWeapon& Blade)
{
	UAbilitySystemComponent* SourceAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent)
	{
		return;
	}

	// The context carries the hit — the part that was hit for its armour and its resists — and points
	// at THIS ABILITY as the ability source, which is where the one execution reads the damage lines
	// from: this ability's own Piercing Damage and its own Penetration, never the blade's.
	FGameplayEffectContextHandle Context = MakeEffectContext(Handle, ActorInfo);
	Context.AddHitResult(Hit, /*bReset=*/ true);

	// The lines also travel ON the context as data, so a dash resolving later than its instance still
	// carries the right numbers. One evaluator answers both ways.
	AddDamageLinesToContext(Context);

	// And then the same effect and the same pen-gate -> resist pipeline a shot travels. The effect is
	// the weapon's: the loadout owns the one damage effect every weapon's damage travels, and the dash
	// is weapon damage, exactly as the bash is.
	UProsperitocracyDamageStatics::ApplyDamageEffectToHit(Context, Hit.GetActor(), SourceAbilitySystemComponent, Blade.GetDamageEffectClass());
}
