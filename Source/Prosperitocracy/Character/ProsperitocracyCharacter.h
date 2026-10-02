// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "AbilitySystem/ProsperitocracyDamageReceiver.h"

#include "ProsperitocracyCharacter.generated.h"

class AProsperitocracyWeapon;
class UChildActorComponent;
class UInputAction;
class UInputComponent;
class UProsperitocracyPlayerStatsComponent;
class UProsperitocracyStatusComponent;
class USkeletalMeshComponent;

/**
 * The BODY's aim numbers: how the man himself comes round.
 *
 * Not the gun's — the gun's feel is its own stats poured into the weapon-handling shape. This is the
 * body: how fast the whole man turns toward where the player is looking, in TWO layers. What he
 * CARRIES — armour and every gun on him — sets the rate he comes round at with his hands empty, and
 * the thing IN those hands then multiplies it off its own Weight. So the same kit turns differently
 * with a pistol out than with a rifle out, and an empty hand drops him back to the plain carried
 * rate. Universal: two constants for every loadout and every thing, no per-gun number anywhere. All
 * values [TUNE].
 */
namespace ProsperitocracyBodyAimHandling
{
	// Degrees per second the body comes round at with nothing carried, and what each carried pound
	// takes off that. THE RATE IS THE ONLY LIMIT, and there is deliberately no cap on how far behind he
	// may be: a cap is a dead zone. With one, the aim holds still until the look has dragged it past,
	// which is not a turn at all — he would be late by a fixed angle instead of catching up. Late is a
	// rate, never a distance.
	constexpr float TurnRateBase = 1080.0f;
	constexpr float TurnRatePerLb = 6.0f;

	// The multiply the thing IN HIS HANDS puts on that rate, off its own Weight: one at an empty hand,
	// one step down for every pound it weighs. Floored, because the guns only get heavier from here
	// and a thing heavy enough to zero the multiply must not stop him turning at all. A thing's Weight
	// is read FINAL off its own home, so a weight perk or attachment on the gun moves his turn with it
	// — and the rate floor below is the last word either way.
	constexpr float EquippedWeightMultiplierPerLb = 0.06f;
	constexpr float EquippedWeightMultiplierMin = 0.25f;

	// The slowest this man may ever come round, whatever he is carrying and whatever is in his hands.
	constexpr float TurnRateMin = 120.0f;

	// How often the aim says where it is while he is behind, so a play test can see the turn.
	constexpr float AimLogIntervalSeconds = 0.25f;
}

/**
 * AProsperitocracyCharacter
 *
 * The player character's C++ half: the one place our aim behaviour reaches the body.
 *
 * The template keeps everything it did before — mesh, animation, movement, the rig, the HUD widget.
 * What lives here is the MAN'S OWN AIM: he turns toward where the player is looking at a rate set by
 * what he carries and multiplied by what he is holding, and nothing about him is instant. The aim is
 * ONE rotation, owned here, and
 * everything that needs to know where his gun points asks it — the bullet, the reticle circle, the
 * gun's own numbers. The template's animation blueprint
 * (`Content/ThirdPerson/Blueprints/Locomotion.uasset`) already drives its aim offset from
 * `GetBaseAimRotation`, so that one function is the whole of what the arms and the gun need in order
 * to come round with him.
 *
 * Three facts belong to the rig and cannot be guessed from C++, so the blueprint answers them. Each is
 * a BlueprintNativeEvent with an honest empty default, which is the whole door:
 *   - which gun is in hand (`GetGunInHand`) — the template's equip state decides it;
 *   - how far into ADS we are (`GetAimingAlpha`) — the template's `Aim_Smooth` timeline owns it.
 *   - which mesh the body is DRAWN with (`GetBodyMesh`) — the rig put the visible body on it.
 *
 * Nobody recomputes either from a proxy: not from a mesh being visible, not from a camera boom
 * length. There is one answer, and it comes from the thing that owns the fact.
 *
 * This body also TAKES damage. It answers IAbilitySystemInterface — its ability system lives on the
 * stats component the template's blueprint gives it — and IProsperitocracyDamageReceiver, so a hit finds
 * where to land and what the body answers with. Anyone's hit: friendly fire is always on and there are no
 * teams (Design/combat.md), so a teammate's shot lands here exactly like anything else's.
 *
 * A player has ONE part — the weave — and NO armour. A weave is a resists-and-weight block, never a
 * pen-gate number, so every pen over-pens the body, and the number it answers a line with is the body's
 * own resist row for that line's type, read FINAL through the one evaluator (a worn weave's resists land
 * on those rows, so a resist perk reaches them like any other stat).
 */
/**
 * HOW R IS READ — the window that lets ONE key mean two things.
 *
 * R held brings the chosen slot's thing out or puts it away; R let go inside this window is the thing
 * in hand answering, which is a gun reloading or a sword turning its blood mode on. The window is a
 * TIME and nothing else: the same key, the same gesture, and no branch anywhere on what is held.
 *
 * It is a constant here rather than a number in a graph because there is exactly one of it and it is
 * read in one place. [TUNE]
 */
namespace ProsperitocracyEquipInput
{
	constexpr float HoldSeconds = 0.25f;
}

