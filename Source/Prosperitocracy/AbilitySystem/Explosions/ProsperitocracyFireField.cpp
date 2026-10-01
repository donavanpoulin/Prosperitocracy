// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Explosions/ProsperitocracyFireField.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/Explosions/ProsperitocracyExplosionStatics.h"
#include "AbilitySystem/Explosions/ProsperitocracyFireStatTable.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Math/RandomStream.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyFireField)

TArray<TWeakObjectPtr<AProsperitocracyFireField>> AProsperitocracyFireField::EveryFireBurning;

AProsperitocracyFireField::AProsperitocracyFireField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// A fire is a place on the ground, not an obstacle: the CIRCLE catches bodies, and this actor is
	// only the home the flames hang on. Nothing bumps into it.
	TheRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TheRoot"));
	RootComponent = TheRoot;
	SetActorEnableCollision(false);
}

void AProsperitocracyFireField::LightTheGround(UProsperitocracyFireStatTable* InFireBlock,
	AActor* TheOneWhoLeftIt_, TSubclassOf<UGameplayEffect> InDamageEffectClass)
{
	UWorld* World = GetWorld();
	if (!World || !InFireBlock)
	{
		return;
	}

	FireBlock = InFireBlock;
	TheOneWhoLeftIt = TheOneWhoLeftIt_;
	DamageEffectClass = InDamageEffectClass;

	// THE FIRE'S OWN HOME. Its rows are read FINAL through the one aggregator, exactly as a gun's or a
	// status's are, so a perk or a buff that ever reaches a fire moves its circle and its clock with it.
	MyOwnHome = World->SpawnActor<AProsperitocracyStatHostActor>();
	if (MyOwnHome)
	{
		MyOwnHome->InitializeFromStatBlock(FireBlock);
	}

	DurationSeconds = GetMyStat(EProsperitocracyStat::Duration);
	const float RangeMeters = GetMyStat(EProsperitocracyStat::Range);
	if (DurationSeconds <= 0.0f || RangeMeters <= 0.0f)
	{
		// An authoring bug, and said out loud as one: a fire with no clock or no circle is a fire that
		// burns nothing. Half of it running would be a fire nobody could account for.
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s was left with no %s in its block — nothing is burning. A fire IS its circle and its clock."),
			*GetNameSafe(FireBlock), DurationSeconds <= 0.0f ? TEXT("Duration") : TEXT("Range"));
		TheClockIsOut();
		return;
	}

	// THE CIRCLE IS THE FIRE: its Range is the WIDTH of that circle, like every other Range in this
	// feature, and a circle is built from its radius — geometry, not a second number anybody carries.
	RadiusCm = RangeMeters * 100.0f * 0.5f;
	EndsAt = World->GetTimeSeconds() + DurationSeconds;
	NextCatchesAt = World->GetTimeSeconds();

	EveryFireBurning.Add(this);

	// NOTHING IS EVER REPLACED. Every fire already burning hands over the flames standing in this one's
	// circle: the pictures stay exactly where they are, on fire, and only their RULE changes hands — they
	// are this fire's now, on this fire's clock. Where they stood comes back, because this fire must not
	// lay its own flame on ground that is already burning.
	TArray<FVector> AlreadyBurning;
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		AProsperitocracyFireField* Other = EveryFireBurning[Index].Get();
		if (!Other)
		{
			EveryFireBurning.RemoveAt(Index);
			continue;
		}
		if (Other != this)
		{
			Other->HandOverTheFlamesTo(this, GetActorLocation(), RadiusCm, AlreadyBurning);
		}
	}

	LayTheFlames(AlreadyBurning);

	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Fire] fire laid at %s — %.1fm across, %d flames burning on this ground (%d of them already alight), %.1fs on the clock, and the ground catches for all of it."),
		*GetActorLocation().ToCompactString(), RangeMeters, Flames.Num(), AlreadyBurning.Num(), DurationSeconds);
}

