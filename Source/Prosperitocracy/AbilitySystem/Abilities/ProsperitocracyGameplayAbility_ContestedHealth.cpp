// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility_ContestedHealth.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "Engine/World.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyGameplayAbility_ContestedHealth)

namespace
{
	/**
	 * How often the pool is walked down.
	 *
	 * Not a game number and not a tunable: it is the clock's own resolution, and the fade itself is
	 * ALWAYS pool ÷ window per second whatever this is. Small enough that the bar slides rather than
	 * steps.
	 */
	constexpr float FadeTickSeconds = 0.05f;
}

UProsperitocracyGameplayAbility_ContestedHealth::UProsperitocracyGameplayAbility_ContestedHealth()
{
	// A passive: it turns itself on when the body has an avatar, and nothing ever presses it, so there is
	// no slot to sit in (Design/stats.md).
	ActivationPolicy = EProsperitocracyAbilityActivationPolicy::OnSpawn;

	// One of these per body: the pool belongs to the body, so the instance does too.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UProsperitocracyGameplayAbility_ContestedHealth::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Nothing is contested until something hits this body.
	Contested = 0.0f;
	DrainPerSecond = 0.0f;
	Phase = EPhase::Idle;
	PhaseSecondsRemaining = 0.0f;

	// The clock, running for as long as the body lives. It does nothing while the pool is empty, so it
	// costs nothing to leave on.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FadeTimerHandle, this, &ThisClass::FadeStep, FadeTickSeconds, /*bLoop=*/ true);
	}
}

void UProsperitocracyGameplayAbility_ContestedHealth::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// A body that goes away takes its clock with it.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FadeTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UProsperitocracyHealthSet* UProsperitocracyGameplayAbility_ContestedHealth::GetHealthSet() const
{
	// The body's own health rows — what the recovery is written back to.
	const UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	return AbilitySystemComponent ? const_cast<UProsperitocracyHealthSet*>(AbilitySystemComponent->GetSet<UProsperitocracyHealthSet>()) : nullptr;
}

float UProsperitocracyGameplayAbility_ContestedHealth::GetWindowSeconds() const
{
	// The window is the Duration row of this ability's OWN block, read FINAL through the one evaluator: a
	// Duration perk widens it, and a wider window is pure upside — the same pool takes longer to slip
	// away. No block means no Duration, which means no window and nothing ever contested (presence is
	// scope).
	return GetStatFinalValue(EProsperitocracyStat::Duration);
}

void UProsperitocracyGameplayAbility_ContestedHealth::StartSit()
{
	// The pool sits still for a whole Duration — and that sit IS the same Duration the drain uses, not a
	// second number to author and tune beside it.
	Phase = EPhase::Sitting;
	PhaseSecondsRemaining = GetWindowSeconds();
	DrainPerSecond = 0.0f;
}

void UProsperitocracyGameplayAbility_ContestedHealth::TakeContested(float DamageTaken)
{
	if (DamageTaken <= 0.0f)
	{
		return;
	}

	if (GetWindowSeconds() <= 0.0f)
	{
		// No Duration on the block, so there is no window to hold anything across. Said out loud because
		// a body that takes hits and contests nothing is an authoring bug, not a choice.
		UE_LOG(LogProsperitocracy, Verbose, TEXT("[Contested] %s took %.1f but its contested ability has no Duration (no block?) — nothing became contested."),
			*GetNameSafe(GetAvatarActorFromActorInfo()), DamageTaken);
		return;
	}

	// The hit ADDS onto whatever is left, and the pool goes back to sitting still: during the sit the
	// pause simply starts over at full, and during the drain this is a fresh hit — the drain is called off
	// and the pool sits again before any of it can leave.
	Contested += DamageTaken;
	StartSit();

	UE_LOG(LogProsperitocracy, Log, TEXT("[Contested] %s: took %.1f — holding %.1f contested, sitting %.2fs then draining over %.2fs"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), DamageTaken, Contested, PhaseSecondsRemaining, GetWindowSeconds());
}

