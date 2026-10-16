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
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyFireField)

TArray<TWeakObjectPtr<AProsperitocracyFireField>> AProsperitocracyFireField::EveryFireBurning;
int32 AProsperitocracyFireField::HowManyFiresHaveBeenLaid = 0;

// WHAT THE EFFECT IS TOLD, by the names its own user parameters carry — read off the effect itself on
// 2026-10-16, and the ONLY names it has besides FlameSizeMin/Max (which are the effect's own business, tuned
// by eye on the asset and never written by us):
//
//   - THE GROUND IS THE WHOLE OF IT: one entry per piece of ground the fire is burning, as an offset from the
//     fire's own origin. Each flame is offset from the component by one of these, so the flames stand on the
//     ground the fire actually walked — up a slope, up a crate's face, over its top — with nothing scattered
//     inside a circle any more. The array is a Niagara POSITION ARRAY (its class display name is "Position
//     Array"), and the effect asks it for a RANDOM element per flame, so nothing pairs up indices and nothing
//     counts: how many flames there are is the rate, and where they stand is the ground.
//   - THE RATE is how many flames a second that ground is asking for, worked out from the density row.
//
// A NAME THAT IS NOT ON THE EFFECT IS A SILENT NO-OP — the picture then runs on its own authored defaults and
// can look plausible while saying nothing about the ground (the 2026-10-01 lesson). If the flames stop
// following the fire's shape, the names here and the effect's own user parameters are the first thing to read.
namespace ProsperitocracyFireVariables
{
	static const FName TheGroundItsBurningOn(TEXT("User.TheGroundItsBurningOn"));
	static const FName TheRate(TEXT("User.FlameRate"));
}

// THE GROUND-SAMPLING'S OWN NUMBERS — all [TUNE] MINE, and none of them is a behaviour of the fire: they are
// how far a look reaches, and the three rules that decide whether two pieces of ground touch.
namespace ProsperitocracyFireHandling
{
	/** How far off a surface a probe starts, in cm, so it is clear of the ground its own piece is standing on. */
	static constexpr float ProbeStandoffCm = 40.0f;

	/** How far a probe may look for the surface it has arrived at, along the piece's own facing, in cm. */
	static constexpr float HowFarAProbeLooksCm = 400.0f;

	/** How far below the fire a fall is looked for, in cm. */
	static constexpr float HowFarBelowItLooksForGroundCm = 2000.0f;

	/** How big the ball is that finds the surface a bang landed ON, in cm — the thing the bang hit. */
	static constexpr float TheBangIsWithinCm = 40.0f;

	/**
	 * HOW FAR ONE LOOK REACHES, as a share of one step.
	 *
	 * A sample looks six ways — down for a floor, up for nothing, across for a wall's face — and the nearest
	 * thing it finds is the ground it stands on. It has to reach more than the gap between two samples or a
	 * surface can fall down a hole between them and never be found; and it must NOT reach so far that a look
	 * wanders past the surface it belongs to. A step and a half covers the worst gap a jittered lattice can
	 * leave and still belongs to the sample's own neighbourhood.
	 */
	static constexpr float HowFarALookReachesShareOfAStep = 1.5f;

	/**
	 * How close two pieces of ground have to be to TOUCH, as a share of one step.
	 *
	 * The first of the three rules a join has to pass. It is a little over a step because a step lands between
	 * the lattice's rows as often as on one.
	 */
	static constexpr float HowCloseTwoPiecesTouchShareOfAStep = 1.5f;

	/**
	 * The second rule: two surfaces may touch unless they FACE AWAY from each other.
	 *
	 * This is what a wall is, seen from the fire's side. The two faces of a wall point opposite ways, so the
	 * ground on the far side of one is never joined to the ground on the near side — and the fire does not
	 * cross it. Anything from back-to-back to side-on joins; only genuinely opposed surfaces are refused.
	 */
	static constexpr float TheFacingAgreementForAJoin = -0.2f;

	/**
	 * The third rule: the step between two pieces has to go ALONG the surface, not off it.
	 *
	 * Without this, two surfaces that face the SAME way and happen to be a step apart — a floor and the shelf
	 * above it — would join, and the fire would burn on a shelf it never climbed to. The step's direction may
	 * lean up to this share of the way off either surface's own facing; a real step along a floor, up a slope
	 * or around a corner leans off it far less than this, and a step that crosses a gap leans off it far more.
	 */
	static constexpr float HowMuchOffTheSurfaceAJoinMayGo = 0.5f;

	/** How close two samples may be before the second is the ground already found, as a share of one step. */
	static constexpr float SameGroundShareOfAStep = 0.9f;

	/** The floor on what one step costs the fire, in cm — a step along a wall is still travel and is never free. */
	static constexpr float LeastAStepCostsCm = 10.0f;

	/** How far BELOW its own ground the fire still reaches a body, in cm — a little, so a fire on a floor does
	 *  not burn somebody in the room underneath it. What it reaches ABOVE is the piece's own height. */
	static constexpr float HowFarBelowItsGroundTheFireReachesCm = 25.0f;

	/** How close two pieces have to be to be ONE body of fire, as a share of one step. */
	static constexpr float OneBodyOfFireShareOfAStep = 1.2f;

	/** [TUNE] MINE — the colour one body of fire is lit with. The pack's own light colour is HIS to place; this
	 *  is the warm the code lights with until he says otherwise. */
	static const FLinearColor TheColourAFireIsLitWith(1.0f, 0.52f, 0.20f);
}

