// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility_IncendiaryStrike.h"

#include "Character/ProsperitocracyCharacter.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Projectiles/ProsperitocracyProjectile.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyGameplayAbility_IncendiaryStrike)

UProsperitocracyGameplayAbility_IncendiaryStrike::UProsperitocracyGameplayAbility_IncendiaryStrike()
{
	// The base's defaults are the right ones for a bombardment that is CALLED: instanced on the body that
	// called it, fired on the input, and independent of every other ability — the man goes on fighting while
	// it falls, and it refuses nobody's move.
}

bool UProsperitocracyGameplayAbility_IncendiaryStrike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		// A REFUSAL SAYS SO. Nothing in the ability system's own answer is logged by itself, and an ability
		// that silently declines is a key that "did nothing" — the exact thing this ability's own lines
		// exist to tell apart.
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Strike] %s: the ability system refused to start it — the ability-system lines just above this one carry the reason. Nothing was called."),
			*GetPathName());
		return false;
	}

	// THE COOLDOWN IS COUNTED FROM THE CALL (the plan's rule), and until it is up the number does nothing.
	// Read FINAL through the ability's own GAS home, so a Cooldown perk reaches it like any other stat, and
	// refused OUT LOUD rather than silently — a call that does nothing has to be readable.
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < ReadyAt)
	{
		UE_LOG(LogProsperitocracy, Log, TEXT("[Strike] %s: on cooldown — %.1fs of %.1fs left"),
			*GetPathName(), FMath::Max(0.0f, ReadyAt - World->GetTimeSeconds()), GetCooldownSeconds());
		return false;
	}

	return true;
}

void UProsperitocracyGameplayAbility_IncendiaryStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base class brings this ability's GAS home up: every number it owns — its Duration, Rate, Range and
	// Cooldown — is evaluated there, and nowhere else.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Caller = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = GetWorld();

	if (!Caller || !World || !StatBlock || !StrikeProjectile || !StrikeStatBlock || !StrikeDamageEffectClass)
	{
		// Said out loud with the half that is missing: an unauthored ability is a bug, not a missing feature.
		const TCHAR* WhatIsMissing =
			!Caller ? TEXT("there is nobody calling it")
			: !World ? TEXT("there is no world")
			: !StatBlock ? TEXT("the ability has no block, so it has no Duration, Rate, Range or Cooldown")
			: !StrikeProjectile ? TEXT("no strike projectile is set")
			: !StrikeStatBlock ? TEXT("the strike has no block, so its blast would carry no mark, no damage and no burn")
			: TEXT("no damage effect is set, so the blast would deal nothing");

		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Strike] %s: nothing falls — %s. This is a bug or an unauthored ability, not a missing feature."),
			*GetPathName(), WhatIsMissing);

		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	// THE CLOCK STARTS ON THE CALL, never on the last strike landing: what he waits for is the call he made.
	CalledAt = World->GetTimeSeconds();
	ReadyAt = CalledAt + GetCooldownSeconds();

	// WHERE IT IS CALLED — the point he is aiming at. Everything after this happens inside the circle of the
	// ability's own Range, sized around this one place.
	if (!FindWhereItLands(TheCircleCentre))
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Strike] %s: there was no line to call it along — nothing fell."), *GetPathName());
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	UE_LOG(LogProsperitocracy, Log, TEXT("[Strike] %s called on %s — %.1fs at %.1f/s, the circle %.1fm across, next one ready in %.0fs"),
		*GetPathName(), *TheCircleCentre.ToCompactString(), GetDurationSeconds(), GetRatePerSecond(),
		GetStatFinalValue(EProsperitocracyStat::Range), GetCooldownSeconds());

	// THE FIRST STRIKE FALLS ON THE CALL, and then one every 1/Rate until the Duration is up — one timer and
	// one clock, because the Duration is the only thing that says when a bombardment has had its run.
	TheBombardmentKeepsFalling();

	const float SecondsBetweenStrikes = (GetRatePerSecond() > 0.0f) ? (1.0f / GetRatePerSecond()) : 0.0f;
	if (SecondsBetweenStrikes > 0.0f)
	{
		World->GetTimerManager().SetTimer(FallingTimerHandle, this,
			&UProsperitocracyGameplayAbility_IncendiaryStrike::TheBombardmentKeepsFalling, SecondsBetweenStrikes, /*bLoop=*/ true);
	}
	else
	{
		// A Rate that says nothing (or nothing per second) is an authoring bug: one strike fell, and the
		// bombardment is over rather than falling forever.
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Strike] %s has no Rate — one strike fell and no more. The Rate row IS how fast they come down."), *GetPathName());
		EndTheBombardment(TEXT("no Rate to fall at"));
	}
}

