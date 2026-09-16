// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyLoadout.h"

#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyLoadout)

const FProsperitocracyWeaponSlot* UProsperitocracyLoadout::FindSlotForBodyClass(const UClass* InBodyClass) const
{
	if (!InBodyClass)
	{
		return nullptr;
	}

	for (const FProsperitocracyWeaponSlot& Entry : Weapons)
	{
		// The body class is a real reference, so the comparison is exact: a gun blueprint either is
		// the body this entry dresses, or it is not this entry.
		if (Entry.BodyClass.LoadSynchronous() == InBodyClass)
		{
			return &Entry;
		}
	}

	return nullptr;
}
