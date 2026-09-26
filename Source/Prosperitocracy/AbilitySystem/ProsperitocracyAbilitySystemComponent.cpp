// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProsperitocracyAbilitySystemComponent.h"

#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility.h"
#include "AbilitySystem/ProsperitocracyAbilityTagRelationshipMapping.h"
// CUT (2026-09-16): Animation/ProsperitocracyAnimInstance.h — ProsperitocracyAnimInstance.cpp includes
// Character/AProsperitocracyCharacter.h and Character/ProsperitocracyCharacterMovementComponent.h, both
// Part 7. Their AnimBP here is Locomotion_C; see the cut inside InitAbilityActorInfo below.
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ProsperitocracyGlobalAbilitySystem.h"
#include "ProsperitocracyLogChannels.h"
// CUT (2026-09-16): System/ProsperitocracyAssetManager.h + System/ProsperitocracyGameData.h — only the
// two dynamic-tag functions below used them, and GameData has no asset in this project.

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyAbilitySystemComponent)

UE_DEFINE_GAMEPLAY_TAG(TAG_Gameplay_AbilityInputBlocked, "Gameplay.AbilityInputBlocked");

UProsperitocracyAbilitySystemComponent::UProsperitocracyAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();

	FMemory::Memset(ActivationGroupCounts, 0, sizeof(ActivationGroupCounts));
}

void UProsperitocracyAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UProsperitocracyGlobalAbilitySystem* GlobalAbilitySystem = UWorld::GetSubsystem<UProsperitocracyGlobalAbilitySystem>(GetWorld()))
	{
		GlobalAbilitySystem->UnregisterASC(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UProsperitocracyAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	check(ActorInfo);
	check(InOwnerActor);

	const bool bHasNewPawnAvatar = Cast<APawn>(InAvatarActor) && (InAvatarActor != ActorInfo->AvatarActor);

	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (bHasNewPawnAvatar)
	{
		// Notify all abilities that a new pawn avatar has been set
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			ensureMsgf(AbilitySpec.Ability && AbilitySpec.Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced, TEXT("InitAbilityActorInfo: All Abilities should be Instanced (NonInstanced is being deprecated due to usability issues)."));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
			TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
			for (UGameplayAbility* AbilityInstance : Instances)
			{
				UProsperitocracyGameplayAbility* ProsperitocracyAbilityInstance = Cast<UProsperitocracyGameplayAbility>(AbilityInstance);
				if (ProsperitocracyAbilityInstance)
				{
					// Ability instances may be missing for replays
					ProsperitocracyAbilityInstance->OnPawnAvatarSet();
				}
			}
		}

		// Register with the global system once we actually have a pawn avatar. We wait until this time since some globally-applied effects may require an avatar.
		if (UProsperitocracyGlobalAbilitySystem* GlobalAbilitySystem = UWorld::GetSubsystem<UProsperitocracyGlobalAbilitySystem>(GetWorld()))
		{
			GlobalAbilitySystem->RegisterASC(this);
		}

		// CUT (2026-09-16): UProsperitocracyAnimInstance::InitializeWithAbilitySystem(this) stood here,
		// handing GAS state to our anim instance. That class pulls in our character and our movement
		// component (both Part 7), and the avatar's AnimBP here is theirs (Locomotion_C), so the cast
		// could never have succeeded anyway. Returns with the character port.

		TryActivateAbilitiesOnSpawn();
	}
}

void UProsperitocracyAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (const UProsperitocracyGameplayAbility* ProsperitocracyAbilityCDO = Cast<UProsperitocracyGameplayAbility>(AbilitySpec.Ability))
		{
			ProsperitocracyAbilityCDO->TryActivateAbilityOnSpawn(AbilityActorInfo.Get(), AbilitySpec);
		}
	}
}

