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
 * AProsperitocracyCharacter
 *
 * The player character's C++ half: the one place our aim behaviour reaches the body.
 *
 * The template keeps everything it did before — mesh, animation, movement, the rig, the HUD widget.
 * What lives here is the one thing that cannot live in a blueprint: the `GetBaseAimRotation`
 * override. The template's own animation blueprint (`Content/ThirdPerson/Blueprints/Locomotion.uasset`)
 * already drives its aim offset from `GetBaseAimRotation`, so overriding that one function is what
 * makes the character's gun and arms point where the next bullet will actually land — the same aim
 * the reticle circle and the bullet use (Design/combat.md: "the sway feeds the same aim the circle,
 * the bullet, and the character pose read").
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
	 * The player's melee: run the bash ability. False when there is nothing in hand, when the ability
	 * system is missing, or when the ability would not activate (mid-swing, for instance).
	 *
	 * The one door the rig's melee action needs. The bash is an ability with its own stat block
	 * (UProsperitocracyGameplayAbility_Bash), so the body needs to know nothing about it — not which
	 * gun is in hand, not how far the swing reaches, not what it is worth. It makes one call per swing,
	 * whatever is held, and there is no cast to a gun type anywhere in the graph.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool MeleeWithGunInHand();

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
	 * The aim the body and the animation read: the base aim rotation plus the in-hand gun's drift.
	 *
	 * The drift is the SAME value the bullet flies along and the circle sits on, added the same way
	 * in all three places (Design/ui.md: every bullet lands exactly where the circle points).
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
	 * True while the FACING belongs to the attack — from the press, through the attack, and through
	 * the recovery behind it.
	 *
	 * It outlives `bAttackLive` ON PURPOSE, and that is the whole of the feel: an attack stops
	 * TRAVELLING when its clock is out, and the body goes on facing its line while the recovery
	 * plays, so the turn back to the player's look only begins once the combo is actually over.
	 */
	bool bFacingHeld = false;

	/** True while the body is turning back to where the player is looking, after the facing was let go. */
	bool bTurningToLook = false;

	/** What `bUseControllerRotationYaw` was before the attack took the facing over, so it goes back. */
	bool bControllerYawBeforeFacing = true;

	/** One frame of a live attack: the body is placed along its line and turned to face it. */
	void TickAttack(float DeltaSeconds);

	/** One frame of the turn back to the player's look. A turn, never a snap. */
	void TickFacingTurn(float DeltaSeconds);

	/** The facing is the controller's again, from here on. */
	void FinishFacingTurn();

	/** This body's own tick, which is where a live attack and the turn behind it are driven. */
	virtual void Tick(float DeltaSeconds) override;
};
