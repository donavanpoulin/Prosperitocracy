// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Explosions/ProsperitocracyBlastVisual.h"

#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "ProsperitocracyLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyBlastVisual)

AProsperitocracyBlastVisual::AProsperitocracyBlastVisual()
{
	// Nothing here ticks except the FLASH: the effect runs itself, and the actor's own job is to leave with it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Blast = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Blast"));
	SetRootComponent(Blast);

	// A SPAWN IS THE BANG. The effect starts because it is here, so nothing anywhere has to say "go" — and
	// the system's own end is what takes this actor down again.
	Blast->SetAutoActivate(true);
	Blast->OnSystemFinished.AddDynamic(this, &AProsperitocracyBlastVisual::TheEffectIsDone);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Blast);

	// A FLASH, and nothing about it is a fixed number: its reach and its worth are read off the blast's own FINAL
	// Range when the bang is shown, and it burns for half a second. No shadows — eighteen shadowed point flashes in
	// one bombardment is a real cost for a light nobody sees for longer than a breath.
	Light->SetIntensity(0.0f);
	Light->SetCastShadows(false);
}

void AProsperitocracyBlastVisual::ShowTheBang(float RangeFinalMeters, float RangeBaseMeters)
{
	// HALF THE GROWTH IS WHAT THE PICTURE TAKES. With no base to compare against — a block that carries no
	// Range — the picture is the size it was authored at rather than a guess at what it should be.
	const float Growth = (RangeBaseMeters > 0.0f) ? (RangeFinalMeters / RangeBaseMeters) : 1.0f;
	const float LookScale = FMath::Max(0.1f, 1.0f + ((Growth - 1.0f) * ShareOfRangeGrowthInTheLook));

	SetActorScale3D(FVector(LookScale));

	// THE FLASH IS SIZED BY THE BLAST IT BELONGS TO. A blast's radius is HALF its Range — the same reading every
	// Range in this game takes — and the number used is the FINAL one, so a Range perk makes a bigger flash with no
	// line of code here. Its worth grows with the ground it covers, which is the square of the same growth.
	HowFarTheFlashReachesCm = FMath::Max(TheLeastABangsLightReachesCm,
		RangeFinalMeters * 100.0f * 0.5f * HowWideABangsLightIsAgainstItsRange);
	WhatTheFlashIsWorth = CandelasABangIsLitWith * LookScale * LookScale;

	const UWorld* World = GetWorld();
	WhenTheFlashStarted = World ? World->GetTimeSeconds() : 0.0f;

	Light->SetAttenuationRadius(HowFarTheFlashReachesCm);
	Light->SetSourceRadius(HowSoftABangsFlashIsCm);
	Light->SetIntensity(0.0f);
	SetActorTickEnabled(true);

	// And a ceiling on its life, so an effect that never reports its own end cannot leave this actor standing
	// in the world for the rest of the mission.
	SetLifeSpan(LongestABangMayBeSeenSeconds);

	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Blast] %s: the picture is x%.2f — Range reads %.1fm against the %.1fm its block ships (the look takes half the growth), and the flash reaches %.1fm."),
		*GetName(), LookScale, RangeFinalMeters, RangeBaseMeters, HowFarTheFlashReachesCm / 100.0f);
}

void AProsperitocracyBlastVisual::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	if (!World || !Light)
	{
		return;
	}

	// UP IN A BREATH, DOWN OVER HALF A SECOND: the flash is the bang's own light and it lasts as long as the front of
	// the bang does. Nothing here is a lamp — the picture is what lives on, and it lights nothing after this.
	const float Since = World->GetTimeSeconds() - WhenTheFlashStarted;
	const float Up = HowLongABangsFlashTakesToComeUpSeconds;
	const float Down = HowLongABangsFlashTakesToGoOutSeconds;

	if (Since >= Up + Down)
	{
		// THE FLASH IS OVER, so this actor stops ticking: what is left of its life is the picture, and a tick that
		// keeps writing nothing onto a light is a cost with no picture attached to it.
		Light->SetIntensity(0.0f);
		SetActorTickEnabled(false);
		return;
	}

	const float ItsShareOfTheFlash = (Since < Up)
		? (Up > 0.0f ? Since / Up : 1.0f)
		: (Down > 0.0f ? 1.0f - ((Since - Up) / Down) : 0.0f);

	Light->SetIntensity(WhatTheFlashIsWorth * FMath::Clamp(ItsShareOfTheFlash, 0.0f, 1.0f));
}

void AProsperitocracyBlastVisual::TheEffectIsDone(UNiagaraComponent* Finished)
{
	// The last particle is gone, so the look is over: this actor goes with it rather than sitting here empty.
	Destroy();
}
