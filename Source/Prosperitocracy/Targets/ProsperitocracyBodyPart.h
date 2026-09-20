// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/ProsperitocracyDamageReceiver.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"

#include "ProsperitocracyBodyPart.generated.h"

class UPrimitiveComponent;
class UProsperitocracyStatTable;
enum class EProsperitocracyStat : uint8;

/**
 * AProsperitocracyBodyPart
 *
 * ONE part of a body — the thing a hit lands on, and the numbers it answers with. A part is a thing with
 * a stat block of its own: its **armour** (0-3, the pen gate's threshold) and **one resistance or
 * none**. Those numbers live on the part's own block, are pushed onto the part's own GAS home, and are
 * read FINAL through the one evaluator — so a perk or a debuff that moves a part's armour or resist
 * moves the answer here too, exactly as a gun's damage is read off the gun's own home.
 *
 * Nothing numeric is authored on the body that owns the part: the body names the mesh and hands over the
 * block, and everything else is the evaluator's.
 *
 * The part is invisible, never ticks and is never damaged itself: the BODY takes the damage and the part
 * answers what the hit ran into. The body owns one of these per part for as long as it lives (see
 * `AProsperitocracyDamageTarget`, and `AProsperitocracyStatHostActor` for what a GAS home is).
 */
UCLASS()
class AProsperitocracyBodyPart : public AProsperitocracyStatHostActor
{
	GENERATED_BODY()

public:
	AProsperitocracyBodyPart();

	/** Dress this part: the mesh it IS, and the block that is its numbers. */
	void InitializePart(UPrimitiveComponent* InPartMesh, UProsperitocracyStatTable* InStatBlock);

	/** The mesh a hit lands on for this part. */
	UPrimitiveComponent* GetPartMesh() const { return PartMesh; }

	/**
	 * WHICH damage this part is built against — one, or none.
	 *
	 * Answered by what the part's BLOCK carries, never by a value: a part wearing no resist row resists
	 * nothing at all, and a part wearing BOTH rows is an authoring bug (the design has one or none), said
	 * out loud and answered as Impact.
	 */
	FGameplayTag GetResistType() const;

	/** The part's answer to a hit of that type: its armour and the resist it has against that type. */
	FProsperitocracyDamageProfile GetProfile(const FGameplayTag& DamageType) const;

	/** What this part does to a line of that type: its FINAL resist if it is the type it carries, else nothing. */
	float GetResistAgainst(const FGameplayTag& DamageType) const;

protected:
	/**
	 * A part's own ability system carries the body's set as well as the thing set every home has.
	 *
	 * Why: a part's answer includes a BODY number — its one resist — and the two resists are body rows,
	 * whose home is the body's stat set. Its armour is a thing row on the thing set. One home per number,
	 * both on this part's own ASC, so a perk that moves either reaches it through the one evaluator.
	 */
	virtual void PostInitializeComponents() override;

	/**
	 * A part derives nothing: the shot's push is a THING's number off its own Weight and damage, and a
	 * body part has neither. Overridden to nothing so a part's home carries only what its block carries —
	 * presence is scope, and a part has no Drag.
	 */
	virtual void ApplyDerivedStats() override;

	UPROPERTY(VisibleAnywhere, Category = "Body Part")
	TObjectPtr<UPrimitiveComponent> PartMesh;

	/**
	 * The block this part wears — its armour and its one resist or none. Kept because "which resist do
	 * I carry" is a question only the BLOCK can answer: a part wearing no resist row resists nothing,
	 * and that is not the same as a part wearing a resist of 0.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyStatTable> StatBlock;
};
