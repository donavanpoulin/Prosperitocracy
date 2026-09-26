// Copyright Prosperitocracy. All Rights Reserved.

#include "UI/Weapons/ProsperitocracyHitMarkerWidget.h"

#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyHitMarkerWidget)

/**
 * SProsperitocracyHitMarker
 *
 * The two strokes, drawn. A leaf widget because that is what "draw these two lines" is: no texture, no
 * material, no border trick — two polylines with a thickness, in the widget's own space, centred.
 *
 * The whole X is described by three numbers: how far a tip is from the centre, how thick a stroke is,
 * and what colour it is. The reticle moves the widget; this paints inside it.
 */
class SProsperitocracyHitMarker : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SProsperitocracyHitMarker) {}
		/** How far a tip sits from the X's centre, in the widget's own units. */
		SLATE_ARGUMENT(float, TipDistance)
		/** How thick one stroke is, in the widget's own units. */
		SLATE_ARGUMENT(float, Thickness)
		/** The stroke's colour, without the fade — the fade is applied as its alpha. */
		SLATE_ARGUMENT(FLinearColor, Color)
		/** The fade, 0-1. */
		SLATE_ARGUMENT(float, Opacity)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		TipDistance = InArgs._TipDistance;
		Thickness = InArgs._Thickness;
		Color = InArgs._Color;
		Opacity = InArgs._Opacity;

		SetCanTick(false);
	}

	/** One call, all of it: the reticle sets these as the marker lives and as it fades. */
	void SetStroke(float InTipDistance, float InThickness, const FLinearColor& InColor, float InOpacity)
	{
		TipDistance = InTipDistance;
		Thickness = InThickness;
		Color = InColor;
		Opacity = InOpacity;

		// Paint only: the box never changes, so nothing has to be re-laid-out for a fade or a resize of
		// the X inside it.
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		if (Opacity <= KINDA_SMALL_NUMBER || TipDistance <= 0.0f || Thickness <= 0.0f)
		{
			return LayerId;
		}

		const FVector2D Centre = AllottedGeometry.GetLocalSize() * 0.5f;

		// A stroke goes corner to corner through the centre, so its ENDS are what the tip distance
		// names: the half-vector of the stroke is (d, d) with d = reach * sqrt(0.5), which puts each end
		// exactly `reach` from the centre whichever way you measure it.
		const float Half = TipDistance * FMath::Sqrt(0.5f);

		const FLinearColor Tint = Color.CopyWithNewOpacity(Color.A * Opacity) * InWidgetStyle.GetColorAndOpacityTint();

		// Two strokes, two polylines: down-right, then up-right. The layer id is left alone so the X sits
		// with the rest of the reticle.
		{
			TArray<FVector2D> Points;
			Points.Reserve(2);
			Points.Add(Centre + FVector2D(-Half, -Half));
			Points.Add(Centre + FVector2D(Half, Half));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, /*bAntialias=*/ true, Thickness);
		}
		{
			TArray<FVector2D> Points;
			Points.Reserve(2);
			Points.Add(Centre + FVector2D(-Half, Half));
			Points.Add(Centre + FVector2D(Half, -Half));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, /*bAntialias=*/ true, Thickness);
		}

		return LayerId;
	}

	/** The box is set by the widget, not asked for: it is the biggest X there is, always. */
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return DesiredSize;
	}

	void SetDesiredSize(const FVector2D& InSize) { DesiredSize = InSize; }

private:
	float TipDistance = 0.0f;
	float Thickness = 0.0f;
	FLinearColor Color = FLinearColor::White;
	float Opacity = 0.0f;
	FVector2D DesiredSize = FVector2D::ZeroVector;
};

