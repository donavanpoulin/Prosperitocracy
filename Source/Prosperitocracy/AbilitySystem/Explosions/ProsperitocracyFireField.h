// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stats/ProsperitocracyStat.h"

#include "ProsperitocracyFireField.generated.h"

class UNiagaraComponent;
class UPointLightComponent;
class UGameplayEffect;
class UProsperitocracyFireStatTable;
class UProsperitocracyStatTable;
class AProsperitocracyStatHostActor;

/**
 * ONE PIECE OF GROUND A FIRE IS BURNING.
 *
 * A fire is not a circle and it is not a patch of particles: it is a set of pieces of REAL ground, each one a
 * place the fire actually reached — the floor it landed on, the slope it ran up, the face of a crate it
 * climbed, the ground on the far side of a container it spilled onto. Where a piece is, the fire is.
 *
 * WhichWayIsUp is the surface's own facing at that piece, because the flames there stand OUT of the surface
 * rather than lying on a plane: up on the ground, sideways on a wall. Nothing is turned to one normal any more
 * — every piece carries its own. (His call, 2026-10-15.)
 *
 * WalkedToGetHereCm is how far the fire had to travel to reach the piece — over the ground AND up everything it
 * climbed — because its Range is the distance it can TRAVEL, never a circle drawn on the ground. It is the
 * number that drops a piece when the fire's reach runs out.
 *
 * bItIsStillThisFires goes false when a NEWER fire covers that ground (his rule, 2026-10-01: the newest fire
 * owns the ground it covers), and comes back if that newer fire goes out with nothing else on top of it.
 */
USTRUCT()
struct FProsperitocracyBurningGround
{
	GENERATED_BODY()

	/** The place on the real surface this piece of fire is — a point ON that surface. */
	UPROPERTY()
	FVector Where = FVector::ZeroVector;

	/** Which way the surface it is standing on faces: straight up on the ground, sideways on a wall. */
	UPROPERTY()
	FVector WhichWayIsUp = FVector::UpVector;

	/** How far the fire walked to get here, in cm — the ground and every climb, spent out of its Range. */
	UPROPERTY()
	float WalkedToGetHereCm = 0.0f;

	/** False once a newer fire has taken this ground; true again when that newer fire is gone. */
	UPROPERTY()
	bool bItIsStillThisFires = true;
};

/**
 * ONE FLAME'S PLACE ON THE GROUND.
 *
 * The cards are a stream — a flame is spawned, lives about a second, and dies — so the thing that BURNS cannot be
 * a card. It is the place cards appear at: a spot on the ground the fire walked over, standing out of its own
 * surface, carrying the size of a card. The picture is drawn on these spots and the burn is tested against them,
 * so what you see and what burns you are the same place — and a spot is a handful of numbers, not a component.
 */
USTRUCT()
struct FProsperitocracyFlameSpot
{
	GENERATED_BODY()

	/** Where the flame stands — a point on the real surface this fire walked over. */
	UPROPERTY()
	FVector Where = FVector::ZeroVector;

	/** Which way that surface faces, so a spot on a wall's face burns what stands against the wall. */
	UPROPERTY()
	FVector WhichWayIsUp = FVector::UpVector;
};

/**
 * ONE PATCH OF FIRE, THE WORLD OVER.
 *
 * A patch is GROUND THAT IS BURNING, gathered across every fire that is burning it — not one fire's own pieces.
 * That is the unit a light belongs to (his call, 2026-10-16): a bombardment over one area is ONE patch, so it
 * is one light, and the lights stop being a hundred lamps over the same ground adding up into spots, seams and
 * flicker.
 *
 * WhoseLightItIs is the OLDEST fire burning any of it, so a patch's light does not jump from one fire to the
 * next as strikes land — it changes hands only when the fire holding it has actually gone.
 */
USTRUCT()
struct FProsperitocracyPatchOfFire
{
	GENERATED_BODY()

	/** A piece of the patch's own ground, nearest its middle — the light stands HERE, never in mid-air. */
	UPROPERTY()
	FVector WhereTheLightStands = FVector::ZeroVector;

	/** Which way that piece of ground faces, so the light is stood off it along its own surface. */
	UPROPERTY()
	FVector WhichWayItFaces = FVector::UpVector;

