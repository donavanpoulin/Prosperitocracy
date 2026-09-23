// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Stats/ProsperitocracyStat.h"
#include "Weapons/ProsperitocracyLoadout.h"

#include "ProsperitocracyWeapon.generated.h"

class AProsperitocracyCharacter;
class AProsperitocracyStatHostActor;
class APawn;
class UGameplayEffect;
class USkeletalMeshComponent;
class UProsperitocracyGameplayAbility;
class UProsperitocracyLoadoutComponent;
class UProsperitocracyStatTable;
struct FProsperitocracyDamageLine;
struct FProsperitocracyWeaponAmmo;

/**
 * The weapon-feel numbers: every constant the aim circle is built from.
 *
 * These are HARD RULES of the design (Design/ui.md, Design/combat.md) and they are universal — the
 * same value for every weapon, always. Nothing here is per-gun config: a gun's feel is exactly three
 * of its own STATS (Accuracy = the per-shot spread, Weight = the sway, Recoil = the up-climb) and
 * these constants are only the shape those stats are poured into. Changing one changes every weapon
 * at once, which is the point. All values [TUNE].
 */
namespace ProsperitocracyWeaponHandling
{
	// Hard cap, so the circle (trail + spread + climb) can never fly off the screen.
	constexpr float MaxDriftDegrees = 7.5f;

	// --- Posture multipliers: fixed for everybody, and they scale Accuracy only. ---
	constexpr float PostureMultiplier_Aiming = 1.4f;            // aiming down sights steadies
	constexpr float PostureMultiplier_StandingStill = 1.2f;     // standing still steadies
	constexpr float PostureMultiplier_Crouching = 1.35f;        // crouching steadies more
	constexpr float PostureMultiplier_JumpingOrFalling = 0.65f; // airborne is sloppier

	// How fast a posture change takes hold (per second), so none of them snaps.
	constexpr float TransitionRate_Posture = 5.0f;

	// At or below this speed (cm/s) the character counts as standing still; the bonus fades over range.
	constexpr float StandingStillSpeedThreshold = 80.0f;
	constexpr float StandingStillToMovingSpeedRange = 20.0f;

	// --- Handling trail (Weight-driven): the circle trails a camera swing. ---
	// Degrees of trail per degree of swing, and the return-to-centre rate per second.
	constexpr float LagTimeBase = 0.012f;       // light weapon: a short trail
	constexpr float LagTimePerWeight = 0.003f;  // heavier: a longer one
	constexpr float ReturnRateBase = 8.0f;      // light weapon: settles fast
	constexpr float ReturnRatePerWeight = 0.7f; // heavier: settles slower
	constexpr float ReturnRateMin = 2.0f;

	// --- Per-shot spread (Accuracy-driven). ---
	// Degrees of random displacement per shot at EffectiveAccuracy == 1.0.
	constexpr float SpreadShoveBaseDegrees = 1.2f;

	// --- Movement trail (Weight-driven): the aim is an inert mass and lags its own motion. ---
	// Degrees per second of displacement per (cm/s) of motion, and how much of a forward/back shift
	// shows vertically — a fraction of the sidestep, which is what makes the oval horizontal.
	constexpr float MoveDisplaceBase = 0.0065f;
	constexpr float MoveDisplacePerWeight = 0.0015f;
	constexpr float MoveVerticalFraction = 0.35f;

	// The gun's MELEE is not here any more: the bash is an ability with its own stat block
	// (UProsperitocracyGameplayAbility_Bash), so its reach is the Range stat and its damage is that
	// ability's own Impact Damage. A gun no longer holds a single melee number or a single melee
	// constant — its Weight is all a gun contributes to the bash.
}

/**
 * The STANCE a weapon asks the body to hold while it is out.
 *
 * A rifle is held like a rifle and a pistol like a pistol wherever they sit, and a melee asks for
 * nothing at all — so this belongs to the WEAPON. It used to be the hand's: the equip chain set
 * "is a rifle equipped?" for whichever hand it was bringing out, which is why a sword came out
 * holding a rifle's stance.
 *
 * The numbers ARE the animation's own enum values, deliberately, so the value crosses into the
 * blueprint that holds what the animation reads without a translation table in between. The one
 * coupling to remember: this enum and the animation's `Animation_State` must agree by value.
 */
