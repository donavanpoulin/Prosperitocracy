// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ProsperitocracyHitMarkerTypes.generated.h"

/**
 * EProsperitocracyHitMarkerKind
 *
 * The three hit markers, and the ONLY three (his spec, 2026-09-25): one X per blow, the same X in
 * every case, and which one it is is decided by what the damage pipeline and the death did — never
 * by the marker.
 *
 *   - Half  — the gate HALVED the line (pen == armour): the same X, in white.
 *   - Full  — full damage through the gate; also any line that carries NO pen at all (fire, which
 *             nothing gates), so a burn tick is always Full: red.
 *   - Kill  — the blow that took a body to zero: the same red X, bigger, its tips past the ring.
 */
UENUM(BlueprintType)
enum class EProsperitocracyHitMarkerKind : uint8
{
	/** Pen only matched the armour, so the gate halved it: white. */
	Half UMETA(DisplayName = "Half (white)"),

	/** Full damage through the gate — or a line with no pen at all, which no gate answers: red. */
	Full UMETA(DisplayName = "Full (red)"),

	/** The killing blow: the same red X, bigger, its tips past the ring. */
	Kill UMETA(DisplayName = "Kill (big red)")
};
