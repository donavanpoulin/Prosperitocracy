// Copyright Prosperitocracy. All Rights Reserved.

#include "UI/ProsperitocracyUISettings.h"

#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyUISettings)

UProsperitocracyUISettings::UProsperitocracyUISettings()
{
	// The palette the design already decides (Design/ui.md), so the one spot holds the colours the game
	// has been painting with rather than starting as a second, empty palette to fill in.
	const auto Row = [](const FGameplayTag& Name, int32 Hex)
	{
		FProsperitocracyUIColour Colour;
		Colour.Name = Name;
		Colour.Hex = Hex;
		return Colour;
	};

	Colours =
	{
		Row(ProsperitocracyGameplayTags::Damage_Type_Impact,   0xCC4A00),
		Row(ProsperitocracyGameplayTags::Damage_Type_Piercing, 0xEC0058),
		Row(ProsperitocracyGameplayTags::Status_Burn,          0xFF3B00),
		Row(ProsperitocracyGameplayTags::Status_Stun,          0x7A2BFF),
	};
}

const UProsperitocracyUISettings& UProsperitocracyUISettings::Get()
{
	return *GetDefault<UProsperitocracyUISettings>();
}

FLinearColor UProsperitocracyUISettings::GetUIColour(FGameplayTag Name) const
{
	for (const FProsperitocracyUIColour& Colour : Colours)
	{
		if (Colour.Name == Name)
		{
			// Stated as a hex because that is how a colour is picked, and converted from sRGB so what is
			// drawn is the colour that was picked rather than a linear-space guess at it.
			return FLinearColor::FromSRGBColor(FColor(
				(Colour.Hex >> 16) & 0xFF, (Colour.Hex >> 8) & 0xFF, Colour.Hex & 0xFF));
		}
	}

	// No colour set for this name yet. Say so, and paint white rather than a guess: a missing colour is
	// one row to add in the one spot, never something to invent where the question was asked.
	UE_LOG(LogProsperitocracy, Warning,
		TEXT("[UI] no colour is set for %s — add its row in Project Settings → Prosperitocracy UI."),
		*Name.ToString());

	return FLinearColor::White;
}
