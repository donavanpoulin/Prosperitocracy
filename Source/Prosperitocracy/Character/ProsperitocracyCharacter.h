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

	/** The gun in hand, resolved to our weapon type, or null when there is none. */
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
	 * One attack, as the BODY owns it: the line it goes along, how far it is worth, and how long it lasts.
	 *
	 * Called by the thing that swings, with the thing's own two numbers — a blade hands over its Range and
	 * the length of its OWN piece of the combo — and from that moment the attack is the body's: the body
	 * faces the line, travels it, and refuses its own movement input until the clock runs out. Nothing
	 * else moves this body while it is live, and the thing that swung never touches the body's speed, its
	 * velocity or its facing.
	 *
	 * Beginning an attack while one is live REPLACES it — the newest line and clock win — which is what a
	 * press that passes through a recovery into the next attack means.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Attack")
	void BeginAttack(const FVector& LineDirection, float DistanceCm, float Seconds);

	/** End a live attack now. A live attack ends itself when its clock runs out; this is for a cut short. */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Attack")
	void EndAttack();

	/**
	 * Whether an attack is live on this body — the question the movement gate asks.
	 *
	 * True for the whole of every attack, the third one included (it is a dash over its whole animation),
	 * and false in a recovery: an attack refuses the player's own movement, and a recovery is where the
	 * player gets it back.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Character")
	bool IsSwingLocked() const { return bAttackLive; }

	/**
	 * Whether the thing in this body's hands is a RANGED weapon — a gun.
	 *
	 * The project's own test, not a new vocabulary: a gun is a weapon with a fire mode, and a melee is a
	 * weapon without one (Design/weapons.md). Everything that must look or behave differently for a melee
	 * — the upper body's pose above all — can ask THIS one question, instead of asking a weapon's name,
	 * which is what the template's rifle/pistol flags do and why a sword inherited a rifle's pose.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Character")
	bool IsHoldingRangedWeapon() const;

	/**
	 * Whether the thing in this body's hands is a MELEE — the other half of the same question, asked once
	 * so nothing has to negate it in its head.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Character")
	bool IsHoldingMeleeWeapon() const;

protected:
	/** True between an attack beginning and its own clock running out. Runtime only. */
	UPROPERTY(Transient)
	bool bAttackLive = false;

	/** The direction the live attack travels and faces, taken once when it began and then held. Flat, always. */
	FVector AttackLine = FVector::ZeroVector;

	/** Where the body was when the attack began — what its number is measured from — and what it is worth. */
	FVector AttackStartLocation = FVector::ZeroVector;
	float AttackDistanceCm = 0.0f;

	/** The attack's own clock: how long it lasts, and how far into it the body is. */
	float AttackSeconds = 0.0f;
	float AttackElapsed = 0.0f;

	/** What `bUseControllerRotationYaw` was before the attack took the facing over, so it goes back. */
	bool bControllerYawBeforeAttack = true;

	/** One frame of a live attack: the body is placed along its line and turned to face it. */
	void TickAttack(float DeltaSeconds);

	/** This body's own tick, which is where a live attack is driven. */
	virtual void Tick(float DeltaSeconds) override;

public:
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
};
