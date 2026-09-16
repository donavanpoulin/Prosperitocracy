// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "ProsperitocracyNumbersReadoutComponent.generated.h"

#define UE_API PROSPERITOCRACY_API

class UProsperitocracyAbilitySystemComponent;
class UProsperitocracyHealthSet;

/**
 * UProsperitocracyNumbersReadoutComponent
 *
 * The on-screen numbers readout (REDEMPTION box 1.7) — development-only. It puts the values that
 * matter on screen so every later port is testable in game instead of taken on faith. It reads and
 * never writes: nothing here changes a value, and deleting this component would change no number.
 *
 * Today it reads health, because health is the first stat that exists. Each stat ported later gets
 * its line here, in the stat's own unit (Design/stats.md: the real thing in the stat's own unit).
 *
 * It requires a UProsperitocracyAbilitySystemComponent on the same actor and does NOT create one:
 * a readout that silently spawns the thing it reads would hide a wiring mistake instead of showing it.
 */
UCLASS(ClassGroup = (Prosperitocracy), Meta = (BlueprintSpawnableComponent))
class UProsperitocracyNumbersReadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UE_API UProsperitocracyNumbersReadoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// The actor's ability system. Found at BeginPlay, never created here.
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyAbilitySystemComponent> AbilitySystemComponent;

	// Health's GAS home on that ability system. Null means health is not live and there is
	// nothing to print.
	UPROPERTY(Transient)
	TObjectPtr<const UProsperitocracyHealthSet> HealthSet;

	// One fixed screen key: the line refreshes in place instead of stacking a new line per frame.
	static constexpr int32 ReadoutMessageKey = 0x9001;
};

#undef UE_API
