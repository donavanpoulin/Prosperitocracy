// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyDamageStatics.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "GameplayEffect.h"

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
