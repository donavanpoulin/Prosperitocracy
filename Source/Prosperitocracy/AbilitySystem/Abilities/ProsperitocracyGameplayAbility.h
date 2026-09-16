// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbility.h"

#include "AbilitySystem/ProsperitocracyAbilitySourceInterface.h"

#include "ProsperitocracyGameplayAbility.generated.h"

#define UE_API PROSPERITOCRACY_API

/**
 * PORT NOTE (2026-09-16) — this file came over from Prosperitocracy_lyra/ verbatim, then four
 * things the new project does not have were cut out of it. Each cut is commented where it stood.
 * Nothing here points at Lyra code, a Lyra plugin, or the old game framework.
 *   - the PlayerController cast getter (only caller was the unported Interact ability)
 *   - the AProsperitocracyCharacter cast getter (our character rides on their BP_ThirdPersonCharacter; Part 7)
 *   - the HeroComponent getter + SetCameraMode/ClearCameraMode (CameraMode drags the whole camera system;
 *     camera is theirs — IA_CameraChange)
 *   - the GameplayMessageSubsystem failure broadcasts (the Lyra plugin, cut by decision at box 1.4a)
 * What is still here and used: the stat host (StatBlock/StatHost, EnsureStatHost), the ability source
 * interface, the activation policy/group pair, additional costs, target data, and GetControllerFromActorInfo.
 */

struct FGameplayAbilityActivationInfo;
struct FGameplayAbilitySpec;
struct FGameplayAbilitySpecHandle;

class AActor;
class AController;
class AProsperitocracyStatHostActor;
class APlayerController;
class FText;
class IProsperitocracyAbilitySourceInterface;
class UAnimMontage;
class UProsperitocracyAbilityCost;
class UProsperitocracyAbilitySystemComponent;
class UProsperitocracyStatTable;
class UObject;
class UPhysicalMaterial;
struct FFrame;
struct FGameplayAbilityActorInfo;
struct FGameplayEffectSpec;
struct FGameplayEventData;
struct FGameplayTagContainer;
struct FProsperitocracyDamageLine;
enum class EProsperitocracyStat : uint8;

/**
 * EProsperitocracyAbilityActivationPolicy
 *
 *	Defines how an ability is meant to activate.
 */
UENUM(BlueprintType)
enum class EProsperitocracyAbilityActivationPolicy : uint8
{
	// Try to activate the ability when the input is triggered.
	OnInputTriggered,

	// Continually try to activate the ability while the input is active.
	WhileInputActive,

	// Try to activate the ability when an avatar is assigned.
	OnSpawn
};


/**
 * EProsperitocracyAbilityActivationGroup
 *
 *	Defines how an ability activates in relation to other abilities.
 */
UENUM(BlueprintType)
enum class EProsperitocracyAbilityActivationGroup : uint8
{
	// Ability runs independently of all other abilities.
	Independent,

	// Ability is canceled and replaced by other exclusive abilities.
	Exclusive_Replaceable,

	// Ability blocks all other exclusive abilities from activating.
	Exclusive_Blocking,

	MAX	UMETA(Hidden)
};

/** Failure reason that can be used to play an animation montage when a failure occurs */
USTRUCT(BlueprintType)
struct FProsperitocracyAbilityMontageFailureMessage
{
	GENERATED_BODY()

public:
	// Player controller that failed to activate the ability, if the AbilitySystemComponent was player owned
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	// Avatar actor that failed to activate the ability
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AActor> AvatarActor = nullptr;

	// All the reasons why this ability has failed
	UPROPERTY(BlueprintReadWrite)
	FGameplayTagContainer FailureTags;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UAnimMontage> FailureMontage = nullptr;
};

/**
 * UProsperitocracyGameplayAbility
 *
 *	The base gameplay ability class used by this project.
 */
UCLASS(MinimalAPI, Abstract, HideCategories = Input, Meta = (ShortTooltip = "The base gameplay ability class used by this project."))
class UProsperitocracyGameplayAbility : public UGameplayAbility, public IProsperitocracyAbilitySourceInterface
{
	GENERATED_BODY()
	friend class UProsperitocracyAbilitySystemComponent;

public:

	UE_API UProsperitocracyGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~IProsperitocracyAbilitySourceInterface — an ability with a stat block IS its own ability
	// source: its damage lines come from its GAS home (the ONE evaluator), never from raw data.
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;

	virtual void GetDamageLines(TArray<FProsperitocracyDamageLine>& OutLines) const override;
	//~End of IProsperitocracyAbilitySourceInterface

	/** FINAL value of a stat through the ability's GAS home (its host ASC's aggregator —
	 * (base + Σflat) × Σpercent, the ONE evaluator). Absent stat = 0 (presence-is-scope). */
	float GetStatFinalValue(EProsperitocracyStat Stat) const;

