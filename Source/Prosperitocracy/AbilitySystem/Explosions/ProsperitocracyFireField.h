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
 * ONE FLAME ON THE GROUND — a PICTURE at a place, with its own moment.
 *
 * The picture is the ground's, not a fire's: a fire landing on ground that is already burning ADOPTS the
 * flame standing there rather than making its own, so a flame outlives the fire that lit it and changes
 * hands without ever being destroyed, re-created or resized (PLANS/incendiary-strike.md §4).
 */
USTRUCT()
struct FProsperitocracyFlame
{
	GENERATED_BODY()

	/** The picture burning here. Nothing reads it — the circle is what catches a body. */
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Picture = nullptr;

	/** World seconds at which this one is told to stop. */
	float GoesOutAt = 0.0f;

	/** True once it has been told. It then burns out on its own, and nothing else is done to it. */
	bool bToldToStop = false;
};

/**
 * THE FIRE A BANG LEAVES BURNING ON THE GROUND (PLANS/incendiary-strike.md §4).
 *
 * It is spawned by the ONE explosion pass, at the place the bang went off, and the only thing it is
 * told is WHICH fire it is (the explosive's block names it). Everything a fire does is read off that
 * one block: how wide it burns, how long it burns, what it puts on what it catches, what it looks like.
 *
 * WHAT IT IS, in the order it happens:
 *   - THE CIRCLE IS THE FIRE. Its own Range row is the circle a body has to be standing in, read as a
 *     WIDTH like every other Range here. Nothing reads the flames: "in it" is the circle, never the
 *     mesh, the particles or the flames.
 *   - IT CATCHES ANYTHING STANDING IN IT — hit or not, no gate, no pen question, and it deals no damage
 *     of its own. The only thing it does to a body is apply the Burn the block names, and it applies it
 *     again for as long as the body stands there. Walk out and the Burn's clock runs down on its own.
 *   - IT GOES OUT AS ONE CLOCK. Its own Duration; the fade lives INSIDE that Duration and the ground
 *     keeps catching people the whole way through it.
 *   - IT NEVER REPLACES A FLAME. A newer fire takes over the RULE of the ground it covers — the clock
 *     and the Burn — and adopts the pictures already standing there; it only lays its own where the
 *     ground is empty. So an overlap is invisible: nothing pops, nothing flickers, nothing is re-placed.
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
	 * FireBlock — which fire this is: its circle, its clock, its Burn and its flames all come off that
	 * one block, read FINAL through the fire's own GAS home.
	 * TheOneWhoLeftIt — whose fire it is: the Burn's damage is dealt and credited as his, so a fire that
	 * kills contests health and shows a number exactly as his own shot would.
	 * InDamageEffectClass — the effect the ONE damage pipeline runs on, carried so a Burn's tick is a
	 * normal damage line and never a second way to take health off someone.
	 */
	void LightTheGround(UProsperitocracyFireStatTable* InFireBlock, AActor* TheOneWhoLeftIt,
		TSubclassOf<UGameplayEffect> InDamageEffectClass);

	virtual void Tick(float DeltaSeconds) override;

	/** How many flames are burning in the whole world right now — what a new fire reads before it lays its own. */
	static int32 FlamesBurningInTheWorld();

	/** The circle this fire catches in, in cm. Read by a newer fire deciding what to take over. */
	float GetRadiusCm() const { return RadiusCm; }

	/**
	 * HAND THE GROUND OVER — called by a NEWER fire on every fire already burning.
	 *
	 * Every flame of this one standing inside that circle changes hands: the PICTURE stays exactly where
	 * it is, burning, and only its rule changes — it is now the newer fire's, on the newer fire's clock.
	 * Nothing is destroyed and nothing is re-created, so an overlap can never pop or flicker.
	 *
	 * Where each handed-over flame stood is written out, because the newer fire must not lay its own
	 * flame on ground that is already burning.
	 */
	void HandOverTheFlamesTo(AProsperitocracyFireField* TheNewerFire, const FVector& TheNewCircleCentre,
		float TheNewCircleRadiusCm, TArray<FVector>& OutWhereTheyStood);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Lay the flames: an even spread over the circle, one to a cell, alternating the block's looks, and
	 * NEVER on ground that is already burning.
	 */
	void LayTheFlames(const TArray<FVector>& AlreadyBurning);

	/** Put the Burn on everything standing in the circle — again, every beat, for the whole Duration. */
	void CatchWhateverStandsInIt();

	/** Tell the flames whose moments have come to STOP — and then do nothing else to them. */
	void LetTheFlamesGoOut(float Now);

	/** The clock is out: nothing catches anything here any more, and the last flames burn out on their own. */
	void TheClockIsOut();

	/** A flame has finished burning out: its picture is done and goes. */
	UFUNCTION()
	void AFlameHasBurnedOut(UNiagaraComponent* Finished);

	/** This fire's own rows, read FINAL through its own GAS home — the same read every stat takes. */
	float GetMyStat(EProsperitocracyStat Stat) const;

	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Fire")
	TObjectPtr<USceneComponent> TheRoot;

	/** The flames burning in THIS fire, each with its own moment. */
	UPROPERTY()
	TArray<FProsperitocracyFlame> Flames;

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

	/** World seconds the ground next catches at. */
	float NextCatchesAt = 0.0f;

	/** True once the flames have been laid — the ground is only ever laid once, adoption included. */
	bool bFlamesLaid = false;

	/** True once the clock is out: the circle is dead and only the last flames are still burning down. */
	bool bTheClockIsOut = false;

	/** [TUNE] MINE — how often the circle is asked who is standing in it. Damage-free, so this is only
	 *  a cost knob: nothing about who gets lit depends on it. */
	static constexpr float CatchEverySeconds = 0.2f;

	/**
	 * [TUNE] MINE — the ceiling on flames burning in the WHOLE world, across every fire alive.
	 *
	 * Past it a new fire is laid THINNER, never removed and never weakened: the circle, the Burn and the
	 * clock are the same. A thinned field still reads as lit; fire taken away leaves bare ground that
	 * still burns you, which is the one thing he called out as looking broken.
	 */
	static constexpr int32 MostFlamesInTheWholeWorld = 600;

	/** [TUNE] MINE — the fewest flames a fire is ever laid with, so even a world at its ceiling still
	 *  shows fire where the ground is burning. */
	static constexpr int32 FewestFlamesAFireMayLay = 8;

	/**
	 * [TUNE] MINE — the longest a fire may outlive its own clock, in seconds. A flame that has been told
	 * to stop needs its own particles to finish, and that is the look; this is only the guarantee that a
	 * picture which never reports an end cannot leave a dead fire standing in the world forever.
	 */
	static constexpr float LongestAFireMayOutliveItsClockSeconds = 15.0f;

	/** Every fire burning in the world, so a new one can thin itself and can take ground over. */
	static TArray<TWeakObjectPtr<AProsperitocracyFireField>> EveryFireBurning;
};
