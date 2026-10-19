// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Explosions/ProsperitocracyFireField.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/Explosions/ProsperitocracyExplosionStatics.h"
#include "AbilitySystem/Explosions/ProsperitocracyFireStatTable.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
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
	/**
	 * THE FIRE'S LIGHTS ARE ON (1.0f). They were held at 0.0f for one test on 2026-10-17 — "are the white marks
	 * left on the floor this light?" — and the answer was no, so they are back. Every lamp is made, placed and
	 * sized by the code below, and this number scales what all of them are worth: 1 is the fire lighting the
	 * ground it burns, 0 is every lamp made and contributing nothing.
	 */
	static constexpr float TheFiresLightsAreOffForATest = 1.0f;

	/** How far off a surface a probe starts, in cm, so it is clear of the ground its own piece is standing on. */
	static constexpr float ProbeStandoffCm = 40.0f;

	/**
	 * HOW MANY WAYS ONE PIECE OFFERS THE FIRE, in the piece's OWN surface plane.
	 *
	 * Eight, because a floor needs to fill in every direction, a wall needs up and down as well as sideways (and
	 * gets them for free — its plane is the wall), and a corner needs the ones in between. The ring is rotated
	 * per piece so the fire's edge is ragged and never a lattice.
	 */
	static constexpr int32 WaysOutOfOnePiece = 8;

	/**
	 * How big the probe is that is moved along the surface, in cm — a shape, not a point, so a gap of a couple of
	 * centimetres cannot swallow a step and the fire cannot be stopped by a hair.
	 */
	static constexpr float TheProbeSphereReachesCm = 25.0f;

	/** The least a step is worth taking, in cm — below this the walk would be placing a piece on itself. */
	static constexpr float TheLeastAStepIsWorthCm = 15.0f;

	/**
	 * HOW WIDE ONE FLAME CARD IS, in cm — the biggest card the effect rolls (`User.FlameSizeMax`, 70).
	 *
	 * The cards keep their own random size; this is the LARGEST of them, and it is what a flame spot's box is
	 * worth. So the smallest card sits inside its box rather than sticking out of it, and no burn reaches further
	 * than a flame could be. Move it with the effect's own parameter, never alone.
	 */
	static constexpr float HowWideOneFlameCardIsCm = 70.0f;

	/**
	 * HOW FAR A FLAME MAY WANDER OFF ITS SPOT, in cm — the effect's own `Sphere Radius` on NE_Flame_02, and it
	 * must be the same number here as there.
	 *
	 * The spots are scattered by the code; this is the last little drift each card gets so a spot never reads as a
	 * single repeating stamp. It is also exactly how much wider than a card the box has to be, so a wandering
	 * flame is still inside the thing that burns — no flame outside its box, and no box reaching past where a
	 * flame could be.
	 */
	static constexpr float HowFarAFlameMayWanderOffItsSpotCm = 15.0f;

	/**
	 * HOW MANY FLAME SPOTS ONE PATCH OF GROUND CARRIES.
	 *
	 * Three, spread around the patch's own middle — enough that the cards standing on them overlap into one fire
	 * with no bare ground between them, and few enough that a bombardment is not paying for a box per flame card.
	 */
	static constexpr int32 SpotsEachPatchOfGroundCarries = 3;

	/**
	 * HOW FAR A BURN BOX REACHES OUT FROM THE MIDDLE OF ITS SPOT, in cm — half its width, so a card's half plus the
	 * wander. THE WALK STOPS THIS FAR SHORT OF THE RANGE, so the box's outer edge lands exactly on the Range
	 * (his rule: if the Range is 8.45m then it is 8.45m). A flame card can still poke a few centimetres past it,
	 * because a card is a picture; the thing that burns you never does.
	 */
	static constexpr float HowFarABurnBoxReachesCm =
		(HowWideOneFlameCardIsCm * 0.5f) + HowFarAFlameMayWanderOffItsSpotCm;

	/** [DEV] On, every fire draws the exact volume that burns — the box, not a picture of it. */
	static bool bShowTheBurnBoxes = false;


	/** How far a probe may look for the surface it has arrived at, along the piece's own facing, in cm. */
	static constexpr float HowFarAProbeLooksCm = 400.0f;

	/** How far below the fire a fall is looked for, in cm. */
	static constexpr float HowFarBelowItLooksForGroundCm = 2000.0f;

	/** How big the ball is that finds the surface a bang landed ON, in cm — the thing the bang hit. */
	static constexpr float TheBangIsWithinCm = 40.0f;





	/**
	 * How close two pieces of ground have to be before the second one is THE SAME GROUND, as a share of a step.
	 *
	 * THIS MUST STAY UNDER A PIECE'S OWN CATCH RADIUS, or the fire gets holes — places where the flames are
	 * plainly there and nothing catches. A piece catches to half its width (50 cm), so the walk may not leave
	 * neighbouring pieces more than a step apart; at 0.9 a step a legitimate neighbour was thrown away as a
	 * duplicate and the gap between pieces came out at 128 cm, leaving a man standing in the fire 64 cm from the
	 * nearest piece and unburnt (measured 2026-10-18, on the landscape map).
	 */
	static constexpr float SameGroundShareOfAStep = 0.5f;


	/** How far BELOW its own ground the fire still reaches a body, in cm — a little, so a fire on a floor does
	 *  not burn somebody in the room underneath it. What it reaches ABOVE is the piece's own height. */
	static constexpr float HowFarBelowItsGroundTheFireReachesCm = 25.0f;

	/** How close two pieces have to be to be ONE body of fire, as a share of one step. */
	static constexpr float OneBodyOfFireShareOfAStep = 1.2f;

	/** [TUNE] MINE — the colour one body of fire is lit with. The pack's own light colour is HIS to place; this
	 *  is the warm the code lights with until he says otherwise. */
	static const FLinearColor TheColourAFireIsLitWith(1.0f, 0.52f, 0.20f);
}

