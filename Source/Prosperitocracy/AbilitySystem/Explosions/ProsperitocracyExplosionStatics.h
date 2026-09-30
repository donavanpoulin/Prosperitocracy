// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ProsperitocracyExplosionStatics.generated.h"

class AActor;
class IProsperitocracyAbilitySourceInterface;
class UGameplayEffect;
class UPrimitiveComponent;
class UProsperitocracyStatTable;
class UWorld;

/**
 * The numbers and the two rules every explosion in the game shares, in ONE block
 * (Design/explosions.md).
 *
 * Universal means universal: these are constants of the pass, not per-explosive config. An explosion
 * carries its OWN Range, Impact Damage, Pen and statuses on its block, and nothing else — the shape of
 * what happens around those numbers is here, once, for every explosive in the game.
 */
namespace ProsperitocracyExplosionHandling
{
	/**
	 * Where both of an explosion's ramps START, halfway out along what they run over.
	 *
	 * The DAMAGE: full from the middle to halfway out to the ball's edge, then 100% → 0% at the edge
	 * (Design/explosions.md). The SHAKE and the SOUND: the same shape over the shake's own, wider range
	 * ("50%, then 100% → 0% to the end of that range").
	 *
	 * One constant because it is one rule — the two ramps are the same shape over different reaches.
	 */
	constexpr float FalloffStartsHalfwayOut = 0.5f;

	/**
	 * [TUNE] How far an explosion's SHAKE reaches, as a multiple of the thing's own FINAL Range.
	 *
	 * **10 — MINE, and HIS to move.** He called the DIRECTION (the shake's reach does the work, not its
	 * strength) and not a number: any crank of this is his to say.
	 *
	 * What it is FOR: a blast's damage zone against its FELT zone is about twenty to one in the world (a
	 * grenade kills inside ~5 m and is felt ~100 m out), and the ball reaches 4 m, so 80 m — whose
	 * full-strength half, 40 m, covers everywhere a called strike actually lands, 20 to 40 m from the man who
	 * called it.
	 *
	 * A bigger number is a longer tail, never a harder hit: the strength is the thing's own Shake row and
	 * does not move with this. **Reach is also not what makes a blast FELT** — the lean's DECAY is
	 * (Design/ui.md): a bigger reach only means more of the map leans.
	 *
	 * THE SOUND WILL USE THIS SAME NUMBER AND THE SAME RAMP when it exists — it reaches the same range, as
	 * volume — because it is the same bang read two ways, which is why the constant is named for both.
	 */
	constexpr float ShakeAndSoundRangeMultiplier = 10.0f;

	/**
	 * [TUNE] The shove, in cm/s per point of damage that ACTUALLY CAME OFF the body.
	 *
	 * His rule (2026-09-30): the blast "goes through the pipeline with the dmg number of the source…
	 * including falloff", and the shove reuses it — so the shove's strength IS the damage that landed,
	 * with the gate and the falloff already inside it, turned into a speed. The outer ball does less
	 * damage and therefore blasts you less far, by construction rather than by a second falloff.
	 *
	 * For scale: a man runs at 600 cm/s, so a 100-damage blast at this number shoves at 1200.
	 */
	constexpr float ShoveSpeedPerDamagePoint = 12.0f;

	/**
	 * [TUNE] The ceiling on the shove, in cm/s — so a very powerful explosion cannot send you to space.
	 *
	 * Nothing to do with the falloff (his correction, 2026-09-30): the falloff is what makes the OUTER
	 * ball shove less, this is what stops the INNER ball from launching you out of the level.
	 */
	constexpr float MaxShoveSpeedCmS = 2000.0f;

	/**
	 * WHERE A THING'S DAMAGE STARTS FALLING OFF, given its own FINAL rows — the ONE door both a gun and
	 * an explosion reach the universal falloff through.
	 *
	 * A gun: its own Falloff row is where its ramp starts, and its Range row is where the damage hits 0.
	 * An explosion: Range is the WIDTH of its ball, so the damage hits 0 at HALF of it, and the ramp
	 * starts halfway out to there. An explosive authors no Falloff number at all — the rule IS the number.
	 *
	 * Read LIVE, per hit, off the final rows the caller hands in: that is what makes a Range perk move an
	 * explosion's ball AND its ramp together, with no derived number stored anywhere to go stale.
	 */
	float ComputeAttenuation(const UProsperitocracyStatTable* Block, float DistanceCm, float RangeFinalMeters, float FalloffFinalMeters);

