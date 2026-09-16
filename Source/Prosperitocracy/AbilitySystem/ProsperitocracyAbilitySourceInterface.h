// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "ProsperitocracyAbilitySourceInterface.generated.h"

class UObject;
class UPhysicalMaterial;
struct FGameplayTagContainer;
struct FProsperitocracyDamageLine;

/** Base interface for anything acting as a ability calculation source */
UINTERFACE()
class UProsperitocracyAbilitySourceInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IProsperitocracyAbilitySourceInterface
{
	GENERATED_IINTERFACE_BODY()

	/**
	 * Compute the multiplier for effect falloff with distance
	 * 
	 * @param Distance			Distance from source to target for ability calculations (distance bullet traveled for a gun, etc...)
	 * @param SourceTags		Aggregated Tags from the source
	 * @param TargetTags		Aggregated Tags currently on the target
	 * 
	 * @return Multiplier to apply to the base attribute value due to distance
	 */
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const = 0;

	/**
	 * The damage line(s) this source deals (data-driven: the weapon/ability knows its own damage).
	 * The shared execution reads the context's lines first; if there are none it falls back to here.
	 */
	virtual void GetDamageLines(TArray<FProsperitocracyDamageLine>& OutLines) const
	{
	}
};