void AProsperitocracyFireField::LayTheFlames(const TArray<FVector>& AlreadyBurning)
{
	UWorld* World = GetWorld();
	if (!World || !FireBlock || RadiusCm <= 0.0f || bFlamesLaid)
	{
		return;
	}
	bFlamesLaid = true;

	if (FireBlock->Flames.Num() == 0)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s names no flames — the ground is burning with nothing to see. The circle, the Burn and the clock are all running."),
			*GetNameSafe(FireBlock));
		return;
	}

	// WHAT THE FIRE LIES ON. A bang happens at the surface it hit, and the flames lie on that surface,
	// flat against it — the floor today; a wall's own patch is a later thing because a flat circle is
	// what the ground is. Asked by OBJECT TYPE and never by a channel, for the same reason the ball is.
	FVector GroundPoint = GetActorLocation();
	FVector SurfaceNormal = FVector::UpVector;
	{
		// [TUNE] MINE — how far the search looks, up and down from where the bang happened.
		const float SearchUpCm = 200.0f;
		const float SearchDownCm = 2000.0f;

		FCollisionObjectQueryParams ObjectTypes;
		ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(FireOnTheGround), /*bTraceComplex=*/ false);
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActor(TheOneWhoLeftIt);

		FHitResult Ground;
		if (World->LineTraceSingleByObjectType(Ground, GetActorLocation() + FVector::UpVector * SearchUpCm,
			GetActorLocation() - FVector::UpVector * SearchDownCm, ObjectTypes, Params))
		{
			GroundPoint = Ground.ImpactPoint;
			SurfaceNormal = Ground.ImpactNormal;
		}
		else
		{
			// A bang with no ground under it leaves nothing burning, and says so: a fire laid in mid-air
			// would be a circle on nothing that still lights people up.
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("[Fire] %s had no ground under it at %s — nothing caught fire."),
				*GetNameSafe(FireBlock), *GetActorLocation().ToCompactString());
			return;
		}
	}

	const FRotator SurfaceRotation = FRotationMatrix::MakeFromZ(SurfaceNormal).Rotator();
	const FVector InTheSurfaceX = FRotationMatrix(SurfaceRotation).GetUnitAxis(EAxis::X);
	const FVector InTheSurfaceY = FRotationMatrix(SurfaceRotation).GetUnitAxis(EAxis::Y);

	// HOW THICK THE FLAMES LIE — the ONE density knob, and how the ceilings are honoured. Past them the
	// ground is covered THINNER (a wider spread) and nothing else changes: the circle, the Burn and the
	// clock are the same. A thinned field still reads as lit; a flame taken away leaves bare ground that
	// still burns you, which is the one thing he called out as looking broken.
	const int32 BurningAlready = FlamesBurningInTheWorld() - AlreadyBurning.Num();
	const int32 RoomInTheWorld = FMath::Max(0, MostFlamesInTheWholeWorld - FMath::Max(0, BurningAlready));
	const int32 AllowedHere = FMath::Max(FewestFlamesAFireMayLay,
		FMath::Min(FireBlock->MostFlamesInOneFire, RoomInTheWorld));

	const float AreaCm2 = PI * RadiusCm * RadiusCm;
	float SpacingCm = FMath::Max(FireBlock->SpacingMeters * 100.0f, 1.0f);
	SpacingCm = FMath::Max(SpacingCm, FMath::Sqrt(AreaCm2 / (float)AllowedHere));

	// AN EVEN SPREAD AND NOT A RANDOM ONE — no clumps, no bare patches, and the same arrangement of
	// flames wherever this fire lands on any machine: the stream is seeded off the fire's own place, so
	// the same ground burns the same way for everybody looking at it.
	FRandomStream Spread(GetTypeHash(FIntVector(FMath::RoundToInt(GetActorLocation().X),
		FMath::RoundToInt(GetActorLocation().Y), FMath::RoundToInt(GetActorLocation().Z))));

	// GROUND THAT IS ALREADY BURNING IS LEFT ALONE: a cell is skipped when a flame is already standing
	// there, so a new fire fills the gaps in what is there instead of doubling up on it.
	const float OccupiedCm = SpacingCm * 0.6f;
	auto GroundIsAlreadyBurning = [&AlreadyBurning, OccupiedCm](const FVector& Where)
	{
		for (const FVector& Standing : AlreadyBurning)
		{
			if (FVector::Dist2D(Where, Standing) <= OccupiedCm)
			{
				return true;
			}
		}
		return false;
	};

	const int32 Steps = FMath::CeilToInt(RadiusCm / SpacingCm);
	for (int32 AlongX = -Steps; AlongX <= Steps; ++AlongX)
	{
		for (int32 AlongY = -Steps; AlongY <= Steps; ++AlongY)
		{
			// One flame to a CELL, and the cell is what gets nudged: the spread stays even without ever
			// reading as a grid. The nudge is most of a cell, so no flame sits in the middle of one.
			const FVector2D CellCentre((AlongX + 0.5f) * SpacingCm, (AlongY + 0.5f) * SpacingCm);
			const FVector2D Nudge(Spread.FRandRange(-0.45f, 0.45f), Spread.FRandRange(-0.45f, 0.45f));
			const FVector2D Offset = CellCentre + Nudge * SpacingCm;
			if (Offset.Size() > RadiusCm)
			{
				continue;
			}

			// Down the surface's own normal by the block's sink: the fire's origin is not its middle, so
			// this is what puts the flames IN the ground instead of floating over it.
			const FVector Place = GroundPoint + InTheSurfaceX * Offset.X + InTheSurfaceY * Offset.Y
				- SurfaceNormal * FireBlock->SinkCm;

			if (GroundIsAlreadyBurning(Place))
			{
				continue;
			}

			// WHICH FLAME IT IS alternates down the block's list, so twenty-odd flames never read as
			// twenty-odd copies of one picture.
			const int32 LookIndex = Flames.Num() % FireBlock->Flames.Num();
			UNiagaraSystem* Picture = FireBlock->Flames[LookIndex].LoadSynchronous();
			if (!Picture)
			{
				continue;
			}

			// A different turn on every one, so the fires in a patch do not all face the same way.
			const FRotator Turn = SurfaceRotation + FRotator(0.0f, Spread.FRandRange(0.0f, 360.0f), 0.0f);

			UNiagaraComponent* Burning = NewObject<UNiagaraComponent>(this);
			Burning->SetAsset(Picture);
			Burning->SetupAttachment(TheRoot);
			Burning->RegisterComponent();
			Burning->SetWorldLocationAndRotation(Place, Turn);
			// The FIRE says when its flames stop, never the effect: these are the ground burning, and
			// they are told to stop when the block's clock says so.
			Burning->SetAutoDestroy(false);
			Burning->Activate(true);

			FProsperitocracyFlame& Flame = Flames.AddDefaulted_GetRef();
			Flame.Picture = Burning;
			Flame.bToldToStop = false;
		}
	}

	// EACH FLAME HAS ITS OWN MOMENT, spread across the fade and never past the fire's own end. The fade
	// is the LAST part of the Duration and inside it, so the ground catches people the whole way through
	// it — the thinning is the look, and his rule is that the look changes nothing. This covers the
	// flames that changed hands a moment ago too: from here they die on THIS fire's clock.
	const float FadeSeconds = FMath::Clamp(FireBlock->FadeSeconds, 0.0f, DurationSeconds);
	const float FadeStartsAt = EndsAt - FadeSeconds;
	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		const float ShareDownTheFade = (float)(Index + 1) / (float)(Flames.Num() + 1);
		Flames[Index].GoesOutAt = FadeStartsAt + FadeSeconds * ShareDownTheFade;
	}
}

