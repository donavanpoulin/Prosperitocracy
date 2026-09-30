// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ProsperitocracyBlastVisual.generated.h"

class UNiagaraComponent;
class UPointLightComponent;

/**
 * THE LOOK OF A BANG, on a plain actor (PLANS/incendiary-strike.md §2: "the blast Niagara and a point
 * light, on a plain actor").
 *
 * It is spawned by the ONE explosion pass, at the place an explosion goes off, and it lives exactly as long
 * as its own effect: the system starts on the spawn — a spawn IS the bang, there is no second "go" — and
 * when its last particle dies this actor goes with it.
 *
 * WHICH SYSTEM IT IS belongs to the blueprint, and WHICH BLUEPRINT belongs to the explosive: the thing's own
 * block names the look it puts up (UProsperitocracyStatTable::BlastVisual), so a firey explosive and a
 * shrapnel one differ by pointing at different actors and nothing anywhere needs a line of code for it.
 *
 * WHAT THIS CLASS OWNS IS HOW THE PICTURE GROWS: the look takes HALF the Range's growth (his rule), so an
 * explosion whose Range reads twice its block's base looks half again as big rather than twice. The effect
 * and the light's reach take the same factor, so a bigger blast is a bigger light.
 */
UCLASS()
class AProsperitocracyBlastVisual : public AActor
{
	GENERATED_BODY()

public:
	AProsperitocracyBlastVisual();

	/**
	 * SHOW IT, sized from the thing that went off: what its Range reads NOW against what its block shipped
	 * with. Both numbers come from the explosion — one from the one evaluator, one from the thing's own
	 * block — so a Range perk grows the picture and there is no second size anywhere to keep in step.
	 */
	void ShowTheBang(float RangeFinalMeters, float RangeBaseMeters);

protected:
	/** The effect's own end, and this actor's: no fade is authored here and nothing runs on a clock. */
	UFUNCTION()
	void TheEffectIsDone(UNiagaraComponent* Finished);

	/** The blast itself. WHICH system it is, is the blueprint's. */
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Blast")
	TObjectPtr<UNiagaraComponent> Blast;

	/** The light the bang throws, hanging on the effect so the whole picture grows together. */
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Blast")
	TObjectPtr<UPointLightComponent> Light;

	/** [TUNE] The share of the Range's growth the picture takes — half of it, so a range upgrade reads as a
	 *  bigger blast without ballooning when range and rate upgrades stack. */
	static constexpr float ShareOfRangeGrowthInTheLook = 0.5f;

	/** [TUNE] The light's reach at BASE range, in cm — the pack's own explosion ships 500 (5 m). It is
	 *  scaled by the same factor as the effect, so a bigger blast is a bigger light. */
	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Blast")
	float LightReachAtBaseCm = 500.0f;

	/** [TUNE] A ceiling on how long one bang's picture may live, so an effect that never reports an end
	 *  cannot leave this actor standing in the world for the rest of the mission. */
	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Blast")
	float LongestABangMayBeSeenSeconds = 30.0f;
};
