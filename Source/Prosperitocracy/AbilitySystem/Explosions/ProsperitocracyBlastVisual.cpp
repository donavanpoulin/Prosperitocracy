// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Explosions/ProsperitocracyBlastVisual.h"

#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "ProsperitocracyLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyBlastVisual)

AProsperitocracyBlastVisual::AProsperitocracyBlastVisual()
{
	// Nothing here ticks: the effect runs itself and the actor's only job is to leave with it.
	PrimaryActorTick.bCanEverTick = false;

	Blast = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Blast"));
	SetRootComponent(Blast);

	// A SPAWN IS THE BANG. The effect starts because it is here, so nothing anywhere has to say "go" — and
	// the system's own end is what takes this actor down again.
	Blast->SetAutoActivate(true);
	Blast->OnSystemFinished.AddDynamic(this, &AProsperitocracyBlastVisual::TheEffectIsDone);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Blast);

	// The pack's own explosion actor, as it shipped: a point light at intensity 300 with a reach of 500 cm
	// (5 m). [TUNE] Both live on the blueprint; the reach is the one that scales with the picture.
	Light->SetIntensity(300.0f);
	Light->SetAttenuationRadius(LightReachAtBaseCm);
}

void AProsperitocracyBlastVisual::ShowTheBang(float RangeFinalMeters, float RangeBaseMeters)
{
	// HALF THE GROWTH IS WHAT THE PICTURE TAKES. With no base to compare against — a block that carries no
	// Range — the picture is the size it was authored at rather than a guess at what it should be.
	const float Growth = (RangeBaseMeters > 0.0f) ? (RangeFinalMeters / RangeBaseMeters) : 1.0f;
	const float LookScale = FMath::Max(0.1f, 1.0f + ((Growth - 1.0f) * ShareOfRangeGrowthInTheLook));

	SetActorScale3D(FVector(LookScale));
	Light->SetAttenuationRadius(LightReachAtBaseCm * LookScale);

	// And a ceiling on its life, so an effect that never reports its own end cannot leave this actor standing
	// in the world for the rest of the mission.
	SetLifeSpan(LongestABangMayBeSeenSeconds);

	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Blast] %s: the picture is x%.2f — Range reads %.1fm against the %.1fm its block ships (the look takes half the growth)."),
		*GetName(), LookScale, RangeFinalMeters, RangeBaseMeters);
}

void AProsperitocracyBlastVisual::TheEffectIsDone(UNiagaraComponent* Finished)
{
	// The last particle is gone, so the look is over: this actor goes with it rather than sitting here empty.
	Destroy();
}