void AProsperitocracyFireField::CatchWhateverStandsInIt()
{
	UWorld* World = GetWorld();
	if (!World || !FireBlock || !TheOneWhoLeftIt)
	{
		return;
	}

	// The Burn's ticks are dealt by whoever left the fire, so a fire that kills contests health and shows
	// a number exactly as his own shot would. His ability system gone means nothing to credit: no burn.
	UAbilitySystemComponent* Source = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TheOneWhoLeftIt);
	if (!Source)
	{
		return;
	}

	// WHO IS STANDING IN IT. The body-folding is the ball's, reused rather than written a second time:
	// an overlap answers per PART and a fire catches a BODY once, whoever it belongs to.
	TArray<FProsperitocracyExplosionTarget> Standing;
	UProsperitocracyExplosionStatics::GatherTheBall(World, GetActorLocation(), RadiusCm, this, Standing);

	const FVector Middle = GetActorLocation();
	for (const FProsperitocracyExplosionTarget& Target : Standing)
	{
		if (!Target.Actor)
		{
			continue;
		}

		// "IN IT" IS THE CIRCLE, and a circle on the ground is FLAT: a body overhead or on a ledge above
		// the fire is not standing in it. One flat distance, asked of the nearest point the ball reached.
		if (FVector::Dist2D(Target.Point, Middle) > RadiusCm)
		{
			continue;
		}

		// NO HIT, NO GATE, NO PEN QUESTION (his spec): the fire's door asks nothing about damage, because
		// a fire has none. It applies its Burn again, every beat, for as long as the body stands here —
		// and a body that walks out keeps the Burn it has, running down on its own clock.
		UProsperitocracyDamageStatics::ApplyStatusesOnContact(Target.Actor, Source, FireBlock, DamageEffectClass);
	}
}

