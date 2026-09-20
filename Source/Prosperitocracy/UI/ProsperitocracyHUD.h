// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "ProsperitocracyHUD.generated.h"

class UProsperitocracyHealthSet;

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

	// The bar's geometry, in one list so the look is one place. [TUNE] — all four are mine to pick, and
	// moving any of them moves nothing else.
	static constexpr float BarWidthShareOfScreen = 0.4f;
	static constexpr float BarHeightPixels = 10.0f;
	static constexpr float BarBottomMarginPixels = 70.0f;
	static constexpr float BorderThicknessPixels = 2.0f;

	/**
	 * The health's colour: a dark red with a crimson lean (his call, 2026-09-20 — the first pass read a
	 * touch too purple, so this is the same colour nudged red). Stated as a hex because that is how a
	 * colour is picked, and converted from sRGB so what is drawn is the colour that was picked rather than
	 * a linear-space guess at it.
	 */
	static constexpr uint32 HealthColorHex = 0xA21634;

protected:
	/** The health set of the body this HUD is showing, or null when there is none to show. */
	const UProsperitocracyHealthSet* GetOwningHealthSet() const;
};
