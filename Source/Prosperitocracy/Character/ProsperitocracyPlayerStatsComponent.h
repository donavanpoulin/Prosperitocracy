// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySet.h"
#include "Stats/ProsperitocracyStat.h"

#include "ProsperitocracyPlayerStatsComponent.generated.h"

class AProsperitocracyStatHostActor;
class UCharacterMovementComponent;
class UProsperitocracyAbilitySystemComponent;
class UProsperitocracyHealthSet;
class UProsperitocracyStatSet;
class UProsperitocracyStatTable;

/**
 * UProsperitocracyPlayerStatsComponent
 *
 * The player's own numbers, and the one place they reach the body.
 *
 * Every one of these numbers is a GAS attribute (Design/stats.md: "GAS is the evaluator" — nothing
 * reads a raw base, there is no non-GAS numeric path). This component does three things and nothing
 * else: it puts the baseline stat block into the character's ability system when the character comes
 * up, it grants the abilities the character owns (the bash), and it pushes the movement stats onto
 * the character movement component. Anything that wants a stat asks GAS for its FINAL value, through
 * the same door as everything else (`UProsperitocracyStatSystemStatics::GetStatFinal`).
 *
 * The template's movement is untouched: its walk and run states still decide WHEN each speed
 * applies, and its graph asks this component for the number instead of holding one of its own. That
 * is deliberate — the number has one owner, and a stat that is not set never zeroes the body out
 * (movement keeps whatever it had).
 *
 * There is ONE speed stat (Move Speed = the run speed) and every speed the body can move at is a
 * fraction of it. Walking is not a second stat: it is that same number times one universal constant,
 * so a walk is always half a run. Crouching is not a third number either — a crouch is not faster
 * than a walk, so crouch speed IS the walk number. Jump is one stat too, and it is a VELOCITY (Jump
 * Velocity, cm/s) because that is what the body is driven by — a height would be the same jump
 * described in a unit the movement cannot use.
 *
 * What the character CARRIES then moves both: every pound of gear costs the same slice of run speed
 * and jump velocity, read from the loadout's carried total. Walking is never scaled directly — it is
 * half of the already-scaled run speed, so it follows, and crouching follows the walk. That ordering
 * is the whole design of it: one stat (weight) feeds one formula (percent per pound) which scales two
 * numbers (run, jump), and the other two speeds (walk, crouch) are derived from one of them.
 *
 * And what the character SHOOTS moves the first of them again, from the other end: a shot pushes the
 * body back along the line it went down (see the shot push below), so firing drags you while you walk
 * forward and carries you while you walk back. How hard it does that is the GUN's own stat — Drag, and
 * Carry at half of it — derived by one fixed formula off the gun's Weight and damage (see
 * AProsperitocracyStatHostActor), so the thing that shoves you hardest is the thing that is heavy or
 * hits hard. Nothing here decides the size of the push: this component applies the gun's number.
 *
 * And a shot moves one more thing on that body: the animation. While a push is live the body plays its
 * animation at the same share the push is moving it by — slower while the shot drags them, faster while
 * they ride it back — and at normal speed the whole rest of the time. Nothing else re-times the
 * animation: not walking, not crouching, not how much the character carries, and not a reload or a
 * mantle, which are not things the gun in their hands gets to slow down. That is why it hangs off the
 * shot rather than off the body's speed — a body that is always a little slower would slow everything
 * it plays, and only a shot is meant to.
 */