namespace
{
	/** [DEV] `Prosperitocracy.FireBoxes` — on/off. */
	void ToggleTheBurnBoxes(UWorld* World)
	{
		ProsperitocracyFireHandling::bShowTheBurnBoxes = !ProsperitocracyFireHandling::bShowTheBurnBoxes;

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(0x9511, 4.0f, FColor::Orange,
				ProsperitocracyFireHandling::bShowTheBurnBoxes ? TEXT("fire boxes ON") : TEXT("fire boxes OFF"));
		}
	}

	/**
	 * THE VOLUME THAT BURNS, DRAWN.
	 *
	 * It is the box itself — the same numbers the burn test uses — so what is on the screen is exactly what a body
	 * has to touch, and there is nothing to argue with. Nothing is drawn while it is off.
	 */
	FAutoConsoleCommandWithWorld ToggleTheBurnBoxesCommand(
		TEXT("Prosperitocracy.FireBoxes"),
		TEXT("on/off — draws the exact volume that burns on every live fire, so a touch can be seen"),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ToggleTheBurnBoxes));
}

AProsperitocracyFireField::AProsperitocracyFireField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// A FIRE IS A PLACE ON THE GROUND, NOT AN OBSTACLE — and its collision IS its pieces now. Every piece is a
	// query-only box that overlaps bodies and ignores the world, so nothing bumps into a fire, while the engine
	// still has something to report contact with. The actor's collision must stay ENABLED for those boxes to
	// exist at all: with it off every component it owns is born without collision, which is a fire that can
	// never touch anybody (measured 2026-10-18: 0 bodies touching across 144 beats).
	TheRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TheRoot"));
	RootComponent = TheRoot;
	SetActorEnableCollision(true);
}

AProsperitocracyFireField* AProsperitocracyFireField::LightWhatItsBlockNames(const UProsperitocracyStatTable* ThingBlock,
	AActor* TheOneWhoLeftIt, const FVector& Place)
{
	if (!ThingBlock || !TheOneWhoLeftIt)
	{
		return nullptr;
	}

	// PRESENCE IS SCOPE, ASKED OF THE PATH AND NEVER OF THE OBJECT (2026-09-30, and it cost a whole play
	// test): `if (Block->Fire)` reads TSoftObjectPtr::operator bool, which is IsValid() — "is the object
	// LOADED" — and not "is a fire NAMED". Nothing loads a fire block until something asks for it, so that
	// test was false for a fire that had been named all along, and the whole thing was skipped without one
	// word in the log — because a thing that names no fire is silent by design.
	if (ThingBlock->Fire.IsNull())
	{
		return nullptr;
	}

	UWorld* World = TheOneWhoLeftIt->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UProsperitocracyFireStatTable* FireBlock = ThingBlock->Fire.LoadSynchronous();
	if (!FireBlock)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s names a fire block that could not be loaded — no ground is burning."), *GetNameSafe(ThingBlock));
		return nullptr;
	}

	FActorSpawnParameters FireParams;
	FireParams.Owner = TheOneWhoLeftIt;
	FireParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProsperitocracyFireField* TheFire = World->SpawnActor<AProsperitocracyFireField>(
		AProsperitocracyFireField::StaticClass(), Place, FRotator::ZeroRotator, FireParams);
	if (!TheFire)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s names a fire that could not be spawned at %s — no ground is burning."),
			*GetNameSafe(ThingBlock), *Place.ToCompactString());
		return nullptr;
	}

	TheFire->LightTheGround(FireBlock, TheOneWhoLeftIt);
	return TheFire;
}