void UProsperitocracyAbilitySystemComponent::CancelAbilitiesByFunc(TShouldCancelAbilityFunc ShouldCancelFunc, bool bReplicateCancelAbility)
{
	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.IsActive())
		{
			continue;
		}

		UProsperitocracyGameplayAbility* ProsperitocracyAbilityCDO = Cast<UProsperitocracyGameplayAbility>(AbilitySpec.Ability);
		if (!ProsperitocracyAbilityCDO)
		{
			UE_LOG(LogProsperitocracyAbilitySystem, Error, TEXT("CancelAbilitiesByFunc: Non-ProsperitocracyGameplayAbility %s was Granted to ASC. Skipping."), *AbilitySpec.Ability.GetName());
			continue;
		}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ensureMsgf(AbilitySpec.Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced, TEXT("CancelAbilitiesByFunc: All Abilities should be Instanced (NonInstanced is being deprecated due to usability issues)."));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			
		// Cancel all the spawned instances.
		TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
		for (UGameplayAbility* AbilityInstance : Instances)
		{
			UProsperitocracyGameplayAbility* ProsperitocracyAbilityInstance = CastChecked<UProsperitocracyGameplayAbility>(AbilityInstance);

			if (ShouldCancelFunc(ProsperitocracyAbilityInstance, AbilitySpec.Handle))
			{
				if (ProsperitocracyAbilityInstance->CanBeCanceled())
				{
					ProsperitocracyAbilityInstance->CancelAbility(AbilitySpec.Handle, AbilityActorInfo.Get(), ProsperitocracyAbilityInstance->GetCurrentActivationInfo(), bReplicateCancelAbility);
				}
				else
				{
					UE_LOG(LogProsperitocracyAbilitySystem, Error, TEXT("CancelAbilitiesByFunc: Can't cancel ability [%s] because CanBeCanceled is false."), *ProsperitocracyAbilityInstance->GetName());
				}
			}
		}
	}
}

void UProsperitocracyAbilitySystemComponent::CancelInputActivatedAbilities(bool bReplicateCancelAbility)
{
	auto ShouldCancelFunc = [this](const UProsperitocracyGameplayAbility* ProsperitocracyAbility, FGameplayAbilitySpecHandle Handle)
	{
		const EProsperitocracyAbilityActivationPolicy ActivationPolicy = ProsperitocracyAbility->GetActivationPolicy();
		return ((ActivationPolicy == EProsperitocracyAbilityActivationPolicy::OnInputTriggered) || (ActivationPolicy == EProsperitocracyAbilityActivationPolicy::WhileInputActive));
	};

	CancelAbilitiesByFunc(ShouldCancelFunc, bReplicateCancelAbility);
}

void UProsperitocracyAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	// We don't support UGameplayAbility::bReplicateInputDirectly.
	// Use replicated events instead so that the WaitInputPress ability task works.
	if (Spec.IsActive())
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		FPredictionKey OriginalPredictionKey = Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : Spec.ActivationInfo.GetActivationPredictionKey();
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// Invoke the InputPressed event. This is not replicated here. If someone is listening, they may replicate the InputPressed event to the server.
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, OriginalPredictionKey);
	}
}

void UProsperitocracyAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	// We don't support UGameplayAbility::bReplicateInputDirectly.
	// Use replicated events instead so that the WaitInputRelease ability task works.
	if (Spec.IsActive())
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		FPredictionKey OriginalPredictionKey = Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : Spec.ActivationInfo.GetActivationPredictionKey();
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// Invoke the InputReleased event. This is not replicated here. If someone is listening, they may replicate the InputReleased event to the server.
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, OriginalPredictionKey);
	}
}

void UProsperitocracyAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void UProsperitocracyAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.Remove(AbilitySpec.Handle);
			}
		}
	}
}

bool UProsperitocracyAbilitySystemComponent::TryActivateAbilityByInputTag(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	// The same resolution the input path above uses: the granted spec carries the tag, so a door can
	// ask for "the bash" without knowing the ability class or holding its handle.
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
		{
			return TryActivateAbility(AbilitySpec.Handle);
		}
	}

	return false;
}

void UProsperitocracyAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (HasMatchingGameplayTag(TAG_Gameplay_AbilityInputBlocked))
	{
		ClearAbilityInput();
		return;
	}

	static TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	AbilitiesToActivate.Reset();

	//@TODO: See if we can use FScopedServerAbilityRPCBatcher ScopedRPCBatcher in some of these loops

	//
	// Process all abilities that activate when the input is held.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability && !AbilitySpec->IsActive())
			{
				const UProsperitocracyGameplayAbility* ProsperitocracyAbilityCDO = Cast<UProsperitocracyGameplayAbility>(AbilitySpec->Ability);
				if (ProsperitocracyAbilityCDO && ProsperitocracyAbilityCDO->GetActivationPolicy() == EProsperitocracyAbilityActivationPolicy::WhileInputActive)
				{
					AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
				}
			}
		}
	}

	//
	// Process all abilities that had their input pressed this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = true;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputPressed(*AbilitySpec);
				}
				else
				{
					const UProsperitocracyGameplayAbility* ProsperitocracyAbilityCDO = Cast<UProsperitocracyGameplayAbility>(AbilitySpec->Ability);

					if (ProsperitocracyAbilityCDO && ProsperitocracyAbilityCDO->GetActivationPolicy() == EProsperitocracyAbilityActivationPolicy::OnInputTriggered)
					{
						AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
					}
				}
			}
		}
	}

	//
	// Try to activate all the abilities that are from presses and holds.
	// We do it all at once so that held inputs don't activate the ability
	// and then also send a input event to the ability because of the press.
	//
	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(AbilitySpecHandle);
	}

	//
	// Process all abilities that had their input released this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = false;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputReleased(*AbilitySpec);
				}
			}
		}
	}

	//
	// Clear the cached ability handles.
	//
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UProsperitocracyAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

