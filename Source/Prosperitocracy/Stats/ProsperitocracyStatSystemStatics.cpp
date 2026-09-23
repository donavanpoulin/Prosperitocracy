// Copyright Prosperitocracy. All Rights Reserved.

#include "Stats/ProsperitocracyStatSystemStatics.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/Attributes/ProsperitocracyStatSet.h"
#include "AbilitySystem/Attributes/ProsperitocracyThingStatSet.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "ProsperitocracyGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyStatSystemStatics)

namespace
{
	/**
	 * Whether an attribute's GAS home is one of the BODY's sets — the character side of the table.
	 * The one test behind IsCharacterStat, kept where the registry that answers it lives.
	 */
	bool IsCharacterStatAttribute(const FGameplayAttribute& Attribute)
	{
		if (!Attribute.IsValid())
		{
			return false;
		}

		const UClass* SetClass = Attribute.GetAttributeSetClass();
		return SetClass == UProsperitocracyStatSet::StaticClass()
			|| SetClass == UProsperitocracyHealthSet::StaticClass();
	}
}

FGameplayAttribute UProsperitocracyStatSystemStatics::GetAttributeForStat(EProsperitocracyStat Stat)
{
	// stat-ID -> GAS attribute home. Presence-is-scope: if a stat is not mapped here it has no
	// actor-backed home (yet) and is considered absent on an actor. Thing stats (weapons, abilities,
	// turrets, deployables) live on each thing's OWN ability system (the thing's GAS home) via
	// UProsperitocracyThingStatSet — every stat resolves through the SAME vocabulary + evaluator.
	static const TMap<EProsperitocracyStat, FGameplayAttribute> Registry =
	{
		// --- Character stats (actor ASC) ---
		{ EProsperitocracyStat::Health, UProsperitocracyHealthSet::GetHealthAttribute() },
		{ EProsperitocracyStat::MoveSpeed, UProsperitocracyStatSet::GetMoveSpeedAttribute() },
		{ EProsperitocracyStat::JumpVelocity, UProsperitocracyStatSet::GetJumpVelocityAttribute() },
		{ EProsperitocracyStat::DiveDistance, UProsperitocracyStatSet::GetDiveDistanceAttribute() },
		{ EProsperitocracyStat::PackCapacity, UProsperitocracyStatSet::GetPackCapacityAttribute() },
		{ EProsperitocracyStat::WeightCapacity, UProsperitocracyStatSet::GetWeightCapacityAttribute() },
		{ EProsperitocracyStat::CarriedWeight, UProsperitocracyStatSet::GetCarriedWeightAttribute() },
		{ EProsperitocracyStat::ImpactResist, UProsperitocracyStatSet::GetImpactResistAttribute() },
		{ EProsperitocracyStat::PiercingResist, UProsperitocracyStatSet::GetPiercingResistAttribute() },
		// --- Thing stats (each thing's own GAS home) ---
		{ EProsperitocracyStat::ImpactDamage, UProsperitocracyThingStatSet::GetImpactDamageAttribute() },
		{ EProsperitocracyStat::PiercingDamage, UProsperitocracyThingStatSet::GetPiercingDamageAttribute() },
		{ EProsperitocracyStat::Rate, UProsperitocracyThingStatSet::GetRateAttribute() },
		{ EProsperitocracyStat::ReloadTime, UProsperitocracyThingStatSet::GetReloadTimeAttribute() },
		{ EProsperitocracyStat::Accuracy, UProsperitocracyThingStatSet::GetAccuracyAttribute() },
		{ EProsperitocracyStat::Recoil, UProsperitocracyThingStatSet::GetRecoilAttribute() },
		{ EProsperitocracyStat::Range, UProsperitocracyThingStatSet::GetRangeAttribute() },
		{ EProsperitocracyStat::Falloff, UProsperitocracyThingStatSet::GetFalloffAttribute() },
		{ EProsperitocracyStat::Duration, UProsperitocracyThingStatSet::GetDurationAttribute() },
		{ EProsperitocracyStat::Cooldown, UProsperitocracyThingStatSet::GetCooldownAttribute() },
		{ EProsperitocracyStat::Capacity, UProsperitocracyThingStatSet::GetCapacityAttribute() },
		{ EProsperitocracyStat::MagSize, UProsperitocracyThingStatSet::GetMagSizeAttribute() },
		{ EProsperitocracyStat::Pellets, UProsperitocracyThingStatSet::GetPelletsAttribute() },
		{ EProsperitocracyStat::Weight, UProsperitocracyThingStatSet::GetWeightAttribute() },
		{ EProsperitocracyStat::Penetration, UProsperitocracyThingStatSet::GetPenetrationAttribute() },
		{ EProsperitocracyStat::Drag, UProsperitocracyThingStatSet::GetDragAttribute() },
		{ EProsperitocracyStat::Carry, UProsperitocracyThingStatSet::GetCarryAttribute() },
		// What the armour LOOKS like: its colour, one row per piece, on the armour's own GAS home
		// beside its Weight (Design/armor.md). Carried, never aggregated — the look reads it.
		{ EProsperitocracyStat::TrimColor1, UProsperitocracyThingStatSet::GetTrimColor1Attribute() },
		{ EProsperitocracyStat::TrimColor2, UProsperitocracyThingStatSet::GetTrimColor2Attribute() },
		{ EProsperitocracyStat::TrimColor3, UProsperitocracyThingStatSet::GetTrimColor3Attribute() },
		// The pen gate's threshold, on the enemy PART's own GAS home beside its resists
		// (Design/damage.md). Enemy parts only: a player carries no armour and answers with resists.
		{ EProsperitocracyStat::Armor, UProsperitocracyThingStatSet::GetArmorAttribute() },
		// What a thing that drinks blood costs to use — the slow bleed while it is out and what one
		// use of it takes, both on the thing's own GAS home like every other thing stat
		// (Design/classes/reclaimer.md).
		{ EProsperitocracyStat::BloodDrain, UProsperitocracyThingStatSet::GetBloodDrainAttribute() },
		{ EProsperitocracyStat::BloodCost, UProsperitocracyThingStatSet::GetBloodCostAttribute() },
	};

	if (const FGameplayAttribute* Found = Registry.Find(Stat))
	{
		return *Found;
	}

	return FGameplayAttribute();
}

