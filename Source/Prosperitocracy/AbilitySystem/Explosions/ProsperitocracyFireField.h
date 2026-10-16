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
	 * LIGHT THE GROUND — the one door a fire is born through, walked by the explosion pass.
	 *
	 * FireBlock — which fire this is: its reach, its clock, its Burn, its pieces and its one effect all come
	 * off that block, read FINAL through the fire's own GAS home.
	 * TheOneWhoLeftIt — whose fire it is: the Burn's damage is dealt and credited as his, so a fire that
	 * kills contests health and shows a number exactly as his own shot would.
	 * InDamageEffectClass — the effect the ONE damage pipeline runs on, carried so a Burn's tick is a
	 * normal damage line and never a second way to take health off someone.
	 */
	void LightTheGround(UProsperitocracyFireStatTable* InFireBlock, AActor* TheOneWhoLeftIt,
		TSubclassOf<UGameplayEffect> InDamageEffectClass);

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
	 * It does not walk the surface any more (his call, 2026-10-16). It SAMPLES it: a jittered cloud of points
	 * filling the ball its reach could cover, each one asked what ground is nearest to it; then the pieces that
	 * TOUCH each other are joined, and the reach is spent as the real distance travelled over those joins. What
	 * it can afford is the fire; what it cannot is not.
	 *
	 * Why it is built this way: nothing here depends on anything else. A piece is found by itself, so a thing
	 * standing on one piece — the missile that had just gone off, a crate, a body — costs that one piece and not
	 * the fire. The walk this replaced was one contact away from collapsing into a single dot, which is exactly
	 * what it did.
	 *
	 * It is done ONCE: after it there are no traces per frame, only pieces.
	 */
	void FindTheGroundItIsBurning();

	/**
	 * WHAT GROUND IS NEAREST THIS SAMPLE, in six short looks — down for a floor, up for nothing, across for a
	 * wall's face. The nearest hit is the ground the sample stands on; a sample out in the open finds nothing
	 * and dies, which is what "air" means here.
	 */
	bool TheSurfaceNearestTheSample(const FVector& Sample, FProsperitocracyBurningGround& OutPiece) const;

	/**
	 * HOW FAR THE FIRE HAD TO TRAVEL TO STAND ON EACH PIECE — the shortest way over the pieces that touch,
	 * starting from the one the bang hit, every join costing the real distance between the two pieces.
	 *
	 * OutWalked is filled one entry per piece, in the order they were given; a piece the fire cannot reach at
	 * all is left further away than anything, which is how it is dropped.
	 */
	void SettleTheGroundItCanReach(const TArray<FProsperitocracyBurningGround>& Pieces, TArray<float>& OutWalked) const;

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

	/** Is there something solid between two places — what keeps a body on the far side of cover unburnt. */
	bool SomethingSolidBetween(const FVector& From, const FVector& To) const;

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

	/** Put the Burn on everything standing in the fire — again, every beat, for the whole Duration. */
	void CatchWhateverStandsInIt();

	/**
	 * ONE LIGHT PER BODY OF FIRE (his call, 2026-10-15).
	 *
	 * The pieces that touch each other are one body of fire, and each body gets ONE light, standing on the fire
	 * where the fire is — at the middle of that body, reaching as wide as that body actually is. A ragged fire
	 * therefore lights raggedly, and a fire that has come apart into two (a wall between, a gap) is two lights,
	 * which is what it looks like. The count is held down on purpose: past the ceiling the smallest bodies are
	 * folded into the nearest, because the light is the expensive part of a fire and his ask was the least of it.
	 */
	void LightTheBodiesOfFire();

	/** Every piece that touches this one, folded into one body of fire — the whole grouping is this, run over all pieces. */
	void GroupThePiecesIntoBodiesOfFire(TArray<int32>& OutWhichBodyEachPieceIsIn, int32& OutHowManyBodies) const;

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

	/** How much fire this ground wants right now — the whole of it, until the burning down begins. */
	float TheRateThisGroundWants(float Now) const;

	/** The clock is out: nothing catches anything here any more, and the fire burns down on its own. */
	void TheClockIsOut();

	/** The last of the fire is gone: the picture is done and this fire leaves the world. */
	UFUNCTION()
	void TheFireHasBurnedOut(UNiagaraComponent* Finished);

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

	/** ONE LIGHT PER BODY OF FIRE, ours rather than the effect's, so a body of fire can have exactly one. */
	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> TheBodiesOfFire;

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

	/** World seconds the burning down begins at — the fire's own FadeSeconds, measured back from the end. */
	float FadeStartsAt = 0.0f;

	/** World seconds the ground next catches at. */
	float NextCatchesAt = 0.0f;

	/**
	 * World seconds a fire that has been told to stop is given to finish burning out, and the only thing
	 * that can end it early after that is the picture reporting that it is done.
	 */
	float GraceEndsAt = 0.0f;

	/** What this ground asked for when it was lit, in flames a second — the ceiling and the world applied. */
	float BaseFlameRate = 0.0f;

	/** What the effect was last told, so a rate that has barely moved is not written again. */
	float RateSent = -1.0f;

	/** This fire's place in the order fires were laid: the newest fire owns the ground it covers. */
	int32 LaidOrder = 0;

	/** True once the fire has been laid — the ground is only ever walked and laid once. */
	bool bTheFireIsLaid = false;

	/** True once the clock is out: nothing is burning here any more and the fire is only burning down. */
	bool bTheClockIsOut = false;

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

	/** [TUNE] MINE — the most bodies of fire one fire may be lit by, and therefore the most lights it may put
	 *  in the world. Past it the smallest bodies are folded into the nearest: one light per body of fire is his
	 *  rule, and this is the ceiling that keeps a shattered fire from costing a lamp per shard. */
	static constexpr int32 MostBodiesOfFireOneLightEach = 6;

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
	 * [TUNE] MINE — the longest a fire may outlive its own clock, in seconds. A fire that has been told
	 * to stop needs its own particles to finish, and that is the look; this is only the guarantee that a
	 * picture which never reports an end cannot leave a dead fire standing in the world forever.
	 */
	static constexpr float LongestAFireMayOutliveItsClockSeconds = 15.0f;

	/** A rate that has moved less than this much (a hundredth of itself) is not worth a write. */
	static constexpr float RateChangeWorthSending = 0.01f;

	/** Every fire burning in the world, so a new one can read how much fire is already being asked for. */
	static TArray<TWeakObjectPtr<AProsperitocracyFireField>> EveryFireBurning;

	/** How many fires have ever been laid, so the order fires were laid in is a number and not a guess. */
	static int32 HowManyFiresHaveBeenLaid;
};
