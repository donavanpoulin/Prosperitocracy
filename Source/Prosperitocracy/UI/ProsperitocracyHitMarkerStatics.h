// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/ProsperitocracyHitMarkerTypes.h"

#include "ProsperitocracyHitMarkerStatics.generated.h"

class AActor;

/**
 * UProsperitocracyHitMarkerStatics
 *
 * The ONE door every hit marker goes through, wherever it was decided:
 *
 *   - the damage pipeline says what a blow DID (full through the gate, or halved because the pen only
 *     matched the armour) — it hands that answer here;
 *   - a body reaching zero says a blow was the KILLING one — it hands that here.
 *
 * Both tell the same man: the one whose damage it was. That is the whole reason this is a door and not
 * two calls — the marker belongs to the shooter's OWN screen, and a listener never has to work out
 * whose damage it is afterwards.
 *
 * Server-decided, client-shown. Damage is resolved where the hit happened (the authority), so the
 * answer is pushed to that player's own client — his screen, and nobody else's. Damage dealt by a
 * thing rather than a body (a turret, a trap) belongs to no player's screen and is dropped here, by
 * having no ability system to answer to.
 */
UCLASS()
class UProsperitocracyHitMarkerStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Tell the man who dealt this damage what his hit DID. Null instigator (a body killing itself, a
	 * fall out of the world) is nothing to show anyone, and is ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Hit Marker")
	static void NotifyHitMarker(AActor* InstigatorOfDamage, EProsperitocracyHitMarkerKind Kind);
};