void UProsperitocracyAbilitySystemComponent::NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	Super::NotifyAbilityActivated(Handle, Ability);

	if (UProsperitocracyGameplayAbility* ProsperitocracyAbility = Cast<UProsperitocracyGameplayAbility>(Ability))
	{
		AddAbilityToActivationGroup(ProsperitocracyAbility->GetActivationGroup(), ProsperitocracyAbility);
	}
}

void UProsperitocracyAbilitySystemComponent::NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason)
{
	Super::NotifyAbilityFailed(Handle, Ability, FailureReason);

	if (APawn* Avatar = Cast<APawn>(GetAvatarActor()))
	{
		if (!Avatar->IsLocallyControlled() && Ability->IsSupportedForNetworking())
		{
			ClientNotifyAbilityFailed(Ability, FailureReason);
			return;
		}
	}

	HandleAbilityFailed(Ability, FailureReason);
}

void UProsperitocracyAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);

	if (UProsperitocracyGameplayAbility* ProsperitocracyAbility = Cast<UProsperitocracyGameplayAbility>(Ability))
	{
		RemoveAbilityFromActivationGroup(ProsperitocracyAbility->GetActivationGroup(), ProsperitocracyAbility);
	}
}

void UProsperitocracyAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags)
{
	FGameplayTagContainer ModifiedBlockTags = BlockTags;
	FGameplayTagContainer ModifiedCancelTags = CancelTags;

	if (TagRelationshipMapping)
	{
		// Use the mapping to expand the ability tags into block and cancel tag
		TagRelationshipMapping->GetAbilityTagsToBlockAndCancel(AbilityTags, &ModifiedBlockTags, &ModifiedCancelTags);
	}

	Super::ApplyAbilityBlockAndCancelTags(AbilityTags, RequestingAbility, bEnableBlockTags, ModifiedBlockTags, bExecuteCancelTags, ModifiedCancelTags);

	//@TODO: Apply any special logic like blocking input or movement
}

void UProsperitocracyAbilitySystemComponent::HandleChangeAbilityCanBeCanceled(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bCanBeCanceled)
{
	Super::HandleChangeAbilityCanBeCanceled(AbilityTags, RequestingAbility, bCanBeCanceled);

	//@TODO: Apply any special logic like blocking input or movement
}

void UProsperitocracyAbilitySystemComponent::GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const
{
	if (TagRelationshipMapping)
	{
		TagRelationshipMapping->GetRequiredAndBlockedActivationTags(AbilityTags, &OutActivationRequired, &OutActivationBlocked);
	}
}

void UProsperitocracyAbilitySystemComponent::SetTagRelationshipMapping(UProsperitocracyAbilityTagRelationshipMapping* NewMapping)
{
	TagRelationshipMapping = NewMapping;
}

void UProsperitocracyAbilitySystemComponent::ClientNotifyAbilityFailed_Implementation(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason)
{
	HandleAbilityFailed(Ability, FailureReason);
}

void UProsperitocracyAbilitySystemComponent::ClientNotifyHitMarker_Implementation(EProsperitocracyHitMarkerKind Kind)
{
	// The last hop: this component IS the man's, so the broadcast lands on his screen and on nobody
	// else's. The reticle is the listener; it owns what a marker looks like and how long it lasts.
	OnHitMarker.Broadcast(Kind);
}

void UProsperitocracyAbilitySystemComponent::HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason)
{
	//UE_LOG(LogProsperitocracyAbilitySystem, Warning, TEXT("Ability %s failed to activate (tags: %s)"), *GetPathNameSafe(Ability), *FailureReason.ToString());

	if (const UProsperitocracyGameplayAbility* ProsperitocracyAbility = Cast<const UProsperitocracyGameplayAbility>(Ability))
	{
		ProsperitocracyAbility->OnAbilityFailedToActivate(FailureReason);
	}	
}

