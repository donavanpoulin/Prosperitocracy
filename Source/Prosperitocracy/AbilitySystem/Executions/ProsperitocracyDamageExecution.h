// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "ProsperitocracyDamageExecution.generated.h"

class UObject;


/**
 * UProsperitocracyDamageExecution
 *
 *	Execution used by gameplay effects to apply damage to the health attributes.
 */
UCLASS()
class UProsperitocracyDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:

	UProsperitocracyDamageExecution();

protected:

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
