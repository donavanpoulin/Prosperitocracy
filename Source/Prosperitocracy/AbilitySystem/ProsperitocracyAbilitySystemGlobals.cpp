// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProsperitocracyAbilitySystemGlobals.h"

#include "ProsperitocracyGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyAbilitySystemGlobals)

struct FGameplayEffectContext;

UProsperitocracyAbilitySystemGlobals::UProsperitocracyAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FGameplayEffectContext* UProsperitocracyAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FProsperitocracyGameplayEffectContext();
}

