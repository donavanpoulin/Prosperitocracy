// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/ProsperitocracyGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "NativeGameplayTags.h"

#include "ProsperitocracyAbilitySystemComponent.generated.h"

#define UE_API PROSPERITOCRACY_API

class AActor;
class UGameplayAbility;
class UProsperitocracyAbilityTagRelationshipMapping;
class UObject;
struct FFrame;
struct FGameplayAbilityTargetDataHandle;

PROSPERITOCRACY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_AbilityInputBlocked);

/**
 * UProsperitocracyAbilitySystemComponent
 *
 *	Base ability system component class used by this project.
 *
 * PORT NOTE (2026-09-16) — came over from Prosperitocracy_lyra/ with four cuts, each commented where it
 * stood: the AnimInstance call in InitAbilityActorInfo, the AssetManager + GameData includes, and
 * AddDynamicTagGameplayEffect / RemoveDynamicTagGameplayEffect (their only callers are the unported
 * CheatManager, and GameData has no asset in this project). Kept: TagRelationshipMapping and
 * GetAdditionalActivationTagRequirements — ProsperitocracyGameplayAbility.cpp:360 calls the latter.
 *
 * One deliberate divergence from the old file: it is MinimalAPI there. Here it keeps Epic's own
 * UAbilitySystemComponent specifiers, so the component that box 1.4a already placed on
 * BP_ThirdPersonCharacter stays placeable and editable in the editor exactly like the stock one.
 */
UCLASS(ClassGroup = AbilitySystem, hidecategories = (Object, Transform), Meta = (BlueprintSpawnableComponent))
class UProsperitocracyAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:

	UE_API UProsperitocracyAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	UE_API virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	typedef TFunctionRef<bool(const UProsperitocracyGameplayAbility* ProsperitocracyAbility, FGameplayAbilitySpecHandle Handle)> TShouldCancelAbilityFunc;
	UE_API void CancelAbilitiesByFunc(TShouldCancelAbilityFunc ShouldCancelFunc, bool bReplicateCancelAbility);

	UE_API void CancelInputActivatedAbilities(bool bReplicateCancelAbility);

	UE_API void AbilityInputTagPressed(const FGameplayTag& InputTag);
	UE_API void AbilityInputTagReleased(const FGameplayTag& InputTag);

	UE_API void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	UE_API void ClearAbilityInput();

	UE_API bool IsActivationGroupBlocked(EProsperitocracyAbilityActivationGroup Group) const;
	UE_API void AddAbilityToActivationGroup(EProsperitocracyAbilityActivationGroup Group, UProsperitocracyGameplayAbility* ProsperitocracyAbility);
	UE_API void RemoveAbilityFromActivationGroup(EProsperitocracyAbilityActivationGroup Group, UProsperitocracyGameplayAbility* ProsperitocracyAbility);
	UE_API void CancelActivationGroupAbilities(EProsperitocracyAbilityActivationGroup Group, UProsperitocracyGameplayAbility* IgnoreProsperitocracyAbility, bool bReplicateCancelAbility);

	// CUT (2026-09-16): AddDynamicTagGameplayEffect / RemoveDynamicTagGameplayEffect stood here. Their
	// only callers are Player/ProsperitocracyCheatManager.cpp:248-421 (unported), and they read
	// UProsperitocracyGameData::Get().DynamicTagGameplayEffect — AssetManager + GameData, which have no
	// asset in this project. They return with the cheat manager / asset-manager port.

	/** Gets the ability target data associated with the given ability handle and activation info */
	UE_API void GetAbilityTargetData(const FGameplayAbilitySpecHandle AbilityHandle, FGameplayAbilityActivationInfo ActivationInfo, FGameplayAbilityTargetDataHandle& OutTargetDataHandle);

	/** Sets the current tag relationship mapping, if null it will clear it out */
	UE_API void SetTagRelationshipMapping(UProsperitocracyAbilityTagRelationshipMapping* NewMapping);
	
	/** Looks at ability tags and gathers additional required and blocking tags */
	UE_API void GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const;

	UE_API void TryActivateAbilitiesOnSpawn();

	// True while the given ability's input tag is held (pressed, not yet released).
	// Maintained by the tag-based input path (AbilityInputTagPressed/Released) — the reliable
	// "is the trigger down" state for the fire loop.
	bool IsAbilityInputHeld(const FGameplayAbilitySpecHandle& Handle) const
	{
		return InputHeldSpecHandles.Contains(Handle);
	}

protected:

	UE_API virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	UE_API virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	UE_API virtual void NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;
	UE_API virtual void NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason) override;
	UE_API virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	UE_API virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags) override;
	UE_API virtual void HandleChangeAbilityCanBeCanceled(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bCanBeCanceled) override;

	/** Notify client that an ability failed to activate */
	UFUNCTION(Client, Unreliable)
	UE_API void ClientNotifyAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);

	UE_API void HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);

protected:

	// If set, this table is used to look up tag relationships for activate and cancel
	UPROPERTY()
	TObjectPtr<UProsperitocracyAbilityTagRelationshipMapping> TagRelationshipMapping;

	// Handles to abilities that had their input pressed this frame.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	// Handles to abilities that had their input released this frame.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// Handles to abilities that have their input held.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	// Number of abilities running in each activation group.
	int32 ActivationGroupCounts[(uint8)EProsperitocracyAbilityActivationGroup::MAX];
};

#undef UE_API
