// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProsperitocracyGameplayAbility.h"
#include "ProsperitocracyLogChannels.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystemLog.h"
// CUT (2026-09-16): Player/ProsperitocracyPlayerController.h — unported; only the Interact ability used the cast.
// CUT (2026-09-16): Character/AProsperitocracyCharacter.h — unported; our character is a Part 7 job.
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyAbilityCost.h"
// CUT (2026-09-16): Character/ProsperitocracyHeroComponent.h — unported; the camera modes went with it.
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "ProsperitocracyAbilitySimpleFailureMessage.h"
// CUT (2026-09-16): GameFramework/GameplayMessageSubsystem.h — the Lyra plugin (decision at box 1.4a).
#include "AbilitySystem/ProsperitocracyAbilitySourceInterface.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Physics/PhysicalMaterialWithTags.h"
#include "GameFramework/PlayerState.h"
// CUT (2026-09-16): Camera/ProsperitocracyCameraMode.h — CameraMode drags the camera component and the
// player camera manager; camera here is theirs (IA_CameraChange).

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyGameplayAbility)

#define ENSURE_ABILITY_IS_INSTANTIATED_OR_RETURN(FunctionName, ReturnValue)																				\
{																																						\
	if (!ensure(IsInstantiated()))																														\
	{																																					\
		ABILITY_LOG(Error, TEXT("%s: " #FunctionName " cannot be called on a non-instanced ability. Check the instancing policy."), *GetPathName());	\
		return ReturnValue;																																\
	}																																					\
}

UE_DEFINE_GAMEPLAY_TAG(TAG_ABILITY_SIMPLE_FAILURE_MESSAGE, "Ability.UserFacingSimpleActivateFail.Message");
UE_DEFINE_GAMEPLAY_TAG(TAG_ABILITY_PLAY_MONTAGE_FAILURE_MESSAGE, "Ability.PlayMontageOnActivateFail.Message");

UProsperitocracyGameplayAbility::UProsperitocracyGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;

	ActivationPolicy = EProsperitocracyAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EProsperitocracyAbilityActivationGroup::Independent;

	bLogCancelation = false;

	// CUT (2026-09-16): ActiveCameraMode = nullptr; — the member went with SetCameraMode/ClearCameraMode.
}

UProsperitocracyAbilitySystemComponent* UProsperitocracyGameplayAbility::GetProsperitocracyAbilitySystemComponentFromActorInfo() const
{
	return (CurrentActorInfo ? Cast<UProsperitocracyAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr);
}

// CUT (2026-09-16): GetProsperitocracyPlayerControllerFromActorInfo — only caller was the unported
// Interact ability, and AProsperitocracyPlayerController is not in this project.

AController* UProsperitocracyGameplayAbility::GetControllerFromActorInfo() const
{
	if (CurrentActorInfo)
	{
		if (AController* PC = CurrentActorInfo->PlayerController.Get())
		{
			return PC;
		}

		// Look for a player controller or pawn in the owner chain.
		AActor* TestActor = CurrentActorInfo->OwnerActor.Get();
		while (TestActor)
		{
			if (AController* C = Cast<AController>(TestActor))
			{
				return C;
			}

			if (APawn* Pawn = Cast<APawn>(TestActor))
			{
				return Pawn->GetController();
			}

			TestActor = TestActor->GetOwner();
		}
	}

	return nullptr;
}

// CUT (2026-09-16): GetProsperitocracyCharacterFromActorInfo — only caller was the Jump ability;
// our character sits on their BP_ThirdPersonCharacter and is a Part 7 job.
// CUT (2026-09-16): GetHeroComponentFromActorInfo — only used by SetCameraMode/ClearCameraMode.

void UProsperitocracyGameplayAbility::NativeOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const
{
	for (FGameplayTag Reason : FailedReason)
	{
		if (const FText* pUserFacingMessage = FailureTagToUserFacingMessages.Find(Reason))
		{
			// CUT (2026-09-16): this branch built FProsperitocracyAbilitySimpleFailureMessage and
			// broadcast it through UGameplayMessageSubsystem — the Lyra plugin, cut by decision at
			// box 1.4a, exactly like the health set's damage verb. The lookup stays and the failure
			// is logged on our own channel until our own messages port carries it instead.
			UE_LOG(LogProsperitocracyAbilitySystem, Warning, TEXT("Ability %s failed to activate [%s]: %s"), *GetPathName(), *Reason.ToString(), *pUserFacingMessage->ToString());
		}
		
		if (const UAnimMontage* pMontage = FailureTagToAnimMontage.FindRef(Reason))
		{
			// CUT (2026-09-16): same broadcast — whoever listened to it played the montage. Logged so
			// the mapping is not silently ignored; the montage plays again when our messages port lands.
			UE_LOG(LogProsperitocracyAbilitySystem, Warning, TEXT("Ability %s failed to activate [%s]: montage %s mapped but not played (messages port pending)."), *GetPathName(), *Reason.ToString(), *GetNameSafe(pMontage));
		}
	}
}

bool UProsperitocracyGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	//@TODO Possibly remove after setting up tag relationships
	UProsperitocracyAbilitySystemComponent* ProsperitocracyASC = CastChecked<UProsperitocracyAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (ProsperitocracyASC->IsActivationGroupBlocked(ActivationGroup))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(ProsperitocracyGameplayTags::Ability_ActivateFail_ActivationGroup);
		}
		return false;
	}

	return true;
}