float UProsperitocracyStatSystemStatics::GetStatBase(const UAbilitySystemComponent* ASC, EProsperitocracyStat Stat)
{
	if (!ASC)
	{
		return 0.0f;
	}

	const FGameplayAttribute Attribute = GetAttributeForStat(Stat);
	if (!Attribute.IsValid())
	{
		return 0.0f;
	}

	return ASC->GetNumericAttributeBase(Attribute);
}

float UProsperitocracyStatSystemStatics::GetStatFinal(const UAbilitySystemComponent* ASC, EProsperitocracyStat Stat)
{
	if (!ASC)
	{
		return 0.0f;
	}

	const FGameplayAttribute Attribute = GetAttributeForStat(Stat);
	if (!Attribute.IsValid())
	{
		return 0.0f;
	}

	// The aggregator-evaluated current value is exactly (base + Σflat) × Σpercent.
	return ASC->GetNumericAttribute(Attribute);
}

bool UProsperitocracyStatSystemStatics::IsCharacterStat(EProsperitocracyStat Stat)
{
	return IsCharacterStatAttribute(GetAttributeForStat(Stat));
}

EProsperitocracyStat UProsperitocracyStatSystemStatics::GetResistStatForDamageType(FGameplayTag DamageType)
{
	// Exactly two damage types exist (Design/damage.md), so anything that is not Impact is Piercing —
	// which is also the type a burn and every other no-pen source carries.
	return (DamageType == ProsperitocracyGameplayTags::Damage_Type_Impact)
		? EProsperitocracyStat::ImpactResist
		: EProsperitocracyStat::PiercingResist;
}

void UProsperitocracyStatSystemStatics::ApplyBlockBodyRows(UAbilitySystemComponent* ASC, const UProsperitocracyStatTable* Block, bool bBare)
{
	if (!ASC || !Block)
	{
		return;
	}

	for (const FProsperitocracyStatTableEntry& Entry : Block->StatEntries)
	{
		// This call dresses the BODY's numbers and nothing else. A thing stat (a gun's damage, a gun's or
		// a body part's armour) belongs to that thing's own home; pushing it here would put a thing's
		// number on a body. Presence-is-scope, in both directions, asked in one place.
		if (!IsCharacterStat(Entry.Stat))
		{
			continue;
		}

		// Bare = the block's rows come back off. Not a subtraction and not a second rule: the body's
		// number IS the block's number, so no block means the number it left is gone.
		const float Value = bBare ? 0.0f : Entry.BaseValue;

		// Health is ONE authored number, and GAS needs it in two attributes to mean anything: the body's
		// Health and its MaxHealth. They are never two rows and never two numbers — max health is the
		// ceiling that makes Health a value at all, and the health set clamps Health to it, so a body
		// whose row says 300 must not be sitting under a 100 ceiling. Written FIRST, because the clamp
		// runs when the ceiling moves.
		if (Entry.Stat == EProsperitocracyStat::Health)
		{
			ASC->SetNumericAttributeBase(UProsperitocracyHealthSet::GetMaxHealthAttribute(), Value);
		}

		const FGameplayAttribute Attribute = GetAttributeForStat(Entry.Stat);
		if (Attribute.IsValid())
		{
			ASC->SetNumericAttributeBase(Attribute, Value);
		}
	}
}

float UProsperitocracyStatSystemStatics::ComputeDistanceAttenuation(float Distance, float RangeMeters, float FalloffMeters)
{
	// Presence-is-scope: absent stats (<= 0) = no falloff.
	if (RangeMeters <= 0.0f || FalloffMeters <= 0.0f)
	{
		return 1.0f;
	}

	const float DistanceMeters = Distance / 100.0f;
	if (DistanceMeters <= FalloffMeters)
	{
		return 1.0f;
	}
	if (DistanceMeters >= RangeMeters)
	{
		return 0.0f;
	}
	return 1.0f - (DistanceMeters - FalloffMeters) / (RangeMeters - FalloffMeters);
}
