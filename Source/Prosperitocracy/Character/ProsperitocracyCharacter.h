// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "ProsperitocracyCharacter.generated.h"

class AProsperitocracyWeapon;
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
};