UENUM(BlueprintType)
enum class EProsperitocracyStance : uint8
{
	/** Nothing of its own — the body goes on running the anims it was already running. A melee. */
	None = 0		UMETA(DisplayName = "None"),
	Unarmed = 4		UMETA(DisplayName = "Unarmed"),
	Pistol = 5		UMETA(DisplayName = "Pistol"),
	Rifle = 6		UMETA(DisplayName = "Rifle"),
};

/**
 * AProsperitocracyWeapon
 *
 * The NUMBERS and the DAMAGE of one gun. Nothing visual.
 *
 * The template's own gun blueprints are the gun: BP_MasterWeapon, and BP_Pistol / BP_Rifle under it,
 * already carry the mesh, the Muzzle socket, the fire and reload anims, the character montages, the
 * sounds with their attenuation and concurrency, the muzzle flash, the tracer, the decal and the
 * impact particle — and their EventFire/EventReload already trace and play all of it. This class is
 * what those blueprints inherit, so that the SAME shot:
 *
 *   (a) spends a round out of OUR magazine instead of their Weapon_Details ammo struct, and
 *   (b) deals OUR damage on the hit they already trace.
 *
 * (b) is the whole missing half: their EventFire traced, drew FX and stopped — there is no
 * ApplyDamage anywhere in it. That is why their guns deal nothing today.
 *
 * The gun's numbers are not here either. They are its stat block (UProsperitocracyStatTable),
 * resolved through this gun's own GAS home (AProsperitocracyStatHostActor) by the ONE evaluator:
 * MagSize, Capacity, Rate, PiercingDamage, Penetration, Range, Falloff, Accuracy, Recoil, Weight.
 *
 * There is deliberately NO body asset: the gun blueprint IS the body, so a second asset pointing at
 * the same mesh, anims and sounds would be a competing source for one thing.
 */
UCLASS(BlueprintType)
class AProsperitocracyWeapon : public AActor
{
	GENERATED_BODY()

public:
	AProsperitocracyWeapon();

