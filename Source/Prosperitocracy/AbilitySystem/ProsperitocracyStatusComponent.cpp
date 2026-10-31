// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyStatusComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyStatusComponent)

// THE ONE FIRE COLOUR: what a burning body is lit with, warm rather than white so it reads as fire.
const FLinearColor UProsperitocracyStatusComponent::TheColourAFireIsLitWith(1.0f, 0.52f, 0.20f);

namespace ProsperitocracyStatus
{
	/**
	 * A status's colour on screen, per Design/ui.md — the debuff palette, Fire #FF3B00 and Stun
	 * #7A2BFF. Read as the design's own numbers rather than picked here, so the line that says a
	 * status landed and the UI that will show it cannot disagree.
	 */
	FColor ColorForStatus(const FGameplayTag& StatusTag)
	{
		if (StatusTag == ProsperitocracyGameplayTags::Status_Stun)
		{
			return FColor(0x7A, 0x2B, 0xFF);
		}
		if (StatusTag == ProsperitocracyGameplayTags::Status_Burn)
		{
			return FColor(0xFF, 0x3B, 0x00);
		}
		return FColor::White;
	}

	/** Say a status landed or ended, in the log and on screen, in the status's own colour. */
	void Report(const FGameplayTag& StatusTag, int32 OnScreenKey, const FString& Message)
	{
		UE_LOG(LogProsperitocracy, Display, TEXT("[Status] %s"), *Message);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(OnScreenKey, /*TimeToDisplay=*/ 3.0f, ColorForStatus(StatusTag), Message);
		}
	}

	/**
	 * A status's LOOK, paired here and nowhere else — the same shape as the colour above: ask the
	 * status's own identity and get the thing it wears. A status that is not named here simply has no
	 * look and is left alone, which is how a new status can arrive without this file being touched.
	 *
	 * These effects are the modern kind (the pack they come from is all Niagara), so a stop is a
	 * die-out rather than a cut: no more fire is spawned and the flames already on the body burn out
	 * at their own pace.
	 */
	const TCHAR* const BurnEffectPath = TEXT("/Game/Vefects/Free_Fire/Shared/Particles/NS_Fire_Medium.NS_Fire_Medium");

	UNiagaraSystem* FindStatusEffect(const FGameplayTag& StatusTag)
	{
		if (StatusTag == ProsperitocracyGameplayTags::Status_Burn)
		{
			return LoadObject<UNiagaraSystem>(nullptr, BurnEffectPath);
		}

		return nullptr;
	}
}

UProsperitocracyStatusComponent::UProsperitocracyStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UAbilitySystemComponent* UProsperitocracyStatusComponent::GetOwnerAbilitySystemComponent() const
{
	// The body's ASC, asked the same way the gun asks it of the pawn it is held by — one answer,
	// whichever actor it is on.
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
}

FProsperitocracyStatusQueue* UProsperitocracyStatusComponent::FindQueue(const FGameplayTag& StatusTag)
{
	for (FProsperitocracyStatusQueue& Queue : Queues)
	{
		if (Queue.StatusTag == StatusTag)
		{
			return &Queue;
		}
	}

	return nullptr;
}

const FProsperitocracyStatusQueue* UProsperitocracyStatusComponent::FindQueue(const FGameplayTag& StatusTag) const
{
	for (const FProsperitocracyStatusQueue& Queue : Queues)
	{
		if (Queue.StatusTag == StatusTag)
		{
			return &Queue;
		}
	}

	return nullptr;
}

FProsperitocracyLiveStatus* UProsperitocracyStatusComponent::FindWaiting(FProsperitocracyStatusQueue& Queue, const UProsperitocracyStatTable* Block)
{
	// IDENTITY IS THE BLOCK, never the tag. The rifle's burn and the fire's burn are the same STATUS and
	// different versions, so a version already on the body is the one that refreshes, and a different one
	// goes behind it — which is the whole reason two things that both set alight can both be felt.
	for (FProsperitocracyLiveStatus& Live : Queue.Waiting)
	{
		if (Live.Block == Block)
		{
			return &Live;
		}
	}

	return nullptr;
}

