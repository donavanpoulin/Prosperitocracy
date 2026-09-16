// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyDamageStatics.h"

#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyDamageStatics)

void UProsperitocracyDamageStatics::AddDamageLine(FGameplayEffectContextHandle Context, FGameplayTag DamageType, uint8 PenTier, float Amount)
{
	if (FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context))
	{
		TypedContext->AddDamageLine(DamageType, PenTier, Amount);
	}
}

bool UProsperitocracyDamageStatics::IsContextValid(FGameplayEffectContextHandle Context)
{
	return Context.IsValid();
}