UCLASS(ClassGroup = (Prosperitocracy), meta = (BlueprintSpawnableComponent))
class UProsperitocracyPlayerStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProsperitocracyPlayerStatsComponent();

	/** What this character's numbers ARE: one entry per stat it has. Presence is scope. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Stats")
	TObjectPtr<UProsperitocracyStatTable> BaselineStats;

	/**
	 * What this character OWNS as abilities — the gun bash today.
	 *
	 * Granted to the character's ability system when it comes up, through the project's own ability
	 * set (the same data asset the ability's input tag travels in). One home for a granted ability:
	 * nothing else has to remember what the character has, and nothing holds a spec handle of its own.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Abilities")
	TObjectPtr<UProsperitocracyAbilitySet> AbilitySet;

	/**
	 * The weave this body is wearing — one armor, one block.
	 *
	 * An armor is a thing the way a gun is: what it is worth is a stat block, and its rows are the
	 * weave's (Design/armor.md — Impact Resist, Piercing Resist, Weight). Nothing about it is a
	 * second system: its resists are the body's own resist rows and its weight is a thing stat on its
	 * own GAS home, so a perk reaches a weave's resists and its weight exactly like anything else.
	 *
	 * Null = a bare body: no armor, so no armor numbers. What a body spawns wearing is authored here;
	 * wearing a different one is the same call made again (`WearWeave`), never a second path.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Armor")
	TObjectPtr<UProsperitocracyStatTable> ArmorWeave;

	/** FINAL value of one of this character's stats, through GAS — the ONE evaluator. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetStat(EProsperitocracyStat Stat) const;

	/** The run speed: Move Speed, through the weight penalty. Running is the number; walk is a fraction. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetRunSpeed() const { return GetStat(EProsperitocracyStat::MoveSpeed) * GetWeightSpeedMultiplier(); }

	/**
	 * Walking: the same Move Speed through the one walk multiplier, weight penalty included with it.
	 * Crouching moves at this number too — a crouch is not faster than a walk, so there is no third
	 * speed to keep in step with the stat.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetWalkSpeed() const;

	//~ The armor ----------------------------------------------------------------------------------

	/**
	 * Wear a weave — the ONE door a body's armor comes through: what it spawns wearing, and what a
	 * swap wears.
	 *
	 * One call does the whole thing, because it is one decision:
	 *   1. what the LAST weave gave the body is taken back first, so a row a weave does not carry
	 *      reads as bare rather than as the previous weave's number,
	 *   2. the armor gets its own GAS home holding this block — a thing's stats are evaluated on the
	 *      thing, so a weight perk reaches the armor's weight exactly like a gun's,
	 *   3. the block's BODY rows (its two resists) go onto this character's own stats, through the
	 *      same act and the same guard the baseline block comes in through,
	 *   4. what the body carries just changed, so the numbers the body moves by are re-read: the JUMP
	 *      is written here (nothing else owns it), and the walk/run numbers are re-read by the
	 *      movement state that owns them — the same split a change in what a gun weighs already makes.
	 *
	 * Passing null takes the armor off. Nothing here decides a number: every one of them is the
	 * block's, resolved by the one evaluator.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Armor")
	void WearWeave(UProsperitocracyStatTable* Weave);

	/** The weave on this body right now. Null = bare. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Armor")
	UProsperitocracyStatTable* GetWornWeave() const { return ArmorWeave; }

	/**
	 * The worn armor's own GAS home — where its Weight is evaluated, the same home a gun's numbers
	 * get. Null while the body is bare. Read by whoever counts what the body carries.
	 */
	AProsperitocracyStatHostActor* GetArmorHost() const { return ArmorHost; }

	/**
	 * Write what this body is CARRYING onto its own Carried Weight row — the sum of its things, which
	 * the loadout is the one thing that knows.
	 *
	 * Called whenever the kit changes (a gun dressed, a weave worn). This is the only writer of that
	 * row's base, so what you carry has exactly one home — and it is a row, so a weight perk can move
	 * it and this body's movement (which listens to that row) follows the change by itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Stats")
	void SyncCarriedWeight();

	//~ The shot's push on the body ----------------------------------------------------------------

	/**
	 * Tell the body a shot left the gun: what the gun's push is worth, both ways, and the line the
	 * shot went down.
	 *
	 * One call per committed shot, made by the gun from the same per-shot hook its own feel already
	 * hangs off (AProsperitocracyWeapon::ApplyShotFeel). Every gun tells the body the same way, so no
	 * gun carries code of its own for this and every gun gets it for free.
	 *
	 * Both numbers are the gun's OWN stats — Drag and Carry — arriving as FINAL values through GAS,
	 * the one evaluator: never a raw base, and never a formula re-run down here. The body applies the
	 * gun's push; it does not decide what the push is worth.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Stats")
	void NotifyShotFired(float DragPercent, float CarryPercent, const FVector& ShotDirection);

	/**
	 * What the live shot is doing to this character's speed right now, signed and in cm/s — the
	 * number the body is actually being pushed by this frame.
	 *
	 * Positive when the character is moving BACK along the shot's line — the push is carrying them, so
	 * their speed goes up. Negative when they are moving forward — they are spending speed fighting
	 * it, so their speed goes down. Zero when they are standing still or moving across the line: there
	 * is nothing to fight and nothing to ride.
	 *
	 * It is a share of the speed the character is actually at, so the same push bites the same at a
	 * walk, at a sprint and crouched. The sign, the share and the decay are one number — the two
	 * "ways" are not two rules.
	 *
	 * Forward the share is the gun's own Drag; backwards it is its Carry, which is Drag at half — so
	 * the ride back is always the weaker half of one push. Both are the gun's stats, read off the one
	 * evaluator by the gun that owns them.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetShotPushDrag() const;

	/** The jump's launch velocity: Jump Velocity through the weight penalty, onto the movement component. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetJumpVelocity() const { return GetStat(EProsperitocracyStat::JumpVelocity) * GetWeightSpeedMultiplier(); }

	/** Everything this character carries, in lbs, from its loadout. 0 when it carries nothing. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetCarriedWeightLbs() const;

	/**
	 * The weight penalty: what the carried weight does to run speed and jump velocity.
	 *
	 * It is a multiplier, not a delta, because it has to compose with everything else that already
	 * moved those numbers (a perk, an attachment) without knowing any of them: the stat's FINAL value
	 * is what gets scaled. Walking is not scaled again — it is half of the already-scaled run speed,
	 * and crouching follows the walk.
	 *
	 * This is the one formula, with one constant, for every character and every item: heavier is
	 * slower, and there is nowhere else that decides how much.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetWeightSpeedMultiplier() const;

	/**
	 * Push the movement numbers onto the character movement component: walk and run speed from Move
	 * Speed, crouch speed from the walk number, jump velocity from Jump Velocity. Safe to call again
	 * after the numbers change.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Stats")
	void ApplyToMovement();

	/**
	 * The jump number on its own. Safe at any moment, because no movement state writes JumpZVelocity —
	 * so this is what a change in what the character carries re-applies, while the walk and run speeds
	 * wait for the movement state that owns them to read the stat again.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Stats")
	void ApplyJumpVelocity();

	/**
	 * The one walk multiplier, universal — every character walks at the same fraction of its run
	 * speed, so a slower character is slower walking too. It is not a stat and not a perk target:
	 * the stat is the run speed, and this is the fraction that turns it into a walk. Crouch speed is
	 * this number, not a third fraction.
	 */
	static constexpr float WalkSpeedMultiplier = 0.5f;

	/**
	 * The floor on the animation rate, so a body dragged to nearly a standstill crawls rather than
	 * freezing its animation. The push can never drive the rate below it. [TUNE]
	 */
	static constexpr float MinAnimationRate = 0.25f;

	/** How fast the animation rate slides to a new value, per second, so it never snaps. [TUNE] */
	static constexpr float AnimationRateEase = 6.0f;

	/**
	 * The one weight constant: percent of run speed and jump velocity that one pound costs.
	 *
	 * One number for every weapon, every armor, every character — Design/loadout.md: "Total weight
	 * reduces everything: jump height, walk/run speed, etc.", and Design/stats.md: a derived value is
	 * computed by ONE fixed formula with universal constants. This is that constant. [TUNE]
	 *
	 * It is deliberately soft, and the WEIGHT of things is where the bite comes from: a suit's weight
	 * spans a wide range on purpose, so the lightest build barely notices its gear and the heaviest
	 * build (heavy weave, heavy guns) walks rather than runs. Sugar-coating this constant instead would
	 * change every weapon's feel at once, which is not what a suit should get to do.
	 */
	static constexpr float WeightPenaltyPercentPerLb = 0.5f;

	/**
	 * How long a shot's push lasts, in seconds — one window, REFRESHED by every shot and never added
	 * to. The push DECAYS from the full share down to nothing across this, so a gun that keeps firing
	 * holds a push near the top of its strength and a single shot visibly eases away instead of
	 * snapping off.
	 *
	 * It is not where the push's STRENGTH lives: that is the gun's own Drag and Carry stat. [TUNE]
	 */
	static constexpr float ShotPushSeconds = 0.5f;