AProsperitocracyFireField::AProsperitocracyFireField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// A fire is a place on the ground, not an obstacle: the PIECES catch bodies, and this actor is only the
	// home the fire hangs on. Nothing bumps into it.
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
	// status's are, so a perk or a buff that ever reaches a fire moves its reach and its clock with it.
	MyOwnHome = World->SpawnActor<AProsperitocracyStatHostActor>();
	if (MyOwnHome)
	{
		MyOwnHome->InitializeFromStatBlock(FireBlock);
	}

	DurationSeconds = GetMyStat(EProsperitocracyStat::Duration);
	const float RangeMeters = GetMyStat(EProsperitocracyStat::Range);
	if (DurationSeconds <= 0.0f || RangeMeters <= 0.0f)
	{
		// An authoring bug, and said out loud as one: a fire with no clock or no reach is a fire that
		// burns nothing. Half of it running would be a fire nobody could account for.
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s was left with no %s in its block — nothing is burning. A fire IS its reach and its clock."),
			*GetNameSafe(FireBlock), DurationSeconds <= 0.0f ? TEXT("Duration") : TEXT("Range"));
		TheClockIsOut();
		return;
	}

	// THE RANGE IS THE DISTANCE THE FIRE MAY TRAVEL, and he reads it as the WIDTH of the fire he sees (every
	// Range in this feature is a diameter): the walk is measured outward from where it landed, so the reach is
	// half of it. On flat ground that is exactly the radius of the fire he sees.
	HowFarItReachesCm = RangeMeters * 100.0f * 0.5f;
	EndsAt = World->GetTimeSeconds() + DurationSeconds;
	NextCatchesAt = World->GetTimeSeconds();

	EveryFireBurning.Add(this);

	// THIS FIRE'S PLACE IN THE QUEUE, so "newer" is a number and never a guess: a fire laid later owns
	// the ground it covers, and every fire here knows which fires came after it.
	LaidOrder = ++HowManyFiresHaveBeenLaid;

	// THE FIRE'S SHAPE IS ITS GROUND, and the ground is found here — once, now, on the real surfaces. After
	// this there is no tracing: the fire IS the pieces, and everything else reads them.
	FindTheGroundItIsBurning();
	if (TheGroundItsBurningOn.Num() == 0)
	{
		// A bang with no surface under it leaves nothing burning, and says so: a fire with no ground would be
		// a hitbox on nothing that still lights people up.
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s had no ground under it at %s — nothing caught fire."),
			*GetNameSafe(FireBlock), *GetActorLocation().ToCompactString());
		TheClockIsOut();
		return;
	}

	LayTheFire();

	// A NEW FIRE HAS TAKEN SOMEONE'S GROUND. Every fire burning reads its own ground back — this one too,
	// though nothing newer exists yet — so the ground that changed hands stops burning twice, and every fire
	// re-lights itself to the ground it still owns.
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		if (AProsperitocracyFireField* Fire = EveryFireBurning[Index].Get())
		{
			Fire->ReReadWhichGroundIsStillIts();
			Fire->LightTheBodiesOfFire();
			if (UWorld* TheirWorld = Fire->GetWorld())
			{
				Fire->SetTheRate(Fire->TheRateThisGroundWants(TheirWorld->GetTimeSeconds()));
			}
		}
	}

	// Where the fire got to, in numbers: how many pieces of ground it owns, how far it had to travel for the
	// furthest of them, and what it is asking for. A fire read off the log is a fire that can be argued with.
	float FurthestCm = 0.0f;
	for (const FProsperitocracyBurningGround& Piece : TheGroundItsBurningOn)
	{
		FurthestCm = FMath::Max(FurthestCm, Piece.WalkedToGetHereCm);
	}

	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Fire] fire laid at %s — %d pieces of ground, the furthest %s walked for, a reach of %.1fm, asking for %.0f flames a second (about %.0f alight), %.1fs on the clock, and the ground catches for all of it."),
		*GetActorLocation().ToCompactString(), TheGroundItsBurningOn.Num(),
		*FString::Printf(TEXT("%.1fm"), FurthestCm / 100.0f), HowFarItReachesCm / 100.0f,
		BaseFlameRate, BaseFlameRate * 0.8f, DurationSeconds);
}

// ---------------------------------------------------------------------------------------------------
// WHERE THE FIRE IS — the ground is SAMPLED, what touches is JOINED, and the reach is FLOODED over it.
//
// What it replaced, and why (his call, 2026-10-16). The fire used to WALK: one piece at a time, six probing
// directions out of each, every step guessing where the surface went next. That is fragile in a way that has
// nothing to do with fire — the missile that had just gone off was still standing on the piece it started
// from, every one of the six probes hit it, every step came back as a point 51cm above the fire, and every
// one of them was thrown away as "ground it already has". One thing touching one piece killed the whole fire,
// and it came out a dot.
//
// Sampling cannot fail that way, because NOTHING DEPENDS ON ANYTHING: every piece is found on its own by
// asking the world what is nearest to it, and only afterwards are the pieces joined and measured. A piece that
// lands somewhere silly costs that one piece.
//
// The rules, and nothing else:
//   - a piece is where a look found real ground: six short looks, nearest wins, air finds nothing;
//   - two pieces TOUCH when they are within a step, do not face away from each other (a wall's far side faces
//     the other way, which is why a fire never crosses one), and the step between them goes ALONG the surface
//     rather than off it (which is why a floor never joins the shelf above it);
//   - the reach is spent as the REAL DISTANCE TRAVELLED over those joins, so climbing costs it, and anything
//     it cannot afford is not fire.
// ---------------------------------------------------------------------------------------------------

void AProsperitocracyFireField::FindTheGroundItIsBurning()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// IT STARTS ON WHAT THE BANG HIT. A strike lands on a floor, a crate or a wall, and where it hit IS a
	// surface — so the first piece is the surface nearest the bang, whatever that surface is.
	FProsperitocracyBurningGround TheFirst;
	if (!TheSurfaceUnder(GetActorLocation(), TheFirst.Where, TheFirst.WhichWayIsUp))
	{
		return;
	}

	const float StepCm = GetPieceStepCm();
	const FVector Middle = GetActorLocation();

	// EVERY PIECE THIS FIRE COULD BE MADE OF, before anything is joined or measured: a jittered cloud filling
	// the ball the reach could cover, one look per sample for the ground nearest it.
	TArray<FProsperitocracyBurningGround> AroundIt;
	AroundIt.Add(TheFirst);

	const int32 HowManyStepsOut = FMath::Max(1, FMath::CeilToInt(HowFarItReachesCm / StepCm));
	const float TheJitter = StepCm * 0.4f;
	const float ReachSquared = FMath::Square(HowFarItReachesCm);

	int32 HowManySamples = 0;
	int32 HowManyFoundNothing = 0;
	int32 HowManyWereTheSameGround = 0;

	for (int32 Z = -HowManyStepsOut; Z <= HowManyStepsOut; ++Z)
	{
		for (int32 Y = -HowManyStepsOut; Y <= HowManyStepsOut; ++Y)
		{
			for (int32 X = -HowManyStepsOut; X <= HowManyStepsOut; ++X)
			{
				const FVector Sample = Middle + FVector(
					(X * StepCm) + FMath::FRandRange(-TheJitter, TheJitter),
					(Y * StepCm) + FMath::FRandRange(-TheJitter, TheJitter),
					(Z * StepCm) + FMath::FRandRange(-TheJitter, TheJitter));

				// Outside the ball the fire could ever cover: not worth a look. Travel is never shorter than the
				// straight line, so this skips nothing the flood could have kept.
				if (FVector::DistSquared(Sample, Middle) > ReachSquared)
				{
					continue;
				}
				++HowManySamples;

				FProsperitocracyBurningGround Found;
				if (!TheSurfaceNearestTheSample(Sample, Found))
				{
					++HowManyFoundNothing;
					continue;
				}

				if (IsThatGroundAlreadyBurning(Found.Where, AroundIt))
				{
					++HowManyWereTheSameGround;
					continue;
				}

				AroundIt.Add(Found);
			}
		}
	}

	// JOIN THEM, AND MEASURE THE REACH OVER THE JOINS: how far it had to travel to stand on each piece, and
	// therefore which pieces are fire. Every sample is a piece here, including the ones the fire cannot afford.
	TArray<float> WalkedToGetThere;
	SettleTheGroundItCanReach(AroundIt, WalkedToGetThere);

	int32 HowManyOutOfReach = 0;
	for (int32 Index = 0; Index < AroundIt.Num(); ++Index)
	{
		if (WalkedToGetThere[Index] > HowFarItReachesCm)
		{
			++HowManyOutOfReach;
			continue;
		}

		FProsperitocracyBurningGround Piece = AroundIt[Index];
		Piece.WalkedToGetHereCm = WalkedToGetThere[Index];
		AddTheGroundPiece(Piece);
	}

	// How the shape came out, in numbers: what it looked at, what it found, and where the reach stopped it.
	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Fire] the ground is found: %d samples looked at, %d found no ground, %d were ground already found, %d pieces of ground stand inside the %.0fm reach, %d were past it."),
		HowManySamples, HowManyFoundNothing, HowManyWereTheSameGround, TheGroundItsBurningOn.Num(),
		HowFarItReachesCm / 100.0f, HowManyOutOfReach);
}