/**
 * BODIES THROWN — a body caught by a blast goes limp and is thrown, done the way Epic's own page says
 * and no other way: "Physics Driven Animation in Unreal Engine" (UE 5.8 documentation).
 *
 * WHERE THE WORK HAPPENS, in Epic's words: "the two primary tools you will need are the Set All Bodies
 * Simulate Physics and Set All Bodies Below Physics Blend Weight nodes, which will generally be placed
 * within your character's Animation Blueprint Event Graph." So BOTH nodes live in the animation
 * blueprint's event graph (Locomotion) and NOWHERE ELSE — nothing in this file calls either of them.
 * What this file owns is the state that node is driven with:
 *
 *   * "Get the name of the bone that was hit" — the bone the throw goes on.
 *   * the doc's own requirement: "the use of physics on a Skeletal Mesh in any form requires that the
 *     mesh have a Physics Asset set up and applied to it" — refused out loud without one.
 *   * THE ONE NUMBER, animated here and applied by that node every tick: "At a value of 1.0, the given
 *     bone and all those below it are completely driven by physics. At a value of 0.0, the Skeletal Mesh
 *     has returned to its original keyframe animation. Often, you will want to drive this node at each
 *     tick so that you can smoothly animate the Physics Blend Weight value." One clock, so the body's
 *     state and the number the node applies cannot disagree.
 *   * the exit: "Once the reaction is complete and the Physics Blend Weight returns to 0, you should use
 *     the Set All Bodies Below Simulate Physics node once more to deactivate the simulation" — the body
 *     says when it has let go, the node makes that call.
 *
 * WHAT IS OURS, and the whole of it: the TRIGGER (a blast's shove, handed over by the one explosion pass)
 * and THE WEIGHT DAMPING — his rule, "heavier is thrown less" — the ONLY number here Epic's page does not
 * answer. It shapes the SPEED before it goes on the hit bone and it touches nothing else.
 *
 * NOT DONE, and named rather than quietly skipped: there is no get-up animation (his call, 2026-10-02), so
 * the weight returns to 0 and the animation owns him again; and the capsule, the collisions and the camera
 * are UNTOUCHED, because Epic's page says nothing about any of them.
 */
namespace ProsperitocracyBodiesThrown
{
	/**
	 * The bone the doc's nodes start from — "starting at a given bone and moving recursively down the bone
	 * chain". The root of the chain on this rig is the pelvis (`PA_Mannequin`'s first body), and starting
	 * there is what makes the WHOLE man go limp rather than one limb.
	 */
	inline const TCHAR* BeginAtBoneName = TEXT("pelvis");

	/**
	 * How long the weight takes to travel, in seconds, both ways — "quickly animate this going up to 1.0
	 * and then back down to 0.0". The doc names the shape and not a number. MINE, `[TUNE]`.
	 */
	constexpr float PhysicsBlendSeconds = 0.2f;

	/**
	 * How long he lies where he stopped before the weight is let go. There is no get-up clip to play (his
	 * call), so this wait is the whole of the get-up for now. MINE, `[TUNE]`.
	 */
	constexpr float GetUpHoldSeconds = 0.6f;

	/**
	 * THE WEIGHT DAMPING — the one number in the whole stack that is ours.
	 *
	 * His rule (2026-09-30): "YES YOUR total weight FINAL with perks DECREASES the velocity you are thrown
	 * in ragdoll". The blast's own speed times `Reference / (Reference + his carried weight)`: one at
	 * nothing carried, a half at the reference, less for every pound after. It never exceeds one, so the
	 * blast's own cap stays the ceiling, and it never reaches zero.
	 *
	 * MINE, `[TUNE]`, calibrated against the kits we have: 21 lb → 0.50, 30 lb → 0.41, 53 lb → 0.28.
	 */
	constexpr float WeightDampingReferenceLbs = 21.0f;
}

UCLASS()
class AProsperitocracyCharacter : public ACharacter, public IAbilitySystemInterface, public IProsperitocracyDamageReceiver
{
	GENERATED_BODY()

public:
	AProsperitocracyCharacter();

