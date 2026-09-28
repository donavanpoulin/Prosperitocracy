// Copyright Prosperitocracy. All Rights Reserved.

#include "UI/ProsperitocracyDamageFeedbackStatics.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyDamageFeedbackStatics)

void UProsperitocracyDamageFeedbackStatics::NotifyDamageFeedback(AActor* InstigatorOfDamage, AActor* Victim, const FProsperitocracyDamageFeedback& Feedback)
{
	// The marker belongs to the man whose DAMAGE it was (see the header): a blow nothing dealt carries its
	// numbers to the man who took it, and owes him no X. Decided here, once, so no sender remembers it.
	FProsperitocracyDamageFeedback ToSend = Feedback;
	ToSend.bCarriesMarker = Feedback.bCarriesMarker && (InstigatorOfDamage != nullptr);

	// Nothing to say to anyone: no marker for a blow nothing dealt, and no numbers at all. Not a failure
	// — there is simply nothing this screen was owed.
	if (!ToSend.bCarriesMarker && ToSend.Numbers.Num() == 0)
	{
		return;
	}

	// Who is told: the man whose damage it was, and — when nothing dealt it — the man it landed on. A body
	// that killed itself, a fall out of the world, and a thing with no player behind it all end here:
	// there is simply no screen this belongs on.
	AActor* Recipient = InstigatorOfDamage ? InstigatorOfDamage : Victim;
	if (!Recipient)
	{
		return;
	}

	UProsperitocracyAbilitySystemComponent* RecipientAbilitySystemComponent =
		Cast<UProsperitocracyAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Recipient));
	if (!RecipientAbilitySystemComponent)
	{
		return;
	}

	// Only the authority decides what a hit did. On a client this is nothing at all: the readout is never
	// predicted, it is TOLD to that client by the server (the ASC's own ClientNotifyDamageFeedback) — so a
	// client calling this would be answering a question only the server is allowed to answer.
	const UWorld* World = Recipient->GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	RecipientAbilitySystemComponent->ClientNotifyDamageFeedback(ToSend);
}