UProsperitocracyHitMarkerWidget::UProsperitocracyHitMarkerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Nothing here takes input; the widget exists to be looked at.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FProsperitocracyHitMarkerStroke UProsperitocracyHitMarkerWidget::GetStroke() const
{
	FProsperitocracyHitMarkerStroke Stroke;

	// The ONE table: what makes a kill a kill is that its arms come out past the ring, and nothing else
	// about it is a special case. Half is the full X in white — the same size, the same thickness.
	switch (Kind)
	{
	case EProsperitocracyHitMarkerKind::Kill:
		Stroke.ReachShareOfRingRadius = KillReachShareOfRingRadius;
		Stroke.ThicknessShareOfRingRadius = KillThicknessShareOfRingRadius;
		Stroke.bWhite = false;
		break;

	case EProsperitocracyHitMarkerKind::Half:
		Stroke.ReachShareOfRingRadius = HitReachShareOfRingRadius;
		Stroke.ThicknessShareOfRingRadius = HitThicknessShareOfRingRadius;
		Stroke.bWhite = true;
		break;

	case EProsperitocracyHitMarkerKind::Full:
	default:
		Stroke.ReachShareOfRingRadius = HitReachShareOfRingRadius;
		Stroke.ThicknessShareOfRingRadius = HitThicknessShareOfRingRadius;
		Stroke.bWhite = false;
		break;
	}

	return Stroke;
}

FVector2D UProsperitocracyHitMarkerWidget::GetMarkerBoxSize() const
{
	// The biggest X there is (a kill's), plus half a stroke either side so the tips are never clipped by
	// the box itself. Fixed on purpose: a hit's X is drawn smaller INSIDE this box, so the box — and
	// therefore the point the X is centred on — never moves between a hit and a kill.
	const float Reach = KillReachShareOfRingRadius * RingRadius;
	const float HalfStroke = KillThicknessShareOfRingRadius * RingRadius * 0.5f;
	const float HalfBox = Reach + HalfStroke;

	return FVector2D(HalfBox * 2.0f, HalfBox * 2.0f);
}

void UProsperitocracyHitMarkerWidget::SetMarker(EProsperitocracyHitMarkerKind InKind, float InOpacity, float InRingRadius)
{
	Kind = InKind;
	MarkerOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
	RingRadius = FMath::Max(InRingRadius, 0.0f);

	// Nothing to show: collapse it so it costs nothing and can never paint a stale X.
	SetVisibility(MarkerOpacity > KINDA_SMALL_NUMBER ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	SynchronizeProperties();
}

void UProsperitocracyHitMarkerWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!MarkerSlate.IsValid())
	{
		return;
	}

	const FProsperitocracyHitMarkerStroke Stroke = GetStroke();
	const float Reach = Stroke.ReachShareOfRingRadius * RingRadius;
	const float Thickness = Stroke.ThicknessShareOfRingRadius * RingRadius;

	MarkerSlate->SetDesiredSize(GetMarkerBoxSize());
	MarkerSlate->SetStroke(Reach, Thickness, Stroke.bWhite ? FLinearColor::White : FLinearColor::FromSRGBColor(FColor(
		(RedHex >> 16) & 0xFF, (RedHex >> 8) & 0xFF, RedHex & 0xFF)), MarkerOpacity);
}

TSharedRef<SWidget> UProsperitocracyHitMarkerWidget::RebuildWidget()
{
	const FProsperitocracyHitMarkerStroke Stroke = GetStroke();
	const float Reach = Stroke.ReachShareOfRingRadius * RingRadius;
	const float Thickness = Stroke.ThicknessShareOfRingRadius * RingRadius;

	SAssignNew(MarkerSlate, SProsperitocracyHitMarker)
		.TipDistance(Reach)
		.Thickness(Thickness)
		.Color(Stroke.bWhite ? FLinearColor::White : FLinearColor::FromSRGBColor(FColor(
			(RedHex >> 16) & 0xFF, (RedHex >> 8) & 0xFF, RedHex & 0xFF)))
		.Opacity(MarkerOpacity);

	MarkerSlate->SetDesiredSize(GetMarkerBoxSize());

	return MarkerSlate.ToSharedRef();
}

void UProsperitocracyHitMarkerWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MarkerSlate.Reset();
}
