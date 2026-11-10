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
 * WHAT THIS CLASS OWNS IS HOW BIG THE PICTURE IS: THE BLAST'S OWN SIZE, in metres, against the size this look
 * is drawn at (his rule, 2026-11-01 — the rocket strike's 6 m bang IS the size the effect ships at, so it puts
 * the look up at 1:1 and every other blast is its real size beside it). A 4 m incendiary bang is two thirds of
 * that picture and a 14 m bomb two and a third times it. The light's reach takes the Range itself and its worth
 * the square of the same factor, so a bigger blast is a bigger light.
 */
UCLASS()
class AProsperitocracyBlastVisual : public AActor
{
	GENERATED_BODY()

public:
	AProsperitocracyBlastVisual();

	/**
	 * SHOW IT, sized from the blast the thing makes: its Range FINAL, in metres, over the size this look is
	 * drawn at. One number, read through the one evaluator, so a Range perk sizes the picture and there is no
	 * second size anywhere to keep in step.
	 */
	void ShowTheBang(float RangeFinalMeters);

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

	/** THE SIZE THIS LOOK IS DRAWN AT, in metres of blast Range — his number, 2026-11-01: the rocket strike's
	 *  bang, so a 6 m blast puts the effect up at 1:1 and every other blast is its real size against it. */
	static constexpr float TheBlastItsPictureIsDrawnForMeters = 6.0f;

	/** [TUNE] MINE — what a bang throws, in candelas, and how wide its light is against the blast it makes: the
	 *  Range IS the ball's radius, so the flash reaches that Range × this. The number used is the FINAL one, so a
	 *  Range perk makes a bigger light without a line of code here. */
	static constexpr float CandelasABangIsLitWith = 4000.0f;
	static constexpr float HowWideABangsLightIsAgainstItsRange = 1.5f;
	static constexpr float TheLeastABangsLightReachesCm = 200.0f;

	/** [TUNE] MINE — A FLASH IS NOT A LAMP. It comes up in a breath and is gone inside half a second, because the
	 *  picture it belongs to is; a bang's light never outlives its bang. */
	static constexpr float HowLongABangsFlashTakesToComeUpSeconds = 0.06f;
	static constexpr float HowLongABangsFlashTakesToGoOutSeconds = 0.40f;
	static constexpr float HowSoftABangsFlashIsCm = 40.0f;

	/** When this bang's flash began, what it is worth at its brightest and how far it reaches — set once, when the
	 *  bang is shown, and spent by the ramp in Tick. */
	float WhenTheFlashStarted = 0.0f;
	float WhatTheFlashIsWorth = 0.0f;
	float HowFarTheFlashReachesCm = 0.0f;

	/** The flash's own ramp: up in a breath, down over the rest of half a second, and then the light is nothing and
	 *  this actor stops ticking — the picture stays, lighting nothing. */
	virtual void Tick(float DeltaSeconds) override;

	/** [TUNE] A ceiling on how long one bang's picture may live, so an effect that never reports an end
	 *  cannot leave this actor standing in the world for the rest of the mission. */
	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Blast")
	float LongestABangMayBeSeenSeconds = 5.0f;
};