protected:
	virtual void BeginPlay() override;

	/** The armor's GAS home only exists for as long as the body does — the lifetime a gun gives its host. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Runs every frame, for two jobs: the shot's push, which has a life of its own and nothing else in
	 * the engine ends, and the animation rate, which follows that push down and back up — the ease back
	 * to normal has to keep running after the push itself has lapsed.
	 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/**
	 * A block's BODY rows onto this character — the one act a block becomes this body's numbers.
	 *
	 * Values: the block's own numbers (the baseline block at spawn, a weave being worn). Bare: zero,
	 * which is what takes a block's rows back OFF the body (a weave coming off, so the next one is
	 * never wearing the last one's numbers where it carries nothing).
	 *
	 * One loop and one guard, `IsCharacterStat`: a thing stat in the block (an armor's Weight) is the
	 * THING's number, evaluated on the thing's own GAS home, and is skipped here.
	 */
	void ApplyBlockBodyRows(const UProsperitocracyStatTable* Block, bool bBare);

	/**
	 * Listen to the numbers the movement is built from — Move Speed, Jump Velocity, and what the body
	 * carries — so that a change to any of them re-prices the legs on the spot.
	 *
	 * This is the whole answer to "a change made later never reaches the body": with the numbers
	 * themselves doing the notifying, nothing has to remember to re-apply anything. A weave going on, a
	 * perk proccing, a value re-tuned — they all arrive here, because all of them are just a change to
	 * a number, and every one of them resolves through the one evaluator.
	 */
	void BindMovementStatListeners();

	/** Let those notifications go with the body. */
	void UnbindMovementStatListeners();

	/** Push the baseline block into the character's attributes, once per life. */
	void ApplyBaselineStats();

	/**
	 * Grant what this character OWNS as abilities (the bash), once per life.
	 *
	 * Through the project's own ability set, so an ability is granted in one place, carries its input
	 * tag with it, and can be handed back as a set if the character ever stops owning it.
	 */
	void GrantAbilities();

	/** The owner's movement component — the body every number here ends up on. */
	UCharacterMovementComponent* GetMovementComponent() const;

	/**
	 * The body, and which of its speed numbers it is moving at right now.
	 *
	 * Crouching moves at its own number — slot 1 — and every other way of moving on the ground is the
	 * standing number, slot 0. The push rides whichever one the body is actually using, which is what
	 * makes it land crouched without a crouch rule anywhere.
	 */
	UCharacterMovementComponent* GetMovingBody(int32& OutSlot) const;

	/**
	 * Take the numbers the push rides on, each from whoever owns it: the standing number from the
	 * movement state, the crouch number from the stat.
	 *
	 * A standing number that is what THIS component last wrote is ours, not the state's, and is left
	 * out — so adopting can never mistake the push for the state's number and compound it with
	 * itself. The crouch number is never adopted off the body: this component is the only thing that
	 * writes it (it is the walk number, off the stat), so whatever is read back is this component's
	 * own write — adopting it would leave the push riding, and later handing back, nothing.
	 */
	void AdoptBodySpeedNumbers();

	/**
	 * Every speed number the body moves at, onto it: walk/run, crouch, and the jump. The one place
	 * MaxWalkSpeed and MaxWalkSpeedCrouched are written from stats.
	 *
	 * Floored at a standstill: a shot push big enough to take the last of the character's speed must
	 * be able to stop them, never to walk them backwards (the same reasoning as the weight penalty's
	 * floor). A stat that is not set still leaves the body alone.
	 */
	void PushWalkSpeed();

	/**
	 * The animation rate onto the body: how fast its animation should play for what a shot is doing to
	 * it right now.
	 *
	 * It reads the live push, so the legs cycle slower while the shot drags the body and faster while
	 * they ride it back, and it is 1.0 whenever there is no push to ride — which is every moment the
	 * character is not firing. One number, one place: the same component that reaches the body with the
	 * movement numbers reaches it with this.
	 *
	 * Called every frame: the push's own decay is what moves it, so a rate written only on the shot
	 * would sit still while the push faded. DeltaSeconds eases the change; 0 snaps.
	 */
	void PushAnimationRate(float DeltaSeconds);

	/**
	 * That same rate as a number, before it is eased onto the body: normal, plus the share of their
	 * speed the push is moving them by this frame — negative while it drags them, positive while it
	 * carries them, and nothing at all while they stand still or move across the shot's line.
	 */
	float GetAnimationRate() const;

	/**
	 * The body's speed number with the live push folded in: what the body is actually moving at while
	 * a shot's push is running.
	 *
	 * The push cannot simply overwrite MaxWalkSpeed, because the movement state owns that number and
	 * writes it whenever the character changes between walking and running — and while a sprint is
	 * held it writes it every frame. So the push ADOPTS whatever the body's own number is at that
	 * moment, rides on top of it, and gives it back untouched when the window lapses: the state's
	 * number is never lost and a sprint is never quietly turned into a walk.
	 */
	void ApplyShotPushToWalkSpeed();

	/**
	 * How much of the shot's push is left, 1 on the shot down to 0 when the window lapses.
	 *
	 * One straight ramp for every gun. A shot resets it to 1, so a burst of fire holds the push near
	 * full and a single shot visibly eases off.
	 */
	float GetShotPushDecay() const;

	/** What was granted, so it could be taken away again. */
	FProsperitocracyAbilitySet_GrantedHandles GrantedAbilityHandles;

	/** The owning pawn's ability system — where every stat of theirs lives. */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * The worn armor's own GAS home, spawned when a weave is worn and destroyed with the body.
	 *
	 * Its Weight is a real attribute here, resolved by the one evaluator — which is what lets what the
	 * body carries count an armor that something has modified, the same way a gun's weight is counted.
	 */
	UPROPERTY(Transient)
	TObjectPtr<AProsperitocracyStatHostActor> ArmorHost;

	/**
	 * The movement's own notifications — the handles for what BeginPlay binds, kept so they can be let
	 * go with the body. They are removed BY HANDLE, because the bindings are lambdas and a multicast
	 * delegate cannot find a lambda by object.
	 */
	TArray<FDelegateHandle> MovementStatChangeHandles;

	/**
	 * The walk and run numbers this component last published, so a re-price knows which of the two the
	 * body is already moving at: a change re-prices the speed the body is IN — walk stays walk, run
	 * stays run — and never picks one for it.
	 */
	float LastPublishedWalkSpeed = 0.0f;
	float LastPublishedRunSpeed = 0.0f;

	//~ The live shot push. One slot per speed number, and one push: a shot refreshes it, it never
	//~ stacks. Slot 0 = the standing number (walk/run/sprint), slot 1 = the crouch number.

	/** True while a shot's push is running. */
	bool bShotPushActive = false;

	/** When the push lapses, in world seconds. A new shot pushes this out, it never adds to it. */
	float ShotPushEndTime = 0.0f;

	/**
	 * What the push is worth this window at full strength, as a percent, from the gun that fired it:
	 * moving FORWARD it is that gun's Drag, moving BACK it is its Carry. Two numbers, one push — which
	 * one applies is decided by the character's own movement direction, not by a branch in here.
	 */
	float ShotPushDragPercent = 0.0f;
	float ShotPushCarryPercent = 0.0f;

	/** Straight BACK down the line the shot went down (horizontal), the axis the push rides. */
	FVector ShotPushBackwardAxis = FVector::ZeroVector;

	/**
	 * What the push rides on, per slot — each from its own owner, not two owners: slot 0 is the
	 * standing number (walk/run/sprint) as the movement state last set it, slot 1 is the crouch
	 * number off the stat (the walk number).
	 */
	float ShotPushBaseSpeed[2] = { 0.0f, 0.0f };

	/** What this component last wrote into each number, so somebody else's write can be told apart. */
	float ShotPushLastWritten[2] = { 0.0f, 0.0f };

	/** The animation rate currently on the mesh, eased toward the target so a change is never a snap. */
	float CurrentAnimationRate = 1.0f;

	/** One warning per component, not one per read, when there is no baseline to apply. */
	bool bBaselineWarned = false;
};