	/**
	 * What this body is holding right now, or null when nothing is in hand.
	 *
	 * ONE answer, and it lives HERE: which thing is out is the body's own state — the chosen slot and
	 * whether its thing is out (see `SetSlotOut`) — so it is answered here rather than copied into a
	 * blueprint and asked back. It answers with the ACTOR because that is what the rig actually holds
	 * (a child actor on one of its channels), and the type is resolved right here, once per reader —
	 * the same shape as `GetChildActor` itself. Null is a truthful answer, not a failure: nothing in
	 * hand means no drift, so the aim stays the plain camera aim everywhere it is read.
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Prosperitocracy|Aim")
	AActor* GetGunInHand() const;

	/**
	 * The gun in hand, resolved to our weapon type, or null when there is none.
	 *
	 * BlueprintPure so a graph can ASK which weapon is up and then talk to it, without casting a hand
	 * to a weapon class first. Every action belongs to the weapon that is HELD — a reload especially:
	 * the template wired its reload to one hand's channel, so the other hand could never reload at
	 * all, and a weapon's own job should never depend on which hand it happens to be in.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	AProsperitocracyWeapon* GetGunWeaponInHand() const;

	/**
	 * The melee picture of the thing in hand, or nothing when it declares none.
	 *
	 * The body's melee door runs the swing; this is only the ANIMATION that goes with it, and it is the
	 * WEAPON's own answer (`MeleeMontage`) rather than a hand's — a pistol bashed with a rifle's swing
	 * while the hand chose it. Nothing in hand, or a thing that declares no picture, answers nothing,
	 * which is the same answer a melee gives.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	UAnimMontage* GetMeleeMontageOfTheWeaponInHand() const;

	/**
	 * The MELEE KEY, as this body reads it: ask the thing in hand what that key means to it.
	 *
	 * A gun answers with the bash (its own melee); a blade answers with its blood-mode toggle; a thing
	 * with neither answers with nothing. The body asks and learns nothing — the same door shape as the
	 * fire, the reload and the second button, so ONE key can mean two things without a single branch on
	 * what is held.
	 *
	 * False when there is nothing in hand: a key with nothing behind it does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool MeleeWithGunInHand();

	/**
	 * Run the bash ability — the melee every GUN has.
	 *
	 * The answer a gun's own melee action gives, and the only thing that knows the ability's tag: the
	 * bash is addressed by its own input tag (the grant carries it), so nothing here knows what the
	 * ability is or where it was granted. False when the ability system is missing or the bash would
	 * not activate (mid-swing, for instance).
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool RunTheBash();

	/**
	 * Reload whatever this body is holding — the one door the rig's reload action needs.
	 *
	 * The body asks the thing in its hand and knows nothing else, exactly as it does for the melee:
	 * a gun swaps its magazine, and a thing with no magazine does nothing at all because its answer
	 * is nothing. The template wired its reload to ONE hand's channel (the rifle's), so the other
	 * hand could never reload in its life — and a weapon's own job must never depend on which hand it
	 * happens to be sitting in.
	 *
	 * False when there is nothing in hand: a key with nothing behind it does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool ReloadTheWeaponInHand();

	/**
	 * Do whatever the LEFT mouse button means for this body's weapon — the one door that press needs.
	 *
	 * The same shape as the two doors below, and for the same reason: the body asks the thing in its
	 * hand and learns nothing else about it. On a gun the answer is a SHOT; on the blade it is a SWING.
	 * Neither is the character's business, and nothing outside the weapon has to know which of them the
	 * player is holding — which is exactly how a sword came to be unable to swing at all: the press was
	 * gated on "is the thing in hand a gun", a question about a WEAPON asked by a character graph.
	 *
	 * False when there is nothing in hand: a button with nothing behind it does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool PrimaryActionTheWeaponInHand();

	/**
	 * Do whatever the right mouse button means for this body's weapon — the one door that press needs.
	 *
	 * The same shape as the reload door above, and for the same reason: the body asks the thing in its
	 * hand and knows nothing else about it. On a gun the answer is AIMING; on the blade it is the
	 * DASH. Neither is the character's business, and nothing here casts a hand to a weapon class —
	 * which is exactly how a sword came to be unusable unless it were dressed up as a rifle.
	 *
	 * False when there is nothing in hand: a key with nothing behind it does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool SecondaryActionTheWeaponInHand();

	/**
	 * The right mouse button coming back up, on the same terms as the press above.
	 *
	 * A gun lets its aim go here; a melee owes nothing, because a swing owes nothing the moment the
	 * button comes up. The body asks and does not learn which of the two answered.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool SecondaryActionReleasedTheWeaponInHand();

	/**
	 * Whether the thing in this body's hand is being AIMED right now — the one question the aim's own
	 * chain asks, every time the right button changes.
	 *
	 * The INTENT is the WEAPON's (see `AProsperitocracyWeapon::IsAiming`) and the mechanism is this
	 * body's: the aim blend, the camera, the crosshair and the pose all hang off this one answer, and
	 * nothing here decides it. A blade never aims, so a sword in hand answers FALSE and the body's
	 * camera stays where it is while the sword does its own thing on the press.
	 *
	 * False when there is nothing in hand: nothing held, nothing aimed.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	bool IsTheWeaponInHandAiming() const;

	/**
	 * Whether this body is holding a GUN — a weapon with a fire mode — which is the project's own test
	 * and the one question the melee key's own chain needs.
	 *
	 * It is the GATE the template's melee chain is behind, and it is asked of the thing in the hand
	 * rather than of anything the graph could guess: the gun-side of that chain (the swing's animation,
	 * its lock) belongs to a GUN, and a sword in hand must not reach any of it. False for a melee, and
	 * false for nothing in hand.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	bool IsTheWeaponInHandAGun() const;

	/**
	 * Bring whatever a slot carries OUT, or put it away. THE one door, and what happens is the
	 * WEAPON's.
	 *
	 * This is the whole of equipping, and it is deliberately this small. Which thing is in the slot
	 * is the loadout's answer; where that thing SITS is the weapon's own answer (its own sockets);
	 * and whether stepping out plays anything is the weapon's answer too (its own draw). So a rifle
	 * comes out to a rifle's socket playing a rifle's draw, and a sword comes out to its socket
	 * playing NOTHING — because it has nothing to play, and nobody here has to know which of the two
	 * it is holding.
	 *
	 * False when the slot carries nothing: a key with nothing behind it does nothing at all.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool SetSlotOut(FGameplayTag Slot, bool bOut);

	/**
	 * THE WHEEL: walk the slots this body carries and choose the next one.
	 *
	 * `Step` is the wheel's own direction (+1 forward, -1 back) and the walk WRAPS, so the wheel is
	 * never stuck at an end. A slot carrying nothing is passed over, which is what keeps an empty
	 * Special slot OPEN rather than in the way: the pipeline answers for it, nothing gets filled in.
	 *
	 * If a thing is OUT the swap is real — the thing that was out goes away and the chosen slot's
	 * thing comes out, both through `SetSlotOut`, which stays the only writer of where a thing sits.
	 * With nothing out, only the choice moves: the wheel is not a draw.
	 *
	 * The order is the LOADOUT's (`GetSlotOrder`) — one home for it, so no graph holds a second.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool CycleSlot(int32 Step);

	/**
	 * HOLD R: bring the chosen slot's thing out, or put it back away.
	 *
	 * The same door as everything else — `SetSlotOut` — so a rifle comes out to a rifle's socket
	 * playing a rifle's draw, a sword simply appears, and nothing here has to know which it is.
	 * Nothing chosen yet asks the loadout for the first slot that carries something, because the
	 * first press has to answer for a body that has never cycled.
	 *
	 * R's OTHER job is untouched and lives elsewhere: the tap is the thing in hand answering
	 * (`ReloadTheWeaponInHand`) — a gun swaps a magazine, a sword turns its blood mode on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool ToggleTheChosenSlot();

	/** Which slot the wheel is sitting on — the body's own state, asked plainly. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	FGameplayTag GetChosenSlot() const { return ChosenSlot; }

	/** Whether the chosen slot's thing is out. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	bool IsTheChosenSlotOut() const { return bSlotIsOut; }

	/**
	 * Put EVERY channel's thing where its state says it goes, and say what the body holds — the whole of
	 * the body's equip state, re-stated at the dress.
	 *
	 * The channel whose slot is OUT goes to that weapon's own HAND socket and names the stance the body
	 * holds; every other one goes to its own BACK socket, and an empty hand names unarmed. Nothing is
	 * drawn and nothing is played: this is the state being said again, not an animation.
	 *
	 * Both halves belong here because the dress RE-BUILDS the rig. Placement alone was half of it: the
	 * stance the animation reads was left over — or, when nothing had written it yet, at the read's own
	 * default, which is UNARMED. That is a gun in the hand playing the unarmed pose, and it is why a
	 * loadout change used to strip the pose off a drawn gun. Called through the dress path (spawn, class
	 * change, loadout change), never by a key.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void PlaceTheChannelsForTheirState();

	/**
	 * What the WHEEL is, and what R is — the two things the game reads as keys.
	 *
	 * They are properties rather than paths spelled in code, so nothing here hardcodes where an asset
	 * lives and the blueprint keeps the one job it is good at: saying which action means what.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Input")
	TObjectPtr<UInputAction> CycleSlotAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Input")
	TObjectPtr<UInputAction> DrawHolsterAction;

	/**
	 * THE FOUR BAR KEYS (1–4), one action each, in bar order.
	 *
	 * The same shape as the wheel and R above, and for the same reason: the blueprint says which asset
	 * means what and nothing here spells a key or a path. The order IS the numbering — the first of
	 * these is the ability in the loadout's first slot — and none of the four names an ability: each
	 * hands the ability system the NUMBER of the slot it stands for, and whatever that loadout granted
	 * in the slot is what answers.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Input")
	TObjectPtr<UInputAction> AbilitySlot1Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Input")
	TObjectPtr<UInputAction> AbilitySlot2Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Input")
	TObjectPtr<UInputAction> AbilitySlot3Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Input")
	TObjectPtr<UInputAction> AbilitySlot4Action;

	/**
	 * The body has been told which stance to hold: the thing that came out declared it.
	 *
	 * A BlueprintImplementableEvent because the stance the ANIMATION reads is a blueprint value on this
	 * body, and code cannot write it directly. So the weapon's answer crosses into the blueprint HERE,
	 * once, and the body's own blueprint keeps the value its animation reads. The stance is the
	 * weapon's to name and the body's to hold — this is the one place the two halves meet.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prosperitocracy|Weapon")
	void OnStanceChosen(EProsperitocracyStance Stance);

	/**
	 * One attack, as the BODY owns it: the line it goes along, how far it is worth, how long the
	 * TRAVEL takes, and how long the ATTACK lasts.
	 *
	 * Two times, deliberately, because they are two different things: the TRAVEL is the dash, and the
	 * attack is the window the player's own movement is refused for. A swing covers its Range EARLY —
	 * by the moment the blade bites — so the dash and the cut land together and the body then holds
	 * its ground for the rest of the swing instead of drifting through it.
	 *
	 * The thing that swings hands all of it over and then holds nothing: the line, the distance and
	 * the two times. From that moment the attack is the body's — it faces the line, travels it, and
	 * refuses its own movement until the attack's clock runs out — and the thing that swung never
	 * touches the body's speed, its velocity or its facing.
	 *
	 * The DISTANCE is the authority, not the clock: the body is placed along its line off its own
	 * clock, so the frame rate cannot shorten the number it was given.
	 *
	 * Beginning an attack while one is live REPLACES it — the newest line and clocks win — which is
	 * what a press that passes through a recovery into the next attack means.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Attack")
	void BeginAttack(const FVector& LineDirection, float DistanceCm, float TravelSeconds, float Seconds);

	/**
	 * Put ONE move's picture on this body, taking off whatever the last move put there.
	 *
	 * THE BODY OWNS THE STAGE, and this is why it has to: a body can only wear one move's animation at
	 * a time, and two of our montages play in the SAME slot — so a second move started without the
	 * first one's picture being taken off does not replace it, it BLENDS with it, and the pose the
	 * player sees is half of each clip. That is a thing that looks like the animation itself is wrong.
	 *
	 * The move being left behind is stopped with ITS OWN blend-out — the animation says how it leaves,
	 * and never a zero-second cut — and the new one comes on with its own blend-in, so a handover
	 * reads as a transition rather than a snap. A section is asked for BY NAME, so moving Rate cannot
	 * move a window that was never in seconds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Attack")
	void BeginMovePicture(UAnimMontage* Montage, FName Section, float PlayRate);

	/**
	 * Take THIS body's move picture off, because the move that put it there has ended.
	 *
	 * The other half of `BeginMovePicture`, and it exists because a move can end WITHOUT another one
	 * starting: a hold that is let go, a combo whose window runs out. Until this existed, nothing took the
	 * picture off in that case — the move's own state stopped (no more bites, no window, no facing) while
	 * its ANIMATION went on playing to the end of the clip, which reads to the player as the move carrying
	 * on after it has clearly ended. That is the mismatch this closes.
	 *
	 * The animation says how it leaves — the same rule as everywhere else: the montage's OWN blend-out, a
	 * quarter of a second on ours, and never a zero cut that jumps the body into locomotion.
	 *
	 * It is deliberately NOT called when a new move takes the stage: in that case the NEW move's own
	 * `BeginMovePicture` does the handover, and stopping the picture here would kill the picture the new
	 * move has just put on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Attack")
	void EndMovePicture();

	/** End a live attack now. A live attack ends itself when its clock runs out; this is a cut short. */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Attack")
	void EndAttack();