void AProsperitocracyFireField::LightTheGround(UProsperitocracyFireStatTable* InFireBlock,
	AActor* TheOneWhoLeftIt_)
{
	UWorld* World = GetWorld();
	if (!World || !InFireBlock)
	{
		return;
	}

	FireBlock = InFireBlock;
	TheOneWhoLeftIt = TheOneWhoLeftIt_;

	// THE EFFECT ITS BURN RUNS THROUGH comes off the fire's OWN block (2026-10-19). A fire used to be handed
	// whatever effect the thing that lit it happened to carry — which tied every fire in the game to there
	// being an explosion present to pass one along. A fire that names statuses and no effect would tick them
	// into nothing, so it says so out loud AND burns anyway: the ground is still a fire, and hiding the
	// mistake behind "no fire at all" would be the worse of the two failures.
	DamageEffectClass = InFireBlock->DamageEffectClass;
	if (!DamageEffectClass && InFireBlock->AppliedEffects.Num() > 0)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s names %d status(es) but no damage effect — its burn will deal NOTHING. Set the effect on the fire's own block."),
			*GetNameSafe(InFireBlock), InFireBlock->AppliedEffects.Num());
	}

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

	// WHERE THE FLAMES STAND, FIRST: the effect is told these spots and every box is built from them, so the
	// picture and the burn are the same places — and the count the fire is sized from is the count it was laid at.
	HowManyPiecesWereLaid = TheGroundItsBurningOn.Num();
	LayTheFlameSpots();

	LayTheFire();

	// A NEW FIRE HAS TAKEN SOMEONE'S GROUND. Every fire burning reads its own ground back — this one too,
	// though nothing newer exists yet — so the ground that changed hands stops burning twice.
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		if (AProsperitocracyFireField* Fire = EveryFireBurning[Index].Get())
		{
			Fire->ReReadWhichGroundIsStillIts();
			if (UWorld* TheirWorld = Fire->GetWorld())
			{
				Fire->SetTheRate(Fire->TheRateThisGroundWants(TheirWorld->GetTimeSeconds()));
			}
		}
	}

	// AND THE WORLD'S FIRE IS GATHERED BACK INTO PATCHES — once, for the whole world, because a light belongs to
	// GROUND THAT IS BURNING and not to any one fire (his call, 2026-10-16).
	RegroupTheWorldsFire();

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
	// THE FIRST PIECE IS THE GROUND THE BANG HIT, and it is the only piece anything is ever grown from.
	TheFirst.WalkedToGetHereCm = 0.0f;
	TheFirst.bItIsStillThisFires = true;
	AddTheGroundPiece(TheFirst);

	// THE WALK. The ground is found by MOVING ALONG IT, one step at a time — never by throwing probes into a
	// ball and keeping what they happen to hit. Every step puts a piece where the surface actually is, so the
	// ground is covered at one piece per step by construction: nothing is too far to be looked at, and nothing
	// is thrown away for sitting near something else.
	TArray<int32> TheFrontier;
	TheFrontier.Add(0);

	int32 HowManyTheSurfaceRanOut = 0;
	int32 HowManyWereAlreadyTaken = 0;
	float HowFarTheFurthestWalked = 0.0f;

	for (int32 Head = 0; Head < TheFrontier.Num(); ++Head)
	{
		const int32 ParentIndex = TheFrontier[Head];
		const FProsperitocracyBurningGround From = TheGroundItsBurningOn[ParentIndex];

		// THE WAYS OUT LIE IN THE PIECE'S OWN SURFACE PLANE: a floor walks out flat, a wall walks up and down
		// itself, a slope walks along its own face. The ring is turned a little per piece, so the edge is ragged
		// and never a lattice.
		const FVector Helper = (FMath::Abs(From.WhichWayIsUp.Z) < 0.9f) ? FVector::UpVector : FVector::ForwardVector;
		const FVector SideA = FVector::CrossProduct(From.WhichWayIsUp, Helper).GetSafeNormal();
		const FVector SideB = FVector::CrossProduct(From.WhichWayIsUp, SideA).GetSafeNormal();
		const float TheTurn = (float)ParentIndex * 2.399963f;

		// How much of the Range is left at this piece — when less than a step is left, the next piece IS the tip.
		const float LeftToWalkCm = HowFarItReachesCm - From.WalkedToGetHereCm;
		if (LeftToWalkCm < ProsperitocracyFireHandling::TheLeastAStepIsWorthCm)
		{
			continue;
		}
		const bool bTheNextOneIsTheTip = LeftToWalkCm < StepCm;
		const float HowFarCm = FMath::Min(StepCm, LeftToWalkCm);

		for (int32 Way = 0; Way < ProsperitocracyFireHandling::WaysOutOfOnePiece; ++Way)
		{
			const float Angle = TheTurn + (2.0f * PI * (float)Way) / (float)ProsperitocracyFireHandling::WaysOutOfOnePiece;
			const FVector Direction = (SideA * FMath::Cos(Angle)) + (SideB * FMath::Sin(Angle));

			FProsperitocracyBurningGround Found;
			if (!TheSurfaceOneStepOn(From, Direction, HowFarCm, Found))
			{
				++HowManyTheSurfaceRanOut;
				continue;
			}

			const float HowLongTheStepWasCm = FVector::Dist(From.Where, Found.Where);
			if (HowLongTheStepWasCm < ProsperitocracyFireHandling::TheLeastAStepIsWorthCm)
			{
				continue;
			}

			const float WalkedToItCm = From.WalkedToGetHereCm + HowLongTheStepWasCm;

			// THE RANGE IS THE RANGE: a piece is only kept if its burn box still fits inside it, so the volume
			// that burns ends exactly on the Range and never a step past it.
			if (WalkedToItCm + ProsperitocracyFireHandling::HowFarABurnBoxReachesCm > HowFarItReachesCm)
			{
				++HowManyTheSurfaceRanOut;
				continue;
			}

			if (IsThatGroundAlreadyBurning(Found.Where, TheGroundItsBurningOn))
			{
				++HowManyWereAlreadyTaken;
				continue;
			}

			Found.WalkedToGetHereCm = WalkedToItCm;
			Found.bItIsStillThisFires = true;
			HowFarTheFurthestWalked = FMath::Max(HowFarTheFurthestWalked, WalkedToItCm);

			const int32 NewIndex = TheGroundItsBurningOn.Num();
			AddTheGroundPiece(Found);

			// A TIP IS WHERE THE WALK ENDS: everything past it is further than the Range, so it has no children.
			if (!bTheNextOneIsTheTip)
			{
				TheFrontier.Add(NewIndex);
			}
		}
	}

	// How the shape came out, in numbers: what the walk found, what the surface refused, and where the Range
	// stopped it. `the furthest walked` should sit just inside the reach — far short of it is a walk that is
	// being cut off by something other than its own Range.
	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Fire] the ground is walked: %d pieces, %d the surface ran out, %d already taken, the furthest %.1fm walked for, a reach of %.1fm, asking for %.0f flames a second."),
		TheGroundItsBurningOn.Num(), HowManyTheSurfaceRanOut, HowManyWereAlreadyTaken,
		HowFarTheFurthestWalked / 100.0f, HowFarItReachesCm / 100.0f,
		SquareMetresItIsBurning() * (FireBlock ? FireBlock->FlamesASquareMetreASecond : 0.0f));
}

