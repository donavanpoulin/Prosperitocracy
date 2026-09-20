// Copyright Prosperitocracy. All Rights Reserved.

#include "Targets/ProsperitocracyBodyPart.h"

#include "AbilitySystem/Attributes/ProsperitocracyStatSet.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyBodyPart)

AProsperitocracyBodyPart::AProsperitocracyBodyPart()
{
	// A part is a numbers home on a body: nothing to see.
	SetActorHiddenInGame(true);
}

void AProsperitocracyBodyPart::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// The body's set, beside the thing set every home carries: a part's answer has one number from each
	// (its resist is a body row, its armour a thing row). Added AFTER the base has initialised the actor
	// info, because an ability system only takes sets once it knows whose it is.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddSet<UProsperitocracyStatSet>();
	}
}

void AProsperitocracyBodyPart::InitializePart(UPrimitiveComponent* InPartMesh, UProsperitocracyStatTable* InStatBlock)
{
	PartMesh = InPartMesh;
	StatBlock = InStatBlock;

	// The part's block carries rows for BOTH homes — its armour is a thing row, its resist is a body row —
	// so it is dressed by asking each home for its own, one call each. The thing home first (this part's
	// own), then the body rows onto this part's own body rows, exactly as a worn weave's resists go onto
	// a player's. Handing the whole block to one home would silently drop the other half.
	InitializeFromStatBlock(InStatBlock);
	UProsperitocracyStatSystemStatics::ApplyBlockBodyRows(AbilitySystemComponent, InStatBlock, /*bBare=*/ false);
}

void AProsperitocracyBodyPart::ApplyDerivedStats()
{
	// Nothing derived — see the header.
}

FGameplayTag AProsperitocracyBodyPart::GetResistType() const
{
	const UProsperitocracyStatTable* Block = StatBlock;
	if (!Block)
	{
		return FGameplayTag();
	}

	const bool bImpact = Block->Carries(EProsperitocracyStat::ImpactResist);
	const bool bPiercing = Block->Carries(EProsperitocracyStat::PiercingResist);

	if (bImpact && bPiercing)
	{
		// The design has one resistance on a part, or none. A block carrying both is a mistake worth
		// saying out loud rather than quietly picking one of them.
		UE_LOG(LogProsperitocracy, Error, TEXT("[Part] %s wears a block carrying BOTH resistances — a part carries one or none. Answering as Impact; fix the block."), *GetName());
		return ProsperitocracyGameplayTags::Damage_Type_Impact;
	}

	if (bImpact)
	{
		return ProsperitocracyGameplayTags::Damage_Type_Impact;
	}
	if (bPiercing)
	{
		return ProsperitocracyGameplayTags::Damage_Type_Piercing;
	}

	// No resist row on the block: this part resists nothing.
	return FGameplayTag();
}

float AProsperitocracyBodyPart::GetResistAgainst(const FGameplayTag& DamageType) const
{
	const FGameplayTag ResistType = GetResistType();
	if (!ResistType.IsValid() || ResistType != DamageType)
	{
		// A part that is not built against this type takes this type at full: a row a part does not
		// carry is not a row it has.
		return 0.0f;
	}

	// The FINAL value off this part's own home — after perks and buffs, never the authored base.
	return GetStatFinal(UProsperitocracyStatSystemStatics::GetResistStatForDamageType(ResistType));
}

FProsperitocracyDamageProfile AProsperitocracyBodyPart::GetProfile(const FGameplayTag& DamageType) const
{
	FProsperitocracyDamageProfile Profile;

	// The part's own armour, FINAL. The 0-3 scale is enforced where the gate runs, so an authored
	// number off the scale is reported rather than silently hidden here.
	Profile.Armor = static_cast<uint8>(FMath::Max(0, FMath::RoundToInt(GetStatFinal(EProsperitocracyStat::Armor))));

	// And what this part does to THIS line's type — nothing at all when it is not the type the part is
	// built against.
	Profile.Resist = GetResistAgainst(DamageType);

	return Profile;
}
