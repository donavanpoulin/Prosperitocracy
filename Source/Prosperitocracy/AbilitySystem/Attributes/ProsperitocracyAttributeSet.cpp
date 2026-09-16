// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProsperitocracyAttributeSet.h"

#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyAttributeSet)

class UWorld;


UProsperitocracyAttributeSet::UProsperitocracyAttributeSet()
{
}

UWorld* UProsperitocracyAttributeSet::GetWorld() const
{
	const UObject* Outer = GetOuter();
	check(Outer);

	return Outer->GetWorld();
}

UProsperitocracyAbilitySystemComponent* UProsperitocracyAttributeSet::GetProsperitocracyAbilitySystemComponent() const
{
	return Cast<UProsperitocracyAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