bool AProsperitocracyFireField::TheSurfaceOneStepOn(const FProsperitocracyBurningGround& From, const FVector& Direction,
	float HowFarCm, FProsperitocracyBurningGround& OutPiece) const
{
	const UWorld* World = GetWorld();
	if (!World || HowFarCm <= 0.0f)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FireWalksTheGround), /*bTraceComplex=*/ false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(TheOneWhoLeftIt);

	// ASKED BY CHANNEL, the way the missile asks the world — never by object type. A surface that answers one
	// and not the other is exactly what left the landscape map with one piece of fire on it (0 of 905 samples).
	const ECollisionChannel TheChannel = ECC_Visibility;

	// A BODY IS NEVER GROUND (his rule, 2026-10-18): fire is placed on the world, never on a player, an enemy or
	// anything else that carries an ability system. A body is burnt by STANDING in the fire, not built on.
	auto IsABody = [](const FHitResult& Hit)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor)
		{
			return false;
		}
		if (UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor) != nullptr)
		{
			return true;
		}

		const UPrimitiveComponent* HitPart = Hit.GetComponent();
		return HitPart && HitPart->GetCollisionObjectType() == ECC_Pawn;
	};

	const FVector& Up = From.WhichWayIsUp;
	const FVector StandOff = Up * ProsperitocracyFireHandling::ProbeStandoffCm;
	const FVector ClearOfItsOwnSurface = From.Where + StandOff;

	// ONE: IS THERE A FACE IN THE WAY? A shape is moved forward out of the surface, not a ray thrown from it, so
	// the step cannot be lost down a gap. A hit here is a wall or a crate's side, and the piece goes ON it facing
	// out of it — which is how the fire climbs.
	FHitResult TheFace;
	if (World->SweepSingleByChannel(TheFace, ClearOfItsOwnSurface, ClearOfItsOwnSurface + Direction * HowFarCm,
		FQuat::Identity, TheChannel, FCollisionShape::MakeSphere(ProsperitocracyFireHandling::TheProbeSphereReachesCm), Params))
	{
		if (!TheFace.bStartPenetrating && !IsABody(TheFace))
		{
			if (!TheFireMayStandOn(TheFace.ImpactNormal))
			{
				// The underside of something is not ground: heat goes up, so the walk stops this way.
				return false;
			}

			OutPiece.Where = TheFace.ImpactPoint;
			OutPiece.WhichWayIsUp = TheFace.ImpactNormal;
			OutPiece.WalkedToGetHereCm = 0.0f;
			OutPiece.bItIsStillThisFires = true;
			return true;
		}
	}

	// TWO: THE SURFACE CARRIES ON. Nothing in the way, so look onto whatever is one step ahead — the floor going
	// on, a crate's top, a slope's face, the ground below a ledge.
	const FVector OneStepAhead = From.Where + Direction * HowFarCm;
	FHitResult TheGround;
	if (World->LineTraceSingleByChannel(TheGround, OneStepAhead + StandOff,
		OneStepAhead - Up * ProsperitocracyFireHandling::HowFarAProbeLooksCm, TheChannel, Params))
	{
		if (!TheGround.bStartPenetrating && !IsABody(TheGround) && TheFireMayStandOn(TheGround.ImpactNormal))
		{
			OutPiece.Where = TheGround.ImpactPoint;
			OutPiece.WhichWayIsUp = TheGround.ImpactNormal;
			OutPiece.WalkedToGetHereCm = 0.0f;
			OutPiece.bItIsStillThisFires = true;
			return true;
		}
	}

	return false;
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

	// ONLY THE GROUND IT STILL OWNS, AS ITS FLAMES' OWN PLACES: the spots are where the picture is drawn and
	// where the burn is tested, so what the effect is told IS the fire — no flame stands anywhere else.
	//
	// AND AS OFFSETS FROM THE FIRE'S OWN ORIGIN, because that is what the effect does with them: each flame is
	// offset from the component, which stands at the fire's origin. The SINK is taken off here too, along EACH
	// SPOT'S OWN FACING — his number of sitting the flames into what they are burning, done per spot, so a flame
	// on a wall's face is pushed into the wall and a flame on a floor is pushed into the floor.
	TArray<FVector> TheGround;
	TheGround.Reserve(TheFlameSpots.Num());

	// His number, off the block, read once: how deep in the surface a flame sits.
	const float HowDeepTheFlameSitsCm = FireBlock ? FireBlock->SinkCm : 0.0f;

	for (const FProsperitocracyFlameSpot& Spot : TheFlameSpots)
	{
		TheGround.Add(Spot.Where - Spot.WhichWayIsUp * HowDeepTheFlameSitsCm - GetActorLocation());
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(TheFire,
		ProsperitocracyFireVariables::TheGroundItsBurningOn, TheGround);

	SetTheRate(BaseFlameRate * HowMuchOfItsGroundItStillOwns());
}

// ---------------------------------------------------------------------------------------------------
// WHO IS IN IT — the fire's own pieces, and nothing solid in between.
// ---------------------------------------------------------------------------------------------------

void AProsperitocracyFireField::LayTheFlameSpots()
{
	TheFlameSpots.Reset();
	if (TheGroundItsBurningOn.Num() == 0)
	{
		return;
	}

	// SCATTERED, NOT A GRID (his rule): every spot is a random place on the ground this fire walked over, and a
	// spot never leaves its own patch — so no flame, no box and no burn can stand outside the reach.
	//
	// Two spots per patch, a card's width apart at most, so the cards a patch ever shows overlap into one fire
	// instead of reading as rows. The card's own random size is untouched: the biggest card (70cm, the effect's
	// own `FlameSizeMax`) is what the spot's box is worth, so a small card is inside its box rather than outside.
	const float PatchRadiusCm = GetPieceWidthCm();
	const float HowFarApartSpotsSitCm = PatchRadiusCm * 0.5f;
	const float TheJitterCm = ProsperitocracyFireHandling::HowFarAFlameMayWanderOffItsSpotCm;

	for (const FProsperitocracyBurningGround& Piece : TheGroundItsBurningOn)
	{
		const FVector Helper = (FMath::Abs(Piece.WhichWayIsUp.Z) < 0.9f) ? FVector::UpVector : FVector::ForwardVector;
		const FVector SideA = FVector::CrossProduct(Piece.WhichWayIsUp, Helper).GetSafeNormal();
		const FVector SideB = FVector::CrossProduct(Piece.WhichWayIsUp, SideA).GetSafeNormal();

		for (int32 Which = 0; Which < ProsperitocracyFireHandling::SpotsEachPatchOfGroundCarries; ++Which)
		{
			const float Around = ((float)Which / (float)ProsperitocracyFireHandling::SpotsEachPatchOfGroundCarries) * 2.0f * PI;
			const FVector Spot = Piece.Where
				+ SideA * (FMath::Cos(Around) * HowFarApartSpotsSitCm + FMath::FRandRange(-TheJitterCm, TheJitterCm))
				+ SideB * (FMath::Sin(Around) * HowFarApartSpotsSitCm + FMath::FRandRange(-TheJitterCm, TheJitterCm));

			FProsperitocracyFlameSpot& NewSpot = TheFlameSpots.AddDefaulted_GetRef();
			NewSpot.Where = Spot;
			NewSpot.WhichWayIsUp = Piece.WhichWayIsUp;
		}
	}
}