	/**
	 * Take the gun's place in the world: which slot it is, its numbers, what its shot applies, and
	 * where its ammo lives.
	 *
	 * Called by the owner's loadout component, which is the only thing that can say which slot this
	 * gun is — the rig that created it never does. All of it arrives together, because it is one
	 * decision. Callable again whenever the loadout changes what this body carries (a body can be two
	 * guns: the pistol's body is also the SMG's).
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool ApplyLoadoutEntry(const FGameplayTag& InSlot, UProsperitocracyStatTable* InStatBlock, TSubclassOf<UGameplayEffect> InDamageEffectClass, UProsperitocracyLoadoutComponent* InOwnerLoadout, APawn* InOwningPawn = nullptr);

	/**
	 * This thing's PRIMARY ACTION — what a press means when this is the weapon in the hand.
	 *
	 * THE PRESS BELONGS TO THE WEAPON, and never to the character's graph. The input asks the thing
	 * in the hand to do its own job and then forgets about it: a gun runs its own fire graph, a melee
	 * runs its own swing, and neither the graph nor the body knows which it is holding.
	 *
	 * That is why this is declared HERE, on the base every weapon shares, and not left as a custom
	 * event on one gun's blueprint: a graph that has to cast the hand to a specific weapon before it
	 * can do anything has to be told what every weapon in the game is — which is exactly how a sword
	 * came to be unusable unless it was dressed up as a rifle.
	 *
	 * BlueprintNativeEvent because both kinds of implementation are real: the gun's is a blueprint
	 * graph it already has, and a melee's is code. The C++ default does nothing, which is the honest
	 * answer for a thing with no primary action.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void PrimaryAction();
	virtual void PrimaryAction_Implementation();

	/**
	 * This thing's RELOAD action, on the same terms as the primary one.
	 *
	 * A gun swaps a magazine; a melee has no reload at all and its default says so by doing nothing.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void ReloadAction();
	virtual void ReloadAction_Implementation();

	/**
	 * This thing's SECONDARY action — what the right mouse button means when this is the weapon in the
	 * hand. Declared here for the same reason the primary is: the input asks the thing in the hand to
	 * do its own job and never learns what it is holding.
	 *
	 * **The default answer is AIMING, because aiming is what a GUN's second button does** — a firearm
	 * is aimed by holding the right button, and the body's camera, crosshair and pose are driven off
	 * the state this sets. A weapon with NO FIRE MODE is not a gun and gets no default here: a melee
	 * answers this for itself (the blade runs its own ability), and a thing that answers nothing does
	 * nothing.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void SecondaryAction();
	virtual void SecondaryAction_Implementation();

	/**
	 * The button coming back up, on the same terms — because a press is not the whole of a secondary
	 * action. AIMING IS A HOLD: the press starts it and this stops it. A melee owes nothing here, and
	 * that is why this has an honest empty default rather than being folded into the press.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void SecondaryActionReleased();
	virtual void SecondaryActionReleased_Implementation();

	/**
	 * Whether this weapon is being AIMED right now.
	 *
	 * The weapon OWNS the intent — it is the thing the player is holding, and it is the only thing
	 * that knows whether right-click does anything for it — and the BODY owns the mechanism: the aim
	 * blend, the camera, the crosshair and the pose all read this one value. One writer, one reader,
	 * exactly like a weapon's stance and its draw.
	 *
	 * A melee never sets it: aiming is not a thing a sword does.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	bool IsAiming() const { return bAiming; }

	/**
	 * Whether this thing is a GUN — a weapon carrying a fire-mode tag (Design/weapons.md: a gun is a
	 * weapon with a fire mode, a melee is a weapon without one). It is the project's own test, read
	 * off the thing's block, and it is what decides whether the secondary press has a default meaning
	 * at all.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	bool HasAFireMode() const;

	/**
	 * Whether the right mouse button is DOWN on this thing right now.
	 *
	 * The press and the release both land on the weapon in the hand (`SecondaryAction` /
	 * `SecondaryActionReleased`), so this — and nothing in the input graph — is the one holder of what
	 * a HOLD is. A move that is held rather than pressed asks its own weapon this every step, which is
	 * why the heavy combo needs no event of its own to be told the button came up: the thing it was
	 * pressed on already knows, and answers.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	bool IsSecondaryHeld() const { return bSecondaryHeld; }

	/**
	 * The thing in hand's OWN move on the right button, dressed onto it by its carrier's loadout.
	 *
	 * A SLOT, not a move, and the weapon never learns what is in it: the loadout grants its four
	 * abilities and hands the one that names this weapon's slot to the weapon, so a loadout is what
	 * decides what the right button does — the dash today, the heavy combo on a loadout that takes it,
	 * a sword throw tomorrow — and a weapon is never edited to change it.
	 *
	 * It is DRESSED rather than authored on the weapon blueprint, for the same reason a gun's numbers
	 * are: an authored value is a second copy of an answer that has an owner, and a copy goes stale the
	 * moment the owner changes. Null is a real answer — a weapon carries no second move unless a loadout
	 * gives it one, and then its right button falls back to what it is by itself (a gun's aim).
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	TSubclassOf<UProsperitocracyGameplayAbility> GetSecondPressAbility() const { return SecondPressAbility; }

	/** Hand this thing the ability its right button runs. Called by the loadout that dressed it. */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void SetSecondPressAbility(TSubclassOf<UProsperitocracyGameplayAbility> InAbility);

	/**
	 * Another move is about to take the stage on this weapon's owner: stand down.
	 *
	 * A body wears ONE move at a time, so whatever this weapon had running has to end when something
	 * else starts — a blade's combo when an ability takes over, and nothing at all for a gun, which
	 * holds no move of its own. It is declared HERE so the thing that is starting does not have to
	 * know WHAT it is taking over from: it asks the thing in the hand, exactly as every other door in
	 * the weapon does.
	 */
	virtual void StandDownForANewMove();

	/**
	 * Pay BLOOD for one use of this thing.
	 *
	 * True when it was paid and the use may happen. A thing with no blood owes nothing and answers true
	 * without doing anything, which is what lets a weapon's own ability ask the thing in the hand to
	 * pay for it WITHOUT knowing what it is holding: the blade takes it out of its pool (and says no
	 * when the pool cannot cover it), and a gun never touches blood at all.
	 */
	virtual bool SpendBlood(float Cost);

