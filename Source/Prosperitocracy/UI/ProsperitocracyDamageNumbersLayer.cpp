// Copyright Prosperitocracy. All Rights Reserved.

#include "UI/ProsperitocracyDamageNumbersLayer.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "UI/ProsperitocracyUISettings.h"

void SProsperitocracyDamageNumbersLayer::Construct(const FArguments& InArgs)
{
	OwningPlayerController = InArgs._OwningPlayerController;

	// Paint, and nothing else: the numbers must never eat the mouse or the keyboard, and they sit over
	// whatever they landed on.
	SetVisibility(EVisibility::HitTestInvisible);

	// They age, so this widget ticks.
	SetCanTick(true);
}

void SProsperitocracyDamageNumbersLayer::AddNumber(const FProsperitocracyDamageFeedback& Feedback)
{
	if (Feedback.Numbers.Num() == 0)
	{
		// A payload with no numbers owes this screen nothing. The killing blow's own marker rides the same
		// door and carries none: a marker is an answer, not a number.
		return;
	}

	const UFont* NumberFontObject = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!NumberFontObject)
	{
		return;
	}

	// The ceiling, before anything is measured: a burning horde cannot cost unbounded draws and unbounded
	// text. The oldest goes first — it is the one already fading.
	while (LiveNumbers.Num() >= MaxLiveNumbers)
	{
		LiveNumbers.RemoveAt(0);
	}

	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const UProsperitocracyUISettings& UISettings = UProsperitocracyUISettings::Get();
	const FString SpaceText(TEXT(" "));

	// MEASURED HERE, ONCE. The paint does none of this: the text, the box it draws in and the gap after it
	// are fixed for the number's whole life, and only its position and its opacity ever move.
	FLiveNumber& Live = LiveNumbers.AddDefaulted_GetRef();
	Live.Location = Feedback.Location;
	Live.AgeSeconds = 0.0f;
	Live.TotalWidth = 0.0f;

	for (const FProsperitocracyDamageNumber& Number : Feedback.Numbers)
	{
		// A whole number: this is read, not measured. A line that took off less than half a point owes no
		// number at all rather than a "0" pretending to be a hit.
		const int32 Rounded = FMath::RoundToInt(Number.Amount);
		if (Rounded <= 0)
		{
			continue;
		}

		FNumberPart& Part = Live.Parts.AddDefaulted_GetRef();
		Part.Text = FString::Printf(TEXT("%d"), Rounded);
		// The size THIS number is drawn at, off its own damage: the band's ends are the caps, so a sliver
		// reads as a sliver and a rocket reads as a rocket.
		Part.FontSize = SizeFor(static_cast<float>(Rounded));
		// The type's colour, straight off the one spot — never a colour of the number's own.
		Part.Colour = UISettings.GetUIColour(Number.Type);

		// The ruler Slate itself draws with, so what is measured is what lands. The black edge is the
		// FONT's own outline, so it is part of the box that is measured here.
		const FSlateFontInfo PartFont(NumberFontObject, Part.FontSize, TEXT("Bold"), FFontOutlineSettings(OutlineSize));

		const auto TextSize = FontMeasure->Measure(Part.Text, PartFont);
		Part.Size = FVector2D(TextSize.X, TextSize.Y);
		Part.SpaceWidth = static_cast<float>(FontMeasure->Measure(SpaceText, PartFont).X);

		Live.TotalWidth += static_cast<float>(Part.Size.X);
		if (Live.Parts.Num() > 1)
		{
			// The gap belongs to the number on its left, at that number's own size: `26 47` reads exactly as
			// evenly spaced as `227 37`, whatever the two numbers are.
			Live.TotalWidth += Live.Parts[Live.Parts.Num() - 2].SpaceWidth;
		}
	}

	// Nothing to show after all (every line rounded away): do not hold an empty group.
	if (Live.Parts.Num() == 0)
	{
		LiveNumbers.RemoveAt(LiveNumbers.Num() - 1);
	}
}

void SProsperitocracyDamageNumbersLayer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// One frame older, and gone once its time is up. Done HERE and not in paint, so painting only paints.
	for (FLiveNumber& Live : LiveNumbers)
	{
		Live.AgeSeconds += InDeltaTime;
	}
	LiveNumbers.RemoveAll([](const FLiveNumber& Live)
	{
		return Live.AgeSeconds >= LifeSeconds;
	});
}

float SProsperitocracyDamageNumbersLayer::SizeAlpha(float Amount)
{
	const float Span = MostDamage - LeastDamage;
	return Span > 0.0f ? FMath::Clamp((Amount - LeastDamage) / Span, 0.0f, 1.0f) : 1.0f;
}