float UProsperitocracyStatusComponent::GetStatusStat(const FProsperitocracyLiveStatus& Live, EProsperitocracyStat Stat) const
{
	// Presence is scope, asked on the block: a status whose block does not carry the stat does not have
	// it, and "0" from the evaluator would not tell us that.
	if (!Live.Block || !Live.Block->Carries(Stat) || !Live.Host)
	{
		return 0.0f;
	}

	// And it is the FINAL value, through the one aggregator, on the status's own GAS home — the same
	// read every stat in the game takes. Nothing reads the block's authored base.
	return UProsperitocracyStatSystemStatics::GetStatFinal(Live.Host->GetProsperitocracyAbilitySystemComponent(), Stat);
}

bool UProsperitocracyStatusComponent::DoesTick(const FProsperitocracyLiveStatus& Live)
{
	// ONE rule, from the block's own presence, with no status vocabulary anywhere: a status block that
	// carries a Rate and a Piercing Damage ticks that damage. Stun's block carries neither, so it
	// simply lasts — which is the whole of what the design says separates the two.
	return Live.Block
		&& Live.Block->Carries(EProsperitocracyStat::Rate)
		&& Live.Block->Carries(EProsperitocracyStat::PiercingDamage);
}

float UProsperitocracyStatusComponent::GetTickInterval(const FProsperitocracyLiveStatus& Live) const
{
	const float Rate = GetStatusStat(Live, EProsperitocracyStat::Rate);

	// Rate is a per-second number, so the interval between ticks is one over it — one formula, the
	// same for any status that ticks, never an authored "tick every" number of its own.
	return (Rate > 0.0f) ? (1.0f / Rate) : 0.0f;
}

