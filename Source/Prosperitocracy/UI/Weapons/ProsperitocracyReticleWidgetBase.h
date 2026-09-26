// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "UI/ProsperitocracyHitMarkerTypes.h"

#include "ProsperitocracyReticleWidgetBase.generated.h"

class AProsperitocracyCharacter;
class AProsperitocracyWeapon;
class UAudioComponent;
class UImage;
class UProsperitocracyHitMarkerWidget;
class USoundBase;

/**
 * UProsperitocracyReticleWidgetBase
 *
 * The universal aim reticle (Design/ui.md): exactly two layers, the same for every weapon.
 *
 *   - "Dot"    — the fixed dot at screen centre. Never moves. Where you WANT to aim.
 *   - "Circle" — a constant-size marker at the landing point. Where the bullet WILL hit.
 *
 * The circle is moved every frame, never resized: inaccuracy is the circle sitting somewhere other
 * than the dot, not a circle growing. Its offset is the gun's own drift (handling trail and movement
 * trail from Weight, per-shot spread from Accuracy, recoil climb from Recoil), which is the very same
 * value the bullet flies along — so the circle genuinely is the landing point.
 *
 * It is an ADS-only aid: at hipfire only the dot shows, and the circle fades in with the template's
 * own ADS blend. It also cover-snaps: the circle sits on whatever is between the player and the aim,
 * because that is where the bullet will actually stop.
 *
 * And it is where the HIT MARKERS live (his spec, 2026-09-25), because a marker is part of the ring's
 * UI: it sits inside the ring, it is moved by the very same value the ring is moved by, and it is
 * shown only while the ring is. The widget tree holds the look ("Dot", "Circle", "HitMarker") and this
 * class holds the behaviour — the same split as everywhere else in the template.
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
	 * The camera aim plus the gun's drift, along whatever the shot would hit first (cover snap), or a
	 * reference distance in open air.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	FVector2D ComputeCircleScreenPosition() const;

	/**
	 * The circle's constant screen radius, in pixels. It NEVER changes — the gun's inaccuracy is the
	 * circle's POSITION, never its size (Design/ui.md: "not a bloom or spread-size indicator").
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetCircleScreenRadius() const;

	/**
	 * A marker arrived for the man this reticle belongs to: damage HE dealt just landed.
	 *
	 * Bound to his own ability system's OnHitMarker (see UProsperitocracyHitMarkerStatics), so this
	 * only ever hears about his own damage — nobody else's hits can reach his screen.
	 */
	UFUNCTION()
	void HandleHitMarker(EProsperitocracyHitMarkerKind Kind);

	//~ The marker's timing and its sounds. [TUNE] — mine to pick, his to move once he has felt them.
	//
	// One marker at a time, and it lasts long enough to be seen and no longer: burn ticks land every
	// quarter second while something is alight, so a marker that lingered would never leave the screen.
	static constexpr float HitMarkerLifetimeSeconds = 0.4f;
	static constexpr float KillMarkerLifetimeSeconds = 0.6f;

	/** The share of a marker's life it spends at full strength before it starts to fade. */
	static constexpr float HitMarkerHoldFraction = 0.25f;

	/**
	 * How loud a marker's sound is played. [TUNE]
	 *
	 * His report, 2026-09-25: the gunshots bury it. Measured off his own three files — they peak at
	 * about -6 dBFS (half scale), so 2.0 is the most this can be lifted before THEY clip. If a marker
	 * still loses to the guns, the levers left are the files themselves (louder, punchier — they are
	 * 0.096s clicks) or the guns' own level, which is a feel call and not this number's business.
	 */
	static constexpr float HitMarkerSoundVolume = 2.0f;

	/**
	 * The sound cap: the same marker's sound will not restart more than once in this long, so a horde
	 * alight (a burn ticking on a dozen bodies) cannot machine-gun it.
	 */
	static constexpr float HitMarkerSoundRetriggerSeconds = 0.1f;

protected:
	/** Finds the fixed dot, the moving circle and the marker by name in the widget tree. */
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(Transient)
	TObjectPtr<UImage> DotImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CircleImage;

	/** The X itself — one widget in the tree, moved and faded from here. */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyHitMarkerWidget> HitMarkerWidget;

	//~ The three sounds, one per marker. They live on the widget so whoever owns the look owns the
	//~ noise it makes, and they are assigned in the widget blueprint — no path in code.
	UPROPERTY(EditDefaultsOnly, Category = "Hit Marker")
	TObjectPtr<USoundBase> FullHitSound;

	UPROPERTY(EditDefaultsOnly, Category = "Hit Marker")
	TObjectPtr<USoundBase> HalfHitSound;

	UPROPERTY(EditDefaultsOnly, Category = "Hit Marker")
	TObjectPtr<USoundBase> KillHitSound;

	/** The gun whose drift the circle shows — the one in hand, re-read every tick. */
	UPROPERTY(BlueprintReadOnly, Transient)
	TObjectPtr<AProsperitocracyWeapon> WeaponInstance;

private:
	/** The character this reticle belongs to, or null when there is none yet. */
	AProsperitocracyCharacter* GetOwningCharacter() const;

	/**
	 * Keep the bound gun equal to the one in hand.
	 *
	 * Re-read every tick on purpose. Binding once froze the circle on a gun that had been put away:
	 * its drift stopped updating while the newly drawn gun's recoil and spread lived on an instance
	 * the circle never read. One reticle, one source — the gun in hand right now.
	 */
	void AcquireWeaponFromCharacter();

	/** Listen to the man's own damage. Done on the first tick that can find him, once. */
	void BindToHitMarkersOnce();

	/** One frame of the marker: the fade, the aiming gate, and riding the ring. */
	void UpdateHitMarker(float DeltaTime, float AimingAlpha, const FVector2D& CircleOffset, float RingRadius);

	/** Take the X off the ring. Told once, on the frame a marker ends — never once a frame. */
	void HideHitMarker();

	/** The sound one marker makes, or null when the widget blueprint has not been given it yet. */
	USoundBase* GetHitMarkerSound(EProsperitocracyHitMarkerKind Kind) const;

	/** Play a marker's sound, under the cap — and let a kill take over from the hit's own sound. */
	void PlayHitMarkerSound(EProsperitocracyHitMarkerKind Kind);

	/** The marker showing right now, and how long it has been up. The newest hit always restarts it,
	 *  whatever was fading before — including a kill's. */
	EProsperitocracyHitMarkerKind LiveMarkerKind = EProsperitocracyHitMarkerKind::Full;
	float MarkerAgeSeconds = 0.0f;
	bool bMarkerLive = false;

	/**
	 * True once the live marker has been ON SCREEN for at least one frame. Its life is measured from
	 * there, never from the moment it arrived — a long frame between the two must not be able to eat a
	 * marker nobody ever saw (see UpdateHitMarker).
	 */
	bool bMarkerDrawn = false;

	bool bBoundToHitMarkers = false;

	/** When each kind's sound last played, for the cap. Indexed by the kind. */
	float LastHitMarkerSoundTimes[3] = { -1000.0f, -1000.0f, -1000.0f };

	/** The sound still playing, so the blow that kills can take over from the hit's own sound. */
	TWeakObjectPtr<UAudioComponent> PlayingHitMarkerSound;
};
