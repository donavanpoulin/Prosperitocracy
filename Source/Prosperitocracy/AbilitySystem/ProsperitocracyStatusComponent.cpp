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

FProsperitocracyLiveStatus* UProsperitocracyStatusComponent::FindLive(const FGameplayTag& StatusTag)
{
	for (FProsperitocracyLiveStatus& Live : LiveStatuses)
	{
		if (Live.StatusTag == StatusTag)
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

float UProsperitocracyStatusComponent::GetSecondsLeft(const FProsperitocracyLiveStatus& Live, float Now) const
{
	return FMath::Max(0.0f, Live.EndTime - Now);
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

	// The status's numbers, read through their own home — the same evaluator every other number takes.
	// Spawned before the comparison because the comparison IS a question about these numbers.
	AProsperitocracyStatHostActor* NewHost = World->SpawnActor<AProsperitocracyStatHostActor>();
	if (!NewHost)
	{
		return;
	}
	NewHost->InitializeFromStatBlock(Block);

	FProsperitocracyLiveStatus Prepared;
	Prepared.StatusTag = StatusTag;
	Prepared.Block = Block;
	Prepared.Host = NewHost;
	Prepared.SourceAbilitySystemComponent = SourceAbilitySystemComponent;
	Prepared.DamageEffectClass = DamageEffectClass;
	Prepared.Hit = Hit;

	const float Duration = GetStatusStat(Prepared, EProsperitocracyStat::Duration);
	if (Duration <= 0.0f)
	{
		// A status with no Duration is not a status: nothing to carry, nothing to compare. Left alone
		// rather than applied forever, and said out loud because it is an authoring bug.
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Status] %s applied with no Duration in its block — nothing applied."), *StatusTag.ToString());
		NewHost->Destroy();
		return;
	}

	// NO STACKING, AND NO TAKING OVER EITHER (his rule, 2026-10-01). A body keeps the status it is
	// carrying until that one EXPIRES, and only then can the next one start: the burn runs its own clock
	// and deals its own ticks, and a fire that keeps setting a body alight lights it again the moment the
	// last one runs out.
	//
	// What this replaces, and why it had to go: the older rule kept whichever instance was worth more —
	// the newcomer's whole value against what was LEFT of the one on the body — and a winner RESET the
	// clock. Standing in fire re-applies every fifth of a second, so every re-application beat the burn
	// left on the body and pushed its first tick back again: a body turned on screen and took no damage
	// at all until it walked out. And a stronger thing in the room overrode every burn in it, which is
	// the balance he called out. A status that is carried is not re-decided.
	FProsperitocracyLiveStatus* Live = FindLive(StatusTag);
	if (Live)
	{
		// The instance on the body keeps its block, its numbers, its clock and its look; nothing about it
		// moves and the arriving one is thrown away.
		NewHost->Destroy();
		return;
	}

	LiveStatuses.Add(Prepared);
	Live = &LiveStatuses.Last();

	Live->EndTime = Now + Duration;
	// A ticking status's first tick comes one interval after it lands, not on the same frame: N ticks
	// over N intervals is what "X damage every X seconds for X seconds" says.
	const float Interval = DoesTick(*Live) ? GetTickInterval(*Live) : 0.0f;
	Live->NextTickTime = (Interval > 0.0f) ? (Now + Interval) : Live->EndTime;

	// The tag is the status's identity while it lasts, stamped once however many times it is applied —
	// a count written, never added to, so a hundred shots are still one status.
	if (UAbilitySystemComponent* TargetAbilitySystemComponent = GetOwnerAbilitySystemComponent())
	{
		TargetAbilitySystemComponent->SetLooseGameplayTagCount(StatusTag, 1);
	}

	// The status's own look goes on with it — once, at the moment it lands, and there is only ever one
	// landing now that a status is never taken over. This is the only place a look is ever put on a
	// body, so any body that can carry a burn wears it the same way.
	BeginStatusEffect(*Live);

	ProsperitocracyStatus::Report(StatusTag, /*OnScreenKey=*/ 0x9010,
		FString::Printf(TEXT("%s on %s — %.1fs"), *StatusTag.ToString(), *Owner->GetName(), Duration));
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

void UProsperitocracyStatusComponent::BeginStatusEffect(FProsperitocracyLiveStatus& Live)
{
	AActor* Owner = GetOwner();
	UNiagaraSystem* Effect = Owner ? ProsperitocracyStatus::FindStatusEffect(Live.StatusTag) : nullptr;

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
	Live.Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(Effect, AttachTo, TheBone,
		FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/ true, /*bAutoActivate=*/ true);

	if (!Live.Effect)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Status] %s names a look that could not be put on %s — nothing spawned."),
			*Live.StatusTag.ToString(), *GetNameSafe(Owner));
		return;
	}

	// THE FLAMES KEEP RISING, whichever way the body falls: it rides the chest for its POSITION and keeps its own
	// upright rotation, so a body on its side has fire standing out of it rather than fire lying flat on it.
	Live.Effect->SetUsingAbsoluteRotation(true);

	// A burning body an enemy can see burning is burning on every client that can see it, so the fire
	// is replicated with the body it stands on rather than drawn on one machine only.
	Live.Effect->SetIsReplicated(true);

	// AND ITS LIGHT IS OURS — one per body, on its own clock. The effect's own light is gone from the asset,
	// because a light per particle is a light that strobes as the cards spawn and die. It hangs on the SAME bone the
	// flames do: on the body's component alone it sat at the mesh's origin, which is between the feet.
	BeginTheLightOnThisBody(Live, AttachTo, TheBone, Now);

	ProsperitocracyStatus::Report(Live.StatusTag, /*OnScreenKey=*/ 0x9012,
		FString::Printf(TEXT("%s — its fire is on %s, on its chest, with one light of ours"),
			*Live.StatusTag.ToString(), *GetNameSafe(Owner)));
}