	/**
	 * Let the FACING go.
	 *
	 * The facing is the attack's from the press until the thing that swung says the combo is over —
	 * it ran out, or the player moved out of the recovery behind it. This is that word, and from here
	 * the body TURNS back to where the player is looking at its own rate: a turn, never a snap.
	 *
	 * The combo's own running is the SWORD's business (a body cannot see a montage) and the turn is
	 * the BODY's, because the facing is the body's — so this is the one thing that crosses between
	 * them, and it is one call with no numbers in it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Attack")
	void ReleaseFacing();

	/**
	 * Let the PLAYER'S OWN MOVEMENT go — the attack stops refusing his input, right now, without ending.
	 *
	 * The other half of what an attack holds: an attack is handed a line, a distance and two times, and one
	 * of those times (`Seconds`) is how long HIS movement is refused for. This ends that refusal early.
	 *
	 * It exists for a move whose LOCK is longer than its swing — the heavy combo holds the player for the
	 * whole BEAT while its key is down, so that nothing he does with his legs interrupts the chain, and the
	 * moment he lets go the recovery is his own time again, which is exactly where walking out of it
	 * becomes possible. A move that hands over the attack alone has no use for it.
	 *
	 * What it does NOT do: it does not end the attack, it does not snap the body to the distance it was
	 * worth, and it does not touch the facing. He keeps the ground he has already covered, and the swing
	 * keeps its line. It is a hand-over, not a write — the thing that swung never moves the body itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Attack")
	void ReleaseMovementLock();

	/**
	 * Whether an attack is live on this body.
	 *
	 * True for the whole of every attack and false in a recovery: an attack refuses the player's own
	 * movement, and a recovery is where he gets it back.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Attack")
	bool IsSwingLocked() const { return bAttackLive; }
	/**
	 * How far into aiming down sights we are: 0 = hipfire, 1 = fully aiming.
	 *
	 * The template's `Aim_Smooth` timeline IS that blend — it already drives the camera boom and the
	 * crosshair — so the blueprint reports its alpha here instead of anyone recomputing it from the
	 * boom length. The same number drives the posture multiplier on Accuracy and the reticle
	 * circle's fade, which is why the circle always appears with the camera, never on a separate clock.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Prosperitocracy|Aim")
	float GetAimingAlpha() const;
	virtual float GetAimingAlpha_Implementation() const;

	/**
	 * The skeletal mesh this body is DRAWN with — the one you actually see, or null when the
	 * blueprint has not said which it is.
	 *
	 * A character here has two skeletal meshes: the one it is ANIMATED from (the hidden mannequin,
	 * what `ACharacter::GetMesh()` returns) and the one on screen — the retargeted visible body the
	 * armor is painted on. Which component is which is a fact about the rig, and nothing in C++ can
	 * honestly work it out: a mesh being visible is not a statement about what it is, and the answer
	 * must not be guessed from a proxy. So the blueprint answers, exactly as it does for the gun in
	 * hand and the aim alpha.
	 *
	 * Null is a truthful answer, not a failure: a body that does not say which mesh is drawn has
	 * nothing painted on it, and whatever asked says so rather than painting the wrong mesh.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Prosperitocracy|Body")
	USkeletalMeshComponent* GetBodyMesh() const;
	virtual USkeletalMeshComponent* GetBodyMesh_Implementation() const;

	/**
	 * THE BODY IS CAUGHT BY A BLAST AND THROWN — the one door every thrown body in the game comes through.
	 *
	 * `ShoveVelocityCmS` is the explosion's own number (the damage that came off this body, turned into a
	 * speed by the explosion pass); `HitPointWorld` is where the ball touched, and the throw goes on the
	 * bone nearest it, which is Epic's own first step: "Get the name of the bone that was hit."
	 *
	 * Nothing here makes him limp: the simulation and the blend are the ANIMATION BLUEPRINT's nodes (see
	 * `ProsperitocracyBodiesThrown` above). This door states the state those nodes read.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Body")
	void ThrowTheBody(const FVector& ShoveVelocityCmS, const FVector& HitPointWorld);

	/** Whether this body is limp — the animation blueprint's switch for simulating, and the body's own state. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Body")
	bool IsBodyLimp() const { return bBodyLimp; }

	/**
	 * THE NUMBER THE ANIMATION BLUEPRINT'S BLEND-WEIGHT NODE IS DRIVEN WITH, every tick: 0 is the
	 * animation's pose, 1 is the physics'. Animated by this body, applied by that node, read nowhere else.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Body")
	float GetBodyPhysicsBlendWeight() const { return PhysicsBlendWeight; }

	/**
	 * Whether the physics should be simulating AT ALL — true while he is limp and, on the way out, until
	 * the weight has finished coming back down. Epic's own last line: "Once the reaction is complete and
	 * the Physics Blend Weight returns to 0, you should use the Set All Bodies Below Simulate Physics node
	 * once more to deactivate the simulation." The switch is the node's; this is its input.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Body")
	bool ShouldBodySimulatePhysics() const;

	/**
	 * ONE FRAME, and only the frame the simulation state changed — the edge Epic's own simulate node is
	 * called on.
	 *
	 * The page calls that node ONCE to activate the simulation and ONCE more to deactivate it; it is the
	 * WEIGHT node that is driven at each tick. The engine's own code says why that matters:
	 * `FBodyInstance::SetInstanceSimulatePhysics` forces the body's `PhysicsBlendWeight` to 1 (or 0) and then
	 * `UpdateInstanceSimulatePhysics` **wakes the body** — `WakeUp_AssumesLocked`, `BodyInstance.cpp:2600-2611`.
	 * A body told to simulate every frame is therefore a body that can never fall asleep, and a ragdoll whose
	 * bodies never sleep jitters where it lies and never counts as stopped — which is exactly what "stuck
	 * jittering, never gets up" is. Driving both nodes every frame was the mistake; this is the edge that
	 * lets the blueprint call the simulate node on the transition, as the page does.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Body")
	bool DidBodySimulateJustChange() const { return bBodySimulateJustChanged; }

	/**
	 * THE BONE EPIC'S TWO NODES START FROM — "starting at a given bone and moving recursively down the bone
	 * chain". The chain's own start on this rig is the pelvis (`PA_Mannequin`'s first body), and starting
	 * there is what makes the WHOLE man go limp: the doc's own example starts at the bone that was HIT
	 * because its example is a stagger of one limb, and a body thrown by a blast is the whole chain. The
	 * bone the FORCE lands on is still the hit bone (`ThrowTheBody`); this is the bone the physics starts
	 * from.
	 *
	 * It is handed over from here rather than typed into the animation blueprint so the bone's name has ONE
	 * home — which is the same move the doc describes: "Transfer that bone name into the Character's
	 * Animation Blueprint for the Event Graph to use."
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Body")
	FName GetBodyPhysicsBoneName() const;

	/**
	 * The loadout has just been dressed — at spawn, and after EVERY change to what this character
	 * carries: a class, a loadout, a gun, a weave, a colour.
	 *
	 * ONE place, so the rig's hand channels are brought in line from the loadout exactly when the
	 * loadout is applied, whether that change came from a switch, a dev command or the picker — and
	 * never from a dozen spots each remembering to do it separately. The blueprint implements this:
	 * which component is a channel, and where a channel's body hangs, is the blueprint's business;
	 * WHAT belongs in that channel is the loadout's, and nothing else decides it.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prosperitocracy|Loadout")
	void OnLoadoutDressed();

	/**
	 * WHERE THIS MAN'S GUN IS POINTING — the one answer every reader asks.
	 *
	 * His own turn (the facing, and the pitch, at the rate his load allows) PLUS the gun's own
	 * numbers (the climb from Recoil, the shove from Accuracy, the movement's inertia). One rotation,
	 * composed in one place, so the bullet, the reticle circle and the pose cannot disagree about
	 * where the next shot goes.
	 *
	 * Nothing outside this needs to know how the two are put together: a reader asks for the aim.
	 * No gun in hand = his turn alone.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Aim")
	FRotator GetAimRotation() const;

	/**
	 * How fast this man comes round right now, in degrees per second — from what he is CARRYING and what
	 * he is HOLDING.
	 *
	 * Two layers, and both of them weight. Everything on him — armour and every gun, not just the one in
	 * his hands — sets the rate he turns at with an empty hand; the thing in his hand then multiplies it
	 * off its own Weight. Nothing carried is the quickest this body turns, and nothing in hand means no
	 * multiply at all.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Aim")
	float GetTurnRateDegreesPerSecond() const;

	/**
	 * The aim the body and the animation read — THE MAN'S AIM, whole.
	 *
	 * The template's animation blueprint takes its aim offset from this function, so this is the one
	 * place the pose is handed where the gun points: the arms and gun come round with the man, and the
	 * climb and shove move the arms because they moved his aim. Nothing is added here to make the
	 * animation match anything — the animation is given the aim itself.
	 */
	virtual FRotator GetBaseAimRotation() const override;