void UProsperitocracyStatusComponent::ApplyStatus(const FGameplayTag& StatusTag, const UProsperitocracyStatTable* Block,
	UAbilitySystemComponent* SourceAbilitySystemComponent, TSubclassOf<UGameplayEffect> DamageEffectClass, const FHitResult& Hit)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!Owner || !World || !Block || !StatusTag.IsValid() || !SourceAbilitySystemComponent || !DamageEffectClass)
	{
		return;
	}

	// A status is decided where the hit was resolved, once.
	if (!Owner->HasAuthority())
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	// THE QUEUE THIS STATUS BELONGS TO, when this body already has one of that kind — one per kind, and
	// the two are parallel, so a body can be burning and stunned at the same time.
	//
	// THE SAME BLOCK LANDS AGAIN: it keeps its place and its next tick, and only its clock moves — a
	// full one, from now. The clock is read off the entry's own home, so a refresh is priced exactly the
	// way it was first priced.
	//
	// Its TICK must not move with it. Standing in fire re-applies every fifth of a second, and a refresh
	// that pushed the first tick back again is the bug that once left a body burning on screen while
	// taking no damage at all until it walked out.
	FProsperitocracyStatusQueue* Queue = FindQueue(StatusTag);
	if (Queue)
	{
		if (FProsperitocracyLiveStatus* Already = FindWaiting(*Queue, Block))
		{
			const float RefreshDuration = GetStatusStat(*Already, EProsperitocracyStat::Duration);
			if (RefreshDuration > 0.0f)
			{
				Already->EndTime = Now + RefreshDuration;
			}
			return;
		}
	}

	// A DIFFERENT BLOCK — a different version of the status, whether it is worth more or less. Its own
	// GAS home is minted only now: a refresh above never needs one, and a host spawned to be thrown away
	// is a spawn per fifth of a second for nothing.
	AProsperitocracyStatHostActor* NewHost = World->SpawnActor<AProsperitocracyStatHostActor>();
	if (!NewHost)
	{
		return;
	}
	NewHost->InitializeFromStatBlock(Block);

	FProsperitocracyLiveStatus Prepared;
	Prepared.Block = Block;
	Prepared.Host = NewHost;
	Prepared.SourceAbilitySystemComponent = SourceAbilitySystemComponent;
	Prepared.DamageEffectClass = DamageEffectClass;
	Prepared.Hit = Hit;

	const float Duration = GetStatusStat(Prepared, EProsperitocracyStat::Duration);
	if (Duration <= 0.0f)
	{
		// A status with no Duration is not a status: nothing to carry, nothing to queue. Left alone
		// rather than applied forever, and said out loud because it is an authoring bug.
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Status] %s applied with no Duration in its block — nothing applied."), *StatusTag.ToString());
		NewHost->Destroy();
		return;
	}

	// ITS CLOCK STARTS NOW, WHETHER IT APPLIES NOW OR NOT. That is the whole difference between a queue
	// and a line of turns: the one behind is already burning down while it waits, and takes over with
	// what it has left — or is dropped, if its own time runs out first.
	Prepared.EndTime = Now + Duration;
	// A ticking status's first tick comes one interval after it lands, not on the same frame: N ticks
	// over N intervals is what "X damage every X seconds for X seconds" says.
	const float Interval = DoesTick(Prepared) ? GetTickInterval(Prepared) : 0.0f;
	Prepared.NextTickTime = (Interval > 0.0f) ? (Now + Interval) : Prepared.EndTime;

	// A QUEUE IS ITS CONTENTS: made here, only when there is something to put in it, so a status refused
	// above never leaves an empty queue behind it. Asked for again because minting the status's GAS home
	// above can run that actor's BeginPlay, and nothing may hold a pointer into an array across that.
	Queue = FindQueue(StatusTag);
	if (!Queue)
	{
		Queue = &Queues.Add_GetRef(FProsperitocracyStatusQueue());
		Queue->StatusTag = StatusTag;
	}

	// Only the head applies, so only the first one in is the status landing on this body.
	const bool bBecomesTheHead = (Queue->Waiting.Num() == 0);
	Queue->Waiting.Add(Prepared);

	if (bBecomesTheHead)
	{
		// The tag is the status's identity while it lasts, stamped once however many versions are behind
		// it — a count written, never added to, so a hundred shots are still one status.
		if (UAbilitySystemComponent* TargetAbilitySystemComponent = GetOwnerAbilitySystemComponent())
		{
			TargetAbilitySystemComponent->SetLooseGameplayTagCount(StatusTag, 1);
		}

		// And the LOOK goes on with the first one, for the body — never again at a handover, because the
		// fire belongs to the body and a handover changes the numbers, not the fire.
		BeginStatusEffect(*Queue);
	}

	ProsperitocracyStatus::Report(StatusTag, /*OnScreenKey=*/ 0x9010,
		bBecomesTheHead
			? FString::Printf(TEXT("%s on %s — %.1fs"), *StatusTag.ToString(), *Owner->GetName(), Duration)
			: FString::Printf(TEXT("%s joins %s — %.1fs, %d ahead of it, its clock already running"),
				*StatusTag.ToString(), *Owner->GetName(), Duration, Queue->Waiting.Num() - 1));
}

void UProsperitocracyStatusComponent::ApplyTickDamage(const FProsperitocracyLiveStatus& Live)
{
	UAbilitySystemComponent* Source = Live.SourceAbilitySystemComponent.Get();
	AActor* Owner = GetOwner();

	if (!Source || !Owner || !Live.DamageEffectClass)
	{
		return;
	}

	const float Amount = GetStatusStat(Live, EProsperitocracyStat::PiercingDamage);
	if (Amount <= 0.0f)
	{
		return;
	}

	// The line carries NO pen. Presence is scope, and the pipeline reads it that way: a status is not an
	// attack that penetrates anything, so no gate is asked and nothing can bounce it off a plate. It
	// reaches the target ITSELF, and what answers it is that target's own Piercing Resist, averaged
	// across its parts — the user's rule (2026-09-20): any burn burns a big guy whatever he is wearing.
	//
	// The damage line goes on the context — the same shape a shot's line takes — and runs through the
	// ONE applier into the ONE execution. The source is whoever set the status, so the kill is theirs.
	// No hit result is passed: a line with no pen has no part to be priced against.
	FGameplayEffectContextHandle Context = Source->MakeEffectContext();
	Context.AddInstigator(Source->GetAvatarActor_Direct(), Source->GetAvatarActor_Direct());

	UProsperitocracyDamageStatics::AddDamageLine(Context, ProsperitocracyGameplayTags::Damage_Type_Piercing, /*PenTier=*/ 0, Amount);
	UProsperitocracyDamageStatics::ApplyDamageEffectToHit(Context, Owner, Source, Live.DamageEffectClass);
}

