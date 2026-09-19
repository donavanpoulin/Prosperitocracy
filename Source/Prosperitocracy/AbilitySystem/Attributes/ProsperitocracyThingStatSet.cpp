// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Attributes/ProsperitocracyThingStatSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyThingStatSet)

class FLifetimeProperty;

UProsperitocracyThingStatSet::UProsperitocracyThingStatSet()
	: ImpactDamage(0.0f)
	, PiercingDamage(0.0f)
	, Rate(0.0f)
	, ReloadTime(0.0f)
	, Accuracy(0.0f)
	, Recoil(0.0f)
	, Range(0.0f)
	, Falloff(0.0f)
	, Duration(0.0f)
	, Cooldown(0.0f)
	, Capacity(0.0f)
	, MagSize(0.0f)
	, Pellets(0.0f)
	, Weight(0.0f)
	, Penetration(0.0f)
	, Drag(0.0f)
	, Carry(0.0f)
	// The armour's colour rows start at NOTHING CHOSEN, not at black: a piece that has never been
	// painted keeps the paint it ships with, and 0 IS a colour a player can pick. (Design/armor.md)
	, TrimColor1(-1.0f)
	, TrimColor2(-1.0f)
	, TrimColor3(-1.0f)
{
}

void UProsperitocracyThingStatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, ImpactDamage, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, PiercingDamage, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Rate, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, ReloadTime, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Accuracy, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Recoil, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Range, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Falloff, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Duration, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Cooldown, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Capacity, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, MagSize, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Pellets, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Weight, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Penetration, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Drag, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, Carry, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, TrimColor1, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, TrimColor2, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyThingStatSet, TrimColor3, COND_OwnerOnly, REPNOTIFY_Always);
}

void UProsperitocracyThingStatSet::OnRep_ImpactDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, ImpactDamage, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_PiercingDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, PiercingDamage, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Rate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Rate, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_ReloadTime(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, ReloadTime, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Accuracy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Accuracy, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Recoil(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Recoil, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Range(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Range, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Falloff(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Falloff, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Duration(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Duration, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Cooldown(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Cooldown, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Capacity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Capacity, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_MagSize(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, MagSize, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Pellets(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Pellets, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Weight(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Weight, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Penetration(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Penetration, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Drag(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Drag, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_Carry(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, Carry, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_TrimColor1(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, TrimColor1, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_TrimColor2(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, TrimColor2, OldValue);
}

void UProsperitocracyThingStatSet::OnRep_TrimColor3(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyThingStatSet, TrimColor3, OldValue);
}