bool AProsperitocracyFireField::TheSurfaceNearestTheSample(const FVector& Sample, FProsperitocracyBurningGround& OutPiece) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// WHAT IS NEAREST TO THIS SAMPLE, in six short looks — down for a floor, up for nothing, across for a wall's
	// face — and the nearest hit is the ground it stands on. A sample out in the open finds nothing and dies:
	// that is what "air" means here, and it is why no fire ever appears where there is no surface.
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FireOnTheGround), /*bTraceComplex=*/ false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(TheOneWhoLeftIt);

	const float HowFarALookReachesCm = GetPieceStepCm() * ProsperitocracyFireHandling::HowFarALookReachesShareOfAStep;

	static const FVector TheSixWays[6] =
	{
		FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f),
		FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f),
		FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, 0.0f, -1.0f)
	};

	float NearestCm = TNumericLimits<float>::Max();
	FHitResult TheOneItFound;
	bool bFoundSomething = false;

	for (const FVector& Way : TheSixWays)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, Sample, Sample + Way * HowFarALookReachesCm, ObjectTypes, Params))
		{
			continue;
		}

		// A look that started already inside something found no surface — it found the inside of a wall. And
		// what a fire may stand on is unchanged: anything but the underside of something.
		if (Hit.bStartPenetrating || !TheFireMayStandOn(Hit.ImpactNormal) || Hit.Distance >= NearestCm)
		{
			continue;
		}

		NearestCm = Hit.Distance;
		TheOneItFound = Hit;
		bFoundSomething = true;
	}

	if (!bFoundSomething)
	{
		return false;
	}

	OutPiece.Where = TheOneItFound.ImpactPoint;
	OutPiece.WhichWayIsUp = TheOneItFound.ImpactNormal;
	OutPiece.WalkedToGetHereCm = 0.0f;
	OutPiece.bItIsStillThisFires = true;
	return true;
}

void AProsperitocracyFireField::SettleTheGroundItCanReach(const TArray<FProsperitocracyBurningGround>& Pieces, TArray<float>& OutWalked) const
{
	// HOW FAR THE FIRE HAD TO TRAVEL TO STAND ON EACH PIECE: the shortest way over the pieces that touch,
	// starting from the one the bang hit, every join costing the real distance between the two pieces. This is
	// the whole of "the reach is travel" — a climb is a longer join, so it spends more, and the pieces it
	// cannot afford are simply further away than the reach.
	const int32 HowMany = Pieces.Num();
	OutWalked.Init(TNumericLimits<float>::Max(), HowMany);
	if (HowMany == 0)
	{
		return;
	}
	OutWalked[0] = 0.0f;

	// The pieces hashed by where they are, one cell a step wide, so each piece only ever looks at the pieces
	// around it instead of at all of them.
	const float StepCm = GetPieceStepCm();
	TMap<FIntVector, TArray<int32>> WhereThePiecesAre;
	for (int32 Index = 0; Index < HowMany; ++Index)
	{
		const FVector& Where = Pieces[Index].Where;
		const FIntVector Cell = FIntVector(FMath::FloorToInt(Where.X / StepCm), FMath::FloorToInt(Where.Y / StepCm),
			FMath::FloorToInt(Where.Z / StepCm));
		WhereThePiecesAre.FindOrAdd(Cell).Add(Index);
	}

	const float TouchingCm = StepCm * ProsperitocracyFireHandling::HowCloseTwoPiecesTouchShareOfAStep;
	TArray<bool> bSettled;
	bSettled.Init(false, HowMany);

	for (;;)
	{
		// The nearest piece not yet settled. At this size the plain shortest-path walk is the honest one: no
		// queue machinery to get subtly wrong. Unreachable pieces stay at "further than anything" and end it.
		int32 TheNextPiece = INDEX_NONE;
		float TheShortestSoFar = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < HowMany; ++Index)
		{
			if (!bSettled[Index] && OutWalked[Index] < TheShortestSoFar)
			{
				TheShortestSoFar = OutWalked[Index];
				TheNextPiece = Index;
			}
		}

		if (TheNextPiece == INDEX_NONE)
		{
			break;
		}
		bSettled[TheNextPiece] = true;

		// EVERY PIECE THIS ONE TOUCHES, out of its own neighbourhood, through the three rules.
		const FProsperitocracyBurningGround& From = Pieces[TheNextPiece];
		const FIntVector Cell = FIntVector(FMath::FloorToInt(From.Where.X / StepCm), FMath::FloorToInt(From.Where.Y / StepCm),
			FMath::FloorToInt(From.Where.Z / StepCm));

		for (int32 Z = -1; Z <= 1; ++Z)
		{
			for (int32 Y = -1; Y <= 1; ++Y)
			{
				for (int32 X = -1; X <= 1; ++X)
				{
					const TArray<int32>* TheOnesNearby = WhereThePiecesAre.Find(Cell + FIntVector(X, Y, Z));
					if (!TheOnesNearby)
					{
						continue;
					}

					for (const int32 Other : *TheOnesNearby)
					{
						if (bSettled[Other])
						{
							continue;
						}

						const FProsperitocracyBurningGround& To = Pieces[Other];
						const FVector TheStep = To.Where - From.Where;
						const float TheStepCm = TheStep.Size();

						// RULE ONE: close enough to be one step.
						if (TheStepCm <= KINDA_SMALL_NUMBER || TheStepCm > TouchingCm)
						{
							continue;
						}

						const FVector StepDirection = TheStep / TheStepCm;

						// RULE TWO: surfaces that face away from each other are not touching — this is a wall.
						if (FVector::DotProduct(From.WhichWayIsUp, To.WhichWayIsUp)
							< ProsperitocracyFireHandling::TheFacingAgreementForAJoin)
						{
							continue;
						}

						// RULE THREE: the step has to go ALONG the surface, not off it — a shelf over a floor is
						// the case this one is for.
						if (FMath::Abs(FVector::DotProduct(StepDirection, From.WhichWayIsUp)) > ProsperitocracyFireHandling::HowMuchOffTheSurfaceAJoinMayGo
							|| FMath::Abs(FVector::DotProduct(StepDirection, To.WhichWayIsUp)) > ProsperitocracyFireHandling::HowMuchOffTheSurfaceAJoinMayGo)
						{
							continue;
						}

						const float WalkedToIt = OutWalked[TheNextPiece] + FMath::Max(TheStepCm, ProsperitocracyFireHandling::LeastAStepCostsCm);
						if (WalkedToIt < OutWalked[Other])
						{
							OutWalked[Other] = WalkedToIt;
						}
					}
				}
			}
		}
	}
}

