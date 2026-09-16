// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "ProsperitocracyGameplayEffect.generated.h"

/**
 * UProsperitocracyGameplayEffect
 *
 * The project's gameplay-effect base. (The StatBlock field was removed in the GAS migration —
 * damage no longer resolves from a GE-carried stat block; the source provides its damage lines
 * through the ONE pipeline, and an unwired source deals 0 + logs.)
 */
UCLASS()
class UProsperitocracyGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()
};
