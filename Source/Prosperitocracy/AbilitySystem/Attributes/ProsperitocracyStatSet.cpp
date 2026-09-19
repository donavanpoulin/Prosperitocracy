// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Attributes/ProsperitocracyStatSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyStatSet)

class FLifetimeProperty;

UProsperitocracyStatSet::UProsperitocracyStatSet()
	: MoveSpeed(0.0f)
	, JumpVelocity(0.0f)
	, DiveDistance(0.0f)
	, PackCapacity(0.0f)
	, WeightCapacity(0.0f)
	, CarriedWeight(0.0f)
	, ImpactResist(0.0f)
	, PiercingResist(0.0f)
{
}

void UProsperitocracyStatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyStatSet, MoveSpeed, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyStatSet, JumpVelocity, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyStatSet, DiveDistance, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyStatSet, PackCapacity, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyStatSet, WeightCapacity, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyStatSet, CarriedWeight, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyStatSet, ImpactResist, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UProsperitocracyStatSet, PiercingResist, COND_OwnerOnly, REPNOTIFY_Always);
}

void UProsperitocracyStatSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyStatSet, MoveSpeed, OldValue);
}

void UProsperitocracyStatSet::OnRep_JumpVelocity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyStatSet, JumpVelocity, OldValue);
}

void UProsperitocracyStatSet::OnRep_DiveDistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyStatSet, DiveDistance, OldValue);
}

void UProsperitocracyStatSet::OnRep_PackCapacity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyStatSet, PackCapacity, OldValue);
}

void UProsperitocracyStatSet::OnRep_WeightCapacity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyStatSet, WeightCapacity, OldValue);
}

void UProsperitocracyStatSet::OnRep_CarriedWeight(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyStatSet, CarriedWeight, OldValue);
}

void UProsperitocracyStatSet::OnRep_ImpactResist(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyStatSet, ImpactResist, OldValue);
}

void UProsperitocracyStatSet::OnRep_PiercingResist(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UProsperitocracyStatSet, PiercingResist, OldValue);
}