void AProsperitocracyFireField::LetTheFlamesGoOut(float Now)
{
	for (FProsperitocracyFlame& Flame : Flames)
	{
		if (!Flame.Picture || Flame.bToldToStop)
		{
			continue;
		}

		if (Now < Flame.GoesOutAt)
		{
			continue;
		}

		// TOLD TO STOP, AND NOTHING ELSE — this is the whole of the fade, and it is the reason it looks
		// right. A flame told to stop makes no new fire and the embers, smoke and sparks it already made
		// finish their own life: a real burn-out, at the effect's own pace, with no size trick and
		// nothing re-created. When it has finished, it says so and only then does the picture go.
		Flame.bToldToStop = true;
		Flame.Picture->OnSystemFinished.AddDynamic(this, &AProsperitocracyFireField::AFlameHasBurnedOut);
		Flame.Picture->Deactivate();
	}
}

void AProsperitocracyFireField::AFlameHasBurnedOut(UNiagaraComponent* Finished)
{
	for (int32 Index = Flames.Num() - 1; Index >= 0; --Index)
	{
		if (Flames[Index].Picture == Finished)
		{
			Flames.RemoveAt(Index);
			break;
		}
	}

	if (Finished)
	{
		Finished->DestroyComponent();
	}

	// The clock is out and this was the last one burning: the ground is bare again and the fire is done.
	if (bTheClockIsOut && Flames.Num() == 0)
	{
		Destroy();
	}
}

void AProsperitocracyFireField::TheClockIsOut()
{
	if (bTheClockIsOut)
	{
		return;
	}
	bTheClockIsOut = true;

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	// NOTHING CATCHES ANYTHING FROM HERE. The circle is dead the moment the clock is; the flames that are
	// still alight are only burning down, and the ground they are on is nobody's any more.
	for (FProsperitocracyFlame& Flame : Flames)
	{
		if (Flame.Picture && !Flame.bToldToStop)
		{
			Flame.bToldToStop = true;
			Flame.Picture->OnSystemFinished.AddDynamic(this, &AProsperitocracyFireField::AFlameHasBurnedOut);
			Flame.Picture->Deactivate();
		}
	}

	// A picture that never reports an end must not leave a dead fire standing in the world forever.
	const float BeGoneBy = Now + LongestAFireMayOutliveItsClockSeconds;
	if (Flames.Num() == 0 || Now >= BeGoneBy)
	{
		Destroy();
		return;
	}

	// Otherwise it lives on the grace of the last flames finishing. Tick watches for them.
	EndsAt = FMath::Min(EndsAt, BeGoneBy);
}