	/**
	 * Stamps the ability's GAS-evaluated damage lines (from its stat host — the ONE evaluator)
	 * onto the given context. A damage ability calls this on the context it applies its damage
	 * GE with: the lines are data, so they survive the ability's lifetime (e.g. a grenade that
	 * detonates after the ability ended). No raw reads — values come from the host's aggregator.
	 * NOTE: deliberately NOT const — a const BlueprintCallable becomes a pure node (no exec pins),
	 * and a pure node with no outputs is culled by the compiler; this must run in the exec flow.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Ability")
	UE_API void AddDamageLinesToContext(UPARAM(ref) FGameplayEffectContextHandle& Context);

	/**
	 * Stamps the ability's stat HOST as the context's ability source. Use this when the damage
	 * applies AFTER the ability ends (e.g. a grenade detonating): the host outlives the ability
	 * instance, so the execution can safely resolve the damage lines + distance falloff from it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Ability")
	UE_API void SetStatHostAsAbilitySource(UPARAM(ref) FGameplayEffectContextHandle& Context);

	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Ability")
	UE_API UProsperitocracyAbilitySystemComponent* GetProsperitocracyAbilitySystemComponentFromActorInfo() const;

	// CUT (2026-09-16): GetProsperitocracyPlayerControllerFromActorInfo — only caller was
	// Interaction/Abilities/ProsperitocracyGameplayAbility_Interact.cpp:43, not ported.
	// CUT (2026-09-16): GetProsperitocracyCharacterFromActorInfo — only caller was
	// ProsperitocracyGameplayAbility_Jump.cpp:51,63; our character is a Part 7 job.
	// CUT (2026-09-16): GetHeroComponentFromActorInfo — only used by SetCameraMode/ClearCameraMode.
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Ability")
	UE_API AController* GetControllerFromActorInfo() const;

	EProsperitocracyAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
	EProsperitocracyAbilityActivationGroup GetActivationGroup() const { return ActivationGroup; }

	UE_API void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	// Returns true if the requested activation group is a valid transition.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Prosperitocracy|Ability", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	UE_API bool CanChangeActivationGroup(EProsperitocracyAbilityActivationGroup NewGroup) const;

	// Tries to change the activation group.  Returns true if it successfully changed.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Prosperitocracy|Ability", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	UE_API bool ChangeActivationGroup(EProsperitocracyAbilityActivationGroup NewGroup);

	// CUT (2026-09-16): SetCameraMode / ClearCameraMode — their only job was to forward to
	// UProsperitocracyHeroComponent, and Camera/ProsperitocracyCameraMode.cpp includes
	// ProsperitocracyCameraComponent.h + ProsperitocracyPlayerCameraManager.h, i.e. the whole camera
	// system. Camera here is theirs (IA_CameraChange), so abilities do not set camera modes.

	void OnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const
	{
		NativeOnAbilityFailedToActivate(FailedReason);
		ScriptOnAbilityFailedToActivate(FailedReason);
	}

protected:

	// Called when the ability fails to activate
	UE_API virtual void NativeOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const;

	// Called when the ability fails to activate
	UFUNCTION(BlueprintImplementableEvent)
	UE_API void ScriptOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const;

	//~UGameplayAbility interface
	UE_API virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	UE_API virtual void SetCanBeCanceled(bool bCanBeCanceled) override;
	UE_API virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	UE_API virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	UE_API virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	UE_API virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	UE_API virtual FGameplayEffectContextHandle MakeEffectContext(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const override;
	UE_API virtual void ApplyAbilityTagsToGameplayEffectSpec(FGameplayEffectSpec& Spec, FGameplayAbilitySpec* AbilitySpec) const override;
	UE_API virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	//~End of UGameplayAbility interface

	UE_API virtual void OnPawnAvatarSet();

	UE_API virtual void GetAbilitySource(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& OutSourceLevel, const IProsperitocracyAbilitySourceInterface*& OutAbilitySource, AActor*& OutEffectCauser) const;

	// The ability's stat block (its row in the universal stat table). When set, this ability IS its
	// own ability source: its stats live as GAS attributes on the ability's own GAS home (StatHost)
	// and its damage lines resolve through the ONE damage pipeline (a grenade ability with a block
	// deals that block's Impact/Piercing, evaluated by the aggregator).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Ability")
	TObjectPtr<UProsperitocracyStatTable> StatBlock = nullptr;

	// The ability's GAS home (see AProsperitocracyStatHostActor): every stat the ability's block
	// carries is a GAS attribute on this host's own ASC — the ONE evaluator. Spawned on give (or
	// first activation), destroyed on remove; bases pushed from the block by the host.
	UPROPERTY(Transient)
	TObjectPtr<AProsperitocracyStatHostActor> StatHost = nullptr;

	/** Spawns the ability's stat host if it doesn't exist yet and pushes the block's bases. */
	void EnsureStatHost(const FGameplayAbilityActorInfo* ActorInfo);

	/** Called when this ability is granted to the ability system component. */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnAbilityAdded")
	UE_API void K2_OnAbilityAdded();

	/** Called when this ability is removed from the ability system component. */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnAbilityRemoved")
	UE_API void K2_OnAbilityRemoved();

	/** Called when the ability system is initialized with a pawn avatar. */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnPawnAvatarSet")
	UE_API void K2_OnPawnAvatarSet();

protected:

	// Defines how this ability is meant to activate.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Ability Activation")
	EProsperitocracyAbilityActivationPolicy ActivationPolicy;

	// Defines the relationship between this ability activating and other abilities activating.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Ability Activation")
	EProsperitocracyAbilityActivationGroup ActivationGroup;

	// Additional costs that must be paid to activate this ability
	UPROPERTY(EditDefaultsOnly, Instanced, Category = Costs)
	TArray<TObjectPtr<UProsperitocracyAbilityCost>> AdditionalCosts;

	// Map of failure tags to simple error messages
	UPROPERTY(EditDefaultsOnly, Category = "Advanced")
	TMap<FGameplayTag, FText> FailureTagToUserFacingMessages;

	// Map of failure tags to anim montages that should be played with them
	UPROPERTY(EditDefaultsOnly, Category = "Advanced")
	TMap<FGameplayTag, TObjectPtr<UAnimMontage>> FailureTagToAnimMontage;

	// If true, extra information should be logged when this ability is canceled. This is temporary, used for tracking a bug.
	UPROPERTY(EditDefaultsOnly, Category = "Advanced")
	bool bLogCancelation;

	// CUT (2026-09-16): TSubclassOf<UProsperitocracyCameraMode> ActiveCameraMode — went with
	// SetCameraMode/ClearCameraMode above.
};

#undef UE_API
