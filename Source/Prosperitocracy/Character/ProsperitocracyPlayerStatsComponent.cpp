// Copyright Prosperitocracy. All Rights Reserved.

#include "Character/ProsperitocracyPlayerStatsComponent.h"

#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/Attributes/ProsperitocracyStatSet.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyPlayerStatsComponent)

UProsperitocracyPlayerStatsComponent::UProsperitocracyPlayerStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UProsperitocracyPlayerStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	AbilitySystemComponent = Owner->FindComponentByClass<UProsperitocracyAbilitySystemComponent>();
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogProsperitocracy, Error,
			TEXT("%s on %s: no ability system component on this actor, so this character has no numbers. "
				 "Add the ability system component to the actor."),
			*GetName(), *Owner->GetName());
		return;
	}

	// A pawn-owned ability system: the pawn is both the owner and the avatar. (The readout component
	// asks for the same thing; it is idempotent.)
	AbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);

	// The character's stats and its health both live on the character's own ability system. Adding a
	// set that is already there returns the one that is there, so the readout and this component
	// cannot end up with two.
	AbilitySystemComponent->AddSet<UProsperitocracyStatSet>();
	AbilitySystemComponent->AddSet<UProsperitocracyHealthSet>();

	ApplyBaselineStats();
	ApplyToMovement();
}

void UProsperitocracyPlayerStatsComponent::ApplyBaselineStats()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!BaselineStats)
	{
		if (!bBaselineWarned)
		{
			bBaselineWarned = true;
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("%s on %s: no baseline stat block set — this character's stats are all zero, so it takes "
					 "template defaults for anything that reads them."),
				*GetName(), *GetNameSafe(GetOwner()));
		}
		return;
	}

	for (const FProsperitocracyStatTableEntry& Entry : BaselineStats->StatEntries)
	{
		const FGameplayAttribute Attribute = UProsperitocracyStatSystemStatics::GetAttributeForStat(Entry.Stat);
		if (!Attribute.IsValid())
		{
			continue;
		}

		// This component carries the CHARACTER's numbers. A thing stat (a gun's damage, a turret's
		// rate) belongs to that thing's own stat host, never here — pushing it would put a weapon
		// number on the player.
		const UClass* AttributeSetClass = Attribute.GetAttributeSetClass();
		if (AttributeSetClass != UProsperitocracyStatSet::StaticClass()
			&& AttributeSetClass != UProsperitocracyHealthSet::StaticClass())
		{
			continue;
		}

		AbilitySystemComponent->SetNumericAttributeBase(Attribute, Entry.BaseValue);
	}
}

float UProsperitocracyPlayerStatsComponent::GetStat(EProsperitocracyStat Stat) const
{
	if (!AbilitySystemComponent)
	{
		return 0.0f;
	}
	return UProsperitocracyStatSystemStatics::GetStatFinal(AbilitySystemComponent, Stat);
}

float UProsperitocracyPlayerStatsComponent::GetSprintSpeed() const
{
	return GetWalkSpeed() * SprintSpeedMultiplier;
}

void UProsperitocracyPlayerStatsComponent::ApplyToMovement()
{
	AActor* Owner = GetOwner();
	UCharacterMovementComponent* Movement = Owner ? Owner->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
	if (!Movement)
	{
		return;
	}

	// A stat that is not set must never zero the body out: movement keeps whatever it had until there
	// is a number to give it.
	const float WalkSpeed = GetWalkSpeed();
	if (WalkSpeed > 0.0f)
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}

	const float JumpHeight = GetStat(EProsperitocracyStat::JumpHeight);
	if (JumpHeight > 0.0f)
	{
		Movement->JumpZVelocity = JumpHeight;
	}

	UE_LOG(LogProsperitocracy, Log, TEXT("%s on %s: movement from stats — walk %.0f | sprint %.0f | jump %.0f"),
		*GetName(), *GetNameSafe(Owner), Movement->MaxWalkSpeed, GetSprintSpeed(), Movement->JumpZVelocity);
}