void AProsperitocracyFireField::ShowTheBurnBoxes() const
{
	if (!ProsperitocracyFireHandling::bShowTheBurnBoxes)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// THE SAME NUMBERS THE BURN TEST USES, so the box on the screen is the box a body has to touch.
	const float HalfAcrossCm = ProsperitocracyFireHandling::HowFarABurnBoxReachesCm;
	const float AboveCm = GetPieceHeightCm();
	const float BelowCm = ProsperitocracyFireHandling::HowFarBelowItsGroundTheFireReachesCm;
	const FVector HalfExtent(HalfAcrossCm, HalfAcrossCm, (AboveCm + BelowCm) * 0.5f);

	for (const FProsperitocracyFlameSpot& Spot : TheFlameSpots)
	{
		const FVector Centre = Spot.Where + Spot.WhichWayIsUp * ((AboveCm - BelowCm) * 0.5f);
		DrawDebugBox(World, Centre, HalfExtent, FRotationMatrix::MakeFromZ(Spot.WhichWayIsUp).ToQuat(),
			FColor::Orange, /*bPersistentLines=*/ false, /*LifeTime=*/ 0.0f, /*DepthPriority=*/ 0, /*Thickness=*/ 0.6f);
	}
}

void AProsperitocracyFireField::ApplyTheBurnToWhateverIsTouchingIt()
{
	if (!FireBlock || !TheOneWhoLeftIt || TheFlameSpots.Num() == 0)
	{
		return;
	}

	// The Burn's ticks are dealt by whoever left the fire, so a fire that kills contests health and shows a
	// number exactly as his own shot would. His ability system gone means nothing to credit: no burn.
	UAbilitySystemComponent* Source = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TheOneWhoLeftIt);
	if (!Source)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ONE ASK OF THE WORLD PER BEAT, for the bodies near this fire — not one per flame spot, and no components.
	// Everything after it is arithmetic against a list we already hold in memory.
	float FurthestSpotCm = 0.0f;
	for (const FProsperitocracyFlameSpot& Spot : TheFlameSpots)
	{
		FurthestSpotCm = FMath::Max(FurthestSpotCm, FVector::Dist2D(Spot.Where, GetActorLocation()));
	}

	const float HalfAcrossCm = (ProsperitocracyFireHandling::HowWideOneFlameCardIsCm * 0.5f)
		+ ProsperitocracyFireHandling::HowFarAFlameMayWanderOffItsSpotCm;
	const float AboveCm = GetPieceHeightCm();
	const float BelowCm = ProsperitocracyFireHandling::HowFarBelowItsGroundTheFireReachesCm;

	TArray<FProsperitocracyExplosionTarget> Nearby;
	UProsperitocracyExplosionStatics::GatherTheBall(World, GetActorLocation(), FurthestSpotCm + HalfAcrossCm, this, Nearby);

	int32 HowManyBodiesTouching = 0;

	for (const FProsperitocracyExplosionTarget& Target : Nearby)
	{
		AActor* Body = Target.Actor;
		if (!Body || !UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Body))
		{
			continue;
		}

		// THE BODY AS TWO POINTS AND A RADIUS — its own collision cylinder, so a capsule is tested as a capsule
		// and not as a point somebody picked.
		float BodyRadiusCm = 0.0f;
		float BodyHalfHeightCm = 0.0f;
		Body->GetSimpleCollisionCylinder(BodyRadiusCm, BodyHalfHeightCm);
		const FVector BodyUp = Body->GetActorUpVector();
		const FVector BodyMiddle = Body->GetActorLocation();
		const float SegmentReachCm = FMath::Max(0.0f, BodyHalfHeightCm - BodyRadiusCm);
		const FVector SegmentFoot = BodyMiddle - BodyUp * SegmentReachCm;
		const FVector SegmentHead = BodyMiddle + BodyUp * SegmentReachCm;

		for (const FProsperitocracyFlameSpot& Spot : TheFlameSpots)
		{
			// THE FLAME'S OWN BOX, in the spot's frame: what a card can be, and not a hair more.
			const FQuat BoxTurn = FRotationMatrix::MakeFromZ(Spot.WhichWayIsUp).ToQuat();
			const FVector NearestOnTheBody = FMath::ClosestPointOnSegment(Spot.Where, SegmentFoot, SegmentHead);
			const FVector InTheBox = BoxTurn.UnrotateVector(NearestOnTheBody - Spot.Where);

			const bool bTouching = FMath::Abs(InTheBox.X) <= HalfAcrossCm + BodyRadiusCm
				&& FMath::Abs(InTheBox.Y) <= HalfAcrossCm + BodyRadiusCm
				&& InTheBox.Z <= AboveCm + BodyRadiusCm
				&& InTheBox.Z >= -(BelowCm + BodyRadiusCm);

			if (!bTouching)
			{
				continue;
			}

			// NO HIT, NO GATE, NO PEN QUESTION (his spec): the fire's door asks nothing about damage, because
			// a fire has none. It applies its Burn again, every beat, for as long as the body is touching it —
			// and a body that walks out keeps the Burn it has, running down on its own clock.
			UProsperitocracyDamageStatics::ApplyStatusesOnContact(Body, Source, FireBlock, DamageEffectClass);
			++HowManyBodiesTouching;
			break;
		}
	}

	// [DIAG 2026-10-18] One line a second: how many pieces and spots it has, and who is touching it.
	if (World->GetTimeSeconds() >= NextTheCatchSaysWhatItSees)
	{
		NextTheCatchSaysWhatItSees = World->GetTimeSeconds() + 1.0f;
		UE_LOG(LogProsperitocracy, Log,
			TEXT("[FireCatch] %d pieces, %d flame spots, %d bodies touching it."),
			TheGroundItsBurningOn.Num(), TheFlameSpots.Num(), HowManyBodiesTouching);
	}
}

