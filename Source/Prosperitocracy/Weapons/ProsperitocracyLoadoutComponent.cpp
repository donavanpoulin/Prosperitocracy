// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyLoadoutComponent.h"

#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "GameFramework/Pawn.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
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

	// A slot can be overridden to a different block at runtime by the dev gun sandbox, and when it is,
	// that IS what this slot carries. The check below still runs on it, so an override cannot file a
	// block under a slot it does not belong to.
	if (const TObjectPtr<UProsperitocracyStatTable>* Override = DevStatBlockBySlot.Find(Slot))
	{
		StatBlock = *Override;
	}

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

	// What this character carries just changed, so the body's Carried Weight row is written again — the
	// ONE number everything else reads, including the movement. The movement numbers follow that row by
	// themselves (the stats component listens to it), so nothing here has to know what to re-apply and
	// nothing can forget to: this only tells the truth about the kit, and the body gets the new weight.
	if (AActor* Owner = GetOwner())
	{
		if (UProsperitocracyPlayerStatsComponent* Stats = Owner->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
		{
			Stats->SyncCarriedWeight();
		}
	}

	return EProsperitocracyWeaponDressResult::Dressed;
}

float UProsperitocracyLoadoutComponent::GetCarriedWeightLbs() const
{
	float TotalLbs = 0.0f;

	// Everything in a slot.
	if (Loadout)
	{
		TArray<TPair<FGameplayTag, UProsperitocracyStatTable*>> Carried;
		Loadout->CollectCarriedBlocks(Carried);

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
	}

	// And what the character is WEARING, asked by that same rule: a thing that exists answers through
	// its own GAS home (so an armor whose weight something modified is counted for what it now is),
	// and a thing that is not up yet answers from the block it will be built from. One total, one
	// place, both kinds of carried thing in it.
	if (const AActor* Owner = GetOwner())
	{
		if (const UProsperitocracyPlayerStatsComponent* Stats = Owner->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
		{
			if (const AProsperitocracyStatHostActor* ArmorHost = Stats->GetArmorHost())
			{
				TotalLbs += UProsperitocracyStatSystemStatics::GetStatFinal(
					ArmorHost->GetProsperitocracyAbilitySystemComponent(), EProsperitocracyStat::Weight);
			}
			else if (const UProsperitocracyStatTable* ArmorWeave = Stats->GetWornWeave())
			{
				TotalLbs += ArmorWeave->GetBaseValue(EProsperitocracyStat::Weight);
			}
		}
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

bool UProsperitocracyLoadoutComponent::DevEquipStatBlock(UProsperitocracyStatTable* StatBlock, FString& OutMessage)
{
	if (!StatBlock)
	{
		OutMessage = TEXT("no stat block given");
		return false;
	}

	// The block says which slot it belongs in — the same rule the real dress path enforces, checked
	// here too so the sandbox cannot put a Secondary gun in Primary.
	const FGameplayTag Slot = StatBlock->GetSlot();
	if (!Slot.IsValid())
	{
		OutMessage = FString::Printf(TEXT("%s carries no slot tag, so there is nowhere to carry it"), *StatBlock->GetName());
		return false;
	}

	if (!Loadout)
	{
		OutMessage = TEXT("this character has no loadout asset, so nothing can be carried");
		return false;
	}

	DevStatBlockBySlot.Add(Slot, StatBlock);

	// A fresh gun's ammo, as asked: drop this slot's store and let the next ask rebuild it. That ask
	// is the same first-ask that fills a gun at spawn — magazine at MagSize, spare pool at
	// Capacity x MagSize and NOT one more (the loaded magazine is one of the Capacity magazines).
	// A gun asking for a store that is not there yet creates it full, so the gun in hand can never
	// be left holding a missing magazine.
	AmmoBySlot.Remove(Slot);

	// If this slot's gun is live, re-dress it now so the numbers change in hand. A gun the rig has
	// not created yet — or has thrown away on a slot switch — is built from this block the next time
	// it asks, because the override is what its slot resolves to from now on.
	FString GunNote = TEXT("no gun in that slot yet; it will be built from this block");
	if (const TWeakObjectPtr<AProsperitocracyWeapon>* LiveGun = DressedGunsBySlot.Find(Slot))
	{
		if (AProsperitocracyWeapon* Gun = LiveGun->Get())
		{
			const EProsperitocracyWeaponDressResult Result = DressGun(Gun);
			GunNote = (Result == EProsperitocracyWeaponDressResult::Dressed)
				? FString::Printf(TEXT("%s re-dressed and loaded"), *Gun->GetName())
				: FString::Printf(TEXT("%s was NOT re-dressed (dress result %d)"), *Gun->GetName(), static_cast<int32>(Result));
		}
	}

	OutMessage = FString::Printf(TEXT("%s into %s: %s"), *StatBlock->GetName(), *Slot.ToString(), *GunNote);

	return true;
}

