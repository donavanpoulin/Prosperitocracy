// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyStatusComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyStatusComponent)

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

float UProsperitocracyStatusComponent::GetStatusValue(const FProsperitocracyLiveStatus& Live, float RemainingSeconds) const
{
	// The dominant-instance comparison needs ONE number per status, and it is the same number read two
	// ways — both of them the damage or the time still to come, never a different currency:
	//   - a status that ticks is worth the damage still to come: Rate x Piercing Damage x time left;
	//   - a status that only lasts is worth the time left.
	if (DoesTick(Live))
	{
		return GetStatusStat(Live, EProsperitocracyStat::Rate)
			* GetStatusStat(Live, EProsperitocracyStat::PiercingDamage)
			* RemainingSeconds;
	}

	return RemainingSeconds;
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

	const float NewValue = GetStatusValue(Prepared, Duration);

	// No stacking: the dominant instance wins, decided now, at the moment the new one lands. The new
	// instance is worth its FULL value (it has all of its time left); the one already there is worth
	// only what it has left.
	FProsperitocracyLiveStatus* Live = FindLive(StatusTag);
	if (Live)
	{
		const float RemainingValue = GetStatusValue(*Live, GetSecondsLeft(*Live, Now));
		if (NewValue <= RemainingValue)
		{
			// The instance already on the body is worth more than the one arriving: it stays, and the
			// newcomer is thrown away — a weaker hit does not shorten a stronger status.
			NewHost->Destroy();
			return;
		}

		// The newcomer wins: it takes the entry over, and the numbers move to its block.
		Live->Block = Block;
		Live->Host->InitializeFromStatBlock(Block);
		Live->SourceAbilitySystemComponent = SourceAbilitySystemComponent;
		Live->DamageEffectClass = DamageEffectClass;
		Live->Hit = Hit;
		NewHost->Destroy();
	}
	else
	{
		LiveStatuses.Add(Prepared);
		Live = &LiveStatuses.Last();
	}

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

	UpdateMovementGate();
}

void UProsperitocracyStatusComponent::EndStatus(int32 Index)
{
	if (!LiveStatuses.IsValidIndex(Index))
	{
		return;
	}

	const FGameplayTag StatusTag = LiveStatuses[Index].StatusTag;

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

	Super::EndPlay(EndPlayReason);
}