	/** How wide the patch actually reaches, in cm, from that piece to its furthest one. */
	UPROPERTY()
	float HowWideItIsCm = 0.0f;

	/** How much ground is burning in it, in square metres — what its light is worth. */
	UPROPERTY()
	float HowMuchGroundIsBurningM2 = 0.0f;

	/** The oldest fire burning any of this patch: whose light it is. */
	UPROPERTY()
	TObjectPtr<AProsperitocracyFireField> WhoseLightItIs = nullptr;
};

/**
 * THE FIRE A BANG LEAVES BURNING ON THE GROUND (PLANS/incendiary-strike.md §4 and "THE FIRE'S SHAPE").
 *
 * It is spawned by the ONE explosion pass, at the place the bang went off, and the only thing it is told is
 * WHICH fire it is (the explosive's block names it). What it is, in the order it happens:
 *
 *   - THE FIRE IS NOT A CIRCLE — IT IS THE GROUND IT COULD REACH. It starts on the surface the bang hit and
 *     grows outward from there over the real geometry: along a floor, up a slope, up the face of a crate, over
 *     its top, down its far side and onto the ground past it (his rule: "the fire wraps"). Its own Range is the
 *     distance it may TRAVEL — every metre of ground and every metre of climb is spent out of the same number —
 *     so flat ground gives the full width he sees and a rise costs it width.
 *   - NOTHING EXISTS WHERE IT COULD NOT WALK TO. A piece is only ever made on a surface the fire could reach
 *     from the piece before it, so there is no fire behind a wall it did not climb, and no fire under a floor.
 *     Being behind cover is not a test anybody runs: there is simply nothing there to burn you.
 *   - IT IS NEVER EVEN. The ways it walks out are jittered and the surfaces decide the rest, so the edge is
 *     ragged and how far it got one way is never how far it got another. His word, and his look.
 *   - EACH PIECE IS A LITTLE VOLUME, KNEE-HIGH AND ABOUT A METRE ACROSS, and "in the fire" is standing in one
 *     of them — never the size of a flame, which is about half that (his numbers). A body is lit when the fire
 *     is on it, and one look is made from the piece to the body so a body on the far side of something thin is
 *     never burnt through it.
 *   - ONE EFFECT, FED THE GROUND ITSELF. The whole fire is still ONE picture with one sim and one clock: the
 *     pieces are handed to it as the ground its flames stand on, and each flame stands out of the surface its
 *     piece is on. Nothing is scattered inside a circle any more (2026-10-15).
 *   - ONE LIGHT AND ONE SOUND PER BODY OF FIRE. Pieces that touch are one fire for this: the light sits on the
 *     fire where the fire is, sized to that body of fire, and never as one lamp over the whole patch. Two fires
 *     that meet read as one and come apart when one of them runs out.
 *   - IT CATCHES ANYTHING STANDING IN IT — hit or not, no gate, no pen question, and it deals no damage of its
 *     own. The only thing it does to a body is apply the Burn the block names, again for as long as the body
 *     stands there. Walk out and the Burn's clock runs down on its own.
 *   - IT GOES OUT AS ONE CLOCK. Its own Duration; the fade lives INSIDE that Duration and the ground keeps
 *     catching people the whole way through it. The fade is the fire THINNING ITSELF — it asks for less and
 *     less fire and its flames go out on their own — and it changes the look and nothing else.
 *
 * NOTHING HERE IS PER-ABILITY OR PER-EXPLOSIVE. A grenade, a barrel, a flamethrower or another strike leaves
 * fire by pointing its block at a fire block; this class never learns what left it.
 */
UCLASS()
class AProsperitocracyFireField : public AActor
{
	GENERATED_BODY()

public:
	AProsperitocracyFireField();