bool AProsperitocracyFireField::TheSurfaceUnder(const FVector& Place, FVector& OutWhere, FVector& OutWhichWayIsUp) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FireOnTheGround), /*bTraceComplex=*/ false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(TheOneWhoLeftIt);

	// WHAT THE BANG HIT IS WHERE THE FIRE STARTS: the surface nearest the bang, whatever kind of surface it is —
	// a floor, a crate's top, a wall's face. Asked as a small ball around the bang, because the bang happened ON
	// something and the fire belongs on that thing rather than on the floor two metres under it.
	{
		TArray<FOverlapResult> AroundTheBang;
		World->OverlapMultiByObjectType(AroundTheBang, Place, FQuat::Identity, ObjectTypes,
			FCollisionShape::MakeSphere(ProsperitocracyFireHandling::TheBangIsWithinCm), Params);

		float NearestCm = TNumericLimits<float>::Max();
		bool bFoundOne = false;
		for (const FOverlapResult& Overlap : AroundTheBang)
		{
			const UPrimitiveComponent* Part = Overlap.GetComponent();
			if (!Part)
			{
				continue;
			}

			FVector PointOnIt = Place;
			const float DistanceCm = Part->GetClosestPointOnCollision(Place, PointOnIt);
			if (DistanceCm < 0.0f || DistanceCm >= NearestCm)
			{
				continue;
			}

			// The way that surface faces at the point the bang caught it — read off the surface itself, so a
			// fire that starts on a wall starts on the wall and not on the floor beside it.
			FVector WhichWayItFaces = (Place - PointOnIt).GetSafeNormal();
			if (WhichWayItFaces.IsNearlyZero())
			{
				WhichWayItFaces = FVector::UpVector;
			}

			NearestCm = DistanceCm;
			OutWhere = PointOnIt;
			OutWhichWayIsUp = WhichWayItFaces;
			bFoundOne = true;
		}

		if (bFoundOne && TheFireMayStandOn(OutWhichWayIsUp))
		{
			return true;
		}
	}

	// NOTHING AROUND IT — a strike that went off in the air: the fire looks for ground below, and if there is
	// none anywhere it is a fire with no ground and nothing burns.
	{
		FHitResult TheGround;
		if (World->LineTraceSingleByObjectType(TheGround, Place + FVector::UpVector * ProsperitocracyFireHandling::ProbeStandoffCm,
				Place - FVector::UpVector * ProsperitocracyFireHandling::HowFarBelowItLooksForGroundCm, ObjectTypes, Params)
			&& TheFireMayStandOn(TheGround.ImpactNormal))
		{
			OutWhere = TheGround.ImpactPoint;
			OutWhichWayIsUp = TheGround.ImpactNormal;
			return true;
		}
	}

	return false;
}

bool AProsperitocracyFireField::TheFireMayStandOn(const FVector& WhichWayIsUp)
{
	// HEAT GOES UP. A fire burns on a floor, a slope, a wall's face and a crate's top — everything the world is
	// built out of — and never on the underside of something, because fire hanging under a roof would be fire
	// that had gone through it. This one rule is what keeps the walk from wrapping under a lip it has climbed.
	return WhichWayIsUp.Z > -0.35f;
}

bool AProsperitocracyFireField::SomethingSolidBetween(const FVector& From, const FVector& To) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// The WORLD is what gets in the way of a fire: a wall, a container, a floor between one room and the next.
	// A body is not cover from flames in this game — a man standing in front of you does not put you out — so
	// this asks for the world's own thing and never for a channel somebody's capsule answers.
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FireReachesTheBody), /*bTraceComplex=*/ false, this);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(TheOneWhoLeftIt);

	FHitResult InTheWay;
	return World->LineTraceSingleByObjectType(InTheWay, From, To, ObjectTypes, Params);
}

bool AProsperitocracyFireField::IsThatGroundAlreadyBurning(const FVector& Where, const TArray<FProsperitocracyBurningGround>& AlreadyFound) const
{
	// NEVER THE SAME GROUND TWICE inside one fire's own sampling — a piece found twice would burn twice and be
	// counted twice. Asked of the pieces this fire has just found, before any of them is measured, so nothing
	// here depends on the order they were found in.
	const float TooCloseToBeNewCm = GetPieceStepCm() * ProsperitocracyFireHandling::SameGroundShareOfAStep;
	for (const FProsperitocracyBurningGround& Piece : AlreadyFound)
	{
		if (FVector::DistSquared(Piece.Where, Where) <= FMath::Square(TooCloseToBeNewCm))
		{
			return true;
		}
	}

	return false;
}

void AProsperitocracyFireField::AddTheGroundPiece(const FProsperitocracyBurningGround& Piece)
{
	// ONE DOOR for a piece of ground joining the fire, so the array is the only place the fire's shape lives.
	TheGroundItsBurningOn.Add(Piece);
}

// ---------------------------------------------------------------------------------------------------
// THE LOOK — one effect, told the ground its flames stand on.
// ---------------------------------------------------------------------------------------------------

