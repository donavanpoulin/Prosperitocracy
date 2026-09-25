// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility_Bash.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Character/ProsperitocracyCharacter.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyGameplayAbility_Bash)

namespace
{
	/** Range is in meters (the stat's own unit); a trace is in cm. */
	constexpr float CentimetersPerMeter = 100.0f;
}

UProsperitocracyGameplayAbility_Bash::UProsperitocracyGameplayAbility_Bash()
{
	// Nothing to configure here: the bash's numbers ARE its stat block, assigned on the Blueprint
	// child of this class (Impact Damage, Penetration, Range). A bash with no block is a bug, and
	// ActivateAbility says so.
}

void UProsperitocracyGameplayAbility_Bash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base class brings the ability's GAS home up (its stat host), which is where every number
	// this ability owns is evaluated.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const AProsperitocracyCharacter* Character = Cast<AProsperitocracyCharacter>(Avatar);
	APawn* AvatarPawn = Cast<APawn>(Avatar);

	// The bash is the GUN's bash, and it is only worth anything with its own numbers: no gun in hand,
	// or no stat block (no reach, no Penetration), means nothing swings.
	const AProsperitocracyWeapon* Gun = Character ? Character->GetGunWeaponInHand() : nullptr;

	// AND THE THING IN HAND HAS TO BE A GUN. A bash is a FIREARM's melee — the hit you get with a gun in
	// your hands — so a melee in hand has no bash at all: it has its own moves, and its own key answers
	// (the blade runs its right-click ability and toggles the blood mode on the melee key). The test is
	// the project's own — a gun is a weapon with a fire mode — so nothing here names a weapon type, and
	// a sword can never reach a rifle's swing through this ability.
	//
	// Quietly, because a melee in hand is not a fault: it is a sword doing what a sword does.
	if (Gun && !Gun->HasAFireMode())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	if (!Gun || !AvatarPawn || !StatBlock || !StatHost)
	{
		if (!bLoggedMissingSetup)
		{
			bLoggedMissingSetup = true;
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("[Bash] %s: nothing swings — %s. This is a bug, not a missing feature."),
				*GetPathName(),
				(!Gun ? TEXT("no gun in hand") : TEXT("the ability has no stat block set, so it has no Range and no Penetration")));
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	// THE REACH IS THE RANGE STAT: meters, one base number the same on every gun, read through this
	// ability's own GAS home — so a Range perk extends the bash with no code change anywhere.
	const float ReachMeters = GetStatFinalValue(EProsperitocracyStat::Range);
	if (ReachMeters <= 0.0f)
	{
		if (!bLoggedMissingSetup)
		{
			bLoggedMissingSetup = true;
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("[Bash] %s: its stat block carries no Range, so the swing reaches nothing. Give the ability a Range (meters)."),
				*GetPathName());
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	// What the bash is worth before perks: 20 x the gun's FINAL Weight, as this ability's own Impact
	// Damage base (see PushDerivedDamageBase). Perks land above it in the execution.
	PushDerivedDamageBase(*Gun);

	UWorld* World = GetWorld();
	if (World)
	{
		// The swing reaches from the player's EYE along the direction the man is actually pointing, so a
		// bash lands where the shot would and where the reticle circle sits. The camera sits a boom
		// length behind the player, so the eye is the origin — a camera-origin sweep would never leave
		// the character's own back.
		const FVector SwingStart = AvatarPawn->GetPawnViewLocation();
		const FVector SwingEnd = SwingStart + Gun->GetAimDirection() * (ReachMeters * CentimetersPerMeter);

		// The same channel and the same ignores as the shot: never the shooter, never the gun in its
		// hands.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AbilityBash), /*bTraceComplex=*/ true, AvatarPawn);
		TArray<AActor*> AttachedActors;
		AvatarPawn->GetAttachedActors(AttachedActors);
		Params.AddIgnoredActors(AttachedActors);

		const FCollisionShape Swing = FCollisionShape::MakeSphere(ProsperitocracyBashHandling::SwingRadius);

		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, SwingStart, SwingEnd, FQuat::Identity, ECC_Visibility, Swing, Params) && Hit.bBlockingHit)
		{
			ApplyBashToHit(Handle, ActorInfo, Hit, *Gun);
		}
	}

	// One swing, one activation — the bash is instant. Everything the player SEES of it is the body's:
	// the body plays the melee montage for the gun it holds, and the body's own "Is Attacking?" lock is
	// what refuses a second swing mid-animation. This ability owns the swing and the numbers, never the
	// animation.
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}

void UProsperitocracyGameplayAbility_Bash::PushDerivedDamageBase(const AProsperitocracyWeapon& Gun)
{
	if (!StatHost)
	{
		return;
	}

	// The gun's FINAL Weight, through the gun's OWN GAS home (the ONE evaluator): a Weight perk is
	// already in this number, which is the whole reason the formula reads a final value and not a base.
	const float WeightLbs = FMath::Max(0.0f, Gun.GetWeaponStat(EProsperitocracyStat::Weight));

	// One formula, one universal constant, every gun — and the result IS this ability's Impact Damage:
	// written as its base, so perks modify the bash from here exactly as they modify a gun's Piercing
	// Damage. A Weight perk moves the base; an Impact Damage perk moves the value above it; neither is
	// a second damage path.
	StatHost->SetStatBase(EProsperitocracyStat::ImpactDamage, WeightLbs * ProsperitocracyBashHandling::DamagePerPound);
}

void UProsperitocracyGameplayAbility_Bash::ApplyBashToHit(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, const FHitResult& Hit, const AProsperitocracyWeapon& Gun)
{
	UAbilitySystemComponent* SourceAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent)
	{
		return;
	}

	// The context carries the hit — the impact distance for the falloff, the part that was hit for its
	// armor and its resists — and points at THIS ABILITY as the ability source, which is where the
	// shared execution reads the damage lines from (this ability's block: its Impact Damage, base plus
	// every perk on it, and its Penetration).
	FGameplayEffectContextHandle Context = MakeEffectContext(Handle, ActorInfo);
	Context.AddHitResult(Hit, /*bReset=*/ true);

	// The lines also travel ON the context as data, so a bash that resolves later than its ability
	// instance still carries the right numbers. One evaluator answers both ways.
	AddDamageLinesToContext(Context);

	// ...and then the same effect and the same pen-gate -> resist pipeline a shot travels: one damage
	// pipeline, not one per way of hitting something. The effect is the gun's — the loadout owns the
	// one damage effect every gun's damage travels, and the bash is gun damage.
	UProsperitocracyDamageStatics::ApplyDamageEffectToHit(Context, Hit.GetActor(), SourceAbilitySystemComponent, Gun.GetDamageEffectClass());
}
