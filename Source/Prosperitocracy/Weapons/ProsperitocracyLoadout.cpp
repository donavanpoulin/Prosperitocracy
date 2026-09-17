// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyLoadout.h"

#include "ProsperitocracyGameplayTags.h"
#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyLoadout)

void UProsperitocracyLoadout::CollectEntries(TArray<TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>>& Out) const
{
	Out.Reset();
	Out.Add(TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>(ProsperitocracyGameplayTags::Weapon_Slot_Primary, &Primary));
	Out.Add(TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>(ProsperitocracyGameplayTags::Weapon_Slot_Secondary, &Secondary));
	Out.Add(TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>(ProsperitocracyGameplayTags::Weapon_Slot_Special, &Special));
	Out.Add(TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>(ProsperitocracyGameplayTags::Weapon_Slot_Grenade, &Grenade));
}

const FProsperitocracyWeaponSlot* UProsperitocracyLoadout::FindEntryForSlot(const FGameplayTag& Slot) const
{
	if (!Slot.IsValid())
	{
		return nullptr;
	}

	TArray<TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>> Entries;
	CollectEntries(Entries);

	for (const TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>& Entry : Entries)
	{
		if (Entry.Key == Slot)
		{
			// A slot that names no body carries nothing — an empty slot, not a broken one.
			return Entry.Value->BodyClass.IsNull() ? nullptr : Entry.Value;
		}
	}

	return nullptr;
}

const FProsperitocracyWeaponSlot* UProsperitocracyLoadout::FindEntryForBodyClass(const UClass* InBodyClass, FGameplayTag& OutSlot, int32& OutMatchCount) const
{
	OutSlot = FGameplayTag();
	OutMatchCount = 0;

	if (!InBodyClass)
	{
		return nullptr;
	}

	TArray<TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>> Entries;
	CollectEntries(Entries);

	const FProsperitocracyWeaponSlot* Match = nullptr;
	for (const TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>& Entry : Entries)
	{
		// The body class is a real reference, so the comparison is exact: a gun blueprint either is
		// the body this entry dresses, or it is not this entry. An empty slot names no body and so
		// never matches.
		if (Entry.Value->BodyClass.LoadSynchronous() != InBodyClass)
		{
			continue;
		}

		++OutMatchCount;
		if (OutMatchCount == 1)
		{
			Match = Entry.Value;
			OutSlot = Entry.Key;
		}
	}

	// Exactly one slot, or no answer at all: two slots carrying the same body is a blueprint that
	// cannot say which gun it is, and the caller needs to hear about it rather than be handed one.
	return (OutMatchCount == 1) ? Match : nullptr;
}

void UProsperitocracyLoadout::CollectCarriedBlocks(TArray<TPair<FGameplayTag, UProsperitocracyStatTable*>>& Out) const
{
	Out.Reset();

	TArray<TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>> Entries;
	CollectEntries(Entries);

	for (const TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>& Entry : Entries)
	{
		// A slot that names no body carries nothing — an empty slot, not a broken one. It is skipped
		// rather than counted as zero, so the list is the things carried and nothing else.
		if (!Entry.Value || Entry.Value->BodyClass.IsNull())
		{
			continue;
		}

		if (UProsperitocracyStatTable* Block = Entry.Value->StatBlock.LoadSynchronous())
		{
			Out.Emplace(Entry.Key, Block);
		}
	}
}