	/**
	 * LIGHT THE FIRE THIS THING NAMES — the ONE door anything that leaves fire walks through.
	 *
	 * ThingBlock — the block of the thing whose act is leaving fire. It answers two questions and nothing
	 * else: whether a fire is named at all, and which one. A block that names none leaves none, silently:
	 * presence is scope, exactly as it is for a mark, a status or a bang's look.
	 * TheOneWhoLeftIt — whose fire it is: its Burn is dealt and credited as his.
	 * Place — where the fire starts, on the ground it is meant to burn.
	 *
	 * No effect, no clock, no width and no look is handed in: every one of those is the fire's own block's,
	 * read through the fire's own GAS home. That is what makes this callable from a bang, an ability, a
	 * barrel's death or anything that exists later — the caller says WHERE and WHOSE, and nothing about what
	 * a fire IS.
	 *
	 * Answers the fire it laid, or null when the thing names none, or the one it names will not load.
	 */
	static AProsperitocracyFireField* LightWhatItsBlockNames(const UProsperitocracyStatTable* ThingBlock,
		AActor* TheOneWhoLeftIt, const FVector& Place);

	/**
	 * LIGHT THE GROUND — the one door a fire is born through, walked by LightWhatItsBlockNames above.
	 *
	 * InFireBlock — which fire this is: its reach, its clock, its Burn, its pieces, the effect its Burn runs
	 * through and its one look all come off that block, read FINAL through the fire's own GAS home.
	 * TheOneWhoLeftIt — whose fire it is: the Burn's damage is dealt and credited as his, so a fire that
	 * kills contests health and shows a number exactly as his own shot would.
	 */
	void LightTheGround(UProsperitocracyFireStatTable* InFireBlock, AActor* TheOneWhoLeftIt_);

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * How far the fire's own Range lets it travel, in cm — its Range read as the WIDTH he sees, halved because
	 * the walk is measured from where it landed outward. On flat ground that is exactly the radius of the fire
	 * he sees; up a slope or a wall the same number is spent climbing and the fire covers less ground with it.
	 */
	float GetHowFarItReachesCm() const { return HowFarItReachesCm; }

	/** Every piece of ground this fire is burning right now, for anything that has to read the fire's shape. */
	const TArray<FProsperitocracyBurningGround>& GetTheGroundItsBurningOn() const { return TheGroundItsBurningOn; }

	/**
	 * How much fire every fire in the world is asking for right now, in flames a second.
	 *
	 * A new fire reads this before it sizes itself: a world already burning at its ceiling lights the
	 * new ground THINNER, never dimmer and never smaller — the ground, the Burn and the clock do not
	 * move, because fire taken away leaves bare ground that still burns you.
	 */
	static float FlamesASecondBurningInTheWorld();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * FIND THE GROUND IT IS BURNING — the fire's shape, worked out once, the moment it is lit.
	 *
	 * It WALKS the surface (his call, 2026-10-18). From the piece the bang hit, out over the real ground one
	 * step at a time, in directions lying in that piece's own surface plane: along a floor, up a slope, up the
	 * face of a crate, over its top, down its far side, onto the ground past it. Every accepted piece costs the
	 * real distance travelled to reach it, and the walk stops when the Range is spent — so flat ground gives the
	 * full width he sees and a rise costs it width, because a climb is distance like any other. No uphill
	 * premium: one metre up a wall is one metre.
	 *
	 * The tip of every branch is placed at EXACTLY the distance that is left, never snapped to a lattice line,
	 * so the edge is ragged and follows the surface. Inside the fire, pieces are one step apart.
	 *
	 * Why it is built this way (2026-10-18, measured): the sampling it replaced threw ~900 probes into a ball,
	 * each looking six ways 1.5m. On the flat map that found ground for only 162 of them — one ground point per
	 * metre at best, and it lost half of those to duplicates and to the reach — and on the landscape map it
	 * found ground for **none of 905**, leaving a single piece and no burn at all. A walk cannot miss the ground
	 * it is standing on, covers the surface at one piece per step by construction, and a bad step costs one
	 * branch rather than the fire.
	 *
	 * It is done ONCE: after it there are no traces per frame, only pieces.
	 */
	void FindTheGroundItIsBurning();

	/** THE VOLUME THAT BURNS, DRAWN — [DEV], and only while `Prosperitocracy.FireBoxes` is on. */
	void ShowTheBurnBoxes() const;

	/**
	 * SCATTER THE FLAMES OVER THE GROUND IT BURNED — the spots every card is drawn on and every burn is tested
	 * against. Laid scattered, never on a grid, and never leaving the patch it belongs to, so nothing can stand
	 * outside the fire's reach.
	 */
	void LayTheFlameSpots();