	/**
	 * This thing's MELEE action — what the melee key means when this is the weapon in the hand.
	 *
	 * **The default answer is the BASH, because a bash is a GUN's melee** — the hit you get with a
	 * firearm in your hands, which is the template's own key and the ability every gun already runs.
	 * A weapon with NO fire mode is not a gun and gets no default: a blade answers this for itself (the
	 * blood-mode toggle), and a thing that answers nothing does nothing.
	 *
	 * Declared here for the same reason as every other door: the key asks the thing in the hand, and
	 * the very same key can therefore mean two things without anything branching on what is held.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void MeleeAction();
	virtual void MeleeAction_Implementation();

	/**
	 * Whether this thing comes out with a DRAW of its own and a STANCE of its own.
	 *
	 * A gun says YES: it has an equip animation and a sound, and while it is out the body holds the
	 * stance that goes with it — so the rig plays its draw and tells the body which stance it is.
	 *
	 * A melee says NO: it is simply in the hand. It appears there, it follows the hand, and the body
	 * keeps the anims it already runs with. There is no draw to play and no stance to take.
	 *
	 * The WEAPON states this about itself, and that is the whole point of it living here: neither the
	 * rig, nor the input, nor the body has to know which of the two it is holding — the rig asks the
	 * thing it is bringing out, and does what it is told. Without this, the hand a weapon sits in is
	 * what decided its draw and its stance, which is exactly how a sword came to be a rifle.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Weapon")
	bool bDrawnWithItsOwnAnimation = true;

	/**
	 * What the body should hold while this thing is out.
	 *
	 * Named by the WEAPON, for the same reason as everything else here: a rifle asks for a rifle's
	 * hold and a pistol a pistol's, and a melee asks for NOTHING — the body keeps the anims it was
	 * already running. It cannot depend on which hand the thing came out of, which is exactly what it
	 * used to do.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Weapon")
	EProsperitocracyStance Stance = EProsperitocracyStance::None;

	/**
	 * The draw THIS weapon plays as it comes out of its socket and into the hand.
	 *
	 * Empty is a real answer, and for a melee it is the true one: there is no draw, because there is
	 * nothing to draw — a sword is simply in the hand. The character's key press used to name a
	 * montage for the hand it was bringing out, which is why every weapon after the first came out to
	 * the first one's animation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Weapon")
	TObjectPtr<UAnimMontage> DrawMontage;

	/**
	 * The animation THIS weapon plays as it goes away again.
	 *
	 * The other half of the draw, and the weapon's answer for the same reason: a gun has a holster of
	 * its own, and a melee has none — it simply leaves the hand. Empty is a real answer.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Weapon")
	TObjectPtr<UAnimMontage> StowMontage;

	/**
	 * Where this weapon sits while it is OUT, and where it sits while it is away.
	 *
	 * Both are the WEAPON's own answers, because a socket is part of what a weapon is: a rifle is
	 * held where a rifle is held and a pistol where a pistol is. While these lived in the character's
	 * key press, they were the first weapon's sockets, worn by everything that came after it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Weapon")
	FName HandSocket;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Weapon")
	FName AwaySocket;

	/**
	 * Which slot of its carrier's loadout this weapon fills.
	 *
	 * Handed over by the loadout when it was dressed, and empty until then. It is how a weapon in the
	 * world says where it belongs without anyone holding a list of which component is which hand.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	FGameplayTag GetSlotTag() const { return Slot; }

	/**
	 * Make sure this gun has its numbers, and return whether it does.
	 *
	 * Called on BeginPlay AND on first use, because a gun spawned by a child actor component is NOT in
	 * the rig yet when BeginPlay runs — a child actor is attached after it spawns, so at that moment
	 * there is no attach parent and no owner to ask about the loadout. By the time the gun is fired or
	 * reloaded it is in the rig, so the same call succeeds. That is why this is a door and not a
	 * one-shot BeginPlay step, and why the gun never initializes from a guess.
	 */
	bool EnsureInitialized();

	//~ The two things their gun graph asks us, plus the damage it hands us -------------------------

