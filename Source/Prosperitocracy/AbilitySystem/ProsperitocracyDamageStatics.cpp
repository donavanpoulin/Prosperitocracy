// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyDamageStatics.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatusComponent.h"
#include "GameplayEffect.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyDamageStatics)

void UProsperitocracyDamageStatics::AddDamageLine(FGameplayEffectContextHandle Context, FGameplayTag DamageType, uint8 PenTier, float Amount)
{
	if (FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context))
	{
		TypedContext->AddDamageLine(DamageType, PenTier, Amount);
	}
}

void UProsperitocracyDamageStatics::ApplyDamageEffectToHit(FGameplayEffectContextHandle Context, AActor* HitActor, UAbilitySystemComponent* SourceAbilitySystemComponent, TSubclassOf<UGameplayEffect> DamageEffectClass)
{
	if (!Context.IsValid() || !SourceAbilitySystemComponent || !DamageEffectClass)
	{
		return;
	}

	// A wall has no ability system: there is nothing to damage. Not a failure — the caller's impact
	// FX has already played on the surface.
	UAbilitySystemComponent* TargetAbilitySystemComponent = HitActor
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor)
		: nullptr;
	if (!TargetAbilitySystemComponent)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystemComponent);
	}
}

bool UProsperitocracyDamageStatics::IsContextValid(FGameplayEffectContextHandle Context)
{
	return Context.IsValid();
}

void UProsperitocracyDamageStatics::ApplyEffectsToHit(FGameplayEffectContextHandle Context, AActor* HitActor,
	UAbilitySystemComponent* SourceAbilitySystemComponent, const UProsperitocracyStatTable* SourceStatBlock,
	TSubclassOf<UGameplayEffect> DamageEffectClass)
{
	if (!HitActor || !SourceAbilitySystemComponent || !SourceStatBlock || SourceStatBlock->AppliedEffects.Num() == 0)
	{
		return;
	}

	// The statuses go on the body that was hit, through the one component that owns statuses on a body
	// — so any body that can carry one (a dummy today, an enemy later) carries them the same way.
	UProsperitocracyStatusComponent* Statuses = HitActor->FindComponentByClass<UProsperitocracyStatusComponent>();
	if (!Statuses)
	{
		return;
	}

	// A hit must have DONE something for its statuses to land (the user's rule, 2026-09-20): a shot that
	// bounced off a plate sets nothing alight. The answer is the execution's own — read from where the
	// gate was decided, never worked out a second time here, where it could disagree with it.
	const FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context);
	if (!TypedContext || !TypedContext->LandedAnyDamage())
	{
		return;
	}

	// The hit, passed along: a status that works on a part keeps working on the part that caught it.
	const FHitResult* HitResultPtr = TypedContext->GetHitResult();
	const FHitResult Hit = HitResultPtr ? *HitResultPtr : FHitResult();

	for (const FProsperitocracyAppliedEffect& Applied : SourceStatBlock->AppliedEffects)
	{
		const UProsperitocracyStatTable* StatusBlock = Applied.StatBlock.LoadSynchronous();
		if (!StatusBlock)
		{
			// A thing that names a status but carries no block for it has no numbers to apply. Said out
			// loud, because it is an authoring bug rather than a missing feature.
			UE_LOG(LogProsperitocracy, Warning, TEXT("[Status] %s names the status %s with no block — nothing applied."),
				*GetNameSafe(SourceStatBlock), *Applied.StatusTag.ToString());
			continue;
		}

		Statuses->ApplyStatus(Applied.StatusTag, StatusBlock, SourceAbilitySystemComponent, DamageEffectClass, Hit);
	}
}