// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "UI/ProsperitocracyHitMarkerTypes.h"

#include "ProsperitocracyHitMarkerWidget.generated.h"

class SProsperitocracyHitMarker;

/**
 * FProsperitocracyHitMarkerStroke
 *
 * The whole look of ONE kind of marker: how far its arms reach, how thick they are, and whether it is
 * the white one. Shares of the ring's radius rather than pixels, so the X can never disagree with the
 * ring about what "inside" means.
 */
struct FProsperitocracyHitMarkerStroke
{
	float ReachShareOfRingRadius = 0.0f;
	float ThicknessShareOfRingRadius = 0.0f;
	bool bWhite = false;
};

/**
 * UProsperitocracyHitMarkerWidget
 *
 * The X — one marker, drawn as two strokes crossing. It is **part of the ring's UI** (his spec,
 * 2026-09-25): a hit's X sits inside the ring, a kill's X is the same X drawn bigger so its tips come
 * out past the ring, and it rides the ring because it is moved by the very same value the ring is.
 *
 * Everything the three kinds differ by is HERE, in one table — the colour, how far the arms reach, how
 * thick they are — and nothing outside this file knows any of it:
 *
 *   - Full : red, arms well inside the ring.
 *   - Half : the same X, in white.
 *   - Kill : red, arms reaching PAST the ring, and a touch heavier.
 *
 * No art: an X is two strokes, so it is drawn rather than textured. That is also what keeps it honest
 * against the ring — every number here is a multiple of the ring's own radius, so resizing the ring
 * resizes the X with it and the two can never disagree about "inside".
 *
 * Nothing here decides WHICH marker to show or how long it lasts: the reticle owns the timing (the
 * fade, the restart, the aiming gate) and hands this widget a kind and an opacity. This owns the look.
 */
UCLASS()
class UProsperitocracyHitMarkerWidget : public UWidget
{
	GENERATED_BODY()

public:
	UProsperitocracyHitMarkerWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Which of the three X's this is. Set by the reticle as the marker arrives and as it fades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Marker")
	EProsperitocracyHitMarkerKind Kind = EProsperitocracyHitMarkerKind::Full;

	/**
	 * The ring's OWN drawn radius, in the widget's own units — read off the ring image itself, never a
	 * number kept in a second place. Everything about the X is a multiple of this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Marker")
	float RingRadius = 10.0f;

	/** The fade, 0-1, owned by the reticle. 0 hides the marker entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Marker")
	float MarkerOpacity = 0.0f;

	/** One call from the reticle each frame while a marker is alive. */
	UFUNCTION(BlueprintCallable, Category = "Hit Marker")
	void SetMarker(EProsperitocracyHitMarkerKind InKind, float InOpacity, float InRingRadius);

	//~UWidget interface
	virtual void SynchronizeProperties() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~End of UWidget interface

	//~ The three X's, and the only numbers that make one. [TUNE] — mine to pick, his to move once he
	//~ has felt them. A hit's arms stay UNDER the ring (his spec, 2026-09-25: "only the kill
	//~ overlaps"); a kill's come out past it.
	static constexpr float HitReachShareOfRingRadius = 0.65f;
	static constexpr float HitThicknessShareOfRingRadius = 0.22f;
	static constexpr float KillReachShareOfRingRadius = 1.15f;
	static constexpr float KillThicknessShareOfRingRadius = 0.31f;

	/** The colour of a hit and of a kill: a strong red that is not a full, bright red, and neither
	 *  orange nor pink. Stated as a hex because that is how a colour is picked (like the HUD's). */
	static constexpr uint32 RedHex = 0xCC1820;

private:
	/** The stroke the current Kind asks for. */
	FProsperitocracyHitMarkerStroke GetStroke() const;

	/** The box the X lives in: the biggest one it can ever need (a kill's), so the slot never resizes
	 *  and the X can never shift the point it is centred on. */
	FVector2D GetMarkerBoxSize() const;

	TSharedPtr<SProsperitocracyHitMarker> MarkerSlate;
};
