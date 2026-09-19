// Copyright Prosperitocracy. All Rights Reserved.

#include "Stats/ProsperitocracyStatSystemStatics.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/Attributes/ProsperitocracyStatSet.h"
#include "AbilitySystem/Attributes/ProsperitocracyThingStatSet.h"

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