USceneComponent* UProsperitocracyStatusComponent::WhereTheFireHangsOnThisBody(FName& OutBoneName) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// THE CHEST IS WHERE A BODY'S FIRE IS. On the capsule it followed a standing man and stayed standing while he
	// lay on the ground; on the chest it goes wherever the BODY goes, a ragdoll included.
	if (USkeletalMeshComponent* Body = Owner->FindComponentByClass<USkeletalMeshComponent>())
	{
		static const FName TheChestBonesToTry[] =
		{
			TEXT("spine_03"), TEXT("spine_02"), TEXT("spine_01"), TEXT("chest"), TEXT("pelvis")
		};

		for (const FName& Candidate : TheChestBonesToTry)
		{
			if (Body->GetBoneIndex(Candidate) != INDEX_NONE)
			{
				OutBoneName = Candidate;
				return Body;
			}
		}

		// No bone by any of those names: the body itself still follows better than the capsule does.
		OutBoneName = NAME_None;
		return Body;
	}

	OutBoneName = NAME_None;
	return Owner->GetRootComponent();
}

void UProsperitocracyStatusComponent::BeginStatusEffect(FProsperitocracyStatusQueue& Queue)
{
	AActor* Owner = GetOwner();
	UNiagaraSystem* Effect = Owner ? ProsperitocracyStatus::FindStatusEffect(Queue.StatusTag) : nullptr;

	// A status with no look of its own is left alone — presence is scope, asked here exactly as it is
	// asked of a stat. Nothing below belongs to any particular body.
	if (!Effect)
	{
		return;
	}

	FName TheBone;
	USceneComponent* AttachTo = WhereTheFireHangsOnThisBody(TheBone);
	if (!AttachTo)
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// THE BODY'S OWN FIRE, ON THE BODY'S OWN CHEST — no offset the author has to keep in step, and it is attached
	// rather than left standing where it was put, so a body that walks away carries its fire with it. It destroys
	// itself once its last flame dies, so nothing has to remember to clean this up.
	// KeepRelativeOffset and not SnapToTarget: snapped, the bone's own scale comes with it, and the fire would
	// take the rig's scale. At the socket with no offset is the chest, at the effect's own size.
	Queue.Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(Effect, AttachTo, TheBone,
		FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/ true, /*bAutoActivate=*/ true);

	if (!Queue.Effect)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Status] %s names a look that could not be put on %s — nothing spawned."),
			*Queue.StatusTag.ToString(), *GetNameSafe(Owner));
		return;
	}

	// THE FLAMES KEEP RISING, whichever way the body falls: it rides the chest for its POSITION and keeps its own
	// upright rotation, so a body on its side has fire standing out of it rather than fire lying flat on it.
	Queue.Effect->SetUsingAbsoluteRotation(true);

	// A burning body an enemy can see burning is burning on every client that can see it, so the fire
	// is replicated with the body it stands on rather than drawn on one machine only.
	Queue.Effect->SetIsReplicated(true);

	// AND ITS LIGHT IS OURS — one per body, on its own clock. The effect's own light is gone from the asset,
	// because a light per particle is a light that strobes as the cards spawn and die. It hangs on the SAME bone the
	// flames do: on the body's component alone it sat at the mesh's origin, which is between the feet.
	BeginTheLightOnThisBody(Queue, AttachTo, TheBone, Now);

	ProsperitocracyStatus::Report(Queue.StatusTag, /*OnScreenKey=*/ 0x9012,
		FString::Printf(TEXT("%s — its fire is on %s, on its chest, with one light of ours"),
			*Queue.StatusTag.ToString(), *GetNameSafe(Owner)));
}

