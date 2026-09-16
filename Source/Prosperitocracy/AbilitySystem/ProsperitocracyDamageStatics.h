// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayEffectTypes.h"

#include "ProsperitocracyDamageStatics.generated.h"

struct FGameplayTag;

/**
 * UProsperitocracyDamageStatics
 *
 * Blueprint-callable helpers for the ONE damage pipeline. Today just one entry point:
 * AddDamageLine — the way a damage source (weapon, ability, deployable) stamps a
 * {Type, PenTier, Amount} line onto an effect context before the shared execution resolves it.
 */
UCLASS()
class UProsperitocracyDamageStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds a damage line (Type = Impact or Piercing, PenTier 1-4, Amount) to the given effect context. */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Damage")
	static void AddDamageLine(FGameplayEffectContextHandle Context, FGameplayTag DamageType, uint8 PenTier, float Amount);

	/** True if the context holds a valid effect context (a source stamped it / lines were added). */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Damage")
	static bool IsContextValid(FGameplayEffectContextHandle Context);
};
