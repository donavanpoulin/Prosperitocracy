// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyStatHostActor.h"

#include "AbilitySystem/Attributes/ProsperitocracyThingStatSet.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "ProsperitocracyGameplayTags.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyStatHostActor)

AProsperitocracyStatHostActor::AProsperitocracyStatHostActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// A pure stat host: never damageable, never visible, never collides (no components to see).
	SetCanBeDamaged(false);

	AbilitySystemComponent = CreateDefaultSubobject<UProsperitocracyAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	ThingStatSet = CreateDefaultSubobject<UProsperitocracyThingStatSet>(TEXT("ThingStatSet"));
}

void AProsperitocracyStatHostActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// The host owns its own ASC and is its own avatar: a self-contained evaluator for the
	// thing's stats. The pawn it serves never becomes this ASC's avatar.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void AProsperitocracyStatHostActor::InitializeFromStatBlock(const UProsperitocracyStatTable* StatBlock)
{
	if (!StatBlock || !AbilitySystemComponent)
	{
		return;
	}

	for (const FProsperitocracyStatTableEntry& Entry : StatBlock->StatEntries)
	{
		const FGameplayAttribute Attribute = UProsperitocracyStatSystemStatics::GetAttributeForStat(Entry.Stat);

		// This host only carries thing stats (UProsperitocracyThingStatSet). Character stats
		// (Health, MoveSpeed, ...) live on the actor's ASC — never pushed here.
		if (!Attribute.IsValid() || Attribute.GetAttributeSetClass() != UProsperitocracyThingStatSet::StaticClass())
		{
			continue;
		}

		AbilitySystemComponent->SetNumericAttributeBase(Attribute, Entry.BaseValue);
	}
}

float AProsperitocracyStatHostActor::GetStatFinal(EProsperitocracyStat Stat) const
{
	if (const UProsperitocracyAbilitySystemComponent* ASC = AbilitySystemComponent)
	{
		return UProsperitocracyStatSystemStatics::GetStatFinal(ASC, Stat);
	}
	return 0.0f;
}

float AProsperitocracyStatHostActor::GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	// The host's Range/Falloff stats, evaluated by its own aggregator — the ONE universal
	// falloff (Design/damage.md): full until Falloff, linear to 0 at Range. Absent = no falloff.
	return UProsperitocracyStatSystemStatics::ComputeDistanceAttenuation(Distance, GetStatFinal(EProsperitocracyStat::Range), GetStatFinal(EProsperitocracyStat::Falloff));
}

void AProsperitocracyStatHostActor::GetDamageLines(TArray<FProsperitocracyDamageLine>& OutLines) const
{
	OutLines.Reset();

	// Presence-is-scope: the thing's block has ImpactDamage and/or PiercingDamage, or it deals
	// nothing. Both values are evaluated by this host's aggregator — the ONE evaluator.
	const float ImpactAmount = GetStatFinal(EProsperitocracyStat::ImpactDamage);
	const float PiercingAmount = GetStatFinal(EProsperitocracyStat::PiercingDamage);
	if (ImpactAmount <= 0.0f && PiercingAmount <= 0.0f)
	{
		return;
	}

	const int32 Pen = FMath::Clamp(FMath::RoundToInt(GetStatFinal(EProsperitocracyStat::Penetration)), 1, 4);

	if (ImpactAmount > 0.0f)
	{
		FProsperitocracyDamageLine Line;
		Line.Type = ProsperitocracyGameplayTags::Damage_Type_Impact;
		Line.PenTier = Pen;
		Line.Amount = ImpactAmount;
		OutLines.Add(Line);
	}
	if (PiercingAmount > 0.0f)
	{
		FProsperitocracyDamageLine Line;
		Line.Type = ProsperitocracyGameplayTags::Damage_Type_Piercing;
		Line.PenTier = Pen;
		Line.Amount = PiercingAmount;
		OutLines.Add(Line);
	}
}