void UProsperitocracyStatusComponent::BeginTheLightOnThisBody(FProsperitocracyStatusQueue& Queue, USceneComponent* AttachTo, FName OnBone, float Now)
{
	if (!AttachTo || Queue.Light.TheLight)
	{
		return;
	}

	UPointLightComponent* TheLight = NewObject<UPointLightComponent>(GetOwner());
	TheLight->SetMobility(EComponentMobility::Movable);
	TheLight->SetupAttachment(AttachTo, OnBone);
	TheLight->SetLightColor(TheColourAFireIsLitWith);
	TheLight->SetAttenuationRadius(HowFarTheLightOfABurningBodyReachesCm);
	TheLight->SetSourceRadius(HowSoftTheBurningBodysLightIsCm);

	// NO SHADOWS ON A BURNING BODY'S LIGHT — his call, and the honest answer to the engine's limit: a light on a body
	// casts that body's own torso onto the ground under it, and Unreal has no per-light-per-object shadow switch (only
	// three project-wide lighting channels). A suit of fire reads as a glow, not as a light with a silhouette.
	TheLight->SetCastShadows(false);
	TheLight->SetIntensity(0.0f);
	TheLight->RegisterComponent();

	Queue.Light.TheLight = TheLight;
	Queue.Light.WhatItIsWorth = CandelasABurningBodyIsLitWith;
	Queue.Light.WhenItWasLit = Now;
}

void UProsperitocracyStatusComponent::RetuneTheLightsOfBurningBodies(float Now)
{
	// WHILE IT BURNS: up over its own short ramp, then full worth for as long as the burn lasts. Every frame, so
	// the ramp is a ramp and not a step.
	for (FProsperitocracyStatusQueue& Queue : Queues)
	{
		UPointLightComponent* TheLight = Queue.Light.TheLight;
		if (!TheLight)
		{
			continue;
		}

		const float HowFarUpItIs = (HowLongABurningBodysLightTakesToComeUpSeconds <= 0.0f) ? 1.0f
			: FMath::Clamp((Now - Queue.Light.WhenItWasLit) / HowLongABurningBodysLightTakesToComeUpSeconds, 0.0f, 1.0f);

		TheLight->SetIntensity(Queue.Light.WhatItIsWorth * HowFarUpItIs);
	}

	// AND THE ONES WHOSE BURN HAS ENDED: out over the second the last flames on the body take, then gone.
	for (int32 Index = LightsGoingOut.Num() - 1; Index >= 0; --Index)
	{
		FProsperitocracyBurningBodyLight& GoingOut = LightsGoingOut[Index];
		if (!GoingOut.TheLight)
		{
			LightsGoingOut.RemoveAt(Index);
			continue;
		}

		const float HowFarOutItIs = (HowLongTheLightOnABurningBodyTakesToGoOutSeconds <= 0.0f) ? 1.0f
			: FMath::Clamp((Now - GoingOut.WhenItStartedGoingOut) / HowLongTheLightOnABurningBodyTakesToGoOutSeconds, 0.0f, 1.0f);

		if (HowFarOutItIs >= 1.0f)
		{
			GoingOut.TheLight->DestroyComponent();
			LightsGoingOut.RemoveAt(Index);
			continue;
		}

		GoingOut.TheLight->SetIntensity(GoingOut.WhatItIsWorth * (1.0f - HowFarOutItIs));
	}
}

void UProsperitocracyStatusComponent::EndStatusEffect(FProsperitocracyStatusQueue& Queue)
{
	if (UNiagaraComponent* Effect = Queue.Effect)
	{
		// A STOP, not a cut: the effect spawns nothing more and puts itself out — the flames already on
		// the body burn for as long as their own particles last, and the component destroys itself when
		// the last one dies. Nothing is killed here and nothing is held on to.
		Effect->Deactivate();
		Queue.Effect = nullptr;
	}

	// THE LIGHT LEAVES WITH THE FLAMES, NOT WITH THE STATUS: the burn's own second is how long the last cards on
	// the body take to die, so the light is handed to the ones going out and fades across exactly that.
	if (Queue.Light.TheLight)
	{
		FProsperitocracyBurningBodyLight GoingOut = Queue.Light;
		GoingOut.WhenItStartedGoingOut = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		LightsGoingOut.Add(GoingOut);
		Queue.Light = FProsperitocracyBurningBodyLight();
	}
}

