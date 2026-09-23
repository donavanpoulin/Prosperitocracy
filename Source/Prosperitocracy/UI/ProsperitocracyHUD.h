// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "ProsperitocracyHUD.generated.h"

class UProsperitocracyHealthSet;
class AProsperitocracyBloodBlade;

/**
 * AProsperitocracyHUD
 *
 * The player's own readout: **the health bar** — a long, thin rectangle low on the screen, with a black
 * border and nothing else on it (Design is the user's call, 2026-09-20):
 *
 *   - **red** is the health you have;
 *   - **white** is the contested part of it — the health you just lost and can still win back before the
 *     clock runs out (see UProsperitocracyGameplayAbility_ContestedHealth);
 *   - **empty is transparent**: nothing is drawn past what you are holding, so the bar reads as what you
 *     HAVE rather than as a slot to be filled.
 *
 * Drawn straight onto the HUD canvas rather than built as a widget: the whole thing is three rectangles,
 * so there is no asset to make, no tree to wire and no second UI system to keep in step. When the UI
 * wants styling (per-class colours, a texture, a title), that is the moment it becomes a widget.
 *
 * Only ever the LOCAL player's bar: a HUD belongs to one player.
 */
UCLASS()
class AProsperitocracyHUD : public AHUD
{
	GENERATED_BODY()

public:
	AProsperitocracyHUD();

	/** Everything is drawn in one pass, once a frame. */
	virtual void DrawHUD() override;

	// The bar's geometry, in one list so the look is one place. [TUNE] — all of them are mine to pick, and
	// moving any of them moves nothing else.
	//
	// It sits in the BOTTOM LEFT corner (his call, 2026-09-20): off to the side so it does not sit in the
	// middle of what you are looking at, half the width it had when it was centred.
	static constexpr float BarWidthShareOfScreen = 0.2f;
	static constexpr float BarHeightPixels = 10.0f;
	static constexpr float BarBottomMarginPixels = 70.0f;
	static constexpr float BarLeftMarginPixels = 40.0f;
	static constexpr float BorderThicknessPixels = 2.0f;

	/**
	 * The health's colour: a dark red with a crimson lean — his own pick, 2026-09-20, after two passes of
	 * his eye. Stated as a hex because that is how a colour is picked, and converted from sRGB so what is
	 * drawn is the colour that was picked rather than a linear-space guess at it.
	 */
	static constexpr uint32 HealthColorHex = 0xB3001C;

	//~ The BLOOD bar — the sword's own pool, and only a body carrying a blade has one.
	//
	// The health bar's own size turned on its side, in the bottom RIGHT corner so the two never crowd
	// each other: the same 20% of the screen long, the same 10 pixels thin, the same margins mirrored.
	// It reads like the health bar does — what you are HOLDING is drawn and the rest is left alone.
	static constexpr float BloodBarThicknessPixels = 10.0f;
	static constexpr float BloodBarLengthShareOfScreen = 0.2f;
	static constexpr float BloodBarRightMarginPixels = 40.0f;
	static constexpr float BloodBarBottomMarginPixels = 70.0f;

	/**
	 * The blood's colour: a DARKER red than the health's crimson, because it is blood rather than health.
	 * The whole mode is three reds with one order to them — the blood lightest, the mode's background
	 * between, and the mode's border darkest.
	 */
	static constexpr uint32 BloodColorHex = 0x8C0014;

	/** The panel behind the bar while the blood mode is ON: a dark red between the blood and the border. */
	static constexpr uint32 BloodModeBackdropHex = 0x3A0008;

	/** And the border in the blood mode: the darkest of the three, so the bar reads as a lit panel. */
	static constexpr uint32 BloodModeBorderHex = 0x140002;

	/**
	 * How fast the bar chases the real number, so it FALLS and RISES smoothly instead of stepping from
	 * one frame's value to the next. `[TUNE]`.
	 */
	static constexpr float BloodBarSmoothingRate = 8.0f;

protected:
	/** The health set of the body this HUD is showing, or null when there is none to show. */
	const UProsperitocracyHealthSet* GetOwningHealthSet() const;

	/** The blade this body carries, or null when it carries none — no blade, no blood bar. */
	AProsperitocracyBloodBlade* FindOwningBloodBlade() const;

	/**
	 * What the bar is DRAWN at (0–1), chasing the real pool. Kept on the HUD because it is a fact about
	 * the picture and not about the game: the pool itself never moves smoothly, and should not.
	 */
	float SmoothedBloodFraction = 1.0f;
};