void UProsperitocracyGameplayAbility::SetCanBeCanceled(bool bCanBeCanceled)
{
	// The ability can not block canceling if it's replaceable.
	if (!bCanBeCanceled && (ActivationGroup == EProsperitocracyAbilityActivationGroup::Exclusive_Replaceable))
	{
		UE_LOG(LogProsperitocracyAbilitySystem, Error, TEXT("SetCanBeCanceled: Ability [%s] can not block canceling because its activation group is replaceable."), *GetName());
		return;
	}

	Super::SetCanBeCanceled(bCanBeCanceled);
}

void UProsperitocracyGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	K2_OnAbilityAdded();

	// The ability's GAS home: spawned as soon as the avatar is known (lazily retried on the
	// first activation if it isn't yet).
	EnsureStatHost(ActorInfo);

	TryActivateAbilityOnSpawn(ActorInfo, Spec);
}

void UProsperitocracyGameplayAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	K2_OnAbilityRemoved();

	if (StatHost)
	{
		StatHost->Destroy();
		StatHost = nullptr;
	}

	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UProsperitocracyGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Retry the stat host if it couldn't spawn at give time (avatar may not have been ready).
	EnsureStatHost(ActorInfo);

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UProsperitocracyGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// CUT (2026-09-16): ClearCameraMode() — went with SetCameraMode/ClearCameraMode; camera is theirs.

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UProsperitocracyGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) || !ActorInfo)
	{
		return false;
	}

	// Verify we can afford any additional costs
	for (const TObjectPtr<UProsperitocracyAbilityCost>& AdditionalCost : AdditionalCosts)
	{
		if (AdditionalCost != nullptr)
		{
			if (!AdditionalCost->CheckCost(this, Handle, ActorInfo, /*inout*/ OptionalRelevantTags))
			{
				return false;
			}
		}
	}

	return true;
}

void UProsperitocracyGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	check(ActorInfo);

	// Used to determine if the ability actually hit a target (as some costs are only spent on successful attempts)
	auto DetermineIfAbilityHitTarget = [&]()
	{
		if (ActorInfo->IsNetAuthority())
		{
			if (UProsperitocracyAbilitySystemComponent* ASC = Cast<UProsperitocracyAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
			{
				FGameplayAbilityTargetDataHandle TargetData;
				ASC->GetAbilityTargetData(Handle, ActivationInfo, TargetData);
				for (int32 TargetDataIdx = 0; TargetDataIdx < TargetData.Data.Num(); ++TargetDataIdx)
				{
					if (UAbilitySystemBlueprintLibrary::TargetDataHasHitResult(TargetData, TargetDataIdx))
					{
						return true;
					}
				}
			}
		}

		return false;
	};

	// Pay any additional costs
	bool bAbilityHitTarget = false;
	bool bHasDeterminedIfAbilityHitTarget = false;
	for (const TObjectPtr<UProsperitocracyAbilityCost>& AdditionalCost : AdditionalCosts)
	{
		if (AdditionalCost != nullptr)
		{
			if (AdditionalCost->ShouldOnlyApplyCostOnHit())
			{
				if (!bHasDeterminedIfAbilityHitTarget)
				{
					bAbilityHitTarget = DetermineIfAbilityHitTarget();
					bHasDeterminedIfAbilityHitTarget = true;
				}

				if (!bAbilityHitTarget)
				{
					continue;
				}
			}

			AdditionalCost->ApplyCost(this, Handle, ActorInfo, ActivationInfo);
		}
	}
}

FGameplayEffectContextHandle UProsperitocracyGameplayAbility::MakeEffectContext(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	FGameplayEffectContextHandle ContextHandle = Super::MakeEffectContext(Handle, ActorInfo);

	FProsperitocracyGameplayEffectContext* EffectContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(ContextHandle);
	check(EffectContext);

	check(ActorInfo);

	AActor* EffectCauser = nullptr;
	const IProsperitocracyAbilitySourceInterface* AbilitySource = nullptr;
	float SourceLevel = 0.0f;
	GetAbilitySource(Handle, ActorInfo, /*out*/ SourceLevel, /*out*/ AbilitySource, /*out*/ EffectCauser);

	UObject* SourceObject = GetSourceObject(Handle, ActorInfo);

	AActor* Instigator = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr;

	EffectContext->SetAbilitySource(AbilitySource, SourceLevel);
	EffectContext->AddInstigator(Instigator, EffectCauser);
	EffectContext->AddSourceObject(SourceObject);

	return ContextHandle;
}

void UProsperitocracyGameplayAbility::ApplyAbilityTagsToGameplayEffectSpec(FGameplayEffectSpec& Spec, FGameplayAbilitySpec* AbilitySpec) const
{
	Super::ApplyAbilityTagsToGameplayEffectSpec(Spec, AbilitySpec);

	if (const FHitResult* HitResult = Spec.GetContext().GetHitResult())
	{
		if (const UPhysicalMaterialWithTags* PhysMatWithTags = Cast<const UPhysicalMaterialWithTags>(HitResult->PhysMaterial.Get()))
		{
			Spec.CapturedTargetTags.GetSpecTags().AppendTags(PhysMatWithTags->Tags);
		}
	}
}

