// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "ProsperitocracyReticleWidgetBase.generated.h"

class AProsperitocracyCharacter;
class AProsperitocracyWeapon;
class UImage;

/**
 * UProsperitocracyReticleWidgetBase
 *
 * The universal aim reticle (Design/ui.md): exactly two layers, the same for every weapon.
 *
 *   - "Dot"    — the fixed dot at screen centre. Never moves. Where you WANT to aim.
 *   - "Circle" — a constant-size marker at the landing point. Where the bullet WILL hit.
 *
 * The circle is moved every frame, never resized: inaccuracy is the circle sitting somewhere other
 * than the dot, not a circle growing. And it is not moved from a number of its own — it is placed on
 * THE SHOT'S OWN LINE, asked of the gun (the muzzle, the man's own aim, and the one trace the
 * bullet travels), so the circle is where the bullet would land because it is the same answer the
 * bullet uses. A wall in the way puts the circle on the wall.
 *
 * It is an ADS-only aid: at hipfire only the dot shows, and the circle fades in on the gun's own
 * aiming state. It also cover-snaps: the circle sits on whatever is between the player and the aim,
 * because that is where the bullet will actually stop.
 *
 * The two images are found by name in the widget tree, so the widget blueprint holds the layout and
 * this class holds the behaviour — the same split the template uses everywhere else.
 */
UCLASS(Abstract)
class UProsperitocracyReticleWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UProsperitocracyReticleWidgetBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Blueprint hook: the gun the reticle is currently reading changed (a swap, or nothing in hand). */
	UFUNCTION(BlueprintImplementableEvent)
	void OnWeaponInitialized();

	/** Point the reticle at a gun. Null is allowed and means "no gun in hand". */
	UFUNCTION(BlueprintCallable)
	void InitializeFromWeapon(AProsperitocracyWeapon* InWeapon);

	/**
	 * Where the circle goes, in viewport pixels: the landing point of the next shot.
	 *
	 * Asked of the gun — the barrel's own line, and the one trace the bullet travels — so the circle
	 * sits on what the shot would hit, or on the end of the gun's own range in open air.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	FVector2D ComputeCircleScreenPosition() const;

	/**
	 * The circle's constant screen radius, in pixels. It NEVER changes — the gun's inaccuracy is the
	 * circle's POSITION, never its size (Design/ui.md: "not a bloom or spread-size indicator").
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetCircleScreenRadius() const;

protected:
	/** Finds the fixed dot and the moving circle by name in the widget tree ("Dot" / "Circle"). */
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(Transient)
	TObjectPtr<UImage> DotImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CircleImage;

	/** The gun whose drift the circle shows — the one in hand, re-read every tick. */
	UPROPERTY(BlueprintReadOnly, Transient)
	TObjectPtr<AProsperitocracyWeapon> WeaponInstance;

private:
	/** The character this reticle belongs to, or null when there is none yet. */
	AProsperitocracyCharacter* GetOwningCharacter() const;

	/** How visible the circle is right now: it follows the gun's aiming state at CircleFadeRate. */
	float CircleOpacity = 0.0f;

	/**
	 * Keep the bound gun equal to the one in hand.
	 *
	 * Re-read every tick on purpose. Binding once froze the circle on a gun that had been put away:
	 * its drift stopped updating while the newly drawn gun's recoil and spread lived on an instance
	 * the circle never read. One reticle, one source — the gun in hand right now.
	 */
	void AcquireWeaponFromCharacter();
};