	/** The shake's own ramp over its own, wider range: the same shape, the same one formula. */
	float ComputeShakeAttenuation(float DistanceCm, float ShakeRangeMeters);
}

/**
 * ONE THING THE BALL CAUGHT — a body, the part of it the ball touched first, and where that was.
 *
 * The ball is a sphere at a place, so there is no bullet hole and no impact point on the body: the
 * nearest point on the body to the middle is the closest thing to "the bit the blast caught", and it is
 * what gives the blast's readers their numbers — the DISTANCE the one falloff formula is measured at,
 * the POINT a damage number comes up at, and the DIRECTION the body is shoved along (off the ball's
 * surface at the target, never flatly out from the middle, Design/explosions.md).
 *
 * One entry per BODY, never one per part: a body is many parts, and a blast catches the body once.
 */
struct FProsperitocracyExplosionTarget
{
	/** The body the ball caught. */
	AActor* Actor = nullptr;

	/** The part of it the ball reached first — the nearest one to the middle. */
	UPrimitiveComponent* Part = nullptr;

	/** Where the ball touched it, off the nearest point on that part's collision to the middle. */
	FVector Point = FVector::ZeroVector;

	/** The direction it was HIT from: from the middle out to that point, unit length. */
	FVector Direction = FVector::ZeroVector;

	/** Middle to that point, in cm — the distance the one falloff formula is measured at. */
	float DistanceCm = 0.0f;
};

/**
 * UProsperitocracyExplosionStatics
 *
 * THE ONE EXPLOSION PASS (Design/explosions.md): a grenade, a rocket, a launcher, a mech stomp, a barrel
 * and an ability's blast all arrive here, and NOTHING is set up per explosive.
 *
 * The whole hook-up is a MARK: the `Explosive` tag on the thing's stat block (UProsperitocracyStatTable,
 * beside its FireMode and Slot tags). A thing wearing it is an explosion; a thing without it is not, and
 * the pass says so out loud rather than half-running.
 *
 * WHAT THE THING OWNS, and what this pass never decides for it:
 *   - its Range (which is the ball's WIDTH),
 *   - its Impact Damage and its Pen — its damage travels the ONE pipeline, falloff and all, exactly as a
 *     gun's does,
 *   - the statuses it sets, and the block that carries their numbers,
 *   - WHEN it goes off. The thing owns its own trigger (a projectile's impact, a barrel's death) and
 *     hands the bang to this door; the pass is only ever the bang.
 */
UCLASS()
class UProsperitocracyExplosionStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * SET ONE OFF, at a place.
	 *
	 * Explosive — the thing that went off: its block says whether it is one at all and which statuses it
	 * sets, and its own GAS home answers its Range, its damage lines, its falloff and its own Shake.
	 * Instigator — whose bang it is: the damage is credited to him (contested health, the readout, the
	 * kill) and the shake belongs on his screen.
	 * Place — where the middle of the ball is (the point of impact, the barrel's own origin).
	 * DamageEffectClass — the effect the ONE damage pipeline runs on. It is owned by the thing that
	 * carries the explosion (a gun's own effect is handed over the same way), never held here.
	 */
	static void DetonateExplosion(IProsperitocracyAbilitySourceInterface* Explosive, AActor* Instigator, const FVector& Place, TSubclassOf<UGameplayEffect> DamageEffectClass);

	/**
	 * THE BALL (Design/explosions.md): everything within the thing's Range of the middle, once per body,
	 * with the nearest part to the middle standing for the body.
	 *
	 * The shape is a sphere and the sphere's radius comes off the thing's Range — the same row a gun's
	 * range lives on, and the same door a swing's own shape is gathered through (an overlap, once, with
	 * the one difference that this one is a ball rather than a box).
	 *
	 * The thing that went off is never caught by its own blast (a grenade is not a target of the
	 * grenade); everything ELSE in the ball is, the man who called it in included — an explosion is
	 * friendly fire like everything else in this game.
	 */
	static void GatherTheBall(UWorld* World, const FVector& Place, float RadiusCm, const AActor* TheExplosiveItself, TArray<FProsperitocracyExplosionTarget>& OutCaught);
};