	/**
	 * ONE STEP OF THE WALK — is there surface one step on from this piece, in this direction, and where is it?
	 *
	 * It is a SHAPE moved along the surface, never a ray thrown from a floating point: a probe sphere is swept
	 * forward out of the piece's own surface. A hit is a FACE — a wall, a crate's side — and the piece goes ON
	 * that hit facing out of it, which is how the fire climbs. Nothing in the way means the surface carries on,
	 * so the probe looks onto what is one step ahead: the floor continuing, a crate's top, a slope.
	 *
	 * The look is asked by CHANNEL, the way the missile asks the world, and never by object type: a surface that
	 * answers one and not the other is what left a whole map with one piece of fire on it (2026-10-18, 0 of 905).
	 * A body is never ground — a hit on a character is refused here — so fire is never placed on a person.
	 */
	bool TheSurfaceOneStepOn(const FProsperitocracyBurningGround& From, const FVector& Direction, float HowFarCm,
		FProsperitocracyBurningGround& OutPiece) const;



	/** The surface under a place — the floor, a crate's top, a slope — with its own facing. */
	bool TheSurfaceUnder(const FVector& Place, FVector& OutWhere, FVector& OutWhichWayIsUp) const;

	/**
	 * Whether the fire may stand on a surface at all.
	 *
	 * It may stand on anything the world is built out of EXCEPT a ceiling: heat goes up, and fire burning on
	 * the underside of something is fire that has gone through it. This is the one rule that keeps the walk from
	 * wrapping under a roof or a container once it has climbed it.
	 */
	static bool TheFireMayStandOn(const FVector& WhichWayIsUp);


	/** A place on the ground the fire has not already found — the sampling never keeps the same ground twice. */
	bool IsThatGroundAlreadyBurning(const FVector& Where, const TArray<FProsperitocracyBurningGround>& AlreadyFound) const;

	/** Remember a piece, and give it its place in the fire's own map of where the fire already is. */
	void AddTheGroundPiece(const FProsperitocracyBurningGround& Piece);

	/** Lay the fire: ONE effect over the whole of it, told the ground its flames stand on. */
	void LayTheFire();

	/**
	 * TELL THE EFFECT WHERE THE FIRE IS — the ground it still owns, one entry per piece, and which way each of
	 * those pieces faces, plus how much fire that ground is asking for.
	 *
	 * Called when the fire is lit and again whenever the ground changes hands, because the picture must never
	 * show flames on ground that is a newer fire's: what the effect is told IS the fire's shape.
	 */
	void HandTheEffectItsGround();

	/** Put the Burn on everything touching the fire right now — again, every beat, for the whole Duration. */
	void ApplyTheBurnToWhateverIsTouchingIt();


	/**
	 * GATHER THE WHOLE WORLD'S FIRE INTO PATCHES, and tell every fire which patches' lights it holds.
	 *
	 * A patch is GROUND THAT IS BURNING, gathered across every fire burning it — not one fire's own pieces (his
	 * call, 2026-10-16). A bombardment over one area is therefore ONE patch and one light, instead of eighteen
	 * fires' worth of lamps over the same ground adding up into spots, seams and flicker.
	 *
	 * Called when a fire is laid, when the ground changes hands, and when a fire goes.
	 */
	static void RegroupTheWorldsFire();

	/**
	 * HOLD THE LIGHTS FOR THESE PATCHES — ONE light each, standing on a piece of the patch's own ground, reaching
	 * as wide as the patch reaches, as bright as the ground burning in it is worth.
	 *
	 * The components are KEPT, moved and retuned. Nothing is destroyed and re-created while a patch lives: a lamp
	 * torn down and put up again three times a second is a lamp that flickers.
	 */
	void HoldTheLightsForThesePatches(const TArray<FProsperitocracyPatchOfFire>& ThePatchesItsLightsAreFor);

	/** Retune the lights this fire holds to how much of it is still on the screen. */
	void RetuneTheLightsItHolds(float Now);

	/**
	 * READ THE GROUND BACK — which of this fire's pieces are still its own.
	 *
	 * A newer fire owns the ground it covers (his call, 2026-10-01): a piece of an older fire inside a newer
	 * fire's ground is taken, and the older fire keeps everything else. Nothing is ever replaced, re-made or
	 * moved — the ground simply changes hands, which is also why a fire that lost ground asks for less fire
	 * rather than shrinking. Called when any fire is laid and when any fire goes, so the ground a dead fire was
	 * sitting on goes back to the fires underneath it.
	 */
	void ReReadWhichGroundIsStillIts();