void UProsperitocracyGameplayAbility_IncendiaryStrike::TheBombardmentKeepsFalling()
{
	UWorld* World = GetWorld();
	const float Duration = GetDurationSeconds();

	// THE DURATION IS THE WHOLE CLOCK: counted from the call, read FINAL, and nothing counts strikes
	// against it. Past it, the bombardment is over — and the ability goes with it, because it was only ever
	// the falling.
	if (!World || (World->GetTimeSeconds() - CalledAt) > Duration)
	{
		EndTheBombardment(World ? TEXT("its Duration ran out") : TEXT("the world went away"));
		return;
	}

	AActor* Caller = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!Caller)
	{
		EndTheBombardment(TEXT("there is nobody left to call it"));
		return;
	}

	// ONLY THE LANDING POINT HAS TO BE INSIDE THE CIRCLE (the plan's rule): every strike is aimed at its own
	// point in there, and where it comes in from is its own business. The blast it makes may spill past the
	// edge of the circle, and that is fine — the ball is never clipped.
	const float RadiusCm = GetCircleRadiusCm();
	const FVector2D InsideTheCircle = FMath::RandPointInCircle(RadiusCm);
	const FVector Landing = TheCircleCentre + FVector(InsideTheCircle.X, InsideTheCircle.Y, 0.0f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Caller;
	SpawnParams.Instigator = Cast<APawn>(Caller);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProsperitocracyProjectile* Strike = World->SpawnActor<AProsperitocracyProjectile>(
		StrikeProjectile, FTransform(FRotator::ZeroRotator, Landing), SpawnParams);
	if (!Strike)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Strike] %s: a strike could not be spawned — nothing fell for that one."), *GetPathName());
		return;
	}

	// The strike's OWN numbers, handed over where it is born: its block carries the explosion mark, so the
	// ball, the damage, the Pen and the burn it sets are all answered by that one asset — this ability never
	// touches what the bang is worth.
	Strike->InitializeFromStatBlock(StrikeStatBlock);
	Strike->LaunchAt(Landing, Caller, StrikeDamageEffectClass);

	++StrikesDropped;
}

bool UProsperitocracyGameplayAbility_IncendiaryStrike::FindWhereItLands(FVector& OutPlace) const
{
	const APawn* Caller = Cast<APawn>(CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr);
	const UWorld* World = GetWorld();
	if (!Caller || !World)
	{
		return false;
	}

	// THE SAME LINE HIS SHOT FLIES DOWN — the man's own aim, taken from his eye, never the raw camera: what
	// he is aiming at is where he is pointing his gun, and a man still coming round is pointing where he is
	// pointing. The gun's line and this one cannot disagree because they are the same read.
	const AProsperitocracyCharacter* Man = Cast<AProsperitocracyCharacter>(Caller);
	const FVector From = Caller->GetPawnViewLocation();
	const FVector Along = Man ? Man->GetAimRotation().Vector() : Caller->GetControlRotation().Vector();
	const FVector To = From + (Along * ProsperitocracyIncendiaryStrikeHandling::PlacementReachCm);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(StrikePlacement), /*bTraceComplex=*/ false, Caller);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
	{
		OutPlace = Hit.ImpactPoint;
		return true;
	}

	// NOTHING OUT THERE TO LAND ON: the call still happens, at the far end of the line he is aiming along —
	// a strike can be sent anywhere, so there is nothing here to refuse, and the strikes land in the air
	// exactly where he was pointing.
	OutPlace = To;
	return true;
}

void UProsperitocracyGameplayAbility_IncendiaryStrike::EndTheBombardment(const TCHAR* Why)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallingTimerHandle);
	}

	UE_LOG(LogProsperitocracy, Log, TEXT("[Strike] %s: the bombardment is over — %d strikes fell, because %s."),
		*GetPathName(), StrikesDropped, Why);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}

void UProsperitocracyGameplayAbility_IncendiaryStrike::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Whatever ended it — its Duration, the man dying, a cancel — the falling stops with it. Without this the
	// clock would go on dropping strikes for a bombardment that no longer exists.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallingTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

float UProsperitocracyGameplayAbility_IncendiaryStrike::GetDurationSeconds() const
{
	// FINAL through this ability's own GAS home, so a Duration perk stretches the bombardment — and the
	// floor fire's own clock is a different row entirely (the plan's three clocks: the bombardment's
	// Duration, the fire's, and each Burn's).
	return GetStatFinalValue(EProsperitocracyStat::Duration);
}

float UProsperitocracyGameplayAbility_IncendiaryStrike::GetRatePerSecond() const
{
	// FINAL, the same read every rate in the game takes: how many strikes come down a second.
	return GetStatFinalValue(EProsperitocracyStat::Rate);
}

float UProsperitocracyGameplayAbility_IncendiaryStrike::GetCircleRadiusCm() const
{
	// EVERY RANGE IN THIS FEATURE IS A DIAMETER (his rule: "in every case its the diameter not the radius"),
	// and this one is no exception: the ability's Range is how far ACROSS the circle the strikes land inside
	// is — 20 means a circle 20 across, so the strikes land within 10 of the place it was called. A circle is
	// placed and scattered inside from its radius, so that half is geometry here exactly as the ball's half is
	// in the explosion pass. Read FINAL, so a Range perk widens the bombardment and nothing else has to know.
	return GetStatFinalValue(EProsperitocracyStat::Range)
		* ProsperitocracyIncendiaryStrikeHandling::CentimetersPerMeter * 0.5f;
}

float UProsperitocracyGameplayAbility_IncendiaryStrike::GetCooldownSeconds() const
{
	// FINAL through this ability's own GAS home, so a Cooldown perk reaches the call like any other stat.
	return GetStatFinalValue(EProsperitocracyStat::Cooldown);
}
