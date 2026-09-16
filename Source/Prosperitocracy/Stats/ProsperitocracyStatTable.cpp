// Copyright Prosperitocracy. All Rights Reserved.

#include "Stats/ProsperitocracyStatTable.h"

#include "Weapons/ProsperitocracyWeaponBodyData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyStatTable)

UProsperitocracyWeaponBodyData* UProsperitocracyStatTable::GetWeaponBody() const
{
	return WeaponBody.LoadSynchronous();
}

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

	// --- Character stats ---
	Add(EProsperitocracyStat::Health, 0.0f);
	Add(EProsperitocracyStat::MoveSpeed, 0.0f);
	Add(EProsperitocracyStat::JumpHeight, 0.0f);
	Add(EProsperitocracyStat::DiveDistance, 0.0f);
	Add(EProsperitocracyStat::PackCapacity, 0.0f);
	Add(EProsperitocracyStat::WeightCapacity, 0.0f);
	Add(EProsperitocracyStat::ImpactResist, 0.0f);
	Add(EProsperitocracyStat::PiercingResist, 0.0f);
}
