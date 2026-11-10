// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility_Strike.h"

#include "Character/ProsperitocracyCharacter.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Projectiles/ProsperitocracyProjectile.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyGameplayAbility_Strike)

UProsperitocracyGameplayAbility_Strike::UProsperitocracyGameplayAbility_Strike()
{
	// The base's defaults are the right ones for a bombardment that is CALLED: instanced on the body that
	// called it, fired on the input, and independent of every other ability — the man goes on fighting while
	// it falls, and it refuses nobody's move.
	//
	// AND IT OUTLIVES A CUT: a stun stops what the body is DOING, and a strike is no longer the body's —
	// it is already committed to the world and keeps falling. The tag says that on the class, so every
	// strike there will ever be carries it without a single asset being remembered.
	AbilityTags.AddTag(ProsperitocracyGameplayTags::Ability_Behavior_SurvivesStun);
}

bool UProsperitocracyGameplayAbility_Strike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
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
	// One gate, on the base ability, so this refuses in the same words a dash does — and out loud.
	return !IsStillOnCooldown();
}

void UProsperitocracyGameplayAbility_Strike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base class brings this ability's GAS home up: every number it owns — its Duration, Rate, Range and
	// Cooldown — is evaluated there, and nowhere else.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Caller = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = GetWorld();

	// WHAT THIS CALL IS — all of it off its OWN block: the shell it drops and the block that shell wears.
	// Nothing here is a field of the ability's, which is why one Blueprint of this class is every strike
	// there will ever be.
	const UProsperitocracyStatTable* TheCallBlock = GetStatBlock();
	const bool bHasAShell = TheCallBlock && TheCallBlock->WhatFalls;
	const bool bHasALanding = TheCallBlock && TheCallBlock->WhatLands;

	if (!Caller || !World || !TheCallBlock || !bHasAShell || !bHasALanding)
	{
		// Said out loud with the half that is missing: an unauthored ability is a bug, not a missing feature.
		const TCHAR* WhatIsMissing =
			!Caller ? TEXT("there is nobody calling it")
			: !World ? TEXT("there is no world")
			: !TheCallBlock ? TEXT("the ability has no block, so it has no Duration, Rate, Range, Cooldown or strike")
			: !bHasAShell ? TEXT("the call's block names nothing that falls, so there is no shell")
			: TEXT("the call's block names nothing that lands, so its blast would carry no mark, no damage and no burn");

		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Strike] %s: nothing falls — %s. This is a bug or an unauthored ability, not a missing feature."),
			*GetPathName(), WhatIsMissing);

		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	// THE CLOCK STARTS ON THE CALL, never on the last strike landing: what he waits for is the call he made.
	CalledAt = World->GetTimeSeconds();
	StartTheCooldown();

	// WHERE IT IS CALLED — the point he is aiming at. Everything after this happens inside the circle of the
	// ability's own Range, sized around this one place.
	if (!FindWhereItLands(TheCircleCentre))
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Strike] %s: there was no line to call it along — nothing fell."), *GetPathName());
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	// WHAT THE CALL SAID IT WOULD DO, in the terms it actually has — a single strike has no Duration and no
	// Rate, and the log must not print zeros for a bombardment nobody asked for.
	const float Duration = GetDurationSeconds();
	const float Rate = GetRatePerSecond();
	const float CircleMeters = GetStatFinalValue(EProsperitocracyStat::Range);

	FString TheCall;
	if (Rate > 0.0f)
	{
		TheCall = FString::Printf(TEXT("%.1fs at %.1f/s"), Duration, Rate);
	}
	else if (Duration > 0.0f)
	{
		TheCall = FString::Printf(TEXT("%.1fs"), Duration);
	}
	if (CircleMeters > 0.0f)
	{
		TheCall += FString::Printf(TEXT("%sthe strikes land within %.0fm"), TheCall.IsEmpty() ? TEXT("") : TEXT(", "), CircleMeters);
	}
	if (TheCall.IsEmpty())
	{
		TheCall = TEXT("one strike");
	}

	UE_LOG(LogProsperitocracy, Log, TEXT("[Strike] %s called on %s — %s, next one ready in %.0fs"),
		*GetPathName(), *TheCircleCentre.ToCompactString(), *TheCall, GetCooldownSeconds());

	// THE FIRST STRIKE FALLS ON THE CALL, and then one every 1/Rate until the Duration is up — one timer and
	// one clock, because the Duration is the only thing that says when a bombardment has had its run.
	TheBombardmentKeepsFalling();

	if (Rate > 0.0f)
	{
		World->GetTimerManager().SetTimer(FallingTimerHandle, this,
			&UProsperitocracyGameplayAbility_Strike::TheBombardmentKeepsFalling, 1.0f / Rate, /*bLoop=*/ true);
		return;
	}

	// NO RATE IS ONLY A MISTAKE IF THERE IS A DURATION TO FILL: a block that says how long it falls and never
	// says how often is asking for strikes it cannot place, so one falls and it stops with a warning. A block
	// with NEITHER is a single strike — a bomb, not a broken bombardment — and presence is scope, so it lands
	// once, silently, and nothing is built around it.
	if (Duration > 0.0f)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Strike] %s has a Duration and no Rate — one strike fell and no more. The Rate row IS how fast they come down."), *GetPathName());
		EndTheBombardment(TEXT("a Duration with no Rate to fill it"));
		return;
	}

	EndTheBombardment(TEXT("it is a single strike — no Rate and no Duration to fall after it"));
}