// ---------------------------------------------------------------------------------------------------
// ONE LIGHT PER PATCH OF FIRE IN THE WORLD — his call, 2026-10-16.
//
// What it replaced, and why. The lights used to belong to a FIRE: one fire, up to six lamps, and every strike
// destroyed and re-created every one of them. A bombardment is eighteen fires, so that was up to a hundred
// lamps over the same patch, torn down and put up again three times a second, all adding into each other —
// which is the flicker, and the spots and seams where two pools met. And they cast no shadows, so a pool of
// firelight washed through a container as a bright patch.
//
// The unit of light is now GROUND THAT IS BURNING, gathered ACROSS every fire burning it: one patch, one lamp.
// A bombardment over one area is ONE patch, so ONE light. The lamps are KEPT — moved and retuned, never
// rebuilt while their patch lives — and because there are few enough of them, they can cast shadows.
// ---------------------------------------------------------------------------------------------------

void AProsperitocracyFireField::RegroupTheWorldsFire()
{
	AProsperitocracyFireField* AnyFire = nullptr;
	for (const TWeakObjectPtr<AProsperitocracyFireField>& FireWeak : EveryFireBurning)
	{
		if (AProsperitocracyFireField* Fire = FireWeak.Get())
		{
			AnyFire = Fire;
			break;
		}
	}
	if (!AnyFire)
	{
		return;
	}

	// EVERY PIECE OF GROUND BURNING IN THE WORLD, with the fire burning it. A fire whose clock is out is not
	// burning ground any more and its pieces are gone, which is what hands its patches on to whoever is left.
	struct FPieceOfTheWorldsFire
	{
		AProsperitocracyFireField* Fire = nullptr;
		FVector Where = FVector::ZeroVector;
		FVector Up = FVector::UpVector;
	};

	TArray<FPieceOfTheWorldsFire> EveryPiece;
	const float StepCm = AnyFire->GetPieceStepCm();

	for (const TWeakObjectPtr<AProsperitocracyFireField>& FireWeak : EveryFireBurning)
	{
		AProsperitocracyFireField* Fire = FireWeak.Get();
		if (!Fire || Fire->bTheClockIsOut)
		{
			continue;
		}

		for (const FProsperitocracyBurningGround& Piece : Fire->TheGroundItsBurningOn)
		{
			if (Piece.bItIsStillThisFires)
			{
				EveryPiece.Add({ Fire, Piece.Where, Piece.WhichWayIsUp });
			}
		}
	}

	// PIECES THAT TOUCH ARE ONE PATCH OF FIRE — his rule, over the whole world. The pieces still burning are
	// few, because a newer fire owns the ground it covers, so the plain pairwise sweep is the honest way here.
	TArray<FProsperitocracyPatchOfFire> TheWorldsPatches;
	{
		const float TouchingCm = StepCm * ProsperitocracyFireHandling::OneBodyOfFireShareOfAStep;
		TArray<int32> WhichPatchEachPieceIsIn;
		WhichPatchEachPieceIsIn.Init(-1, EveryPiece.Num());

		for (int32 Start = 0; Start < EveryPiece.Num(); ++Start)
		{
			if (WhichPatchEachPieceIsIn[Start] != -1)
			{
				continue;
			}

			const int32 PatchIndex = TheWorldsPatches.AddDefaulted();
			WhichPatchEachPieceIsIn[Start] = PatchIndex;

			TArray<int32> ThePatch;
			ThePatch.Add(Start);

			for (int32 Head = 0; Head < ThePatch.Num(); ++Head)
			{
				const FVector& WhereItIs = EveryPiece[ThePatch[Head]].Where;
				for (int32 Other = 0; Other < EveryPiece.Num(); ++Other)
				{
					if (WhichPatchEachPieceIsIn[Other] != -1)
					{
						continue;
					}

					if (FVector::Dist(EveryPiece[Other].Where, WhereItIs) <= TouchingCm)
					{
						WhichPatchEachPieceIsIn[Other] = PatchIndex;
						ThePatch.Add(Other);
					}
				}
			}

			// WHAT THIS PATCH IS: how much ground is burning in it, where its own middle is, which way that
			// ground faces, how far it reaches — and WHOSE LIGHT IT IS, which is the OLDEST fire burning any of
			// it, so the lamp does not change hands as strikes land.
			FProsperitocracyPatchOfFire& Patch = TheWorldsPatches[PatchIndex];
			const float PieceWidthM = AnyFire->GetPieceWidthCm() / 100.0f;

			FVector Centre = FVector::ZeroVector;
			for (const int32 Index : ThePatch)
			{
				Centre += EveryPiece[Index].Where;
				Patch.HowMuchGroundIsBurningM2 += PI * PieceWidthM * PieceWidthM;
				if (!Patch.WhoseLightItIs || EveryPiece[Index].Fire->LaidOrder < Patch.WhoseLightItIs->LaidOrder)
				{
					Patch.WhoseLightItIs = EveryPiece[Index].Fire;
				}
			}
			Centre /= (float)FMath::Max(1, ThePatch.Num());

			// THE LIGHT STANDS ON A PIECE OF THE PATCH'S OWN GROUND — the one nearest its middle — and never in
			// mid-air: a wrapped patch's middle is inside the geometry, and a lamp there lights the wrong thing.
			int32 NearestToTheMiddle = ThePatch[0];
			for (const int32 Index : ThePatch)
			{
				if (FVector::DistSquared(EveryPiece[Index].Where, Centre) < FVector::DistSquared(EveryPiece[NearestToTheMiddle].Where, Centre))
				{
					NearestToTheMiddle = Index;
				}
			}
			Patch.WhereTheLightStands = EveryPiece[NearestToTheMiddle].Where;
			Patch.WhichWayItFaces = EveryPiece[NearestToTheMiddle].Up;

			for (const int32 Index : ThePatch)
			{
				Patch.HowWideItIsCm = FMath::Max(Patch.HowWideItIsCm,
					FVector::Dist(EveryPiece[Index].Where, Patch.WhereTheLightStands));
			}
		}
	}

	// THE COUNT IS HELD DOWN ON PURPOSE: past the ceiling the smallest patch is folded into the nearest, because
	// the light is the expensive part of a fire and his ask was the least of it. Nothing about what burns
	// changes here — this is the light, and only the light.
	while (TheWorldsPatches.Num() > MostPatchesOfFireWithALightInTheWorld)
	{
		int32 Smallest = 0;
		for (int32 Index = 1; Index < TheWorldsPatches.Num(); ++Index)
		{
			if (TheWorldsPatches[Index].HowMuchGroundIsBurningM2 < TheWorldsPatches[Smallest].HowMuchGroundIsBurningM2)
			{
				Smallest = Index;
			}
		}

		int32 Nearest = INDEX_NONE;
		float NearestCm = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < TheWorldsPatches.Num(); ++Index)
		{
			if (Index == Smallest)
			{
				continue;
			}

			const float DistanceCm = FVector::Dist(TheWorldsPatches[Index].WhereTheLightStands, TheWorldsPatches[Smallest].WhereTheLightStands);
			if (DistanceCm < NearestCm)
			{
				NearestCm = DistanceCm;
				Nearest = Index;
			}
		}

		if (Nearest == INDEX_NONE)
		{
			break;
		}

		TheWorldsPatches[Nearest].HowMuchGroundIsBurningM2 += TheWorldsPatches[Smallest].HowMuchGroundIsBurningM2;
		TheWorldsPatches[Nearest].HowWideItIsCm = FMath::Max(TheWorldsPatches[Nearest].HowWideItIsCm,
			NearestCm + TheWorldsPatches[Smallest].HowWideItIsCm);
		TheWorldsPatches.RemoveAt(Smallest);
	}

	// AND EVERY FIRE IS TOLD WHICH PATCHES' LIGHTS IT HOLDS. A fire holding none lets its lamps go — which is
	// the only way a light is ever taken down now.
	for (const TWeakObjectPtr<AProsperitocracyFireField>& FireWeak : EveryFireBurning)
	{
		AProsperitocracyFireField* Fire = FireWeak.Get();
		if (!Fire)
		{
			continue;
		}

		TArray<FProsperitocracyPatchOfFire> TheOnesItHolds;
		for (const FProsperitocracyPatchOfFire& Patch : TheWorldsPatches)
		{
			if (Patch.WhoseLightItIs == Fire)
			{
				TheOnesItHolds.Add(Patch);
			}
		}

		Fire->HoldTheLightsForThesePatches(TheOnesItHolds);
	}
}

