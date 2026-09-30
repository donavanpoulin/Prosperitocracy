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

	// THE INPUT PATH'S OTHER HALF. AbilityInputTagPressed/Released below only RECORD which abilities a key
	// touched; nothing acts on those records until ProcessAbilityInput runs, and in this component that
	// runs on the tick. It is the one piece of the ported input path that was never turned on, and
	// without it a number key banks a press and no ability ever starts.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UProsperitocracyAbilitySystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// SAID OUT LOUD whenever there is a press in hand and this tick is about to look at it: "the key did
	// nothing" must never again be able to mean "the tick never ran" or "the tick looked elsewhere" — those
	// are the two silences this line tells apart, and it prints the actor-info state it found.
	if (!InputPressedSpecHandles.IsEmpty())
	{
		UE_LOG(LogProsperitocracy, Log,
			TEXT("[Bar] the tick has %d press(es) in hand — actor info %s, locally controlled %s."),
			InputPressedSpecHandles.Num(),
			AbilityActorInfo.IsValid() ? TEXT("valid") : TEXT("MISSING"),
			(AbilityActorInfo.IsValid() && AbilityActorInfo->IsLocallyControlled()) ? TEXT("yes") : TEXT("no"));
	}

	// ONLY WHERE THE KEYS ARE. The input handles below are only ever fed from a LOCAL player's keys, so a
	// copy of this component with no keyboard behind it — the same body on someone else's machine — has
	// nothing to process and never activates anything off its own tick. That is the split the ported
	// input path already assumes everywhere else.
	if (AbilityActorInfo.IsValid() && AbilityActorInfo->IsLocallyControlled())
	{
		const UWorld* World = GetWorld();
		ProcessAbilityInput(DeltaTime, World && World->IsPaused());
	}
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
	// THE TICK'S OWN STATE, said out loud once when the body comes up — because a component whose flags say
	// "tick me every frame" and which is never ticked is the exact silence this input path dies of: the
	// engine is asked here, not guessed at.
	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Bar] %s comes up: canEverTick %s, startWithTickEnabled %s, tickEnabledNow %s, tickFunctionRegistered %s."),
		*GetName(),
		PrimaryComponentTick.bCanEverTick ? TEXT("yes") : TEXT("no"),
		PrimaryComponentTick.bStartWithTickEnabled ? TEXT("yes") : TEXT("no"),
		IsComponentTickEnabled() ? TEXT("yes") : TEXT("no"),
		PrimaryComponentTick.IsTickFunctionRegistered() ? TEXT("yes") : TEXT("no"));

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
	int32 Answered = 0;

	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
				++Answered;
			}
		}
	}

	// SAID OUT LOUD, because "the key did nothing" and "no ability answers that number" are one silence
	// in the world and only one of them is a bug. The count is the honest answer to it: a number key only
	// reaches an ability the loadout granted a bar tag onto, so a slot holding a passive — or an empty
	// one — reads as zero here rather than as a key that went missing.
	UE_LOG(LogProsperitocracy, Log, TEXT("[Bar] %s down — %d abilities answer it."), *InputTag.ToString(), Answered);

	// AND THE PRESS ACTS ON ITSELF. Recording a press and waiting for a tick to consume it is a hop this
	// game puts in exactly one place, and it is the hop that died: a number key that banks a press and never
	// starts anything is what "the key did nothing" looked like. Every other input reaches its ability the
	// moment it lands — the melee, the right press, the reload — so the bar does too: the same door that
	// recorded the press asks the ability system to act on it, here, in the same frame. The tick still calls
	// the same pass for anything HELD; it just finds nothing pressed left to start.
	if (Answered > 0)
	{
		ProcessAbilityInput(/*DeltaTime=*/ 0.0f, /*bGamePaused=*/ false);
	}
}

void UProsperitocracyAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	int32 Answered = 0;

	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.Remove(AbilitySpec.Handle);
				++Answered;
			}
		}
	}

	// Logged for the same reason the press is: an ability that should have happened on let-go and did not
	// has to be told apart from a key whose release never arrived at all.
	UE_LOG(LogProsperitocracy, Log, TEXT("[Bar] %s up — %d abilities answer it."), *InputTag.ToString(), Answered);

	// The other half of the same rule: "let go and it happens" IS the release, so it happens now rather than
	// on a tick — a let-go waiting for a tick it never gets is a key the player has to press twice.
	if (Answered > 0)
	{
		ProcessAbilityInput(/*DeltaTime=*/ 0.0f, /*bGamePaused=*/ false);
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
		// SAID OUT LOUD, because THIS is the half that starts an ability: a key that records a press and an
		// ability that never begins are one silence in the world, and this line is the difference. It prints
		// on the tick the press is acted on — before anything can refuse.
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilitySpecHandle))
		{
			UE_LOG(LogProsperitocracy, Log,
				TEXT("[Bar] the tick is starting %s — the press was recorded and this is the door that acts on it."),
				*GetNameSafe(AbilitySpec->Ability));
		}

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

void UProsperitocracyAbilitySystemComponent::ClientNotifyDamageFeedback_Implementation(const FProsperitocracyDamageFeedback& Feedback)
{
	// The last hop: this component IS the man's, so the broadcast lands on his screen and on nobody
	// else's. The HUD draws the numbers and the reticle draws the X; neither has to ask whose damage it
	// was, and neither owns the other's half of the payload.
	OnDamageFeedback.Broadcast(Feedback);
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

