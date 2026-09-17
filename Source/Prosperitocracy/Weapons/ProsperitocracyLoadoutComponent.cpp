// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyLoadoutComponent.h"

#include "Character/ProsperitocracyPlayerStatsComponent.h"
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
	const bool bDressed = Gun->ApplyLoadoutEntry(Slot, StatBlock, Loadout->GunDamageEffectClass, this, Cast<APawn>(GetOwner()));
	if (!bDressed)
	{
		return EProsperitocracyWeaponDressResult::Undressed;
	}

	// Remember the live gun for this slot: its weight is read from its own GAS home from now on, so a
	// gun whose weight was modified by something is counted for what it now is.
	DressedGunsBySlot.Add(Slot, Gun);

	// What this character carries just changed, so the weight-derived numbers are re-applied — the
	// JUMP only. The walk and run speeds are written by the movement state that owns them (the
	// template's sprint start/stop), and writing them from here would stomp the speed the player is
	// currently in; those re-read the stat the next time they run, which is where they belong.
	if (AActor* Owner = GetOwner())
	{
		if (UProsperitocracyPlayerStatsComponent* Stats = Owner->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
		{
			Stats->ApplyJumpVelocity();
		}
	}

	return EProsperitocracyWeaponDressResult::Dressed;
}

float UProsperitocracyLoadoutComponent::GetCarriedWeightLbs() const
{
	if (!Loadout)
	{
		return 0.0f;
	}

	TArray<TPair<FGameplayTag, UProsperitocracyStatTable*>> Carried;
	Loadout->CollectCarriedBlocks(Carried);

	float TotalLbs = 0.0f;
	for (const TPair<FGameplayTag, UProsperitocracyStatTable*>& Entry : Carried)
	{
		// A live gun first: its own GAS home is the truth about its weight once it exists.
		if (const TWeakObjectPtr<AProsperitocracyWeapon>* LiveGun = DressedGunsBySlot.Find(Entry.Key))
		{
			if (const AProsperitocracyWeapon* Gun = LiveGun->Get())
			{
				TotalLbs += Gun->GetWeaponStat(EProsperitocracyStat::Weight);
				continue;
			}
		}

		// Otherwise the block it will be built from — the same number the host would be handed.
		TotalLbs += Entry.Value->GetBaseValue(EProsperitocracyStat::Weight);
	}

	return TotalLbs;
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