	/** The share of its own ground this fire still owns — 1 when nobody newer is on it. */
	float HowMuchOfItsGroundItStillOwns() const;

	/** The area it is actually burning, in square metres — the ground it still owns, piece by piece. */
	float SquareMetresItIsBurning() const;

	/** Hand the effect the rate it should be burning at, and write nothing when it has not really moved. */
	void SetTheRate(float FlamesASecond);

	/** What this ground is asking for right now, in flames a second — its ground, and how much is still being placed. */
	float TheRateThisGroundWants(float Now) const;

	/**
	 * HOW MUCH OF THE FIRE IS STILL BEING PLACED: 1 for the whole Duration, then sliding to nothing across the one
	 * second after it.
	 *
	 * Nothing catches in that second — the fire that could burn anybody is already over — so those flames are the
	 * picture of a fire going out, and the same number dims the lamp so the light goes out with them.
	 */
	float HowMuchIsBeingPlaced(float Now) const;

	/** This fire's own rows, read FINAL through its own GAS home — the same read every stat takes. */
	float GetMyStat(EProsperitocracyStat Stat) const;

	/** The block's own piece of data, read for what it is, with the C++ default when the row is not authored. */
	float GetPieceWidthCm() const;
	float GetPieceHeightCm() const;
	float GetPieceStepCm() const;

	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Fire")
	TObjectPtr<USceneComponent> TheRoot;

	/** THE FIRE ITSELF: one component, one sim, one clock — the whole patch, told where its ground is. */
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> TheFire;

	/** THE GROUND IT IS BURNING — every piece, in the order the fire reached them. This IS the fire's shape. */
	UPROPERTY()
	TArray<FProsperitocracyBurningGround> TheGroundItsBurningOn;

	/** WHERE THE FLAMES STAND — scattered spots over that ground, and what burns you. One box each. */
	UPROPERTY()
	TArray<FProsperitocracyFlameSpot> TheFlameSpots;

	/** THE LIGHTS THIS FIRE HOLDS — one per patch of the world's fire whose light is this fire's to hold. They
	 *  are KEPT and retuned as that fire changes, and destroyed only when the patch they stand for is gone. */
	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> TheLightsItHolds;

	/** What each of those lights is worth with the fire at full strength, in candelas — the burning down scales it. */
	TArray<float> WhatEachHeldLightIsWorth;

	/** Which fire this is. */
	UPROPERTY()
	TObjectPtr<UProsperitocracyFireStatTable> FireBlock;

	/** Whose fire it is — the Burn it applies is his, credited to him. */
	UPROPERTY()
	TObjectPtr<AActor> TheOneWhoLeftIt;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** The fire's own GAS home: its block dressed onto a host, so Range and Duration are read FINAL. */
	UPROPERTY()
	TObjectPtr<AProsperitocracyStatHostActor> MyOwnHome;

	/** How far the fire's Range lets it travel, in cm — the Range he sees, halved. */
	float HowFarItReachesCm = 0.0f;

	/** How long the ground burns, in seconds — the fire's own Duration. */
	float DurationSeconds = 0.0f;

	/** World seconds the clock is out at. */
	float EndsAt = 0.0f;

	/** World seconds the last of its flames has gone out at — after this the empty fire leaves the world. */
	float GoneAt = 0.0f;

	/** World seconds the ground next catches at. */
	float NextCatchesAt = 0.0f;

	/** [DIAG 2026-10-18] World seconds the catch next says what it sees, in the log. One line a second per fire,
	 *  and the thing that names why a fire that is plainly there burns nobody. Delete with the [FireCatch] lines. */
	float NextTheCatchSaysWhatItSees = 0.0f;

	/** What this ground asked for when it was lit, in flames a second — the ceiling and the world applied. */
	float BaseFlameRate = 0.0f;

	/** What the effect was last told, so a rate that has barely moved is not written again. */
	float RateSent = -1.0f;

	/** This fire's place in the order fires were laid: the newest fire owns the ground it covers. */
	int32 LaidOrder = 0;

