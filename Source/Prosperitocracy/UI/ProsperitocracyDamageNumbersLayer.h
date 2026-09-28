// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ProsperitocracyDamageFeedbackTypes.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;

/**
 * SProsperitocracyDamageNumbersLayer
 *
 * The damage numbers, drawn as SLATE text over the HUD (his fix, 2026-09-27).
 *
 * Why they are not on the HUD's canvas: a canvas text item can only be given an outline by drawing the
 * string four more times, a pixel out each way, UNDER the fill — and those four extra passes take the
 * world away behind every digit, so a number went DARK as it faded instead of turning translucent (at
 * half-fade a plain number sits over 50% of the world; that one sat over 3%). Slate draws an outline as
 * its own layer, so a digit's inside is covered once and the number fades as one translucent thing.
 *
 * WHAT IT COSTS, and why it is shaped the way it is (his question, 2026-09-27 — "is this what a AAA game
 * would do, or does it bite us at high numbers"):
 *
 *   - One layer per player, held by that player's HUD, and NOTHING is an Actor or a Component — a number
 *     is a struct in a TArray while it lives (1.2 s) and is then a struct that no longer exists. No
 *     replicated object, no spawn, no GC traffic per hit: that is what keeps a machine-gunned horde cheap.
 *   - Each blow is MEASURED ONCE, when it arrives, and never again: the text, the box it draws in, the
 *     space after it and its colour are all fixed for its whole life. Only where it is drawn and how
 *     opaque it is change per frame — so the per-frame cost is a projection and a draw call per number,
 *     and the text never gets re-measured (measuring every number every frame is the classic way a HUD
 *     like this gets expensive).
 *   - LIVE numbers are CAPPED. A burn ticking on a whole horde is several readouts a second per body, and
 *     a number lives 1.2 s — the cap is what stops that arithmetic from becoming unbounded draws. At the
 *     ceiling the OLDEST go first: they are the ones already fading.
 *   - The readout is NOT replicated state: the server sends it once, unreliable, to the one screen it
 *     belongs on (see UProsperitocracyDamageFeedbackStatics), and every other client never hears it.
 */
class SProsperitocracyDamageNumbersLayer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProsperitocracyDamageNumbersLayer)
		: _OwningPlayerController(nullptr)
	{}
		/** The player whose screen these numbers belong on — the view the projection is made through. */
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, OwningPlayerController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/**
	 * A blow's readout arrived for this player: measure it, hold it, and draw it as the frames go by.
	 *
	 * A payload with no numbers owes this screen nothing and is ignored: the killing blow's own marker
	 * rides the same door and carries no numbers, and a marker is not a number.
	 */
	void AddNumber(const FProsperitocracyDamageFeedback& Feedback);

	//~ SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	//~ The look, in one list so it is one place. [TUNE] — mine to pick, his to move them.
	//
	// It comes up at full strength where it landed, and then rides up while it fades out: "rises up as it
	// fades, slowly". A straighter word than a curve, so the three times are the whole of the timing.
	static constexpr float LifeSeconds = 1.2f;
	static constexpr float FullOpacitySeconds = 0.3f;
	static constexpr float RiseCm = 50.0f;

	/**
	 * How far ABOVE the point it landed the group is DRAWN, in pixels (his ask, 2026-09-27: a straight shot
	 * put the number on the reticle instead of clear of it). Pixels and not centimetres because the reticle
	 * is a constant SCREEN size — a world lift would vanish at range and shoot off the top of the screen at
	 * arm's length. The number's own anchor is still the hit: only the drawing is nudged.
	 *
	 * The reticle's own ring radius is the number to match it to if you want it exactly clear. [TUNE]
	 */
	static constexpr float ReticleClearancePixels = 28.0f;

	/**
	 * A number's SIZE is not one size: a bigger blow is a bigger number, so a shot that took off a sliver
	 * reads as a sliver and a rocket reads as a rocket.
	 *
	 * The BAND the size grows across: damage 1 is the smallest size, the top of the band is the biggest,
	 * and anything past the top is CAPPED there. The band's ends are HIS: 1 → 400. All four numbers are
	 * TUNABLE, [TUNE]: his are the band's ends, mine are the two sizes.
	 *
	 * The sizes are in the font's own units, and a Roboto digit stands about 0.71 of that tall. The passes
	 * his eye has taken them through (2026-09-27): 40/144 was "MASSIVE"; 20/56 was still too big; 14/40
	 * landed at the small end with the big end a little too big; 32 was still a touch big; and the band was
	 * too short. So: the band widened to 400 and the top sits a little under that 32, at 28 — a typical
	 * 80-damage hit draws at ~17 units, 200 damage at ~21, and 400-and-over at 28.
	 *
	 * A blow of both types sizes each of its two numbers off ITS OWN line, the same way each is painted in
	 * its own type's colour — one number, one amount, one size.
	 */
	static constexpr float LeastDamage = 1.0f;
	static constexpr float MostDamage = 400.0f;
	static constexpr float SizeAtLeastDamage = 14.0f;
	static constexpr float SizeAtMostDamage = 28.0f;

	/**
	 * The black edge round every digit, in the font's own units — this is the font's own OUTLINE, not extra
	 * draws of the string: Slate rasterises the glyph with it and lays the outline under the fill, which is
	 * what keeps the fade a fade. [TUNE]
	 */
	static constexpr int32 OutlineSize = 2;

	/**
	 * How many numbers may be alive at once. A CEILING, not a policy: a burn ticking across a horde is
	 * several readouts a second per body while each number lives 1.2 s, so without one the draws are
	 * unbounded. At the ceiling the OLDEST are dropped — they are the ones already fading. [TUNE]
	 */
	static constexpr int32 MaxLiveNumbers = 64;

private:
	/**
	 * ONE number of a blow, measured the moment the blow arrives and never again: the text, the box it
	 * draws in, the space that follows it, the size it is drawn at and its colour.
	 *
	 * Nothing in here changes while the number lives — only where it is drawn and how opaque it is — which
	 * is the whole point: the paint runs every frame and does no measuring at all.
	 */
	struct FNumberPart
	{
		FString Text;
		FVector2D Size = FVector2D::ZeroVector;
		float SpaceWidth = 0.0f;
		float FontSize = 0.0f;
		FLinearColor Colour = FLinearColor::White;
	};

	/** One blow's numbers as the PICTURE holds them: what they were, where in the world they came up, and
	 * how long they have been up. */
	struct FLiveNumber
	{
		FVector Location = FVector::ZeroVector;
		TArray<FNumberPart> Parts;
		float TotalWidth = 0.0f;
		float AgeSeconds = 0.0f;
	};

	/** Where a blow's damage sits in the size band, 0..1 — the band's ends are the caps. */
	static float SizeAlpha(float Amount);

	/** The size a number is drawn at for the damage it shows. */
	static float SizeFor(float Amount);

	/** The numbers on screen right now, in the order they arrived — oldest first. */
	TArray<FLiveNumber> LiveNumbers;

	/** The screen these belong on. Weak: the layer outlives nothing the player owns. */
	TWeakObjectPtr<APlayerController> OwningPlayerController;
};