bool UProsperitocracyGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// Specialized version to handle death exclusion and AbilityTags expansion via ASC

	bool bBlocked = false;
	bool bMissing = false;

	UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();
	const FGameplayTag& BlockedTag = AbilitySystemGlobals.ActivateFailTagsBlockedTag;
	const FGameplayTag& MissingTag = AbilitySystemGlobals.ActivateFailTagsMissingTag;

	// Check if any of this ability's tags are currently blocked
	if (AbilitySystemComponent.AreAbilityTagsBlocked(GetAssetTags()))
	{
		bBlocked = true;
	}

	const UProsperitocracyAbilitySystemComponent* ProsperitocracyASC = Cast<UProsperitocracyAbilitySystemComponent>(&AbilitySystemComponent);
	static FGameplayTagContainer AllRequiredTags;
	static FGameplayTagContainer AllBlockedTags;

	AllRequiredTags = ActivationRequiredTags;
	AllBlockedTags = ActivationBlockedTags;

	// Expand our ability tags to add additional required/blocked tags
	if (ProsperitocracyASC)
	{
		ProsperitocracyASC->GetAdditionalActivationTagRequirements(GetAssetTags(), AllRequiredTags, AllBlockedTags);
	}

	// Check to see the required/blocked tags for this ability
	if (AllBlockedTags.Num() || AllRequiredTags.Num())
	{
		static FGameplayTagContainer AbilitySystemComponentTags;
		
		AbilitySystemComponentTags.Reset();
		AbilitySystemComponent.GetOwnedGameplayTags(AbilitySystemComponentTags);

		if (AbilitySystemComponentTags.HasAny(AllBlockedTags))
		{
			if (OptionalRelevantTags && AbilitySystemComponentTags.HasTag(ProsperitocracyGameplayTags::Status_Death))
			{
				// If player is dead and was rejected due to blocking tags, give that feedback
				OptionalRelevantTags->AddTag(ProsperitocracyGameplayTags::Ability_ActivateFail_IsDead);
			}

			bBlocked = true;
		}

		if (!AbilitySystemComponentTags.HasAll(AllRequiredTags))
		{
			bMissing = true;
		}
	}

	if (SourceTags != nullptr)
	{
		if (SourceBlockedTags.Num() || SourceRequiredTags.Num())
		{
			if (SourceTags->HasAny(SourceBlockedTags))
			{
				bBlocked = true;
			}

			if (!SourceTags->HasAll(SourceRequiredTags))
			{
				bMissing = true;
			}
		}
	}

	if (TargetTags != nullptr)
	{
		if (TargetBlockedTags.Num() || TargetRequiredTags.Num())
		{
			if (TargetTags->HasAny(TargetBlockedTags))
			{
				bBlocked = true;
			}

			if (!TargetTags->HasAll(TargetRequiredTags))
			{
				bMissing = true;
			}
		}
	}

	if (bBlocked)
	{
		if (OptionalRelevantTags && BlockedTag.IsValid())
		{
			OptionalRelevantTags->AddTag(BlockedTag);
		}
		return false;
	}
	if (bMissing)
	{
		if (OptionalRelevantTags && MissingTag.IsValid())
		{
			OptionalRelevantTags->AddTag(MissingTag);
		}
		return false;
	}

	return true;
}

void UProsperitocracyGameplayAbility::OnPawnAvatarSet()
{
	K2_OnPawnAvatarSet();
}

void UProsperitocracyGameplayAbility::GetAbilitySource(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& OutSourceLevel, const IProsperitocracyAbilitySourceInterface*& OutAbilitySource, AActor*& OutEffectCauser) const
{
	OutSourceLevel = 0.0f;
	OutAbilitySource = nullptr;
	OutEffectCauser = nullptr;

	OutEffectCauser = ActorInfo->AvatarActor.Get();

	// An ability with a stat block IS its own source (see StatBlock): its stats live on the
	// ability's GAS home and GetDamageLines resolves them through the ONE evaluator. Fall back
	// to the spec's source object otherwise (a weapon-granted ability keeps its weapon instance).
	if (StatBlock)
	{
		OutAbilitySource = this;
		return;
	}

	// If we were added by something that's an ability info source, use it
	UObject* SourceObject = GetSourceObject(Handle, ActorInfo);

	OutAbilitySource = Cast<IProsperitocracyAbilitySourceInterface>(SourceObject);
}