	/**
	 * One committed shot's worth of ammo. True when a round actually left the magazine, so the gun's
	 * own graph runs its shot; false when the magazine is empty (the graph's dry-fire branch) or the
	 * Rate cadence has not elapsed (a silent refusal — not a dry fire).
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool TryConsumeRound();

	/** True when the magazine is empty — what decides their dry-fire branch, not a refused cadence. */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool IsMagazineEmpty() const;

	/**
	 * Reload = a MAG SWAP, not a top-up: whatever is left in the magazine is wasted, never returned
	 * to the pool, and a fresh magazine comes out of the pool — partial if that is all there is.
	 * False when there is no spare magazine to load.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool ReloadFromStats();

	/**
	 * OUR damage, on the hit the gun's own trace produced. The effect context carries that hit (the
	 * impact point for falloff, the part hit for its armor and resists) and points at this gun's stat
	 * host as the ABILITY SOURCE, which is where UProsperitocracyDamageExecution reads the damage
	 * lines and the falloff from.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void ApplyShotDamage(const FHitResult& Hit);

	//~ Reads -------------------------------------------------------------------------------------

	/** The gun's numbers — its row in the universal stat table. A gun IS its stat block. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	UProsperitocracyStatTable* GetStatBlock() const { return StatBlockAsset; }

	/**
	 * Which slot of its owner's loadout this gun came out of — told to it by the loadout, never
	 * looked up by the gun. Invalid until it is dressed.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	FGameplayTag GetSlot() const { return Slot; }

	/** What the loadout answered when this gun asked to be dressed. Dressed once it has numbers. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	EProsperitocracyWeaponDressResult GetDressResult() const { return DressResult; }

	AProsperitocracyStatHostActor* GetStatHost() const { return StatHost; }

	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const;

	/** FINAL value of one of this gun's stats, through the ONE evaluator (its own GAS home). */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	float GetWeaponStat(EProsperitocracyStat Stat) const;

	/**
	 * The one damage effect this gun's damage travels.
	 *
	 * It is owned by the loadout (one asset for every gun, never a copy per body) and handed over when
	 * the gun is dressed. Anything else that deals THIS gun's damage asks for it here rather than
	 * holding a second copy of the same effect — the bash does exactly that, because a bash is gun
	 * damage and there is one damage pipeline.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	TSubclassOf<UGameplayEffect> GetDamageEffectClass() const { return ShotDamageEffectClass; }

	/** True when the stat block carries the FullAuto fire-mode tag. A gun without a mode is not fired. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	bool IsFullAuto() const;

	/** Seconds between shots = 1 / Rate. The single cadence, for every gun. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	float GetSecondsBetweenShots() const;

	/** Rounds in the magazine right now. Read from the owner's ammo store for this gun's slot. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	int32 GetMagazineAmmo() const;

	/** Rounds in this slot's spare magazines. Read from the owner's ammo store. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	int32 GetSpareAmmo() const;

	/**
	 * The aim circle's current offset from the dot, in degrees: X = yaw (+ = right), Y = pitch (+ = up).
	 *
	 * This one value has three readers — the reticle circle, the bullet, and the character's aim pose —
	 * which is what makes them one aim instead of three (Design/combat.md). Clamped on the way out, so
	 * no reader can ever be handed a circle that has left the screen.
	 */
	FVector2D GetAimDriftDegrees() const
	{
		constexpr float MaxDrift = ProsperitocracyWeaponHandling::MaxDriftDegrees;
		return FVector2D(
			FMath::Clamp(AimDriftDegrees.X, -MaxDrift, MaxDrift),
			FMath::Clamp(AimDriftDegrees.Y, -MaxDrift, MaxDrift));
	}

