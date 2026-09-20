// Copyright Prosperitocracy. All Rights Reserved.

#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyStatTable)

void UProsperitocracyStatTable::BuildCanonicalStats(TArray<FProsperitocracyStatTableEntry>& OutEntries)
{
	OutEntries.Reset();

	const auto Add = [&OutEntries](EProsperitocracyStat Stat, float BaseValue)
	{
		FProsperitocracyStatTableEntry& Entry = OutEntries.AddDefaulted_GetRef();
		Entry.Stat = Stat;
		Entry.BaseValue = BaseValue;
	};

	// --- Thing stats (abilities, weapons, turrets, mech, deployables) ---
	Add(EProsperitocracyStat::ImpactDamage, 0.0f);
	Add(EProsperitocracyStat::PiercingDamage, 0.0f);
	Add(EProsperitocracyStat::Rate, 0.0f);
	Add(EProsperitocracyStat::MagSize, 0.0f);
	Add(EProsperitocracyStat::Pellets, 0.0f);
	Add(EProsperitocracyStat::ReloadTime, 0.0f);
	Add(EProsperitocracyStat::Accuracy, 0.0f);
	Add(EProsperitocracyStat::Recoil, 0.0f);
	Add(EProsperitocracyStat::Range, 0.0f);
	Add(EProsperitocracyStat::Falloff, 0.0f);
	Add(EProsperitocracyStat::Duration, 0.0f);
	Add(EProsperitocracyStat::Cooldown, 0.0f);
	Add(EProsperitocracyStat::Capacity, 0.0f);
	Add(EProsperitocracyStat::Weight, 0.0f);
	Add(EProsperitocracyStat::Penetration, 0.0f);

	// --- Receiver-side, enemy parts only (Design/damage.md) ---

	// The pen-gate threshold a part carries, 0-3. The canonical default is UNARMOURED: a part that
	// says nothing about its armour takes full damage from every pen, which is the same thing the
	// thing stat set reads at base 0.
	Add(EProsperitocracyStat::Armor, 0.0f);

	// --- Character stats ---
	Add(EProsperitocracyStat::Health, 0.0f);
	Add(EProsperitocracyStat::MoveSpeed, 0.0f);
	Add(EProsperitocracyStat::JumpVelocity, 0.0f);
	Add(EProsperitocracyStat::DiveDistance, 0.0f);
	Add(EProsperitocracyStat::PackCapacity, 0.0f);
	Add(EProsperitocracyStat::WeightCapacity, 0.0f);
	Add(EProsperitocracyStat::CarriedWeight, 0.0f);
	Add(EProsperitocracyStat::ImpactResist, 0.0f);
	Add(EProsperitocracyStat::PiercingResist, 0.0f);
}

float UProsperitocracyStatTable::GetBaseValue(EProsperitocracyStat Stat) const
{
	for (const FProsperitocracyStatTableEntry& Entry : StatEntries)
	{
		if (Entry.Stat == Stat)
		{
			return Entry.BaseValue;
		}
	}

	// Presence is scope: a stat the block does not carry is not zero-valued here, it is absent — and
	// every caller of this door is asking about a stat it believes the block carries.
	return 0.0f;
}

bool UProsperitocracyStatTable::Carries(EProsperitocracyStat Stat) const
{
	for (const FProsperitocracyStatTableEntry& Entry : StatEntries)
	{
		if (Entry.Stat == Stat)
		{
			return true;
		}
	}

	return false;
}