void AProsperitocracyFireField::LayTheFire()
{
	UWorld* World = GetWorld();
	if (!World || !FireBlock || bTheFireIsLaid || TheGroundItsBurningOn.Num() == 0)
	{
		return;
	}
	bTheFireIsLaid = true;

	// WHAT A FIRE LOOKS LIKE IS ITS OWN DATA, asked of the PATH and never of the loaded object — a
	// question a soft pointer answers "is it loaded", which is not the same question (2026-09-30, and it
	// cost a whole play test). A fire that names no effect says so out loud; the ground, the Burn and the
	// clock run regardless, because a fire that hurts you and shows nothing is still a fire.
	UNiagaraSystem* TheLook = nullptr;
	if (!FireBlock->FireLook.IsNull())
	{
		TheLook = FireBlock->FireLook.LoadSynchronous();
		if (!TheLook)
		{
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("[Fire] %s names %s as its fire and it could not be loaded — the ground burns with nothing to see."),
				*GetNameSafe(FireBlock), *FireBlock->FireLook.ToString());
		}
	}
	else
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s names no fire effect — the ground is burning with nothing to see. The ground, the Burn and the clock are all running."),
			*GetNameSafe(FireBlock));
	}

	// HOW MUCH FIRE THIS GROUND WANTS — the whole of it, until the burning down begins. The block's own
	// density does the sizing: flames a second per square metre of the ground the fire is actually burning, so
	// a wider fire asks for more fire off one number and nothing anywhere authors "how many". Two ceilings then
	// hold it — the fire's own, and what the world is already burning — and past either the ground is covered
	// THINNER, because the look is what gives under load and never the fire.
	{
		const float TheGroundItIsBurning = SquareMetresItIsBurning();
		const float Wanted = FireBlock->FlamesASquareMetreASecond * TheGroundItIsBurning;
		const float WidthOfTheBlock = (float)FMath::Max(1, FireBlock->MostFlamesASecond);

		// What every OTHER fire is already asking for, and what is left of the world's ceiling after it.
		const float OthersAreBurning = FMath::Max(0.0f, FlamesASecondBurningInTheWorld() - BaseFlameRate);
		const float RoomInTheWorld = FMath::Max(0.0f, MostFlamesASecondInTheWholeWorld - OthersAreBurning);

		BaseFlameRate = FMath::Clamp(Wanted, 0.0f, FMath::Min(WidthOfTheBlock, RoomInTheWorld));
		if (BaseFlameRate < Wanted)
		{
			BaseFlameRate = FMath::Max(BaseFlameRate, FewestFlamesASecondAFireMayAsk);
			UE_LOG(LogProsperitocracy, Log,
				TEXT("[Fire] this ground asked for %.0f flames a second and gets %.0f — the fire is laid thinner, and nothing else about it changes."),
				Wanted, BaseFlameRate);
		}
	}

	// THE FADE IS THE LAST PART OF THE DURATION AND INSIDE IT (his rule): the fire starts thinning itself
	// here and is asking for nothing by the time its clock runs out.
	const float FadeSeconds = FMath::Clamp(FireBlock->FadeSeconds, 0.0f, DurationSeconds);
	FadeStartsAt = EndsAt - FadeSeconds;

	if (!TheLook)
	{
		return;
	}

	// ONE PICTURE FOR THE WHOLE FIRE, standing at the fire's own origin — and nothing is turned to one normal
	// any more: the effect is told every piece of ground and which way each piece faces, so its flames stand up
	// off a floor, out of a wall and down a slope by the same one rule (his call, 2026-10-15).
	TheFire = NewObject<UNiagaraComponent>(this);
	TheFire->SetAsset(TheLook);
	TheFire->SetupAttachment(TheRoot);
	TheFire->RegisterComponent();
	TheFire->SetWorldLocationAndRotation(GetActorLocation(), FRotator::ZeroRotator);

	// The FIRE says when it stops, never the effect: this is a fire on the ground, and it burns down when
	// the block's clock says so.
	TheFire->SetAutoDestroy(false);

	// THE GROUND AND THE COUNT, handed over before the first particle is spawned.
	HandTheEffectItsGround();

	TheFire->Activate(true);
}

void AProsperitocracyFireField::HandTheEffectItsGround()
{
	if (!TheFire)
	{
		return;
	}

	// ONLY THE GROUND IT STILL OWNS: a corner a newer fire has taken is not this fire's to burn any more, so
	// the picture it is given never shows fire on ground that is somebody else's.
	//
	// AND AS OFFSETS FROM THE FIRE'S OWN ORIGIN, because that is what the effect does with them: each flame is
	// offset from the component, which stands at the fire's origin, so a piece's world place has to be handed
	// over as the distance from there. The SINK is taken off here too, along EACH PIECE'S OWN FACING — his 50 cm
	// of sitting the flames into what they are burning, done per piece, so a flame on a wall's face is pushed
	// into the wall and a flame on a floor is pushed into the floor rather than both being pushed downwards.
	TArray<FVector> TheGround;
	TheGround.Reserve(TheGroundItsBurningOn.Num());

	// His number, off the block, read once: how deep in the surface a flame sits.
	const float HowDeepTheFlameSitsCm = FireBlock ? FireBlock->SinkCm : 0.0f;

	for (const FProsperitocracyBurningGround& Piece : TheGroundItsBurningOn)
	{
		if (!Piece.bItIsStillThisFires)
		{
			continue;
		}

		TheGround.Add(Piece.Where - Piece.WhichWayIsUp * HowDeepTheFlameSitsCm - GetActorLocation());
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(TheFire,
		ProsperitocracyFireVariables::TheGroundItsBurningOn, TheGround);

	SetTheRate(BaseFlameRate * HowMuchOfItsGroundItStillOwns());
}

// ---------------------------------------------------------------------------------------------------
// WHO IS IN IT — the fire's own pieces, and nothing solid in between.
// ---------------------------------------------------------------------------------------------------

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

	// WHO COULD BE IN IT, in one ask, over the whole spread of the fire. The body-folding is the ball's, reused
	// rather than written a second time: an overlap answers per PART and a fire catches a BODY once.
	float FurthestPieceCm = 0.0f;
	for (const FProsperitocracyBurningGround& Piece : TheGroundItsBurningOn)
	{
		if (Piece.bItIsStillThisFires)
		{
			FurthestPieceCm = FMath::Max(FurthestPieceCm, FVector::Dist2D(Piece.Where, GetActorLocation()));
		}
	}

	TArray<FProsperitocracyExplosionTarget> Standing;
	UProsperitocracyExplosionStatics::GatherTheBall(World, GetActorLocation(), FurthestPieceCm + GetPieceWidthCm(), this, Standing);

	const float WidthCm = GetPieceWidthCm();
	const float HeightCm = GetPieceHeightCm();

	for (const FProsperitocracyExplosionTarget& Target : Standing)
	{
		if (!Target.Actor)
		{
			continue;
		}

		// "IN IT" IS STANDING IN ONE OF THE FIRE'S PIECES — the fire where the fire actually is, never a circle
		// drawn around where it landed and never the size of a flame. Every piece carries its own facing, so a
		// piece on a wall's face burns what stands against that wall exactly as a piece on a floor burns what
		// stands on the floor; and a body above the flames or under a floor's fire is not in it.
		const FProsperitocracyBurningGround* ThePieceItIsIn = nullptr;
		for (const FProsperitocracyBurningGround& Piece : TheGroundItsBurningOn)
		{
			if (!Piece.bItIsStillThisFires)
			{
				continue;
			}

			const FVector Delta = Target.Point - Piece.Where;
			const float AlongTheFacing = FVector::DotProduct(Delta, Piece.WhichWayIsUp);
			const float AcrossTheFacing = (Delta - Piece.WhichWayIsUp * AlongTheFacing).Size();
			if (AcrossTheFacing <= WidthCm
				&& AlongTheFacing >= -ProsperitocracyFireHandling::HowFarBelowItsGroundTheFireReachesCm
				&& AlongTheFacing <= HeightCm)
			{
				ThePieceItIsIn = &Piece;
				break;
			}
		}

		if (!ThePieceItIsIn)
		{
			continue;
		}

		// NOTHING SOLID IN THE WAY (his rule): a wall or a floor between the fire and a body is why a body on
		// the far side of cover is not burnt. The fire's pieces only exist where the fire could walk to, so this
		// is the second half of the same rule — the first half is that there is no fire over there at all.
		if (SomethingSolidBetween(ThePieceItIsIn->Where, Target.Point))
		{
			continue;
		}

		// NO HIT, NO GATE, NO PEN QUESTION (his spec): the fire's door asks nothing about damage, because
		// a fire has none. It applies its Burn again, every beat, for as long as the body stands here —
		// and a body that walks out keeps the Burn it has, running down on its own clock.
		UProsperitocracyDamageStatics::ApplyStatusesOnContact(Target.Actor, Source, FireBlock, DamageEffectClass);
	}
}