void UProsperitocracyGameplayAbility::EnsureStatHost(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (StatHost || !StatBlock || !ActorInfo)
	{
		return;
	}

	const AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor || !AvatarActor->GetWorld())
	{
		// Avatar not ready yet — ActivateAbility retries.
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = const_cast<AActor*>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	StatHost = AvatarActor->GetWorld()->SpawnActor<AProsperitocracyStatHostActor>(AProsperitocracyStatHostActor::StaticClass(), AvatarActor->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	if (StatHost)
	{
		StatHost->InitializeFromStatBlock(StatBlock);
	}
}

float UProsperitocracyGameplayAbility::GetStatFinalValue(EProsperitocracyStat Stat) const
{
	// The ability's stats are GAS attributes on its own host ASC (the thing's GAS home); the
	// aggregator's current value IS (base + Σflat) × Σpercent — the ONE evaluator. No raw
	// base is ever read: absent stat = base 0 = final 0 (presence-is-scope).
	if (const AProsperitocracyStatHostActor* Host = StatHost)
	{
		if (const UProsperitocracyAbilitySystemComponent* ASC = Host->GetProsperitocracyAbilitySystemComponent())
		{
			return UProsperitocracyStatSystemStatics::GetStatFinal(ASC, Stat);
		}
	}
	return 0.0f;
}

void UProsperitocracyGameplayAbility::AddDamageLinesToContext(FGameplayEffectContextHandle& Context)
{
	if (!Context.IsValid())
	{
		return;
	}

	// Presence-is-scope: the ability's block has ImpactDamage and/or PiercingDamage, or the
	// ability deals nothing. Both values are evaluated by the ability's GAS home (the ONE
	// evaluator) at this moment — no raw base is ever read.
	const float ImpactAmount = GetStatFinalValue(EProsperitocracyStat::ImpactDamage);
	const float PiercingAmount = GetStatFinalValue(EProsperitocracyStat::PiercingDamage);
	if (ImpactAmount <= 0.0f && PiercingAmount <= 0.0f)
	{
		return;
	}

	const int32 Pen = FMath::Clamp(FMath::RoundToInt(GetStatFinalValue(EProsperitocracyStat::Penetration)), 1, 4);

	if (ImpactAmount > 0.0f)
	{
		UProsperitocracyDamageStatics::AddDamageLine(Context, ProsperitocracyGameplayTags::Damage_Type_Impact, Pen, ImpactAmount);
	}
	if (PiercingAmount > 0.0f)
	{
		UProsperitocracyDamageStatics::AddDamageLine(Context, ProsperitocracyGameplayTags::Damage_Type_Piercing, Pen, PiercingAmount);
	}
}

float UProsperitocracyGameplayAbility::GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	// The ability's Range/Falloff stats, evaluated by its GAS home — the ONE universal falloff
	// (Design/damage.md): full until Falloff, linear to 0 at Range. Absent stats = no falloff.
	return UProsperitocracyStatSystemStatics::ComputeDistanceAttenuation(Distance, GetStatFinalValue(EProsperitocracyStat::Range), GetStatFinalValue(EProsperitocracyStat::Falloff));
}

void UProsperitocracyGameplayAbility::SetStatHostAsAbilitySource(FGameplayEffectContextHandle& Context)
{
	if (!Context.IsValid() || !StatHost)
	{
		return;
	}

	if (FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context))
	{
		// The stat host, not the ability instance (which may be dead by the time the damage
		// applies — e.g. a grenade detonating after the ability ended): the host provides the
		// damage lines and the distance falloff from its evaluated stats, safely.
		TypedContext->SetAbilitySource(StatHost, 1.0f);
	}
}

void UProsperitocracyGameplayAbility::GetDamageLines(TArray<FProsperitocracyDamageLine>& OutLines) const
{
	OutLines.Reset();

	// Presence-is-scope: the block has ImpactDamage and/or PiercingDamage, or the ability doesn't
	// deal damage. Both lines evaluate through the ability's GAS home (the ONE evaluator).
	const float ImpactAmount = GetStatFinalValue(EProsperitocracyStat::ImpactDamage);
	const float PiercingAmount = GetStatFinalValue(EProsperitocracyStat::PiercingDamage);
	if (ImpactAmount <= 0.0f && PiercingAmount <= 0.0f)
	{
		return;
	}

	const int32 Pen = FMath::Clamp(FMath::RoundToInt(GetStatFinalValue(EProsperitocracyStat::Penetration)), 1, 4);

	if (ImpactAmount > 0.0f)
	{
		FProsperitocracyDamageLine Line;
		Line.Type = ProsperitocracyGameplayTags::Damage_Type_Impact;
		Line.PenTier = Pen;
		Line.Amount = ImpactAmount;
		OutLines.Add(Line);
	}
	if (PiercingAmount > 0.0f)
	{
		FProsperitocracyDamageLine Line;
		Line.Type = ProsperitocracyGameplayTags::Damage_Type_Piercing;
		Line.PenTier = Pen;
		Line.Amount = PiercingAmount;
		OutLines.Add(Line);
	}
}