	//~IAbilitySystemInterface — where this body's damage lands.
	//
	// The body's ability system is the one the stats component owns (the template's blueprint puts the
	// component on the character; C++ never creates it). Answering it here is what makes this body
	// damageable at all: the gun asks the hit actor for its ability system, and without this the hit has
	// nowhere to go. Null when the body has no stats component, which is a truthful answer.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//~IProsperitocracyDamageReceiver — what this body answers a hit with.
	//
	// One part (the weave), no armour, and the body's own resist for the line's type, read FINAL off the
	// body's rows. Nothing here reads a base and nothing here decides anything: the ONE pipeline asks, and
	// these are the numbers it gets.
	virtual FProsperitocracyDamageProfile GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const override;
	virtual float GetBodyResist_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const override;

protected:
	/**
	 * Where a STATUS on this body lives — the one component that carries a status's numbers, its
	 * ticking, and the movement gate a stun closes.
	 *
	 * It is on the character rather than in a blueprint because every body that can carry a status
	 * carries the SAME component (the damage dummies have it too), so nothing about being stunned is
	 * special to the player. The player is simply the only body with controls to take away today.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prosperitocracy|Status")
	TObjectPtr<UProsperitocracyStatusComponent> Statuses;

	/**
	 * The direction a live attack travels and faces, taken once when it began and then held.
	 * FLAT, always: an attack goes along the ground the player is standing on, whatever the camera
	 * was doing with its pitch — which is what keeps an attack straight while he is still free to
	 * turn and look wherever he likes for the next one.
	 */
	FVector AttackLine = FVector::ZeroVector;