// ---------------------------------------------------------------------------------------------------
// ONE LIGHT PER BODY OF FIRE — his call, 2026-10-15.
// ---------------------------------------------------------------------------------------------------

void AProsperitocracyFireField::GroupThePiecesIntoBodiesOfFire(TArray<int32>& OutWhichBodyEachPieceIsIn, int32& OutHowManyBodies) const
{
	const int32 HowManyPieces = TheGroundItsBurningOn.Num();
	OutWhichBodyEachPieceIsIn.Init(-1, HowManyPieces);
	OutHowManyBodies = 0;

	// A piece a newer fire took is in nobody's body of fire: it is not this fire's ground to light any more.
	for (int32 Index = 0; Index < HowManyPieces; ++Index)
	{
		if (!TheGroundItsBurningOn[Index].bItIsStillThisFires)
		{
			OutWhichBodyEachPieceIsIn[Index] = -2;
		}
	}

	const float TouchingCm = GetPieceStepCm() * ProsperitocracyFireHandling::OneBodyOfFireShareOfAStep;

	// PIECES THAT TOUCH ARE ONE BODY OF FIRE — his rule, and the whole of the grouping: a flood outward from
	// each piece that has no body yet, taking everything within a step of something already in that body.
	TArray<int32> TheBody;
	for (int32 Start = 0; Start < HowManyPieces; ++Start)
	{
		if (OutWhichBodyEachPieceIsIn[Start] != -1)
		{
			continue;
		}

		const int32 Body = OutHowManyBodies++;
		OutWhichBodyEachPieceIsIn[Start] = Body;
		TheBody.Reset();
		TheBody.Add(Start);

		for (int32 Head = 0; Head < TheBody.Num(); ++Head)
		{
			const FVector& WhereItIs = TheGroundItsBurningOn[TheBody[Head]].Where;
			for (int32 Other = 0; Other < HowManyPieces; ++Other)
			{
				if (OutWhichBodyEachPieceIsIn[Other] != -1)
				{
					continue;
				}

				if (FVector::Dist(TheGroundItsBurningOn[Other].Where, WhereItIs) <= TouchingCm)
				{
					OutWhichBodyEachPieceIsIn[Other] = Body;
					TheBody.Add(Other);
				}
			}
		}
	}
}