	/** How many pieces of ground it was LAID with — what "how much of its ground is left" is measured against. */
	int32 HowManyPiecesWereLaid = 0;

	/** True once the fire has been laid — the ground is only ever walked and laid once. */
	bool bTheFireIsLaid = false;

	/** [TUNE] MINE — how often the fire is asked who is standing in it. Damage-free, so this is only
	 *  a cost knob: nothing about who gets lit depends on it. */
	static constexpr float CatchEverySeconds = 0.2f;

	/**
	 * [TUNE] MINE — the ceiling on fire in the WHOLE world, in flames a second, across every fire alive.
	 *
	 * Past it a new fire is laid THINNER, never removed and never weakened: the ground, the Burn and the
	 * clock are the same. A thinned field still reads as lit; fire taken away leaves bare ground that
	 * still burns you, which is the one thing he called out as looking broken.
	 *
	 * It is a BACKSTOP and not the thing that does the work: fires that overlap do not stack any more,
	 * because a newer fire owns the ground it covers (see ReReadWhichGroundIsStillIts), so a whole
	 * bombardment landing on one patch asks for one patch's worth of fire. This only catches a world
	 * burning in several places at once.
	 */
	static constexpr float MostFlamesASecondInTheWholeWorld = 24000.0f;

	/** [TUNE] MINE — the least a fire ever asks for, so even a world at its ceiling still shows fire
	 *  where the ground is burning. */
	static constexpr float FewestFlamesASecondAFireMayAsk = 150.0f;

	/** [TUNE] MINE — the most patches of fire in the WORLD that may have a light each. Past it the smallest
	 *  patch is folded into the nearest: one light per patch of fire is the rule, and this is the ceiling that
	 *  keeps a shattered fire from costing a lamp per shard. Few enough that the lights can cast shadows. */
	static constexpr int32 MostPatchesOfFireWithALightInTheWorld = 8;

	/** [TUNE] MINE — how light one body of fire is lit with, per square metre of that body, in candelas. */
	static constexpr float CandelasASquareMetreOfFireIsWorth = 22.0f;

	/** [TUNE] MINE — the brightest and dimmest a single body of fire is ever lit with, in candelas. */
	static constexpr float BrightestOneBodyOfFireIsLit = 40000.0f;
	static constexpr float DimmestOneBodyOfFireIsLit = 900.0f;

	/** [TUNE] MINE — how far the light stands off the fire, along that piece of ground's own facing, in cm. */
	static constexpr float HowFarTheLightStandsOffTheFireCm = 60.0f;

	/** [TUNE] MINE — how far the light reaches, as a share of the body of fire's own width, plus a floor so
	 *  a single piece of fire is still lit as fire rather than as a dot. */
	static constexpr float HowWideTheLightOfABodyOfFireReaches = 1.6f;
	static constexpr float TheLeastALightOfFireReachesCm = 250.0f;

	/**
	 * HOW LONG THE FIRE KEEPS PUTTING FLAMES DOWN AFTER ITS DURATION, in seconds — his number (2026-10-19).
	 *
	 * The fire's POWER ends on the Duration: from that frame it cannot set anybody alight. What it does for the one
	 * second after is thin out, so it goes out instead of being cut. Those flames catch nothing.
	 */
	static constexpr float HowLongTheFlamesThinOutSeconds = 1.0f;

	/**
	 * HOW LONG THE LAST OF ITS FLAMES LIVES, in seconds — the longest a card the effect spawns can live, read off
	 * the effect itself (flames 0.6–1.0 s, ash and heat 1.1–1.4 s). The empty fire is kept this long past the last
	 * flame placed, so its own picture is never cut.
	 */
	static constexpr float HowLongTheLastOfItsFlamesLivesSeconds = 1.4f;

	/** A rate that has moved less than this much (a hundredth of itself) is not worth a write. */
	static constexpr float RateChangeWorthSending = 0.01f;

	/** Every fire burning in the world, so a new one can read how much fire is already being asked for. */
	static TArray<TWeakObjectPtr<AProsperitocracyFireField>> EveryFireBurning;

	/** How many fires have ever been laid, so the order fires were laid in is a number and not a guess. */
	static int32 HowManyFiresHaveBeenLaid;
};
