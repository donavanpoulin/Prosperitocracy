// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/ProsperitocracyDamageReceiver.h"
#include "Engine/TimerHandle.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "ProsperitocracyDamageTarget.generated.h"

class UProsperitocracyAbilitySystemComponent;
class UProsperitocracyHealthSet;
class UStaticMeshComponent;
class UAbilitySystemComponent;
struct FGameplayEffectSpec;

/**
 * AProsperitocracyDamageTarget
 *
 * A simple, non-animated test target used to exercise the ONE damage pipeline (pen gate + resist).
 * Built of two parts, each a hit target:
 *   - Head (cube on top): armor 0 — any pen over-pens for full.
 *   - Chest (cylinder body): armor 1 — pen 1 matches (50% reduced), pen 2+ is full.
 * Resists default to 0; give a part an ImpactResist/PiercingResist to test the resist axis.
 *
 * Implements IProsperitocracyDamageReceiver so the shared execution asks it for the hit part's
 * profile, and IAbilitySystemInterface so the weapon can find the ASC to apply the damage to.
 */
UCLASS()
class AProsperitocracyDamageTarget : public AActor, public IAbilitySystemInterface, public IProsperitocracyDamageReceiver
{
	GENERATED_BODY()

public:
	AProsperitocracyDamageTarget();

	// IProsperitocracyDamageReceiver: return the profile of the part that was hit.
	virtual FProsperitocracyDamageProfile GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext) const override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Per-part profiles, editable so instances/subclasses can test different resists/armor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Target|Head")
	FProsperitocracyDamageProfile HeadProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Target|Chest")
	FProsperitocracyDamageProfile ChestProfile;

	// Solid color for the whole target (per ui.md). Set per instance: pierce/impact/neutral.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Target")
	FLinearColor BaseColor = FLinearColor::White;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage Target")
	TObjectPtr<UProsperitocracyAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage Target")
	TObjectPtr<UProsperitocracyHealthSet> HealthSet;
};