void AProsperitocracyFireField::LightTheBodiesOfFire()
{
	// THE FIRE'S OWN LIGHT IS OURS NOW — one per body of fire, so a body of fire can have exactly one. The
	// lights the last shape of this fire needed are taken down first: the ground changed, and a light standing
	// over ground the fire no longer owns is a light in the wrong place.
	for (const TObjectPtr<UPointLightComponent>& Light : TheBodiesOfFire)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	TheBodiesOfFire.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<int32> WhichBodyEachPieceIsIn;
	int32 HowManyBodies = 0;
	GroupThePiecesIntoBodiesOfFire(WhichBodyEachPieceIsIn, HowManyBodies);
	if (HowManyBodies == 0)
	{
		return;
	}

	// WHAT EACH BODY OF FIRE IS: its own middle, how wide it actually reaches, and how much ground it covers.
	TArray<FVector> BodyCentre;
	TArray<FVector> BodyFacing;
	TArray<int32> BodyPieces;
	BodyCentre.Init(FVector::ZeroVector, HowManyBodies);
	BodyFacing.Init(FVector::ZeroVector, HowManyBodies);
	BodyPieces.Init(0, HowManyBodies);

	for (int32 Index = 0; Index < TheGroundItsBurningOn.Num(); ++Index)
	{
		const int32 Body = WhichBodyEachPieceIsIn[Index];
		if (Body < 0)
		{
			continue;
		}

		BodyCentre[Body] += TheGroundItsBurningOn[Index].Where;
		BodyFacing[Body] += TheGroundItsBurningOn[Index].WhichWayIsUp;
		++BodyPieces[Body];
	}

	TArray<float> BodyWidthCm;
	TArray<float> BodyGroundM2;
	BodyWidthCm.Init(0.0f, HowManyBodies);
	BodyGroundM2.Init(0.0f, HowManyBodies);

	for (int32 Body = 0; Body < HowManyBodies; ++Body)
	{
		BodyCentre[Body] /= (float)FMath::Max(1, BodyPieces[Body]);
		BodyFacing[Body] = BodyFacing[Body].GetSafeNormal();
		if (BodyFacing[Body].IsNearlyZero())
		{
			BodyFacing[Body] = FVector::UpVector;
		}
	}

	const float PieceWidthM = GetPieceWidthCm() / 100.0f;
	for (int32 Index = 0; Index < TheGroundItsBurningOn.Num(); ++Index)
	{
		const int32 Body = WhichBodyEachPieceIsIn[Index];
		if (Body < 0)
		{
			continue;
		}

		BodyWidthCm[Body] = FMath::Max(BodyWidthCm[Body], FVector::Dist2D(TheGroundItsBurningOn[Index].Where, BodyCentre[Body]));
		BodyGroundM2[Body] += PI * PieceWidthM * PieceWidthM;
	}

	// THE COUNT IS HELD DOWN ON PURPOSE: past the ceiling, the smallest body of fire is folded into the nearest
	// one — one light per body of fire is his rule, and this is the ceiling on the lights a shattered fire may
	// cost the world. Nothing about what burns changes: this is the light, and only the light.
	TArray<bool> bBodyIsStillItsOwn;
	bBodyIsStillItsOwn.Init(true, HowManyBodies);
	while (HowManyBodies > MostBodiesOfFireOneLightEach)
	{
		int32 Smallest = INDEX_NONE;
		for (int32 Body = 0; Body < HowManyBodies; ++Body)
		{
			if (bBodyIsStillItsOwn[Body] && (Smallest == INDEX_NONE || BodyPieces[Body] < BodyPieces[Smallest]))
			{
				Smallest = Body;
			}
		}
		if (Smallest == INDEX_NONE)
		{
			break;
		}

		int32 Nearest = INDEX_NONE;
		float NearestCm = TNumericLimits<float>::Max();
		for (int32 Body = 0; Body < HowManyBodies; ++Body)
		{
			if (!bBodyIsStillItsOwn[Body] || Body == Smallest)
			{
				continue;
			}

			const float DistanceCm = FVector::Dist(BodyCentre[Body], BodyCentre[Smallest]);
			if (DistanceCm < NearestCm)
			{
				NearestCm = DistanceCm;
				Nearest = Body;
			}
		}
		if (Nearest == INDEX_NONE)
		{
			break;
		}

		bBodyIsStillItsOwn[Smallest] = false;
		BodyPieces[Nearest] += BodyPieces[Smallest];
		BodyGroundM2[Nearest] += BodyGroundM2[Smallest];
		BodyWidthCm[Nearest] = FMath::Max(BodyWidthCm[Nearest],
			FVector::Dist(BodyCentre[Nearest], BodyCentre[Smallest]) + BodyWidthCm[Smallest]);
		--HowManyBodies;
	}

	// ONE LIGHT, STANDING ON THE FIRE WHERE THE FIRE IS: at the middle of that body of fire, off the surface
	// along the way that body of ground faces, reaching as wide as that body actually reaches, and as bright as
	// the ground it covers is worth. A ragged fire lights raggedly — that is the point of it (his words).
	for (int32 Body = 0; Body < BodyPieces.Num(); ++Body)
	{
		if (!bBodyIsStillItsOwn[Body])
		{
			continue;
		}

		// ONE LIGHT, STANDING ON THE FIRE WHERE THE FIRE IS: at the middle of that body of fire, off the surface
		// along the way that body of ground faces, reaching as wide as that body actually reaches, and as bright
		// as the ground it covers is worth. A ragged fire lights raggedly — that is the point of it (his words).
		// The five numbers are this class's own (see the header), which is why they are named bare here.
		const FVector WhereTheLightStands = BodyCentre[Body]
			+ BodyFacing[Body] * HowFarTheLightStandsOffTheFireCm;

		UPointLightComponent* TheLight = NewObject<UPointLightComponent>(this);
		TheLight->SetMobility(EComponentMobility::Movable);
		TheLight->SetupAttachment(TheRoot);
		TheLight->RegisterComponent();
		TheLight->SetWorldLocation(WhereTheLightStands);
		TheLight->SetLightColor(ProsperitocracyFireHandling::TheColourAFireIsLitWith);
		TheLight->SetAttenuationRadius(FMath::Max(TheLeastALightOfFireReachesCm,
			BodyWidthCm[Body] * HowWideTheLightOfABodyOfFireReaches));
		TheLight->SetIntensity(FMath::Clamp(BodyGroundM2[Body] * CandelasASquareMetreOfFireIsWorth,
			DimmestOneBodyOfFireIsLit, BrightestOneBodyOfFireIsLit));

		// A fire does not cast shadows: it is the light of the ground burning, and the world's shadows are the
		// sun's. This is also the cheapest light there is, which is what his "minimal" asked for.
		TheLight->SetCastShadows(false);

		TheBodiesOfFire.Add(TheLight);
	}
}

// ---------------------------------------------------------------------------------------------------
// WHOSE GROUND IT STILL IS — the newest fire owns what it covers.
// ---------------------------------------------------------------------------------------------------

void AProsperitocracyFireField::ReReadWhichGroundIsStillIts()
{
	// A NEWER FIRE OWNS THE GROUND IT COVERS, and only that ground (his rule, 2026-10-01). A piece of mine
	// within a piece of a NEWER fire's ground is taken; everything else is mine, untouched. Nothing is ever
	// replaced, re-made or moved, and a fire that lost ground asks for less fire rather than shrinking.
	const float TakenCm = GetPieceWidthCm();

	for (FProsperitocracyBurningGround& Piece : TheGroundItsBurningOn)
	{
		bool bSomebodyNewerIsOnIt = false;
		for (const TWeakObjectPtr<AProsperitocracyFireField>& OtherWeak : EveryFireBurning)
		{
			const AProsperitocracyFireField* Other = OtherWeak.Get();
			if (!Other || Other == this || Other->bTheClockIsOut || Other->LaidOrder <= LaidOrder)
			{
				continue;
			}

			for (const FProsperitocracyBurningGround& Theirs : Other->TheGroundItsBurningOn)
			{
				if (!Theirs.bItIsStillThisFires)
				{
					continue;
				}

				if (FVector::Dist(Piece.Where, Theirs.Where) <= TakenCm)
				{
					bSomebodyNewerIsOnIt = true;
					break;
				}
			}

			if (bSomebodyNewerIsOnIt)
			{
				break;
			}
		}

		Piece.bItIsStillThisFires = !bSomebodyNewerIsOnIt;
	}
}

float AProsperitocracyFireField::HowMuchOfItsGroundItStillOwns() const
{
	if (TheGroundItsBurningOn.Num() == 0)
	{
		return 0.0f;
	}

	int32 HowManyAreStillIts = 0;
	for (const FProsperitocracyBurningGround& Piece : TheGroundItsBurningOn)
	{
		if (Piece.bItIsStillThisFires)
		{
			++HowManyAreStillIts;
		}
	}

	return (float)HowManyAreStillIts / (float)TheGroundItsBurningOn.Num();
}

float AProsperitocracyFireField::SquareMetresItIsBurning() const
{
	const float PieceWidthM = GetPieceWidthCm() / 100.0f;
	return (float)HowMuchOfItsGroundItStillOwns() * (float)TheGroundItsBurningOn.Num() * PI * PieceWidthM * PieceWidthM;
}

