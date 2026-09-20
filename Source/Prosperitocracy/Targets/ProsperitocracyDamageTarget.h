// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/ProsperitocracyDamageReceiver.h"
#include "Engine/TimerHandle.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "ProsperitocracyDamageTarget.generated.h"

class AProsperitocracyBodyPart;
class UPrimitiveComponent;
class UProsperitocracyAbilitySystemComponent;
class UProsperitocracyHealthSet;
class UProsperitocracyStatTable;
class UProsperitocracyStatusComponent;
class UStaticMeshComponent;
class UAbilitySystemComponent;
struct FGameplayEffectSpec;

/**
 * AProsperitocracyDamageTarget
 *
 * A test target used to exercise the ONE damage pipeline (pen gate → resist): an enemy without the
 * movement and the attacks. Built of TWO parts, each a hit target with its own GAS home:
 *   - Head (cube on top).
 *   - Chest (cylinder body).
 *
 * Every part's numbers are the BLOCK the part wears — its armour (0-3) and its one resistance or none —
 * read FINAL off the part's own home. Nothing numeric is authored on this actor: set a block, or swap
 * the block a part wears, and the pen gate and the resist follow with no code touched.
 *
 * Implements IProsperitocracyDamageReceiver so the shared execution asks it for the part that was hit,
 * and IAbilitySystemInterface so the weapon can find the ASC to apply the damage to.
 */
UCLASS()
class AProsperitocracyDamageTarget : public AActor, public IAbilitySystemInterface, public IProsperitocracyDamageReceiver
{
	GENERATED_BODY()

public:
	AProsperitocracyDamageTarget();

	// IProsperitocracyDamageReceiver: the answer of the PART the hit landed on, for the line's type.
	virtual FProsperitocracyDamageProfile GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const override;

	/**
	 * The target AS A WHOLE, for a line of ONE damage type — what a line with no pen (burn) is answered
	 * by. A burn never strikes a part, so no part answers it: the number is the plain mean across this
	 * body's parts (all parts weighing the same — the user's rule, 2026-09-20), each part's number being
	 * its FINAL value off its own home.
	 */
	virtual float GetBodyResist_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** The head part's numbers: its armour (0-3) and its one resistance or none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Target|Parts")
	TObjectPtr<UProsperitocracyStatTable> HeadBlock;

	/** The chest part's numbers, set separately from the head's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Target|Parts")
	TObjectPtr<UProsperitocracyStatTable> ChestBlock;

	/**
	 * The BODY's own numbers — its Health, as a row like everything else. An enemy's health is the
	 * universal Health stat, so it is authored in a block and read through the one evaluator, exactly
	 * like a part's armour. Nothing numeric is authored on this actor.
	 *
	 * Empty is a real state: a body with no block keeps whatever its health set came up with, which is
	 * said out loud at spawn rather than quietly assumed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Target|Body")
	TObjectPtr<UProsperitocracyStatTable> HealthBlock;

	// Solid color for the whole target (per ui.md). Set per instance: pierce/impact/neutral.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Target")
	FLinearColor BaseColor = FLinearColor::White;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Give every part its own GAS home, dressed from the block it wears — once, at BeginPlay, alive for
	 * as long as the body is. A part with no block still gets its home: a part with no numbers answers
	 * with no armour and no resistance, which is a real answer and not a missing one.
	 */
	void SpawnParts();

	/** Dress the body ITSELF from its block: its health, as a row like everything else. */
	void DressBody();

	/** The part a hit landed on, or null when a hit landed on something that is not one of its parts. */
	AProsperitocracyBodyPart* FindPartFor(const UPrimitiveComponent* HitComponent) const;

	/** Handles reaching 0 health: flash (death feedback), then log "Killed" and reset to full health. */
	void HandleOutOfHealth(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec, float EffectMagnitude, float OldValue, float NewValue);

	void ApplyBaseColor();
	void ResetFlashColor();
	void FlashTarget();

	FTimerHandle FlashTimerHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage Target")
	TObjectPtr<UStaticMeshComponent> ChestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage Target")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	/**
	 * The parts' GAS homes, one per part mesh above — the things a hit's answer comes off. Kept so the
	 * body can ask a part for its answer, and so they go away with the body.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AProsperitocracyBodyPart>> PartHomes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage Target")
	TObjectPtr<UProsperitocracyAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage Target")
	TObjectPtr<UProsperitocracyHealthSet> HealthSet;

	/**
	 * Where a STATUS on this target lives: the one component that carries a status's numbers, its
	 * ticking and its ending. A target that can be set alight carries it exactly like any other body
	 * that can (the player's body carries the same component), so nothing about burning is special to
	 * a target.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage Target")
	TObjectPtr<UProsperitocracyStatusComponent> Statuses;
};
