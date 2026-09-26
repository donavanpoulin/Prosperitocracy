// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AbilitySystem/ProsperitocracyDamageReceiver.h"

#include "ProsperitocracyCharacter.generated.h"

class AProsperitocracyWeapon;
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
UCLASS()
class AProsperitocracyCharacter : public ACharacter, public IAbilitySystemInterface, public IProsperitocracyDamageReceiver
{
	GENERATED_BODY()

public:
	AProsperitocracyCharacter();

	/**
	 * The gun this character is holding right now, or null when nothing is in hand.
	 *
	 * The rig decides which gun is in hand, so the answer comes from the blueprint; C++ only asks.
	 * It answers with the ACTOR because that is what the rig actually holds (a child actor on one of
	 * its rig components), and the type is resolved right here, once per reader — the same shape as
	 * `GetChildActor` itself. Null is a truthful answer, not a failure: no gun in hand means no drift,
	 * so the aim stays the plain camera aim everywhere it is read.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Prosperitocracy|Aim")
	AActor* GetGunInHand() const;
	virtual AActor* GetGunInHand_Implementation() const;

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

	/** This body's own tick, which is where the aim's turn and a live attack are driven. */
	virtual void Tick(float DeltaSeconds) override;
};