void AProsperitocracyFireField::HoldTheLightsForThesePatches(const TArray<FProsperitocracyPatchOfFire>& ThePatchesItsLightsAreFor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// WHAT STILL FITS IS KEPT. A lamp whose patch is still there is MOVED and retuned and never rebuilt: a lamp
	// torn down and put up again three times a second is a lamp that flickers. Only a lamp with no patch left to
	// stand for is taken down.
	while (TheLightsItHolds.Num() > ThePatchesItsLightsAreFor.Num())
	{
		if (UPointLightComponent* TheOneToLose = TheLightsItHolds.Last())
		{
			TheOneToLose->DestroyComponent();
		}
		TheLightsItHolds.Pop();
		if (WhatEachHeldLightIsWorth.Num() > 0)
		{
			WhatEachHeldLightIsWorth.Pop();
		}
	}

	for (int32 Index = 0; Index < ThePatchesItsLightsAreFor.Num(); ++Index)
	{
		const FProsperitocracyPatchOfFire& Patch = ThePatchesItsLightsAreFor[Index];

		UPointLightComponent* TheLight = TheLightsItHolds.IsValidIndex(Index) ? TheLightsItHolds[Index].Get() : nullptr;
		if (!TheLight)
		{
			// The only lamp ever made here: a patch of fire that has none yet.
			TheLight = NewObject<UPointLightComponent>(this);
			TheLight->SetMobility(EComponentMobility::Movable);
			TheLight->SetupAttachment(TheRoot);
			TheLight->RegisterComponent();
			TheLight->SetLightColor(ProsperitocracyFireHandling::TheColourAFireIsLitWith);

			// A patch of fire is ONE light, and there are few enough of them that they can afford to cast
			// shadows — which is what stops a pool of firelight washing through a wall as a bright patch. It is
			// not a cheapness decision any more; it is what the light is.
			TheLight->SetCastShadows(true);

			if (TheLightsItHolds.IsValidIndex(Index))
			{
				TheLightsItHolds[Index] = TheLight;
			}
			else
			{
				TheLightsItHolds.Add(TheLight);
			}
		}

		const FVector WhereTheLightStands = Patch.WhereTheLightStands
			+ Patch.WhichWayItFaces * HowFarTheLightStandsOffTheFireCm;

		// WHAT THE LIGHT IS WORTH is the ground burning in its patch; what it is SET to is that, scaled by how
		// much of the fire is still asking for flames — so a fire that is burning down stops lighting the ground
		// as it goes, in step with its own flames.
		const float WhatItIsWorth = FMath::Clamp(Patch.HowMuchGroundIsBurningM2 * CandelasASquareMetreOfFireIsWorth,
			DimmestOneBodyOfFireIsLit, BrightestOneBodyOfFireIsLit);

		TheLight->SetWorldLocation(WhereTheLightStands);
		TheLight->SetAttenuationRadius(FMath::Max(TheLeastALightOfFireReachesCm,
			Patch.HowWideItIsCm * HowWideTheLightOfABodyOfFireReaches));
		TheLight->SetIntensity(WhatItIsWorth * HowMuchOfTheFireIsStillAsking(World->GetTimeSeconds())
			* ProsperitocracyFireHandling::TheFiresLightsAreOffForATest);

		if (WhatEachHeldLightIsWorth.IsValidIndex(Index))
		{
			WhatEachHeldLightIsWorth[Index] = WhatItIsWorth;
		}
		else
		{
			WhatEachHeldLightIsWorth.Add(WhatItIsWorth);
		}
	}
}

