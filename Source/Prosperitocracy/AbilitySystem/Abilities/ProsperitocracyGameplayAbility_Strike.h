// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility.h"

#include "ProsperitocracyGameplayAbility_IncendiaryStrike.generated.h"

class AProsperitocracyProjectile;
class UGameplayEffect;
class UProsperitocracyStatTable;

/**
 * Where a call is PLACED, in cm — the one number the ability needs that is not a stat.
 *
 * A strike can be sent ANYWHERE on the map (his fact: "you can send a strike ANYWHERE on the map, theres
 * no actual range"), so this is the reach of the line the placement is found along, not a range the call
 * has to fit inside: nothing inside it is refused and nothing past it is refused either — the line simply
 * ends there and the strikes land at its end. `[TUNE]`
 */
namespace ProsperitocracyIncendiaryStrikeHandling
{
	constexpr float PlacementReachCm = 100000.0f;
	constexpr float CentimetersPerMeter = 100.0f;
}

/**
 * UProsperitocracyGameplayAbility_IncendiaryStrike
 *
 * THE ANCHOR'S FIRST STRIKE, the bombardment itself (PLANS/incendiary-strike.md part 2).
 *
 * One press is the WHOLE bombardment: the first strike falls on the call and then one every 1/Rate until
 * the ability's Duration is up. Its Range is the circle the strikes land inside — the SIZE of the
 * bombardment, never how far it can be called — and only each strike's LANDING POINT has to be inside that
 * circle: where a strike came in from is its own business, and the blast it makes may spill past the edge.
 *
 * The ability owns the bombardment's four rows (Duration, Rate, Range, Cooldown) off its own block. THE
 * STRIKE ITSELF IS A SEPARATE THING with its own block, handed over where each one is born — so what the
 * blast is worth, how wide it is and what it sets alight are the strike's numbers, and this ability is only
 * the falling.
 */
UCLASS()
class UProsperitocracyGameplayAbility_IncendiaryStrike : public UProsperitocracyGameplayAbility
{
	GENERATED_BODY()

public:
	UProsperitocracyGameplayAbility_IncendiaryStrike();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/**
	 * THE NEXT ONE FALLS, or the bombardment is over.
	 *
	 * It is the ability's own clock that decides when the bombardment has had its run — the Duration row,
	 * read FINAL, counted from the call — so nothing counts strikes and nothing can disagree with it.
	 */
	void TheBombardmentKeepsFalling();

	/**
	 * WHERE THE BOMBARDMENT IS CALLED: the point he is aiming at, found along the SAME line his shot flies
	 * down (the man's own aim, never the raw camera), traced out to the placement reach.
	 *
	 * A strike can be sent anywhere, so there is nothing to refuse here: the line simply ends, and a call
	 * with nothing under it lands where the aim ran out.
	 */
	bool FindWhereItLands(FVector& OutPlace) const;

	/** The ability's own rows, read FINAL through its GAS home — the ONE evaluator. */
	float GetDurationSeconds() const;
	float GetRatePerSecond() const;
	float GetCircleRadiusCm() const;
	float GetCooldownSeconds() const;

	/** The bombardment stops saying why it stopped, and hands the ability back. */
	void EndTheBombardment(const TCHAR* Why);

	/**
	 * THE STRIKE ITSELF: the thing that falls, and the block it flies with.
	 *
	 * Two assets and no code: the projectile's own body (the mesh and its quarter-scale are its blueprint's)
	 * and the block that says what the blast is worth — the explosion mark on it is what enrols the strike in
	 * the one explosion pass, so this ability knows nothing about balls, falloff, shoves or shakes.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Strike")
	TSubclassOf<AProsperitocracyProjectile> StrikeProjectile;

	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Strike")
	TObjectPtr<UProsperitocracyStatTable> StrikeStatBlock;

	/** The one effect the damage pipeline runs on, handed to each strike as it is born. */
	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Strike")
	TSubclassOf<UGameplayEffect> StrikeDamageEffectClass;

private:
	/** When the bombardment was called, and when this ability's own Cooldown runs out. The clock starts on
	 * the CALL, never on the last strike landing (the plan's rule). */
	float CalledAt = 0.0f;
	float ReadyAt = 0.0f;

	/** The middle of the circle the strikes land inside, fixed at the call. */
	FVector TheCircleCentre = FVector::ZeroVector;

	/** How many fell, so the log lines mark the bombardment. */
	int32 StrikesDropped = 0;

	FTimerHandle FallingTimerHandle;
};