bool UProsperitocracyAbilitySystemComponent::IsActivationGroupBlocked(EProsperitocracyAbilityActivationGroup Group) const
{
	bool bBlocked = false;

	switch (Group)
	{
	case EProsperitocracyAbilityActivationGroup::Independent:
		// Independent abilities are never blocked.
		bBlocked = false;
		break;

	case EProsperitocracyAbilityActivationGroup::Exclusive_Replaceable:
	case EProsperitocracyAbilityActivationGroup::Exclusive_Blocking:
		// Exclusive abilities can activate if nothing is blocking.
		bBlocked = (ActivationGroupCounts[(uint8)EProsperitocracyAbilityActivationGroup::Exclusive_Blocking] > 0);
		break;

	default:
		checkf(false, TEXT("IsActivationGroupBlocked: Invalid ActivationGroup [%d]\n"), (uint8)Group);
		break;
	}

	return bBlocked;
}

void UProsperitocracyAbilitySystemComponent::AddAbilityToActivationGroup(EProsperitocracyAbilityActivationGroup Group, UProsperitocracyGameplayAbility* ProsperitocracyAbility)
{
	check(ProsperitocracyAbility);
	check(ActivationGroupCounts[(uint8)Group] < INT32_MAX);

	ActivationGroupCounts[(uint8)Group]++;

	const bool bReplicateCancelAbility = false;

	switch (Group)
	{
	case EProsperitocracyAbilityActivationGroup::Independent:
		// Independent abilities do not cancel any other abilities.
		break;

	case EProsperitocracyAbilityActivationGroup::Exclusive_Replaceable:
	case EProsperitocracyAbilityActivationGroup::Exclusive_Blocking:
		CancelActivationGroupAbilities(EProsperitocracyAbilityActivationGroup::Exclusive_Replaceable, ProsperitocracyAbility, bReplicateCancelAbility);
		break;

	default:
		checkf(false, TEXT("AddAbilityToActivationGroup: Invalid ActivationGroup [%d]\n"), (uint8)Group);
		break;
	}

	const int32 ExclusiveCount = ActivationGroupCounts[(uint8)EProsperitocracyAbilityActivationGroup::Exclusive_Replaceable] + ActivationGroupCounts[(uint8)EProsperitocracyAbilityActivationGroup::Exclusive_Blocking];
	if (!ensure(ExclusiveCount <= 1))
	{
		UE_LOG(LogProsperitocracyAbilitySystem, Error, TEXT("AddAbilityToActivationGroup: Multiple exclusive abilities are running."));
	}
}

void UProsperitocracyAbilitySystemComponent::RemoveAbilityFromActivationGroup(EProsperitocracyAbilityActivationGroup Group, UProsperitocracyGameplayAbility* ProsperitocracyAbility)
{
	check(ProsperitocracyAbility);
	check(ActivationGroupCounts[(uint8)Group] > 0);

	ActivationGroupCounts[(uint8)Group]--;
}

void UProsperitocracyAbilitySystemComponent::CancelActivationGroupAbilities(EProsperitocracyAbilityActivationGroup Group, UProsperitocracyGameplayAbility* IgnoreProsperitocracyAbility, bool bReplicateCancelAbility)
{
	auto ShouldCancelFunc = [this, Group, IgnoreProsperitocracyAbility](const UProsperitocracyGameplayAbility* ProsperitocracyAbility, FGameplayAbilitySpecHandle Handle)
	{
		return ((ProsperitocracyAbility->GetActivationGroup() == Group) && (ProsperitocracyAbility != IgnoreProsperitocracyAbility));
	};

	CancelAbilitiesByFunc(ShouldCancelFunc, bReplicateCancelAbility);
}

// CUT (2026-09-16): AddDynamicTagGameplayEffect / RemoveDynamicTagGameplayEffect stood here. Both read
// UProsperitocracyAssetManager::GetSubclass(UProsperitocracyGameData::Get().DynamicTagGameplayEffect) —
// AssetManager + GameData, which have no asset in this project — and their only callers are
// Player/ProsperitocracyCheatManager.cpp:248-421, which is not ported. They return with that port.
// The tag path itself is not lost: gameplay tags are declared natively in ProsperitocracyGameplayTags.cpp,
// so tags still exist without a dynamic-tag gameplay effect.

void UProsperitocracyAbilitySystemComponent::GetAbilityTargetData(const FGameplayAbilitySpecHandle AbilityHandle, FGameplayAbilityActivationInfo ActivationInfo, FGameplayAbilityTargetDataHandle& OutTargetDataHandle)
{
	TSharedPtr<FAbilityReplicatedDataCache> ReplicatedData = AbilityTargetDataMap.Find(FGameplayAbilitySpecHandleAndPredictionKey(AbilityHandle, ActivationInfo.GetActivationPredictionKey()));
	if (ReplicatedData.IsValid())
	{
		OutTargetDataHandle = ReplicatedData->TargetData;
	}
}