float AProsperitocracyFireField::HowMuchOfTheFireIsStillAsking(float Now) const
{
	// THE BURNING DOWN, AS ONE NUMBER: 1 the whole way while the fire is all there, sliding to nothing as the
	// fade reaches the end of the clock. The flames thin by it, and the light dims by it, so the two cannot say
	// different things about how much fire is left.
	const float FadeSeconds = EndsAt - FadeStartsAt;
	if (FadeSeconds <= 0.0f || Now <= FadeStartsAt)
	{
		return 1.0f;
	}

	return 1.0f - FMath::Clamp((Now - FadeStartsAt) / FadeSeconds, 0.0f, 1.0f);
}

void AProsperitocracyFireField::RetuneTheLightsItHolds()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float HowMuchIsStillAsking = HowMuchOfTheFireIsStillAsking(World->GetTimeSeconds());
	for (int32 Index = 0; Index < TheLightsItHolds.Num(); ++Index)
	{
		if (UPointLightComponent* Light = TheLightsItHolds[Index].Get())
		{
			if (WhatEachHeldLightIsWorth.IsValidIndex(Index))
			{
				Light->SetIntensity(WhatEachHeldLightIsWorth[Index] * HowMuchIsStillAsking
					* ProsperitocracyFireHandling::TheFiresLightsAreOffForATest);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------
// WHOSE GROUND IT STILL IS — the newest fire owns what it covers.
// ---------------------------------------------------------------------------------------------------

void AProsperitocracyFireField::ReReadWhichGroundIsStillIts()
{
	// A NEWER FIRE TAKES THE GROUND IT COVERS — and it TAKES it. The pieces it covers are REMOVED from this fire,
	// with their spots, their boxes and their share of the clock. Two fires never occupy the same square metre, so
	// a bombardment is one fire on one patch instead of eighteen drawing over each other and burning twice
	// (his rule, 2026-10-18). Ground it did not reach keeps burning on its own clock and fades on its own.
	const float TakenCm = GetPieceWidthCm();

	int32 HowManyWereTaken = 0;
	for (int32 Index = TheGroundItsBurningOn.Num() - 1; Index >= 0; --Index)
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
				if (FVector::Dist(TheGroundItsBurningOn[Index].Where, Theirs.Where) <= TakenCm)
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

		if (bSomebodyNewerIsOnIt)
		{
			TheGroundItsBurningOn.RemoveAt(Index);
			++HowManyWereTaken;
		}
	}

	if (HowManyWereTaken > 0)
	{
		// THE FIRE IS LAID AGAIN FROM WHAT IS LEFT: its spots, and the picture it is drawn on.
		LayTheFlameSpots();
		HandTheEffectItsGround();
	}
}

float AProsperitocracyFireField::HowMuchOfItsGroundItStillOwns() const
{
	// THE SHARE OF THE GROUND IT WAS LAID WITH THAT IT STILL HAS. Ground a newer fire took is REMOVED, so this is
	// the one number that tells the picture how much of its fire is still owed to it — a fire that lost half its
	// ground asks for half the flames, and its cards stop standing where somebody else's fire now is.
	if (HowManyPiecesWereLaid <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp((float)TheGroundItsBurningOn.Num() / (float)HowManyPiecesWereLaid, 0.0f, 1.0f);
}

float AProsperitocracyFireField::SquareMetresItIsBurning() const
{
	// The ground it still has, piece by piece. Ground taken by a newer fire is gone from the count, so this needs
	// no share of its own.
	const float PieceWidthM = GetPieceWidthCm() / 100.0f;
	return (float)TheGroundItsBurningOn.Num() * PI * PieceWidthM * PieceWidthM;
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
	// catches people the whole way down, because the fade is inside the Duration (his rule). And the SAME number
	// dims the light over this ground, so the flames and the light cannot disagree about how much fire is left.
	return ForTheGroundItOwns * HowMuchOfTheFireIsStillAsking(Now);
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

	// A FIRE THAT IS OUT BURNS NO GROUND, so the world's patches are gathered again: whatever was this fire's to
	// light passes to the fire still burning that ground, and a patch nobody is burning any more lets its lamp go
	// — which is the only place a light is ever taken down. Nothing is torn down and put back here: a clock
	// running out must not make the world's fire flicker.
	RegroupTheWorldsFire();

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

	// [DEV] the volume that burns, drawn — off unless `Prosperitocracy.FireBoxes` was typed.
	ShowTheBurnBoxes();

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
		ApplyTheBurnToWhateverIsTouchingIt();

		// AND THE LIGHT OVER THIS FIRE'S GROUND FOLLOWS IT DOWN. The same beat that asks who is standing in the
		// fire retunes the lamps, so a fire that is burning out stops lighting the ground as it goes.
		RetuneTheLightsItHolds();
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

	// THE LAMPS THIS FIRE HOLDS GO WITH IT, whichever patch they were standing for: a fire that is gone cannot
	// hold a light for ground. The world's patches are gathered again below, so whatever it was burning passes
	// to the fires still burning it.
	for (const TObjectPtr<UPointLightComponent>& Light : TheLightsItHolds)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	TheLightsItHolds.Reset();
	WhatEachHeldLightIsWorth.Reset();

	TheFlameSpots.Reset();

	EveryFireBurning.Remove(this);

	// THE GROUND GOES BACK. This fire's ground stops being covered the moment it is gone, so every fire still
	// burning reads its own ground back — an older fire gets back whatever this one was sitting on.
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		AProsperitocracyFireField* Fire = EveryFireBurning[Index].Get();
		if (!Fire)
		{
			continue;
		}

		Fire->ReReadWhichGroundIsStillIts();
		Fire->HandTheEffectItsGround();
	}

	// AND THE WORLD'S FIRE IS GATHERED AGAIN — the patches are re-handed, so no lamp keeps standing for ground
	// that is not burning any more and none is left unlit that is.
	RegroupTheWorldsFire();

	Super::EndPlay(EndPlayReason);
}
