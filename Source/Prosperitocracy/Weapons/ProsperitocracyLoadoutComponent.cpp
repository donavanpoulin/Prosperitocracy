// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyLoadoutComponent.h"

#include "ProsperitocracyLoadout.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyLoadoutComponent)

UProsperitocracyLoadoutComponent::UProsperitocracyLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const FProsperitocracyWeaponSlot* UProsperitocracyLoadoutComponent::FindSlotForBodyClass(const UClass* InBodyClass) const
{
	return Loadout ? Loadout->FindSlotForBodyClass(InBodyClass) : nullptr;
}

TSubclassOf<UGameplayEffect> UProsperitocracyLoadoutComponent::GetGunDamageEffectClass() const
{
	return Loadout ? Loadout->GunDamageEffectClass : nullptr;
}