void AProsperitocracyFireField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World || !FireBlock)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	// THE GROUND CATCHES FOR THE WHOLE OF ITS LIFE, the fade included, because the fade is part of the
	// Duration (his rule). Nothing about who gets lit changes at any point in this fire's clock.
	if (!bTheClockIsOut && Now >= NextCatchesAt)
	{
		NextCatchesAt = Now + CatchEverySeconds;
		CatchWhateverStandsInIt();
	}

	LetTheFlamesGoOut(Now);

	if (!bTheClockIsOut && Now >= EndsAt)
	{
		TheClockIsOut();
		return;
	}

	// The clock is out and every flame has burned out: nothing of this fire is left in the world.
	if (bTheClockIsOut && Flames.Num() == 0)
	{
		Destroy();
	}
}

float AProsperitocracyFireField::GetMyStat(EProsperitocracyStat Stat) const
{
	// Presence is scope, asked on the block: a block that does not carry the row does not have it, and a
	// "0" from the evaluator would not say that. And it is the FINAL value, through the one aggregator.
	if (!FireBlock || !MyOwnHome || !FireBlock->Carries(Stat))
	{
		return 0.0f;
	}

	return UProsperitocracyStatSystemStatics::GetStatFinal(MyOwnHome->GetProsperitocracyAbilitySystemComponent(), Stat);
}

int32 AProsperitocracyFireField::FlamesBurningInTheWorld()
{
	int32 Burning = 0;
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		const AProsperitocracyFireField* Fire = EveryFireBurning[Index].Get();
		if (!Fire)
		{
			EveryFireBurning.RemoveAt(Index);
			continue;
		}

		for (const FProsperitocracyFlame& Flame : Fire->Flames)
		{
			if (Flame.Picture)
			{
				++Burning;
			}
		}
	}

	return Burning;
}

void AProsperitocracyFireField::HandOverTheFlamesTo(AProsperitocracyFireField* TheNewerFire,
	const FVector& TheNewCircleCentre, float TheNewCircleRadiusCm, TArray<FVector>& OutWhereTheyStood)
{
	if (!TheNewerFire || TheNewerFire == this)
	{
		return;
	}

	int32 HandedOver = 0;
	for (int32 Index = Flames.Num() - 1; Index >= 0; --Index)
	{
		if (!Flames[Index].Picture)
		{
			Flames.RemoveAt(Index);
			continue;
		}

		// The spots, never the fire: only the flames standing inside the newer circle change hands.
		const FVector Here = Flames[Index].Picture->GetComponentLocation();
		if (FVector::Dist2D(Here, TheNewCircleCentre) > TheNewCircleRadiusCm)
		{
			continue;
		}

		// THE PICTURE DOES NOT MOVE AND IS NOT RE-MADE. The same component, the same particles, the same
		// spot — it is simply hung on the newer fire now, which is what makes an overlap invisible: there
		// is nothing to pop, nothing to flicker and nothing to re-place, only a rule that changed hands.
		TheNewerFire->Flames.Add(Flames[Index]);
		OutWhereTheyStood.Add(Here);
		Flames.RemoveAt(Index);
		++HandedOver;
	}

	if (HandedOver > 0)
	{
		UE_LOG(LogProsperitocracy, Log,
			TEXT("[Fire] a newer fire took this ground over — %d flames changed hands and are burning on its clock now. Nothing was replaced."),
			HandedOver);
	}
}

void AProsperitocracyFireField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The fire's home is an actor of its own, so it does not die with its owner unless it is told to.
	if (MyOwnHome)
	{
		MyOwnHome->Destroy();
		MyOwnHome = nullptr;
	}

	EveryFireBurning.Remove(this);

	Super::EndPlay(EndPlayReason);
}