	/** Where the body was when the attack began — what its number is measured from — and what it is worth. */
	FVector AttackStartLocation = FVector::ZeroVector;
	float AttackDistanceCm = 0.0f;

	/** The attack's own clock: how long it lasts, and how far into it the body is. */
	float AttackSeconds = 0.0f;
	float AttackElapsed = 0.0f;

	/**
	 * How long the TRAVEL takes — the dash itself, which is shorter than the attack.
	 *
	 * The attack's clock above is how long the player's movement is refused for; this is how long the
	 * body takes to cover its Range. They were one number, which spread the dash thin across the whole
	 * swing; the dash belongs at the front of it, arriving as the blade does.
	 */
	float AttackTravelSeconds = 0.0f;

	/** True between an attack beginning and its own clock running out. Runtime only. */
	bool bAttackLive = false;

	/**
	 * The montage the live move last put on this body — the picture this body is wearing.
	 *
	 * Held so the NEXT move can take it off with its own blend-out rather than leaving two animations
	 * blending in one slot, which reads to the player as the animation itself being broken. Weak, so a
	 * montage going away is never held alive by this.
	 */
	TWeakObjectPtr<UAnimMontage> MovePicture;

	/**
	 * True while the FACING belongs to the attack — from the press, through the attack, and through
	 * the recovery behind it.
	 *
	 * It outlives `bAttackLive` ON PURPOSE, and that is the whole of the feel: an attack stops
	 * TRAVELLING when its clock is out, and the body goes on facing its line while the recovery
	 * plays, so the turn back to the player's look only begins once the combo is actually over.
	 */
	bool bFacingHeld = false;

