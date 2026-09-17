// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyCharacter.h"

#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyCharacter)

AProsperitocracyCharacter::AProsperitocracyCharacter()
{
}

AActor* AProsperitocracyCharacter::GetGunInHand_Implementation() const
{
	// There is no truth for this in C++: the rig decides which gun is in hand, and the rig is the
	// template's equip state in the blueprint. Returning null is the honest default — "nothing in
	// hand" — and every reader treats that as no drift rather than inventing a gun.
	return nullptr;
}

AProsperitocracyWeapon* AProsperitocracyCharacter::GetGunWeaponInHand() const
{
	// The rig answers with the actor it holds; this is where that becomes OUR gun. A rig holding
	// something that is not one of our weapons reads as no gun at all — no drift, plain camera aim.
	return Cast<AProsperitocracyWeapon>(GetGunInHand());
}

float AProsperitocracyCharacter::GetAimingAlpha_Implementation() const
{
	// Same story: the template's Aim_Smooth timeline owns the ADS blend, so the blueprint reports it.
	// Hipfire is the honest default.
	return 0.0f;
}

FRotator AProsperitocracyCharacter::GetBaseAimRotation() const
{
	FRotator AimRotation = Super::GetBaseAimRotation();

	// The animation's aim pose must follow the reticle circle: the drift (handling trail from Weight,
	// movement trail, per-shot spread from Accuracy, recoil climb) is the same value the bullet flies
	// along and the circle sits on, so adding it here is what makes the character's gun and arms point
	// where the bullet will land. No gun in hand = no drift, and the pose keeps the plain camera aim.
	if (const AProsperitocracyWeapon* Gun = GetGunWeaponInHand())
	{
		const FVector2D Drift = Gun->GetAimDriftDegrees();
		AimRotation.Pitch += Drift.Y;
		AimRotation.Yaw += Drift.X;
	}

	return AimRotation;
}
