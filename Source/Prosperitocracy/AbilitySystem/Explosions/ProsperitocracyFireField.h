// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stats/ProsperitocracyStat.h"

#include "ProsperitocracyFireField.generated.h"

class UNiagaraComponent;
class UGameplayEffect;
class UProsperitocracyFireStatTable;
class AProsperitocracyStatHostActor;

/**
 * THE FIRE A BANG LEAVES BURNING ON THE GROUND (PLANS/incendiary-strike.md §4).
 *
 * It is spawned by the ONE explosion pass, at the place the bang went off, and the only thing it is
 * told is WHICH fire it is (the explosive's block names it). Everything a fire does is read off that
 * one block: how wide it burns, how long it burns, what it puts on what it catches, and what it looks
 * like — all of it, with two numbers of its own fed in live: how wide its circle is, and how many
 * flames a second it is asking for.
 *
 * WHAT IT IS, in the order it happens:
 *   - THE CIRCLE IS THE FIRE. Its own Range row is the circle a body has to be standing in, read as a
 *     WIDTH like every other Range here. Nothing reads the flames: "in it" is the circle, never the
 *     mesh, the particles or the flames.
 *   - ONE EFFECT, AND NOT A FIELD OF THEM. The whole patch is ONE picture: one Niagara component, one
 *     sim, one light, one clock. Its spread reaches the edge of the circle because the fire's own
 *     radius is handed to it, so a wider fire is a WIDER FIRE and never more fires — the difference
 *     between a hundred fires standing shoulder to shoulder and one fire over a hundred times the
 *     ground, which is also the whole of what it costs (2026-10-01).
 *   - IT CATCHES ANYTHING STANDING IN IT — hit or not, no gate, no pen question, and it deals no damage
 *     of its own. The only thing it does to a body is apply the Burn the block names, and it applies it
 *     again for as long as the body stands there. Walk out and the Burn's clock runs down on its own.
 *   - IT GOES OUT AS ONE CLOCK. Its own Duration; the fade lives INSIDE that Duration and the ground
 *     keeps catching people the whole way through it. The fade is the fire THINNING ITSELF — it asks
 *     for less and less fire and its flames go out on their own — and it changes the look and nothing
 *     else.
 *
 * NOTHING HERE IS PER-ABILITY OR PER-EXPLOSIVE. A grenade, a barrel, a flamethrower or another strike
 * leaves fire by pointing its block at a fire block; this class never learns what left it.
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
	 * FireBlock — which fire this is: its circle, its clock, its Burn and its one effect all come off
	 * that block, read FINAL through the fire's own GAS home.
	 * TheOneWhoLeftIt — whose fire it is: the Burn's damage is dealt and credited as his, so a fire that
	 * kills contests health and shows a number exactly as his own shot would.
	 * InDamageEffectClass — the effect the ONE damage pipeline runs on, carried so a Burn's tick is a
	 * normal damage line and never a second way to take health off someone.
	 */
	void LightTheGround(UProsperitocracyFireStatTable* InFireBlock, AActor* TheOneWhoLeftIt,
		TSubclassOf<UGameplayEffect> InDamageEffectClass);

	virtual void Tick(float DeltaSeconds) override;

	/** The circle this fire catches in, in cm. */
	float GetRadiusCm() const { return RadiusCm; }

	/**
	 * How much fire every fire in the world is asking for right now, in flames a second.
	 *
	 * A new fire reads this before it sizes itself: a world already burning at its ceiling lights the
	 * new ground THINNER, never dimmer and never smaller — the circle, the Burn and the clock do not
	 * move, because fire taken away leaves bare ground that still burns you.
	 */
	static float FlamesASecondBurningInTheWorld();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Lay the fire: ONE effect over the whole circle, placed on the surface the bang hit and sunk into
	 * it, with the fire's own width and its flame rate handed to it.
	 */
	void LayTheFire();

	/** Put the Burn on everything standing in the circle — again, every beat, for the whole Duration. */
	void CatchWhateverStandsInIt();

	/**
	 * Read the ground back — how much of THIS fire's own circle the newer fires have taken.
	 *
	 * A newer fire owns the ground it covers (his call, 2026-10-01), and a circle that only touches
	 * another circle's corner cannot take the whole of it: what a newer fire takes is the SHARED ground
	 * and nothing else. A fire is ONE picture over its whole circle, so what it can do about ground it
	 * lost is ask for LESS — its flames thin to the share of the circle nobody newer has covered, and
	 * the same ground is never asked for twice. Called when any fire is laid and when any fire goes.
	 */
	void ReReadHowMuchGroundItStillOwns();

	/** The ground two circles share, in cm² — the one piece of geometry an overlap needs. */
	static float TheGroundTwoFiresShare(float OneRadiusCm, float OtherRadiusCm,
		const FVector& OneCentre, const FVector& OtherCentre);

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

	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Fire")
	TObjectPtr<USceneComponent> TheRoot;

	/** THE FIRE ITSELF: one component, one sim, one light, one clock — the whole patch. */
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> TheFire;

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

	/** The circle's width, in cm — the fire's own Range, halved because a circle is built from its radius. */
	float RadiusCm = 0.0f;

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

	/** The share of this fire's own circle that no NEWER fire has taken — 1 is all of it. */
	float ShareOfItsOwnGround = 1.0f;

	/** True once the fire has been laid — the ground is only ever laid once. */
	bool bTheFireIsLaid = false;

	/** True once the clock is out: the circle is dead and only the fire is still burning down. */
	bool bTheClockIsOut = false;

	/** [TUNE] MINE — how often the circle is asked who is standing in it. Damage-free, so this is only
	 *  a cost knob: nothing about who gets lit depends on it. */
	static constexpr float CatchEverySeconds = 0.2f;

	/**
	 * [TUNE] MINE — the ceiling on fire in the WHOLE world, in flames a second, across every fire alive.
	 *
	 * Past it a new fire is laid THINNER, never removed and never weakened: the circle, the Burn and the
	 * clock are the same. A thinned field still reads as lit; fire taken away leaves bare ground that
	 * still burns you, which is the one thing he called out as looking broken.
	 *
	 * It is a BACKSTOP and not the thing that does the work: fires that overlap do not stack any more,
	 * because a newer fire owns the ground it covers (see ReReadHowMuchGroundItStillOwns), so a whole
	 * bombardment landing on one patch asks for one patch's worth of fire. This only catches a world
	 * burning in several places at once.
	 */
	static constexpr float MostFlamesASecondInTheWholeWorld = 24000.0f;

	/** [TUNE] MINE — the least a fire ever asks for, so even a world at its ceiling still shows fire
	 *  where the ground is burning. */
	static constexpr float FewestFlamesASecondAFireMayAsk = 150.0f;

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