	/**
	 * THE MAN'S AIM — his facing, and where his gun points up and down — and it is the BODY's own state.
	 *
	 * His, not the camera's: the controller's rotation is only what he is TURNING TOWARD, and he gets
	 * there at the rate his load allows (GetTurnRateDegreesPerSecond). His yaw is written from this
	 * every frame, which is why the whole man comes round late instead of the arms being bent to
	 * pretend. The gun's own numbers are added on top when anyone asks for the aim (GetAimRotation),
	 * and are never stored here.
	 */
	FRotator AimRotation = FRotator::ZeroRotator;

	/**
	 * False until this body has met its controller once, so the first frame SNAPS the aim to the look
	 * instead of measuring him as having been left behind by every frame since the world began.
	 */
	bool bAimInitialized = false;

	/** Counts down between the aim's own log lines, so a play test can see the turn without spam. */
	float AimLogCooldown = 0.0f;

	/** One frame of a live attack: the body is placed along its line and turned to face it. */
	void TickAttack(float DeltaSeconds);

	/**
	 * One frame of THE MAN'S OWN TURN: the aim comes round toward the look at the rate his load allows
	 * and what he is holding allows — never faster, and never instantly — and his facing is written
	 * from it.
	 */
	void TickAimTurn(float DeltaSeconds);

	/**
	 * The multiply the thing in his hands puts on that turn, off its own Weight: one at an empty hand,
	 * one step down for every pound it weighs, floored.
	 *
	 * ONE home for the number, so the rate and the log line that explains it can never disagree about
	 * it — and the rate and the log cannot drift apart the way two copies of a sum always do.
	 */
	float GetEquippedWeightMultiplier() const;

	/**
	 * WHICH SLOT IS CHOSEN, and WHETHER ITS THING IS OUT — the body's equip state, and the whole of it.
	 *
	 * Two facts and no more: the wheel's position (the slot it is sitting on) and whether that slot's
	 * thing is in the hand or away. There is deliberately NO memory of a previous slot: a thing that
	 * is not chosen is not remembered, it is simply not chosen.
	 *
	 * `SetSlotOut` is the only writer of both, which is what keeps the answer to "what is in hand"
	 * from ever disagreeing with where things actually are.
	 */
	FGameplayTag ChosenSlot;
	bool bSlotIsOut = false;

	/**
	 * The CHANNEL carrying a slot, found by asking each channel's thing which slot it is — never by a
	 * list of component names kept here, because a list like that is a second place that has to agree
	 * with the rig. Null when nothing carries that slot.
	 */
	UChildActorComponent* FindChannelForSlot(FGameplayTag Slot) const;

	/**
	 * The keys, read HERE and not in a graph — the one place the project's own key events cannot reach.
	 *
	 * A key EVENT node is the one node no tool this project has can make (measured three ways:
	 * `create_node` in a throwaway blueprint, `create_node` in this very graph, and the graph DSL
	 * compiler, all refusing `Input|EnhancedActionEvents|...`). R also needs a TAP/HOLD measure that a
	 * key mapping cannot express on its own. So the blueprint keeps the mapping context and the two
	 * actions, and every decision a key makes is one of the doors above — the wheel walks the slots,
	 * the hold draws or puts away, and the tap is the thing in hand answering.
	 */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** The wheel, as read: +1 forward, -1 back, straight off the axis it is mapped to. */
	void OnCycleSlot(const FInputActionValue& Value);