void UProsperitocracyGameplayAbility_Strike::TheBombardmentKeepsFalling()
{
	UWorld* World = GetWorld();
	const float Duration = GetDurationSeconds();

	// THE DURATION IS THE WHOLE CLOCK, BUT ONLY A CALL THAT HAS ONE CAN BE PAST IT: counted from the call,
	// read FINAL, nothing counts strikes against it — and it is asked of the BLOCK, because presence is
	// scope. A bomb carries no Duration at all, so it is never measured against a zero: a threshold of zero
	// seconds is not "it is over", it is "there is no clock here".
	const UProsperitocracyStatTable* TheCallBlock = GetStatBlock();
	const bool bItHasAClock = TheCallBlock && TheCallBlock->Carries(EProsperitocracyStat::Duration);

	if (!World || (bItHasAClock && (World->GetTimeSeconds() - CalledAt) > Duration))
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

	// WHAT THIS CALL DROPS, off its own block — the shell and the block it wears. Read here rather than
	// remembered, so a block edited between two strikes cannot leave the ability holding an old answer.
	if (!TheCallBlock || !TheCallBlock->WhatFalls || !TheCallBlock->WhatLands)
	{
		EndTheBombardment(TEXT("its block no longer names what falls"));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Caller;
	SpawnParams.Instigator = Cast<APawn>(Caller);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProsperitocracyProjectile* Strike = World->SpawnActor<AProsperitocracyProjectile>(
		TheCallBlock->WhatFalls, FTransform(FRotator::ZeroRotator, Landing), SpawnParams);
	if (!Strike)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Strike] %s: a strike could not be spawned — nothing fell for that one."), *GetPathName());
		return;
	}

	// The strike's OWN numbers, handed over where it is born: its block carries the explosion mark, so the
	// ball, the damage, the Pen and the burn it sets are all answered by that one asset — this ability never
	// touches what the bang is worth.
	Strike->InitializeFromStatBlock(TheCallBlock->WhatLands);
	Strike->LaunchAt(Landing, Caller);

	++StrikesDropped;
}

bool UProsperitocracyGameplayAbility_Strike::FindWhereItLands(FVector& OutPlace) const
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
	const FVector To = From + (Along * ProsperitocracyStrikeHandling::PlacementReachCm);

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

void UProsperitocracyGameplayAbility_Strike::EndTheBombardment(const TCHAR* Why)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallingTimerHandle);
	}

	UE_LOG(LogProsperitocracy, Log, TEXT("[Strike] %s: the bombardment is over — %d %s fell, because %s."),
		*GetPathName(), StrikesDropped, StrikesDropped == 1 ? TEXT("strike") : TEXT("strikes"), Why);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}

void UProsperitocracyGameplayAbility_Strike::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Whatever ended it — its Duration, the man dying, a cancel — the falling stops with it. Without this the
	// clock would go on dropping strikes for a bombardment that no longer exists.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallingTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

float UProsperitocracyGameplayAbility_Strike::GetDurationSeconds() const
{
	// FINAL through this ability's own GAS home, so a Duration perk stretches the bombardment — and the
	// floor fire's own clock is a different row entirely (the plan's three clocks: the bombardment's
	// Duration, the fire's, and each Burn's).
	return GetStatFinalValue(EProsperitocracyStat::Duration);
}

float UProsperitocracyGameplayAbility_Strike::GetRatePerSecond() const
{
	// FINAL, the same read every rate in the game takes: how many strikes come down a second.
	return GetStatFinalValue(EProsperitocracyStat::Rate);
}

float UProsperitocracyGameplayAbility_Strike::GetCircleRadiusCm() const
{
	// RANGE IS A DISTANCE (his call, 2026-11-01): the ability's Range is how far from the place he called it the
	// strikes may land — 6 means a circle 6 m out, 12 across. A circle is placed and scattered from its radius,
	// and the row IS that radius, so nothing is halved. Read FINAL, so a Range perk widens the bombardment and
	// nothing else has to know.
	return GetStatFinalValue(EProsperitocracyStat::Range)
		* ProsperitocracyStrikeHandling::CentimetersPerMeter;
}
