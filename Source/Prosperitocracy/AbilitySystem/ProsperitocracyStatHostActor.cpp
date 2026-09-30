// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyStatHostActor.h"

#include "AbilitySystem/Attributes/ProsperitocracyThingStatSet.h"
#include "AbilitySystem/Explosions/ProsperitocracyExplosionStatics.h"
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
	if (!StatBlock)
	{
		return;
	}

	// Kept, so this host can answer what the thing IS (GetStatBlock) and so a derived row can be
	// worked out from the block. It is not a second source of numbers: the bases below are those.
	StatBlockAsset = StatBlock;

	for (const FProsperitocracyStatTableEntry& Entry : StatBlock->StatEntries)
	{
		SetStatBase(Entry.Stat, Entry.BaseValue);
	}

	// Then the numbers it does not author, because they ARE its authored numbers: the shot's push on
	// the body, off its own Weight and damage.
	ApplyDerivedStats();
}

void AProsperitocracyStatHostActor::ApplyDerivedStats()
{
	// Damage is the thing's own two lines added: a piercing-only gun is just its piercing, and a hybrid
	// (a railgun) is both.
	const float DirectDamage = GetStatFinal(EProsperitocracyStat::ImpactDamage)
		+ GetStatFinal(EProsperitocracyStat::PiercingDamage);

	// AND THE SCALE IS THE WHOLE EVENT, NOT THE BANG (his rule, 2026-09-30): what a thing leaves burning
	// counts toward its Shake and its sound, because an explosive with fires is not a weaker explosion — it
	// hits softer up front and pays the rest out over time. Only the SHAKE and the sound read this; the push
	// below stays on the direct damage, because a push is the kick of the act itself.
	const float TheWholeEvent = DirectDamage + GetWhatItLeavesBurning();

	// The push is a share of the speed the character is moving at, so it is a PERCENT and it bites the
	// same at a walk, at a sprint and crouched. Weight and damage both buy it: heavier shoves harder,
	// harder-hitting shoves harder, and neither alone gets to the top.
	const float DragPercent = PushBasePercent
		+ (FMath::Max(0.0f, GetStatFinal(EProsperitocracyStat::Weight)) * PushPercentPerWeightLb)
		+ (DirectDamage * PushPercentPerDamagePoint);

	SetStatBase(EProsperitocracyStat::Drag, DragPercent);
	SetStatBase(EProsperitocracyStat::Carry, DragPercent * CarryShareOfDrag);

	// And the same damage, read a second time for a different job: how hard this thing's own act shakes the
	// SCREEN — and it is the WHOLE EVENT here, the bang plus what the thing leaves burning, because the
	// picture is of the explosion and not of its first frame. It is the picture, not the body — the push
	// above is what the act does to the man, this is what it does to the world he is looking through — and
	// both are priced off numbers a thing already has, so nothing is authored for either of them.
	//
	// Worked out HERE, beside the push, because it has the same inputs and the same reason to be read
	// late: damage can move mid-fight (a perk, a proc, an attachment), and a shake priced at the
	// damage a gun had the moment it came up would sit at the old number while the gun hit harder.
	const float ShakeDegrees = TheWholeEvent * ShakeDegreesPerDamagePoint;
	SetStatBase(EProsperitocracyStat::Shake, ShakeDegrees);
}

float AProsperitocracyStatHostActor::GetWhatItLeavesBurning() const
{
	if (!StatBlockAsset)
	{
		return 0.0f;
	}

	float Total = 0.0f;
	for (const FProsperitocracyAppliedEffect& Applied : StatBlockAsset->AppliedEffects)
	{
		const UProsperitocracyStatTable* StatusBlock = Applied.StatBlock.LoadSynchronous();
		if (!StatusBlock)
		{
			continue;
		}

		// A status's whole output, and only for a status that IS a burn — the same one rule, no branch:
		// Rate and Piercing Damage present means it ticks (a burn); Duration alone means it merely lasts (a
		// stun), and a stun contributes nothing to how hard the act shakes.
		if (!StatusBlock->Carries(EProsperitocracyStat::Rate) || !StatusBlock->Carries(EProsperitocracyStat::PiercingDamage))
		{
			continue;
		}

		Total += StatusBlock->GetBaseValue(EProsperitocracyStat::Rate)
			* StatusBlock->GetBaseValue(EProsperitocracyStat::PiercingDamage)
			* StatusBlock->GetBaseValue(EProsperitocracyStat::Duration);
	}

	return Total;
}

void AProsperitocracyStatHostActor::SetStatBase(EProsperitocracyStat Stat, float BaseValue)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayAttribute Attribute = UProsperitocracyStatSystemStatics::GetAttributeForStat(Stat);

	// This host only carries thing stats (UProsperitocracyThingStatSet). Character stats
	// (Health, MoveSpeed, ...) live on the actor's ASC — never pushed here.
	if (!Attribute.IsValid() || Attribute.GetAttributeSetClass() != UProsperitocracyThingStatSet::StaticClass())
	{
		return;
	}

	AbilitySystemComponent->SetNumericAttributeBase(Attribute, BaseValue);
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
	// The ONE falloff read for a thing, and the ONE home of the rules it runs on: a gun's own Falloff
	// row is where its ramp starts, and an EXPLOSION's is the rule — its Range is the WIDTH of its ball,
	// and its ramp starts halfway out to that edge, with no number authored for it anywhere.
	//
	// Read LIVE off this host's final rows, per hit: that is what makes a Range perk move an explosion's
	// ball and its ramp together, with nothing derived stored on the host to go stale.
	return ProsperitocracyExplosionHandling::ComputeAttenuation(StatBlockAsset, Distance,
		GetStatFinal(EProsperitocracyStat::Range), GetStatFinal(EProsperitocracyStat::Falloff));
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