void UProsperitocracyGameplayAbility_ContestedHealth::RecoverFromDamageDealt(float DamageDealt)
{
	if (DamageDealt <= 0.0f || Contested <= 0.0f)
	{
		// Nothing to win back: no pool means the drain already took it, and damage dealt does not create
		// contested health — only damage TAKEN does.
		return;
	}

	UProsperitocracyHealthSet* HealthSet = GetHealthSet();
	if (!HealthSet)
	{
		return;
	}

	// A flat share of what you dealt, capped at what is still contested (the pool is the ceiling) and
	// never past your own maximum — the health you win back is health you lost, and nothing more.
	const float Wanted = DamageDealt * ShareOfDamageDealtRecovered;
	const float Missing = FMath::Max(0.0f, HealthSet->GetMaxHealth() - HealthSet->GetHealth());
	const float Recovered = FMath::Min3(Wanted, Contested, Missing);
	if (Recovered <= 0.0f)
	{
		return;
	}

	HealthSet->SetHealth(HealthSet->GetHealth() + Recovered);
	Contested -= Recovered;

	// Winning health back shortens the pool, not the speed: the drain was set from the pool this window
	// began with, so a smaller pool simply finishes sooner.
	if (Contested <= 0.0f)
	{
		Contested = 0.0f;
		DrainPerSecond = 0.0f;
		Phase = EPhase::Idle;
		PhaseSecondsRemaining = 0.0f;
	}

	UE_LOG(LogProsperitocracy, Log, TEXT("[Contested] %s: dealt %.1f -> won back %.1f — %.1f still contested"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), DamageDealt, Recovered, Contested);
}

void UProsperitocracyGameplayAbility_ContestedHealth::FadeStep()
{
	if (Contested <= 0.0f)
	{
		Contested = 0.0f;
		DrainPerSecond = 0.0f;
		Phase = EPhase::Idle;
		PhaseSecondsRemaining = 0.0f;
		return;
	}

	const float Window = GetWindowSeconds();
	PhaseSecondsRemaining -= FadeTickSeconds;

	if (Phase == EPhase::Sitting)
	{
		// The sit: nothing at all comes off. That is the pause, and it is the Duration itself.
		if (PhaseSecondsRemaining > 0.0f)
		{
			return;
		}

		// And now it drains. The speed is worked out HERE, once, from the pool the sit was holding: the
		// pool over the Duration. Never a stat, and never re-derived while the drain is running.
		Phase = EPhase::Draining;
		PhaseSecondsRemaining += Window;
		DrainPerSecond = (Window > 0.0f) ? (Contested / Window) : 0.0f;

		UE_LOG(LogProsperitocracy, Log, TEXT("[Contested] %s: the sit is over — %.1f contested, draining at %.1f/s over %.2fs"),
			*GetNameSafe(GetAvatarActorFromActorInfo()), Contested, DrainPerSecond, Window);
		return;
	}

	if (Phase == EPhase::Draining)
	{
		Contested = FMath::Max(0.0f, Contested - (DrainPerSecond * FadeTickSeconds));

		if (PhaseSecondsRemaining <= 0.0f || Contested <= 0.0f)
		{
			// Gone: the window is over, and what was left of the chance to win it back went with it. The
			// health itself went when the hit landed — which is why none of this ever saves you from a
			// lethal blow.
			Contested = 0.0f;
			DrainPerSecond = 0.0f;
			Phase = EPhase::Idle;
			PhaseSecondsRemaining = 0.0f;

			UE_LOG(LogProsperitocracy, Verbose, TEXT("[Contested] %s: nothing left contested."), *GetNameSafe(GetAvatarActorFromActorInfo()));
		}
	}
}

void UProsperitocracyGameplayAbility_ContestedHealth::NotifyDamageTaken(AActor* Body, float DamageTaken)
{
	if (!Body || DamageTaken <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Body);
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Found by walking what the body actually has, not by asking for our C++ class: the ability is a
	// blueprint child of this class (that is where its block lives), so the granted class is not ours.
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (UProsperitocracyGameplayAbility_ContestedHealth* Ability = Cast<UProsperitocracyGameplayAbility_ContestedHealth>(Spec.GetPrimaryInstance()))
		{
			Ability->TakeContested(DamageTaken);
			return;
		}
	}
}

void UProsperitocracyGameplayAbility_ContestedHealth::NotifyDamageDealt(AActor* DamageDealer, float DamageDealt)
{
	if (!DamageDealer || DamageDealt <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DamageDealer);
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (UProsperitocracyGameplayAbility_ContestedHealth* Ability = Cast<UProsperitocracyGameplayAbility_ContestedHealth>(Spec.GetPrimaryInstance()))
		{
			Ability->RecoverFromDamageDealt(DamageDealt);
			return;
		}
	}
}

float UProsperitocracyGameplayAbility_ContestedHealth::GetContestedHealth(AActor* Body)
{
	if (!Body)
	{
		return 0.0f;
	}

	const UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Body);
	if (!AbilitySystemComponent)
	{
		return 0.0f;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (const UProsperitocracyGameplayAbility_ContestedHealth* Ability = Cast<UProsperitocracyGameplayAbility_ContestedHealth>(Spec.GetPrimaryInstance()))
		{
			return Ability->GetContested();
		}
	}

	return 0.0f;
}