	/**
	 * The direction the next bullet flies along: the camera's aim plus this gun's drift.
	 *
	 * The shot reads this instead of the raw camera forward, which is what makes the bullet land where
	 * the reticle circle sits — and the circle is projected along this very vector, so the two cannot
	 * drift apart.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	FVector GetShotDirection() const;

	/**
	 * One committed shot's worth of feel: the recoil climb, then the per-shot spread shove.
	 *
	 * Called once per shot that actually leaves the gun, AFTER that shot has been traced — so a shot is
	 * never deflected by its own kick. The bullet goes where the circle was when the trigger broke, and
	 * the climb and shove move the circle for the next one (Design/ui.md: every bullet lands exactly
	 * where the circle points at that instant).
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void ApplyShotFeel();

	//~ The gun's melee is an ability, not a gun function -------------------------------------------
	//
	// A gun's bash used to live here (MeleeAttack / ApplyMeleeDamage and its reach constants). It is
	// now UProsperitocracyGameplayAbility_Bash — an ability with its own stat block, whose Range stat
	// is how far the swing reaches and whose Impact Damage is the bash's own number. A gun contributes
	// its Weight to that ability and nothing else, so there is one place a bash can come from.

	/** (base Accuracy stat) x (the combined posture multiplier) — the driver of the per-shot shove. */
	float GetEffectiveAccuracy() const { return GetAccuracy() * CurrentAccuracyMultiplier; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Whether the RIG says this thing is the one being held — asked of the body, never assumed.
	 *
	 * A READ, and deliberately the only thing this class knows about where it sits. Where a weapon
	 * SITS belongs to the rig: the character blueprint's own equip path is what moves a channel
	 * between the hand and the body, socket by socket, and it is what animates and sounds the move.
	 * A weapon that moved its own channel would be a second writer of the same socket — which is
	 * exactly how the pistol came to be un-equippable — so this class asks where it is and never
	 * moves itself.
	 */
	bool IsHeldInRig() const;

	/**
	 * Which slot of its owner's loadout this gun is. Handed over by the loadout, along with the
	 * numbers — a gun cannot read it off itself, and it must not guess it from its blueprint, because
	 * one body can be two guns (the pistol's body is also the SMG's).
	 */
	UPROPERTY(Transient)
	FGameplayTag Slot;

	/**
	 * The numbers, handed over by the loadout when this gun came up.
	 *
	 * Deliberately NOT a default on the gun blueprint. A default is a second copy of an answer that
	 * has an owner, and the template's child actor components keep an archetype of the gun inside the
	 * character asset — so a defaulted value gets copied there and then goes stale. The gun asks the
	 * loadout what it is; nowhere on the gun can a stale number live.
	 *
	 * Named `StatBlockAsset` rather than the obvious `StatBlock` on purpose: `BP_Pistol` and
	 * `BP_Rifle` still had values stored under the old name from before this was Transient, and
	 * renaming the property is what makes UE drop those stored keys on the next save. Same for
	 * `ShotDamageEffectClass` below.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyStatTable> StatBlockAsset;

	/**
	 * What a committed hit — a shot or a melee — applies to whatever it hits, also from the loadout
	 * — one asset for every gun, for the same reason. Its execution must be
	 * UProsperitocracyDamageExecution: that reads the damage lines and the falloff from the shot's
	 * ability source, which is this gun's stat host.
	 */
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ShotDamageEffectClass;

	/** The gun's GAS home — its stats are attributes on this actor's own ASC (the ONE evaluator). */
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Weapon")
	TObjectPtr<AProsperitocracyStatHostActor> StatHost;

	/**
	 * The owner's loadout, which is where this gun's AMMO lives — one store per slot, on the carrier.
	 *
	 * Nothing about this gun's ammo is kept on the gun: the rig re-creates gun actors, and a magazine
	 * that lived here would come back full every time. Handed over when the loadout dresses the gun.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyLoadoutComponent> OwnerLoadout;

	//~ Runtime state.
	UPROPERTY(Transient) TObjectPtr<APawn> OwningPawn;

	/** Whether the numbers are in hand. False until the loadout has answered. */
	bool bInitialized = false;

	/**
	 * Whether this weapon is being aimed right now — its own state, written by its own secondary press
	 * and release, and read by the body for the aim blend, the camera, the crosshair and the pose.
	 * Never set by a melee.
	 */
	UPROPERTY(Transient)
	bool bAiming = false;

	/**
	 * Whether the right button is down on this thing — its own state, written by the same two doors that
	 * write `bAiming`, and read by any move that is a HOLD rather than a press (see IsSecondaryHeld).
	 */
	UPROPERTY(Transient)
	bool bSecondaryHeld = false;

	/**
	 * The ability this thing's right button runs, handed over by the loadout that dressed it — and
	 * deliberately NOT a default on a weapon blueprint, for exactly the reason the stat block above is
	 * not one: the loadout is the owner of the answer, and a weapon must not hold a copy of it.
	 */
	UPROPERTY(Transient)
	TSubclassOf<UProsperitocracyGameplayAbility> SecondPressAbility;

	/** What the loadout answered. Undressed until it is asked, and until the gun is in the rig. */
	EProsperitocracyWeaponDressResult DressResult = EProsperitocracyWeaponDressResult::Undressed;

	/** One warning per gun, not one per shot, for why the loadout could not dress it. */
	bool bDressFailureLogged = false;

	float LastShotTime = -BIG_NUMBER;

	/**
	 * The circle's offset from the dot, in degrees (X = yaw, Y = pitch) — the gun's whole aim state.
	 *
	 * Four drivers feed it and one decay pulls it back: the handling trail from Weight, the movement
	 * trail from Weight, the per-shot shove from Accuracy, and the climb from Recoil. Everything that
	 * wants to know where this gun is actually pointing — the bullet, the reticle, the pose — reads
	 * this one value (see GetAimDriftDegrees).
	 */
	FVector2D AimDriftDegrees = FVector2D::ZeroVector;

	// The current *combined* posture multiplier: ADS x standing still x crouching x airborne, all
	// folded into the one number that scales Accuracy. Each part is a universal constant above and
	// each eases in over TransitionRate_Posture rather than snapping.
	float CurrentAccuracyMultiplier = 1.0f;
	float StandingStillMultiplier = 1.0f;
	float JumpFallMultiplier = 1.0f;
	float CrouchingMultiplier = 1.0f;

	// The camera rotation last tick, so a swing can be measured against it. The flag is what keeps the
	// first tick after a gun comes up from reading as an enormous swing.
	FRotator LastControlRotation = FRotator::ZeroRotator;
	bool bHasLastControlRotation = false;

private:
	bool CanFireNow() const;

	/** The character holding this gun, or null when it is held by no one (or not a character yet). */
	AProsperitocracyCharacter* GetOwnerCharacter() const;

	/** The player steering this gun, or null when nobody is (an unattended gun, a dummy). */
	class APlayerController* GetOwningPlayerController() const;

	/** The Accuracy stat. Presence-is-scope: an absent stat is the baseline 1.0, not a zero. */
	float GetAccuracy() const;

	/** The Recoil stat, in degrees of up-climb per shot. Presence-is-scope: absent = 0, no climb. */
	float GetRecoil() const;

	/** One random per-shot shove of the circle, sized by Accuracy. This IS the spread. */
	void ApplySpreadShove();

	/**
	 * The ONE path a hit of this gun's damage travels: point the effect context at this gun's stat host
	 * as the ABILITY SOURCE (an ordinary shot carries no lines of its own — the source answers with
	 * this gun's damage stat and Penetration), and run the gun's damage effect on that hit through the
	 * shared applier (UProsperitocracyDamageStatics::ApplyDamageEffectToHit) — the same one the bash
	 * reaches the execution through.
	 *
	 * A second copy of this plumbing would be a second damage path, which is exactly what this class
	 * exists not to have.
	 */
	void ApplyDamageToHit(const FHitResult& Hit);

	/** The per-tick drift: the handling trail, the movement trail, and the return to centre. */
	void UpdateDrift(float DeltaSeconds);

	/** The per-tick posture multipliers: ADS, standing still, crouching, airborne. */
	void UpdatePostureMultipliers(float DeltaSeconds);

	/** Hold the circle on the screen after anything that moved it. */
	void ClampDrift();

	/** One magazine's worth of rounds, from this gun's own MagSize stat. */
	int32 MagazineSize() const;

	/** How many magazines this gun carries, from its own Capacity stat. */
	int32 MagazineCapacity() const;

	/** This gun's ammo in its slot's store, or null when undressed or the slot holds none yet. */
	const FProsperitocracyWeaponAmmo* FindAmmo() const;

	/** Say once, per gun, why the loadout could not dress it — each reason names a different fix. */
	void LogDressFailureOnce(EProsperitocracyWeaponDressResult Result, const AActor* RigOwner);
};