bool UProsperitocracyStatusComponent::IsMovementStopped() const
{
	// The movement gate asks by IDENTITY, not by numbers: Stun is the status whose tag says movement
	// stops. A status being short or long is not what stops you — a slow, a bubble or a bleed can have
	// any duration and none of them take your legs.
	//
	// And it is the HEAD that stops you — but a queue with anything in it always has a head, so the
	// question is simply whether this body is carrying a stun at all. The one behind does not stun you
	// early, and the one that takes over does not stun you late: it stuns you from the frame it is the head.
	const FProsperitocracyStatusQueue* Queue = FindQueue(ProsperitocracyGameplayTags::Status_Stun);
	return Queue != nullptr && Queue->Waiting.Num() > 0;
}

void UProsperitocracyStatusComponent::UpdateMovementGate()
{
	const bool bStopped = IsMovementStopped();
	if (bStopped == bMovementStopped)
	{
		return;
	}

	bMovementStopped = bStopped;
	AActor* Owner = GetOwner();

	if (bStopped)
	{
		// The instant it lands: stopped dead. Its own speed is gone — and that is all that is gone.
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
		}

		ProsperitocracyStatus::Report(ProsperitocracyGameplayTags::Status_Stun, /*OnScreenKey=*/ 0x9011,
			FString::Printf(TEXT("Stun — stopped dead, controls gone%s"),
				Cast<ACharacter>(Owner) && Cast<ACharacter>(Owner)->GetCharacterMovement()
					&& Cast<ACharacter>(Owner)->GetCharacterMovement()->IsFalling() ? TEXT(" (in the air: still falling)") : TEXT("")));
	}

	// The controls, both ways, and nothing else touched. Taking the input away IS the mechanic: with
	// no input the body cannot walk, jump, fire or use anything, while a shove still shoves it and
	// gravity still drops it — which is exactly why a body stunned in the air lands instead of hanging.
	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController()))
		{
			if (bStopped)
			{
				PlayerController->DisableInput(PlayerController);
			}
			else
			{
				PlayerController->EnableInput(PlayerController);
				ProsperitocracyStatus::Report(ProsperitocracyGameplayTags::Status_Stun, /*OnScreenKey=*/ 0x9011,
					TEXT("Stun over — controls back"));
			}
		}
	}
}

void UProsperitocracyStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	// Backwards, because ending a queue that has nothing left removes it from the array.
	for (int32 QueueIndex = Queues.Num() - 1; QueueIndex >= 0; --QueueIndex)
	{
		FProsperitocracyStatusQueue& Queue = Queues[QueueIndex];

		// THE ONES STILL WAITING CAN DIE BEFORE THEIR TURN — their clocks have been running since they
		// landed, so one whose own time is up while it waits never applies at all.
		DropExpiredWaiters(Queue, Now);

		if (Queue.Waiting.Num() == 0)
		{
			Queues.RemoveAt(QueueIndex);
			continue;
		}

		FProsperitocracyLiveStatus& Head = Queue.Waiting[0];

		// Out of time: the head ends and the one behind it takes over with what it has left.
		if (Now >= Head.EndTime)
		{
			EndTheHead(QueueIndex);
			continue;
		}

		// ONLY THE HEAD APPLIES. The ones behind it tick nothing and wear nothing while they wait.
		if (DoesTick(Head) && Now >= Head.NextTickTime)
		{
			const float Interval = GetTickInterval(Head);
			ApplyTickDamage(Head);
			// The next tick is one interval on, from the tick that just fired — a late frame shifts the
			// ticks, it never fires two at once.
			Head.NextTickTime = (Interval > 0.0f) ? (Head.NextTickTime + Interval) : Head.EndTime;
		}
	}

	RetuneTheLightsOfBurningBodies(Now);

	UpdateMovementGate();
}