// ---------------------------------------------------------------------------------------------------
// THE CLOCK, THE RATE AND THE BURNING DOWN.
// ---------------------------------------------------------------------------------------------------

float AProsperitocracyFireField::TheRateThisGroundWants(float Now) const
{
	// THE GROUND THIS FIRE STILL OWNS IS WHAT IT ASKS FOR. Flames over ground a newer fire has taken would be
	// the same ground burning twice, which is what an overlap used to cost.
	const float ForTheGroundItOwns = BaseFlameRate * HowMuchOfItsGroundItStillOwns();

	// Before the fade it is that, unchanging. Through the fade it is less and less fire, reaching nothing
	// exactly as the clock runs out — the fire thinning ITSELF, so the ground goes dark as one fire rather than
	// as a field of little fires stopping one at a time. Nothing about who gets lit reads this: the ground
	// catches people the whole way down, because the fade is inside the Duration (his rule).
	const float FadeSeconds = EndsAt - FadeStartsAt;
	if (FadeSeconds <= 0.0f || Now <= FadeStartsAt)
	{
		return ForTheGroundItOwns;
	}

	const float BurnedDown = FMath::Clamp((Now - FadeStartsAt) / FadeSeconds, 0.0f, 1.0f);
	return ForTheGroundItOwns * (1.0f - BurnedDown);
}

void AProsperitocracyFireField::SetTheRate(float FlamesASecond)
{
	if (!TheFire)
	{
		return;
	}

	// A rate that has barely moved is not worth a write: the effect keeps what it has and nothing is
	// pushed at it every frame for the sake of a number nobody could see change.
	const float WorthSending = FMath::Max(1.0f, FMath::Abs(RateSent) * RateChangeWorthSending);
	if (RateSent >= 0.0f && FMath::Abs(FlamesASecond - RateSent) < WorthSending)
	{
		return;
	}

	TheFire->SetVariableFloat(ProsperitocracyFireVariables::TheRate, FlamesASecond);
	RateSent = FlamesASecond;
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

	// NOTHING CATCHES ANYTHING FROM HERE. The ground is dead the moment the clock is; the fire that is still
	// alight is only burning down, and the ground it is on is nobody's any more — a newer fire may take it.
	if (TheFire)
	{
		// Told to stop, and nothing else: no new fire is made and the flames already alight finish their
		// own lives at the effect's own pace — a real burn-out, with no size trick and nothing re-created.
		// The rate is already nothing by now, because the burning down ends with the clock.
		SetTheRate(0.0f);
		TheFire->OnSystemFinished.AddDynamic(this, &AProsperitocracyFireField::TheFireHasBurnedOut);
		TheFire->Deactivate();
	}

	// A FIRE THAT IS OUT PUTS OUT NO LIGHT: the burning down is a picture, and the ground is not burning any
	// more. The lights go with the clock rather than flickering on over dead ground for another fifteen seconds.
	for (const TObjectPtr<UPointLightComponent>& Light : TheBodiesOfFire)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	TheBodiesOfFire.Reset();

	// A picture that never reports an end must not leave a dead fire standing in the world forever.
	GraceEndsAt = Now + LongestAFireMayOutliveItsClockSeconds;

	if (!TheFire)
	{
		Destroy();
	}
}

void AProsperitocracyFireField::TheFireHasBurnedOut(UNiagaraComponent* Finished)
{
	// The last of the fire is gone and the clock is out: nothing of this fire is left in the world.
	if (bTheClockIsOut)
	{
		Destroy();
	}
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

	// THE FIRE BURNS DOWN, and it burns down as ONE fire: one number, thinning the whole of it evenly.
	if (!bTheClockIsOut)
	{
		SetTheRate(TheRateThisGroundWants(Now));

		if (Now >= EndsAt)
		{
			TheClockIsOut();
			return;
		}
	}

	// The clock is out and the fire has outlived even the grace it was given: it goes.
	if (bTheClockIsOut && Now >= GraceEndsAt)
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

float AProsperitocracyFireField::GetPieceWidthCm() const
{
	// A piece of fire is about a metre across (his number) — the little volume the ground burns in, which is
	// NOT the size of a flame (about half that) and nothing anybody reads off the picture.
	return FireBlock ? FMath::Max(10.0f, FireBlock->HowWideOnePieceBurnsCm) : 50.0f;
}

float AProsperitocracyFireField::GetPieceHeightCm() const
{
	// KNEE HIGH (his words): how far up off its own ground a piece of fire reaches a body.
	return FireBlock ? FMath::Max(10.0f, FireBlock->HowTallOnePieceBurnsCm) : 60.0f;
}

float AProsperitocracyFireField::GetPieceStepCm() const
{
	// How far apart the fire's pieces are laid, and therefore how far it walks in one step. The packing he
	// already liked is the answer here: a flame roughly every 15 cm inside the look, and a piece of ground
	// about a metre across, so a step of a metre covers the ground with no bare patches between pieces.
	return FireBlock ? FMath::Max(10.0f, FireBlock->HowFarApartItsPiecesAreSteppedCm) : 100.0f;
}

float AProsperitocracyFireField::FlamesASecondBurningInTheWorld()
{
	float Burning = 0.0f;
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		const AProsperitocracyFireField* Fire = EveryFireBurning[Index].Get();
		if (!Fire)
		{
			EveryFireBurning.RemoveAt(Index);
			continue;
		}

		// What a fire is actually asking for: its own rate, over the ground it still owns. A fire whose
		// ground a newer one took is asking for little or nothing, and the world's ceiling is a count of
		// fire in the world, not a count of fires.
		Burning += Fire->BaseFlameRate * Fire->HowMuchOfItsGroundItStillOwns();
	}

	return Burning;
}

void AProsperitocracyFireField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The fire's home is an actor of its own, so it does not die with its owner unless it is told to.
	if (MyOwnHome)
	{
		MyOwnHome->Destroy();
		MyOwnHome = nullptr;
	}

	// The lights this fire was lit with are this fire's own; they go with it rather than standing over dead ground.
	for (const TObjectPtr<UPointLightComponent>& Light : TheBodiesOfFire)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	TheBodiesOfFire.Reset();

	EveryFireBurning.Remove(this);

	// THE GROUND GOES BACK. This fire's ground stops being covered the moment it is gone, so every fire still
	// burning reads its own ground back — an older fire gets back whatever this one was sitting on — and every
	// one of them re-lights itself to the ground it now owns.
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		AProsperitocracyFireField* Fire = EveryFireBurning[Index].Get();
		if (!Fire)
		{
			continue;
		}

		Fire->ReReadWhichGroundIsStillIts();
		Fire->LightTheBodiesOfFire();
		Fire->HandTheEffectItsGround();
	}

	Super::EndPlay(EndPlayReason);
}