void UProsperitocracyStatusComponent::BeginTheLightOnThisBody(FProsperitocracyLiveStatus& Live, USceneComponent* AttachTo, FName OnBone, float Now)
{
	if (!AttachTo || Live.Light.TheLight)
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

	Live.Light.TheLight = TheLight;
	Live.Light.WhatItIsWorth = CandelasABurningBodyIsLitWith;
	Live.Light.WhenItWasLit = Now;
}

void UProsperitocracyStatusComponent::RetuneTheLightsOfBurningBodies(float Now)
{
	// WHILE IT BURNS: up over its own short ramp, then full worth for as long as the burn lasts. Every frame, so
	// the ramp is a ramp and not a step.
	for (FProsperitocracyLiveStatus& Live : LiveStatuses)
	{
		UPointLightComponent* TheLight = Live.Light.TheLight;
		if (!TheLight)
		{
			continue;
		}

		const float HowFarUpItIs = (HowLongABurningBodysLightTakesToComeUpSeconds <= 0.0f) ? 1.0f
			: FMath::Clamp((Now - Live.Light.WhenItWasLit) / HowLongABurningBodysLightTakesToComeUpSeconds, 0.0f, 1.0f);

		TheLight->SetIntensity(Live.Light.WhatItIsWorth * HowFarUpItIs);
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

void UProsperitocracyStatusComponent::EndStatusEffect(FProsperitocracyLiveStatus& Live)
{
	if (UNiagaraComponent* Effect = Live.Effect)
	{
		// A STOP, not a cut: the effect spawns nothing more and puts itself out — the flames already on
		// the body burn for as long as their own particles last, and the component destroys itself when
		// the last one dies. Nothing is killed here and nothing is held on to.
		Effect->Deactivate();
		Live.Effect = nullptr;
	}

	// THE LIGHT LEAVES WITH THE FLAMES, NOT WITH THE STATUS: the burn's own second is how long the last cards on
	// the body take to die, so the light is handed to the ones going out and fades across exactly that.
	if (Live.Light.TheLight)
	{
		FProsperitocracyBurningBodyLight GoingOut = Live.Light;
		GoingOut.WhenItStartedGoingOut = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		LightsGoingOut.Add(GoingOut);
		Live.Light = FProsperitocracyBurningBodyLight();
	}
}

bool UProsperitocracyStatusComponent::IsMovementStopped() const
{
	// The movement gate asks by IDENTITY, not by numbers: Stun is the status whose tag says movement
	// stops. A status being short or long is not what stops you — a slow, a bubble or a bleed can have
	// any duration and none of them take your legs.
	for (const FProsperitocracyLiveStatus& Live : LiveStatuses)
	{
		if (Live.StatusTag == ProsperitocracyGameplayTags::Status_Stun)
		{
			return true;
		}
	}

	return false;
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

	for (int32 Index = LiveStatuses.Num() - 1; Index >= 0; --Index)
	{
		FProsperitocracyLiveStatus& Live = LiveStatuses[Index];

		// Out of time: the status ends. Nothing lingers — the body keeps no memory of a burn that is
		// over, so a re-application starts from full value like any other.
		if (Now >= Live.EndTime)
		{
			EndStatus(Index);
			continue;
		}

		if (DoesTick(Live) && Now >= Live.NextTickTime)
		{
			const float Interval = GetTickInterval(Live);
			ApplyTickDamage(Live);
			// The next tick is one interval on, from the tick that just fired — a late frame shifts the
			// ticks, it never fires two at once.
			Live.NextTickTime = (Interval > 0.0f) ? (Live.NextTickTime + Interval) : Live.EndTime;
		}
	}

	RetuneTheLightsOfBurningBodies(Now);

	UpdateMovementGate();
}

void UProsperitocracyStatusComponent::EndStatus(int32 Index)
{
	if (!LiveStatuses.IsValidIndex(Index))
	{
		return;
	}

	const FGameplayTag StatusTag = LiveStatuses[Index].StatusTag;

	// The status's look goes off with it, told to stop BEFORE the entry leaves, so the effect is never
	// orphaned: from here it puts itself out and cleans itself up.
	EndStatusEffect(LiveStatuses[Index]);

	if (AProsperitocracyStatHostActor* Host = LiveStatuses[Index].Host)
	{
		Host->Destroy();
	}

	LiveStatuses.RemoveAt(Index);

	// The tag comes off only when NOTHING of that status is left on the body — the identity is per
	// status, not per instance.
	if (!FindLive(StatusTag))
	{
		if (UAbilitySystemComponent* TargetAbilitySystemComponent = GetOwnerAbilitySystemComponent())
		{
			TargetAbilitySystemComponent->SetLooseGameplayTagCount(StatusTag, 0);
		}

		ProsperitocracyStatus::Report(StatusTag, /*OnScreenKey=*/ 0x9010,
			FString::Printf(TEXT("%s ended on %s"), *StatusTag.ToString(), *GetNameSafe(GetOwner())));
	}
}

void UProsperitocracyStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A body that goes away takes its statuses and their GAS homes with it.
	for (FProsperitocracyLiveStatus& Live : LiveStatuses)
	{
		if (AProsperitocracyStatHostActor* Host = Live.Host)
		{
			Host->Destroy();
		}
	}
	LiveStatuses.Reset();

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