void UProsperitocracyStatusComponent::EndTheHead(int32 QueueIndex)
{
	if (!Queues.IsValidIndex(QueueIndex) || Queues[QueueIndex].Waiting.Num() == 0)
	{
		return;
	}

	FProsperitocracyStatusQueue& Queue = Queues[QueueIndex];
	const FGameplayTag StatusTag = Queue.StatusTag;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// The head's own GAS home goes with it, and nothing lingers behind it.
	if (AProsperitocracyStatHostActor* Host = Queue.Waiting[0].Host)
	{
		Host->Destroy();
	}

	Queue.Waiting.RemoveAt(0);

	// THE ONE BEHIND IT IS THE HEAD NOW, with the time it has LEFT — not a fresh clock, because its clock
	// has been running since the moment it landed. The look does not move: the fire belongs to the body,
	// and what changes at a handover is the tick's damage and how often it lands.
	if (Queue.Waiting.Num() > 0)
	{
		const float SecondsLeft = FMath::Max(0.0f, Queue.Waiting[0].EndTime - Now);
		ProsperitocracyStatus::Report(StatusTag, /*OnScreenKey=*/ 0x9010,
			FString::Printf(TEXT("%s on %s takes over — %.1fs left of its own clock"),
				*StatusTag.ToString(), *GetNameSafe(GetOwner()), SecondsLeft));
		return;
	}

	// Nothing of this status is left on the body: the look goes out, the tag comes off, the queue goes.
	EndStatusEffect(Queue);

	if (UAbilitySystemComponent* TargetAbilitySystemComponent = GetOwnerAbilitySystemComponent())
	{
		TargetAbilitySystemComponent->SetLooseGameplayTagCount(StatusTag, 0);
	}

	ProsperitocracyStatus::Report(StatusTag, /*OnScreenKey=*/ 0x9010,
		FString::Printf(TEXT("%s ended on %s"), *StatusTag.ToString(), *GetNameSafe(GetOwner())));

	Queues.RemoveAt(QueueIndex);
}

void UProsperitocracyStatusComponent::DropExpiredWaiters(FProsperitocracyStatusQueue& Queue, float Now)
{
	// From the back down to the head: the head's own ending is the queue's business (EndTheHead), and this
	// only removes the ones whose clock ran out before they ever got to apply.
	for (int32 Index = Queue.Waiting.Num() - 1; Index >= 1; --Index)
	{
		FProsperitocracyLiveStatus& Live = Queue.Waiting[Index];
		if (Now < Live.EndTime)
		{
			continue;
		}

		if (AProsperitocracyStatHostActor* Host = Live.Host)
		{
			Host->Destroy();
		}

		// Said out loud: a version that never landed and a version that hit are one silence otherwise.
		ProsperitocracyStatus::Report(Queue.StatusTag, /*OnScreenKey=*/ 0x9010,
			FString::Printf(TEXT("%s died waiting on %s — never applied"),
				*Queue.StatusTag.ToString(), *GetNameSafe(GetOwner())));

		Queue.Waiting.RemoveAt(Index);
	}
}

void UProsperitocracyStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A body that goes away takes its statuses and their GAS homes with it — and the lights they throw,
	// rather than leaving a lamp hanging in the world with nothing burning under it.
	for (FProsperitocracyStatusQueue& Queue : Queues)
	{
		for (FProsperitocracyLiveStatus& Live : Queue.Waiting)
		{
			if (AProsperitocracyStatHostActor* Host = Live.Host)
			{
				Host->Destroy();
			}
		}

		if (Queue.Light.TheLight)
		{
			Queue.Light.TheLight->DestroyComponent();
		}
	}
	Queues.Reset();

	// AND THE LIGHTS OF BURNS THAT ARE STILL GOING OUT — a body that goes away takes them too, rather than leaving
	// a lamp hanging in the world with nothing burning under it.
	for (const FProsperitocracyBurningBodyLight& GoingOut : LightsGoingOut)
	{
		if (GoingOut.TheLight)
		{
			GoingOut.TheLight->DestroyComponent();
		}
	}
	LightsGoingOut.Reset();

	Super::EndPlay(EndPlayReason);
}
