// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyLoadoutComponent.h"

#include "GameFramework/Pawn.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyLoadoutComponent)

UProsperitocracyLoadoutComponent::UProsperitocracyLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const FProsperitocracyWeaponSlot* UProsperitocracyLoadoutComponent::GetEntryForSlot(const FGameplayTag& Slot) const
{
	return Loadout ? Loadout->FindEntryForSlot(Slot) : nullptr;
}

TSubclassOf<UGameplayEffect> UProsperitocracyLoadoutComponent::GetGunDamageEffectClass() const
{
	return Loadout ? Loadout->GunDamageEffectClass : nullptr;
}

EProsperitocracyWeaponDressResult UProsperitocracyLoadoutComponent::DressGun(AProsperitocracyWeapon* Gun)
{
	if (!Gun)
	{
		return EProsperitocracyWeaponDressResult::Undressed;
	}

	if (!Loadout)
	{
		return EProsperitocracyWeaponDressResult::NoLoadout;
	}

	// Which slot is this gun? The rig spawned it and said nothing, so the loadout answers by body.
	FGameplayTag Slot;
	int32 Matches = 0;
	const FProsperitocracyWeaponSlot* Entry = Loadout->FindEntryForBodyClass(Gun->GetClass(), Slot, Matches);
	if (!Entry)
	{
		return (Matches > 1) ? EProsperitocracyWeaponDressResult::AmbiguousEntry
		                     : EProsperitocracyWeaponDressResult::NoEntry;
	}

	UProsperitocracyStatTable* StatBlock = Entry->StatBlock.LoadSynchronous();
	if (!StatBlock)
	{
		return EProsperitocracyWeaponDressResult::NoStatBlock;
	}

	// The weapon owns its slot: the tag is on its stat block. When the block says it belongs in a
	// different slot than the one carrying it, one of the two is wrong — and a gun dressed on a guess
	// is exactly the kind of quiet wrong answer this whole shape exists to prevent.
	if (StatBlock->GetSlot() != Slot)
	{
		return EProsperitocracyWeaponDressResult::SlotMismatch;
	}

	// This component goes with the numbers, because it is where the ammo for that slot lives.
	return Gun->ApplyLoadoutEntry(Slot, StatBlock, Loadout->GunDamageEffectClass, this, Cast<APawn>(GetOwner()))
		? EProsperitocracyWeaponDressResult::Dressed
		: EProsperitocracyWeaponDressResult::Undressed;
}

FProsperitocracyWeaponAmmo& UProsperitocracyLoadoutComponent::GetOrCreateAmmoForSlot(const FGameplayTag& Slot, int32 MagazineSize, int32 MagazineCapacity)
{
	// One store per slot. The first ask fills the magazine; every later ask — a gun the rig
	// re-created, a reload, the readout — is answered by this same store, which is what makes a free
	// magazine impossible rather than merely unlikely.
	if (FProsperitocracyWeaponAmmo* Existing = AmmoBySlot.Find(Slot))
	{
		return *Existing;
	}

	const int32 Rounds = FMath::Max(0, MagazineSize);
	const int32 Magazines = FMath::Max(0, MagazineCapacity);

	FProsperitocracyWeaponAmmo& Ammo = AmmoBySlot.Add(Slot);
	Ammo.Magazine = Rounds;
	Ammo.Spare = FMath::Max(0, Magazines * Rounds - Rounds);
	return Ammo;
}

const FProsperitocracyWeaponAmmo* UProsperitocracyLoadoutComponent::FindAmmoForSlot(const FGameplayTag& Slot) const
{
	return AmmoBySlot.Find(Slot);
}
