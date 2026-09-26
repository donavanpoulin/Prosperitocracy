// Copyright Prosperitocracy. All Rights Reserved.

#include "UI/ProsperitocracyHitMarkerStatics.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyHitMarkerStatics)

void UProsperitocracyHitMarkerStatics::NotifyHitMarker(AActor* InstigatorOfDamage, EProsperitocracyHitMarkerKind Kind)
{
	// Nobody to tell: a body that killed itself, a fall out of the world, or a source that never had an
	// instigator. Not a failure — there is simply no man whose screen this belongs on.
	if (!InstigatorOfDamage)
	{
		return;
	}

	// The damage was dealt BY this actor, so the screen it belongs on is THEIRS. One hop: the answer
	// rides to that player's own client, and every other client never sees it.
	UProsperitocracyAbilitySystemComponent* InstigatorAbilitySystemComponent =
		Cast<UProsperitocracyAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorOfDamage));
	if (!InstigatorAbilitySystemComponent)
	{
		return;
	}

	// Only the authority decides what a hit did. On a client this is nothing at all: a marker is never
	// predicted, it is TOLD to that client by the server (the ASC's own ClientNotifyHitMarker) — so a
	// client calling this would be answering a question only the server is allowed to answer.
	const UWorld* World = InstigatorOfDamage->GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	InstigatorAbilitySystemComponent->ClientNotifyHitMarker(Kind);
}
