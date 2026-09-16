// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyNumbersReadoutComponent.h"

#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "ProsperitocracyLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyNumbersReadoutComponent)

UProsperitocracyNumbersReadoutComponent::UProsperitocracyNumbersReadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UProsperitocracyNumbersReadoutComponent::BeginPlay()
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
			TEXT("%s on %s: no UProsperitocracyAbilitySystemComponent on this actor, so there is nothing to read. "
				 "Add the ability system component to the actor."),
			*GetName(), *Owner->GetName());
		return;
	}

	// A pawn-owned ability system: the pawn is both the owner and the avatar.
	AbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);

	// Health's home is a GAS attribute set — GAS is the one evaluator (Design/stats.md).
	HealthSet = AbilitySystemComponent->AddSet<UProsperitocracyHealthSet>();
	if (!HealthSet)
	{
		UE_LOG(LogProsperitocracy, Error, TEXT("%s on %s: the health set could not be added to the ability system."), *GetName(), *Owner->GetName());
		return;
	}

	UE_LOG(LogProsperitocracy, Log, TEXT("%s on %s: readout live — Health %.1f / MaxHealth %.1f"),
		*GetName(), *Owner->GetName(), HealthSet->GetHealth(), HealthSet->GetMaxHealth());
}

void UProsperitocracyNumbersReadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(ReadoutMessageKey);
	}

	Super::EndPlay(EndPlayReason);
}

void UProsperitocracyNumbersReadoutComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HealthSet || !GEngine)
	{
		return;
	}

	// TimeToDisplay 0.0 with a fixed key refreshes the one line every frame.
	GEngine->AddOnScreenDebugMessage(ReadoutMessageKey, 0.0f, FColor::White,
		FString::Printf(TEXT("Health  %.1f / %.1f"), HealthSet->GetHealth(), HealthSet->GetMaxHealth()));
}