void UProsperitocracyGameplayAbility::TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	// Try to activate if activation policy is on spawn.
	if (ActorInfo && !Spec.IsActive() && (ActivationPolicy == EProsperitocracyAbilityActivationPolicy::OnSpawn))
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		const AActor* AvatarActor = ActorInfo->AvatarActor.Get();

		// If avatar actor is torn off or about to die, don't try to activate until we get the new one.
		if (ASC && AvatarActor && !AvatarActor->GetTearOff() && (AvatarActor->GetLifeSpan() <= 0.0f))
		{
			const bool bIsLocalExecution = (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted) || (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly);
			const bool bIsServerExecution = (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly) || (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated);

			const bool bClientShouldActivate = ActorInfo->IsLocallyControlled() && bIsLocalExecution;
			const bool bServerShouldActivate = ActorInfo->IsNetAuthority() && bIsServerExecution;

			if (bClientShouldActivate || bServerShouldActivate)
			{
				ASC->TryActivateAbility(Spec.Handle);
			}
		}
	}
}

bool UProsperitocracyGameplayAbility::CanChangeActivationGroup(EProsperitocracyAbilityActivationGroup NewGroup) const
{
	if (!IsInstantiated() || !IsActive())
	{
		return false;
	}

	if (ActivationGroup == NewGroup)
	{
		return true;
	}

	UProsperitocracyAbilitySystemComponent* ProsperitocracyASC = GetProsperitocracyAbilitySystemComponentFromActorInfo();
	check(ProsperitocracyASC);

	if ((ActivationGroup != EProsperitocracyAbilityActivationGroup::Exclusive_Blocking) && ProsperitocracyASC->IsActivationGroupBlocked(NewGroup))
	{
		// This ability can't change groups if it's blocked (unless it is the one doing the blocking).
		return false;
	}

	if ((NewGroup == EProsperitocracyAbilityActivationGroup::Exclusive_Replaceable) && !CanBeCanceled())
	{
		// This ability can't become replaceable if it can't be canceled.
		return false;
	}

	return true;
}

bool UProsperitocracyGameplayAbility::ChangeActivationGroup(EProsperitocracyAbilityActivationGroup NewGroup)
{
	ENSURE_ABILITY_IS_INSTANTIATED_OR_RETURN(ChangeActivationGroup, false);

	if (!CanChangeActivationGroup(NewGroup))
	{
		return false;
	}

	if (ActivationGroup != NewGroup)
	{
		UProsperitocracyAbilitySystemComponent* ProsperitocracyASC = GetProsperitocracyAbilitySystemComponentFromActorInfo();
		check(ProsperitocracyASC);

		ProsperitocracyASC->RemoveAbilityFromActivationGroup(ActivationGroup, this);
		ProsperitocracyASC->AddAbilityToActivationGroup(NewGroup, this);

		ActivationGroup = NewGroup;
	}

	return true;
}

// CUT (2026-09-16): SetCameraMode / ClearCameraMode stood here. They only forwarded to
// UProsperitocracyHeroComponent (unported) and carried TSubclassOf<UProsperitocracyCameraMode>,
// which drags ProsperitocracyCameraComponent + ProsperitocracyPlayerCameraManager. The camera here is
// theirs (IA_CameraChange), so an ability does not set camera modes.

