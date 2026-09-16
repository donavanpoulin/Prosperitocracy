// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"

#include "ProsperitocracyAbilitySystemComponent.generated.h"

#define UE_API PROSPERITOCRACY_API

/**
 * UProsperitocracyAbilitySystemComponent
 *
 *	Base ability system component class used by this project.
 *
 * PORT NOTE (2026-09-15) — this is the REDUCED form of the old project's file, deliberately.
 * The old file is the root of the ability framework, not a health component: its chain pulls in
 * UProsperitocracyGameplayAbility, UProsperitocracyAbilitySourceInterface, the tag-relationship
 * mapping, UProsperitocracyAnimInstance, UProsperitocracyGlobalAbilitySystem, UProsperitocracyAssetManager
 * and UProsperitocracyGameData. None of that is ported yet, and the old project fatals at startup
 * without a GameData asset — so bringing it in whole would mean porting the entire ability stack to
 * get one health number.
 *
 * What is here: the class and its type, so ported files keep their real casts and nothing has to be
 * re-wired when the rest lands. What is NOT here yet, and comes with the ability port:
 *   - InitAbilityActorInfo override (pawn-avatar handling + ability instance notification)
 *   - CancelAbilitiesByFunc / CancelInputActivatedAbilities
 *   - AbilityInputTagPressed / AbilityInputTagReleased / ProcessAbilityInput / ClearAbilityInput
 *   - the activation-group API (IsActivationGroupBlocked, Add/RemoveAbilityToActivationGroup,
 *     CancelActivationGroupAbilities) and ActivationGroupCounts
 *   - AddDynamicTagGameplayEffect / RemoveDynamicTagGameplayEffect, GetAbilityTargetData,
 *     SetTagRelationshipMapping, GetAdditionalActivationTagRequirements, TryActivateAbilitiesOnSpawn
 *   - AbilitySpecInputPressed / Released and the ability notify overrides
 *   - the input state members (InputPressedSpecHandles, InputReleasedSpecHandles, InputHeldSpecHandles),
 *     IsAbilityInputHeld, and TAG_Gameplay_AbilityInputBlocked
 *
 * One deliberate divergence from the old file: it is not MinimalAPI. It uses Epic's own
 * UAbilitySystemComponent class specifiers (ClassGroup/hidecategories/BlueprintSpawnableComponent) so
 * the component can be placed on an actor in the editor the same way the stock ability system
 * component is. MinimalAPI would still work from Blueprint, but this is the shape the engine's own
 * ability system component has, and there is no reason to differ from it.
 */
UCLASS(ClassGroup = AbilitySystem, hidecategories = (Object, Transform), Meta = (BlueprintSpawnableComponent))
class UProsperitocracyAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:

	UE_API UProsperitocracyAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

#undef UE_API
