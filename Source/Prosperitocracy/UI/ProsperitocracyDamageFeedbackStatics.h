// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/ProsperitocracyDamageFeedbackTypes.h"

#include "ProsperitocracyDamageFeedbackStatics.generated.h"

class AActor;

/**
 * UProsperitocracyDamageFeedbackStatics
 *
 * The ONE door every readout of a player's own damage goes through, wherever it was decided:
 *
 *   - the damage pipeline says what a blow TOOK OFF, where it landed, and what the gate answered (full,
 *     or halved because the pen only matched the armour) — it hands all of that here;
 *   - a body reaching zero says a blow was the KILLING one — it hands that here.
 *
 * Both tell the same man: the one whose damage it was. That is the whole reason this is a door and not
 * two calls — the numbers and the marker belong on the SHOOTER's own screen, and no listener ever has to
 * work out afterwards whose damage it is.
 *
 * A blow nothing dealt (a dev command hurting you) has no such man, and it is still YOUR screen that
 * wants the number: the man who TOOK it is told instead. The marker never rides that case — an X is an
 * answer about damage you dealt, and here there is none.
 *
 * Server-decided, client-shown. Damage is resolved where the hit happened (the authority), so the answer
 * is pushed to that player's own client — his screen, and nobody else's. Damage dealt by a thing rather
 * than a body (a turret, a trap) belongs to no player's screen and is dropped here, by having no ability
 * system to answer to.
 */
UCLASS()
class UProsperitocracyDamageFeedbackStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Tell the man whose screen this belongs on what his damage DID, what it TOOK OFF, and where it comes
	 * up.
	 *
	 * InstigatorOfDamage = whoever's damage it was; Victim = what it landed on, which is who is told when
	 * nothing dealt it. Both null is nothing to show anyone, and is ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Damage Feedback")
	static void NotifyDamageFeedback(AActor* InstigatorOfDamage, AActor* Victim, const FProsperitocracyDamageFeedback& Feedback);
};