	/** R went down: arm the hold. A press that never reaches the window is the TAP. */
	void OnDrawHolsterStarted();

	/** R came up inside the window: that is the tap — the thing in hand answers. */
	void OnDrawHolsterReleased();

	/** R stayed down past the window: that is the hold — bring the chosen slot's thing out, or put it away. */
	void FireTheDrawHolsterHold();

	/** Live while R is down inside the window. A finished handle means the hold already happened. */
	FTimerHandle DrawHolsterHoldTimer;

	/**
	 * A bar key went down, and came up. Eight lines of nothing beyond that, because a number key has
	 * exactly ONE job — saying which slot it stands for — and no per-key behaviour to keep in step with
	 * anything: the numbering lives in the tag each pair hands over, and the pair at the bottom is what
	 * actually touches the game.
	 */
	void OnAbilitySlot1Started();
	void OnAbilitySlot1Released();
	void OnAbilitySlot2Started();
	void OnAbilitySlot2Released();
	void OnAbilitySlot3Started();
	void OnAbilitySlot3Released();
	void OnAbilitySlot4Started();
	void OnAbilitySlot4Released();

	/** THE ONE THING A NUMBER KEY DOES: the press hands the slot's number over, the release takes it back. */
	void PressAbilitySlot(const FGameplayTag& Number);
	void ReleaseAbilitySlot(const FGameplayTag& Number);

	/**
	 * A THROWN BODY'S STATE, and the whole of it: whether he is limp, where the weight is (the animation
	 * blueprint's node applies this number every tick), the force waiting to go on, whether he has come to
	 * rest, and how long he has been lying there.
	 *
	 * The force waits ONE frame on purpose: at a weight of zero the animation still writes the bodies every
	 * frame, so an impulse landing in that same frame is written straight back out again. It goes on the
	 * first frame the weight has left zero.
	 */
	bool bBodyLimp = false;
	float PhysicsBlendWeight = 0.0f;
	FVector PendingThrow = FVector::ZeroVector;
	FName PendingThrowBone = NAME_None;
	bool bThrowPending = false;
	bool bBodyResting = false;
	float RestingSeconds = 0.0f;

	/**
	 * THE MESH'S COLLISION WHILE HE IS LIMP — and this one is the ENGINE's requirement, not the page's.
	 *
	 * A skeletal mesh on a character wears the engine's own `CharacterMesh` profile, which the engine ships
	 * as `CollisionEnabled=QueryOnly` (`Config/BaseEngine.ini`). With no physics collision there is NO physics
	 * state and NO valid bodies: the engine refuses the impulse ("has to have 'CollisionEnabled' set to
	 * 'Query and Physics' or 'Physics only' if you'd like to AddImpulse"), refuses to put the bodies in the
	 * scene (`Invalid Bodies : Make sure collision is enabled or root bone has body in PhysicsAsset`,
	 * `SkeletalMeshComponentPhysics.cpp`), and the blend then reads bones that do not exist — which is a pose
	 * frozen on one frame and a body that vanishes. So while he is limp the mesh wears the profile the engine
	 * ships FOR a simulating mesh, the `Ragdoll` preset (`QueryAndPhysics`, `PhysicsBody`, ignoring Pawn and
	 * Visibility), and what it replaced is remembered here and given back when he is up.
	 */
	FName CollisionProfileBeforeLimp;

	/**
	 * THE CAPSULE THE MESH LETS GO OF WHILE HE IS LIMP, and where the mesh sat inside it.
	 *
	 * A simulating mesh must not be dragged by a moving parent: moving a component that has simulating
	 * bodies TELEPORTS those bodies with it, so a mesh left attached to a capsule that is being moved has
	 * its ragdoll pinned in place — a pose frozen where the blast caught him, with the character still able
	 * to walk. So the mesh is detached for the whole of the limp (keeping its world transform, because the
	 * physics owns it now), the capsule follows the BODY instead, and this is what `GetTheBodyUp` puts back:
	 * the parent, and the transform the mesh sat at inside it.
	 */
	TObjectPtr<USceneComponent> LimpAttachParent;
	FTransform MeshTransformBeforeLimp = FTransform::Identity;

	/** The simulation state's own edge, and where it was last frame (see `DidBodySimulateJustChange`). */
	bool bBodySimulateJustChanged = false;
	bool bBodyWasSimulating = false;

	/** One frame of a limp body: the weight's travel, the force, the wait for him to stop, the let-go. */
	void TickLimpBody(float DeltaSeconds);

	/** The reaction is over: the weight is let go back to 0, and the animation owns him again. */
	void GetTheBodyUp();

	/** What this body WEIGHS for a throw, in pounds — read FINAL through the one evaluator, as the
	 * movement and the turn read it. */
	float GetThrowingWeightLbs() const;

	/** How much of a blast's throw his own weight keeps — ONE home for the arithmetic, read by the throw
	 * and by the line that explains it, so the two cannot disagree about it. OURS, the only one. */
	float GetThrowWeightDamping() const;

	/** This body's own tick, which is where the aim's turn and a live attack are driven. */
	virtual void Tick(float DeltaSeconds) override;
};
