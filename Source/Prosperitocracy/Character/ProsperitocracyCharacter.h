// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "ProsperitocracyCharacter.generated.h"

class AProsperitocracyWeapon;

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
 * Two facts belong to the rig and cannot be guessed from C++, so the blueprint answers them. Each is
 * a BlueprintNativeEvent with an honest empty default, which is the whole door:
 *   - which gun is in hand (`GetGunInHand`) — the template's equip state decides it;
 *   - how far into ADS we are (`GetAimingAlpha`) — the template's `Aim_Smooth` timeline owns it.
 *
 * Nobody recomputes either from a proxy: not from a mesh being visible, not from a camera boom
 * length. There is one answer, and it comes from the thing that owns the fact.
 */
UCLASS()
class AProsperitocracyCharacter : public ACharacter
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
	 * The player's melee: the gun in hand swings. False when there is nothing in hand, or when the
	 * swing found nothing to hit.
	 *
	 * The one door the rig's melee action needs. Which gun is in hand stays the rig's answer and the
	 * swing stays the gun's (its Weight is the damage), so the body needs to know neither — it makes
	 * one call per swing, whatever is held, and there is no cast to a gun type anywhere in the graph.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool MeleeWithGunInHand();
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
	 * The aim the body and the animation read: the base aim rotation plus the in-hand gun's drift.
	 *
	 * The drift is the SAME value the bullet flies along and the circle sits on, added the same way
	 * in all three places (Design/ui.md: every bullet lands exactly where the circle points).
	 */
	virtual FRotator GetBaseAimRotation() const override;
};
