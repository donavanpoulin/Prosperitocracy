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
class UProsperitocracyLoadout;
class UProsperitocracyStatSet;
class UProsperitocracyStatTable;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;

/**
 * The armor's TRIM REGIONS — the three the body carries, each with the row that holds its colour and
 * the name the player speaks it by.
 *
 * One list, one place: the component paints from it and the dev command speaks it, so the regions can
 * never be written down twice and disagree. Each region's colour is a row of the universal stat table
 * (Trim 1 / Trim 2 / Trim 3) — never a second place a colour lives.
 *
 * The regions are numbered by what the player SEES (1 the collar, 2 the shoulders and arms, 3 the
 * legs), which is NOT what the mesh calls its slots — the mesh's labels name the artist's chunks
 * ("head and hands", "torso", "legs"). So `SlotName` is the mesh's own fragment, matched against the
 * mesh's slot names, and `Name` is the player's number: the two are deliberately different things,
 * and this table is the one place they meet.
 */
namespace ProsperitocracyArmor
{
	/** Nothing chosen: the region keeps the paint it ships with. A real colour is 0x000000 or above. */
	constexpr int32 NoColor = -1;

	/** One region: the row that holds its colour, the player's number, and the mesh slot it lands in. */
	struct FPiece
	{
		EProsperitocracyStat Color;
		const TCHAR* Name;
		const TCHAR* SlotName;
	};

	/** The regions, numbered for the player — and the mesh slot each one's colour lands in. */
	constexpr FPiece Pieces[] =
	{
		{ EProsperitocracyStat::TrimColor1, TEXT("1"), TEXT("torso") },  // the collar
		{ EProsperitocracyStat::TrimColor2, TEXT("2"), TEXT("head")  },  // the shoulders and arms
		{ EProsperitocracyStat::TrimColor3, TEXT("3"), TEXT("legs")  },  // the legs
	};