float SProsperitocracyDamageNumbersLayer::SizeFor(float Amount)
{
	return FMath::Lerp(SizeAtLeastDamage, SizeAtMostDamage, SizeAlpha(Amount));
}

int32 SProsperitocracyDamageNumbersLayer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 NumberLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const APlayerController* PlayerController = OwningPlayerController.Get();
	const UFont* NumberFontObject = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!PlayerController || !NumberFontObject || LiveNumbers.Num() == 0)
	{
		return NumberLayer;
	}

	const FVector2D ViewportSize = AllottedGeometry.GetLocalSize();

	// Oldest first, NEWEST LAST: the number that just landed is drawn on top of the ones already fading,
	// which is the order the list is in.
	for (const FLiveNumber& Live : LiveNumbers)
	{
		// Full where it landed, then up and out: the last stretch of its life is spent rising and fading
		// together, so it says what it took and gets out of the way of the next one.
		const float FadeSeconds = LifeSeconds - FullOpacitySeconds;
		const float FadeFraction = FMath::Clamp((Live.AgeSeconds - FullOpacitySeconds) / FadeSeconds, 0.0f, 1.0f);
		const float Opacity = 1.0f - FadeFraction;

		// It rises in the WORLD, from the place it came up: a number that climbed the screen instead would
		// drift off the thing it is about as soon as the man moved.
		const FVector WorldLocation = Live.Location + FVector(0.0f, 0.0f, RiseCm * FadeFraction);

		// Where it comes up, in the VIEWPORT's own pixels — the space the projection answers in.
		FVector2D ViewportPixels;
		if (!PlayerController->ProjectWorldLocationToScreen(WorldLocation, ViewportPixels, /*bPlayerViewportRelative=*/ true))
		{
			continue;
		}

		// And where that is in THIS widget: the layer is a Slate widget, and its own space is placed and
		// scaled by the viewport host — in a window, and in PIE, that is NOT the viewport's pixels, so the
		// two cannot be used for one another (his report, 2026-09-27: "the numbers are not where they
		// should be"). The conversion goes through this widget's own geometry, which is the very transform
		// the paint below is drawn through, so a number lands exactly on the hit.
		FVector2D ScreenPosition;
		USlateBlueprintLibrary::ScreenToWidgetLocal(PlayerController, AllottedGeometry, ViewportPixels, ScreenPosition, /*bIncludeWindowPosition=*/ false);

		// Drawn a little ABOVE the point, so a straight shot at what the reticle is pointing at does not
		// paste the number over the reticle (his ask). The anchor itself is untouched — the number still
		// comes up where it landed, and it still rises from there.
		ScreenPosition.Y -= ReticleClearancePixels;

		// Behind him, or past the edge of the screen: nothing to draw this frame. It keeps its age, so it is
		// simply finished by the time it would have been visible again.
		if (ScreenPosition.X < 0.0 || ScreenPosition.Y < 0.0
			|| ScreenPosition.X > ViewportSize.X || ScreenPosition.Y > ViewportSize.Y)
		{
			continue;
		}

		// Each number is centred on its own slot inside the group, so the space between two of them is
		// exactly the space and nothing else — no per-number padding for a later edit to fall out of step.
		float PenX = ScreenPosition.X - (Live.TotalWidth * 0.5f);

		for (const FNumberPart& Part : Live.Parts)
		{
			// The font, rebuilt per frame from the size measured ONCE — because the number's own fade lives
			// in the outline colour (the engine lays the outline down with the colour it is handed, so
			// handing it the fade is what makes fill and edge go out together). That is a small struct, not
			// a measurement: nothing here is measured again.
			const FSlateFontInfo PartFont(NumberFontObject, Part.FontSize, TEXT("Bold"),
				FFontOutlineSettings(OutlineSize, FLinearColor(0.0f, 0.0f, 0.0f, Opacity)));

			const FVector2D TopLeft(PenX, ScreenPosition.Y - (Part.Size.Y * 0.5f));

			FSlateDrawElement::MakeText(
				OutDrawElements,
				NumberLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(static_cast<float>(Part.Size.X), static_cast<float>(Part.Size.Y)),
					FSlateLayoutTransform(FVector2f(static_cast<float>(TopLeft.X), static_cast<float>(TopLeft.Y)))),
				Part.Text,
				PartFont,
				ESlateDrawEffect::None,
				Part.Colour.CopyWithNewOpacity(Opacity));

			PenX += static_cast<float>(Part.Size.X) + Part.SpaceWidth;
		}
	}

	return NumberLayer + 1;
}