	/** How many pieces an armor is worn as — the count everything about the colour walks over. */
	constexpr int32 PieceCount = sizeof(Pieces) / sizeof(Pieces[0]);
}

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

	/** FINAL value of one of this character's stats, through GAS — the ONE evaluator. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetStat(EProsperitocracyStat Stat) const;

	/**
	 * The BODY's ability system — the one the whole character's stats are evaluated on (the ASC the
	 * template's blueprint gives this component).
	 *
	 * It is exposed because a body that TAKES damage has to hand it over: the character answers
	 * IAbilitySystemInterface through this, which is how a hit finds where to apply its damage and how a
	 * status finds the ability system it stamps. Null before the component has come up, which is a
	 * truthful answer — nothing can be damaged before it exists.
	 */
	UProsperitocracyAbilitySystemComponent* GetAbilitySystemComponent() const { return AbilitySystemComponent; }

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

	/**
	 * Dress the body's armour from a loadout — the ONE place the armour reaches this body.
	 *
	 * The armour is part of what you take in (Design/loadout.md), so a loadout is where the weave and
	 * the three trim colours come from, and this is what turns them into a body that is wearing
	 * something. Two steps, in the armour's own order:
	 *   1. the WEAVE is worn first, because everything else here lives on the armour — a colour has
	 *      nowhere to be carried until there is an armour to carry it (SetArmorColor's own guard),
	 *   2. the three REGIONS are then written, one each, through the same door a customizer will use.
	 *
	 * Null takes the armour off, and so does a loadout that names no weave: a bare body is a real
	 * state, and a loadout with nothing on it is a real loadout (Design/loadout.md).
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Armor")
	void DressFromLoadout(UProsperitocracyLoadout* Loadout);

	//~ The abilities a loadout names — what this body OWNS (Design/loadout.md, Design/abilities.md) ----

	/**
	 * OWN the four abilities this loadout took in, and give back the four the last one took in.
	 *
	 * THE ONE DOOR between "what a loadout selects" and "what this character can do", and the reason it
	 * lives here rather than anywhere else: this component is the one that owns the ability system, and
	 * an ability can only be granted to the system that will run it.
	 *
	 * A class has a ROSTER and a loadout picks FOUR from it (Design/abilities.md), so what a character
	 * owns is these four and never the whole roster — granting the roster would give everyone everything,
	 * which is the opposite of picking. The four here are the loadout's own slots, in bar order.
	 *
	 * The last loadout's four go FIRST, always, and that is the half that makes a swap a swap rather than
	 * an accumulation: an ability the new loadout does not name is taken away with its spec handle, so it
	 * cannot be pressed any more. Nothing else in the game grants or removes a loadout's abilities, so
	 * there is exactly one writer of what this body owns from a loadout.
	 *
	 * Empty slots are skipped, and a loadout with none of them is a real loadout (Design/loadout.md): it
	 * simply owns nothing, which is a legal build and not a broken one. Null takes everything back.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Abilities")
	void DressAbilitiesFromLoadout(UProsperitocracyLoadout* Loadout);

	/** The weave on this body right now. Null = bare. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Armor")
	UProsperitocracyStatTable* GetWornWeave() const { return ArmorWeave; }

	/**
	 * The worn armor's own GAS home — where its Weight is evaluated, the same home a gun's numbers
	 * get. Null while the body is bare. Read by whoever counts what the body carries.
	 */
	AProsperitocracyStatHostActor* GetArmorHost() const { return ArmorHost; }

	//~ The armor's colour — the look, on the armor's own rows (Design/armor.md) --------------------

	/**
	 * Paint one piece of the armor: write that piece's colour row on the armor's OWN GAS home — the
	 * one door a colour comes through, the same shape as WearWeave for the weave.
	 *
	 * One number per piece: the RGB hex (0xRRGGBB). `ProsperitocracyArmor::NoColor` clears it, and the
	 * piece goes back to the paint it ships with. Nothing here decides a colour, and nothing here
	 * touches the mesh: the row changes, and the body's own listener — which is already reading the
	 * rows — paints the piece on the spot. That is the whole reason a customizer will need no new path
	 * when it arrives: it writes the same row through the same call.
	 *
	 * False when the stat is not one of the armor's colour rows, or when no armor is worn: a colour
	 * lives on the armor, so a bare body has nowhere to carry one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Armor")
	bool SetArmorColor(EProsperitocracyStat PieceColor, int32 Hex);

	/** One piece's colour right now, as its hex — or nothing chosen, when none is. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Armor")
	int32 GetArmorColor(EProsperitocracyStat PieceColor) const;

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
	// A block's BODY rows are written by ONE door, `UProsperitocracyStatSystemStatics::ApplyBlockBodyRows`
	// — the baseline block at spawn, a weave going on or coming off, and a body part's own block all go
	// through it, so no body can be dressed one way here and another way there.

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

	//~ Painting the armor (the other half of the colour) -------------------------------------------

	/**
	 * The mesh the armor is painted on: the rig's answer through the character (`GetBodyMesh`), never
	 * a guess — C++ cannot tell the drawn body from the animation source by looking at them.
	 */
	USkeletalMeshComponent* GetBodyMesh() const;

	/**
	 * Make the live copy of each piece's material, once, and remember what each piece looked like
	 * before we touched it.
	 *
	 * A copy (a dynamic material instance) is what makes the colour movable at all: the material
	 * ASSET is a file and cannot be repainted while the game runs, but a copy of it can be, any
	 * number of times. Every player gets their own copies, which is why one player's colour is never
	 * another's. Made FROM the material already on the slot, so the piece's own textures and paint
	 * stay underneath the colour — only the colour is ours to move.
	 */
	void EnsureArmorColorMIDs();

	/**
	 * Which of the body mesh's material slots a piece is drawn in — found by the piece's own name,
	 * matched against the names the mesh carries for its slots. INDEX_NONE when the mesh has no slot
	 * by that name (the mesh's own slot names are logged, so the fix is a name and not a hunt).
	 */
	int32 FindArmorColorSlot(const USkeletalMeshComponent* Body, const TCHAR* SlotName) const;

	/**
	 * Paint every piece from its own row — the half of the colour that touches the body.
	 *
	 * Each piece is read at the moment it is painted, through the one evaluator, so a colour set by
	 * anything (a command today, a customizer later) lands the same way with nothing in between.
	 * Nothing chosen (`NoColor`) is not a colour: the piece is put back to the paint it shipped with.
	 */
	void ApplyArmorColors();

	/**
	 * Listen to the three colour rows on the armour's OWN GAS home, so a colour written by anyone
	 * repaints the piece on the spot — the same shape as the movement listening to its own numbers,
	 * and the reason nothing has to remember to call the paint.
	 */
	void BindArmorColorListeners();

	/** Let those notifications go — with the armour, and with the body. */
	void UnbindArmorColorListeners();

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

	/**
	 * What a LOADOUT granted, kept apart from what the character owns by itself, so the two can never be
	 * taken away by each other's change.
	 *
	 * Everything here belongs to the loadout playing right now: a swap empties this list and fills it
	 * again from the loadout being played, while `GrantedAbilityHandles` above — the bash, contested
	 * health, the passives every body has — is untouched by any loadout change. One list per owner is
	 * what makes "take back the last loadout's abilities" safe to say at all.
	 */
	FProsperitocracyAbilitySet_GrantedHandles LoadoutAbilityHandles;

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
	 * The weave this body is wearing RIGHT NOW — state, not authoring: what a loadout dressed it with
	 * (DressFromLoadout), or what a swap wore (WearWeave). Kept so the next weave can take the last
	 * one's rows back off the body, and read out through GetWornWeave.
	 *
	 * Null = a bare body. Nothing is authored here any more: the weave is the loadout's choice, and
	 * a body with no loadout has none.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyStatTable> ArmorWeave;

	/**
	 * The movement's own notifications — the handles for what BeginPlay binds, kept so they can be let
	 * go with the body. They are removed BY HANDLE, because the bindings are lambdas and a multicast
	 * delegate cannot find a lambda by object.
	 */
	TArray<FDelegateHandle> MovementStatChangeHandles;

	/**
	 * The color rows' notifications on the armour's own ability system, kept so they can be let go.
	 * Removed BY HANDLE for the same reason as the movement's: the bindings are lambdas.
	 */
	TArray<FDelegateHandle> ArmorColorChangeHandles;

	/**
	 * One live copy of each piece's material, in the piece order — made once (a copy per piece per
	 * body is the point: this is what the player's own colour is written into).
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ArmorColorMIDs;

	/**
	 * What each piece's colour was before we touched it, so "nothing chosen" can put the pack's own
	 * paint back rather than a black we invented. Same order as ArmorColorMIDs.
	 */
	TArray<FLinearColor> OriginalArmorColors;

	/** True once the live copies have been made — the one-time half of painting a body. */
	bool bArmorColorMIDsMade = false;

	/** One warning, not one per paint, when the material has no colour parameter by our name. */
	bool bArmorColorParameterWarned = false;

	/**
	 * The parameter the body's materials take their colour through — the pack's own name for it. One
	 * name for all three pieces: a piece's material IS the same material, per part.
	 */
	static const FName ArmorColorParameterName;

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
