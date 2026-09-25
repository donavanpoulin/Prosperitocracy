// Copyright Prosperitocracy. All Rights Reserved.

#include "Camera/ProsperitocracyViewShake.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ProsperitocracyLogChannels.h"

FProsperitocracyViewShake::FProsperitocracyViewShake(const FAutoRegister& AutoReg, APlayerController* InOwner)
	: FSceneViewExtensionBase(AutoReg)
	, Owner(InOwner)
{
}

void FProsperitocracyViewShake::AddShake(float ShakeDegrees)
{
	if (ShakeDegrees <= 0.0f)
	{
		return;
	}

	// A NEW ACT PAINTS A NEW WOBBLE: the phase is re-rolled per act, so a burst is not one wave
	// repeated on a metronome and two acts in the same second never line up into one long lean.
	Phase = FMath::FRandRange(0.0f, 2.0f * PI);

	// THE CEILING IS FOR THE BURST, NEVER FOR A HIT. What several acts add up to is stopped at the
	// ceiling, so a gun faster than the decay settles into one steady rumble instead of stacking a
	// shake per bullet. That ceiling is at least what THIS act is worth, so a single hard act is never
	// cut down to size by a number that exists to stop stacking: three swings that each shake hardest
	// still shake hardest, and what a hold-the-trigger burst does is sit there.
	LeanDegrees = FMath::Min(
		LeanDegrees + ShakeDegrees,
		FMath::Max(ProsperitocracyShakeHandling::CeilingDegrees, ShakeDegrees));
}

void FProsperitocracyViewShake::SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InViewInfo)
{
	// A PICTURE BELONGS TO ONE SCREEN. This is the man's own camera or it is nothing: another man's view
	// on this machine, a spectator's, a scene capture's or the editor's preview have no shake of his to
	// be given, and moving them would be moving a screen that never asked.
	if (Player == nullptr || Player != Owner.Get())
	{
		return;
	}

	const UWorld* World = Player->GetWorld();
	if (!World)
	{
		return;
	}

	// THE LEAN RUNS OFF BY TIME, NOT BY TICKS: this is asked for once a frame in the normal run of
	// things, and if it is ever asked for twice in one frame the second ask moves the clock not at all.
	const float Now = World->GetTimeSeconds();
	const float DeltaSeconds = bClockStarted ? FMath::Max(0.0f, Now - LastTimeSeconds) : 0.0f;
	LastTimeSeconds = Now;
	bClockStarted = true;

	LeanDegrees = FMath::Max(0.0f, LeanDegrees - ProsperitocracyShakeHandling::DecayDegreesPerSecond * DeltaSeconds);

	// Gone, and nothing is written at all: a screen with no shake on it is left exactly as the camera
	// manager made it, which is what makes this cost nothing between acts.
	if (LeanDegrees <= UE_KINDA_SMALL_NUMBER)
	{
		LeanDegrees = 0.0f;
		return;
	}

	// THE WOBBLE: three sines off the one number, each at its own rate, which is what reads as a jolt
	// rather than as a camera being slid about. The pitch leads, the yaw follows it and the roll follows
	// both — see the shares above for why the rates are deliberately not the same.
	const float Turns = Now * ProsperitocracyShakeHandling::FrequencyHz * 2.0f * PI + Phase;

	const FRotator Lean(
		FMath::Sin(Turns) * LeanDegrees,
		FMath::Sin(Turns * ProsperitocracyShakeHandling::YawFrequencyShare) * LeanDegrees * ProsperitocracyShakeHandling::YawShare,
		FMath::Sin(Turns * ProsperitocracyShakeHandling::RollFrequencyShare + Phase) * LeanDegrees * ProsperitocracyShakeHandling::RollShare);

	// The facing BEFORE the lean is added, so the slide below is a slide in the world and not one that
	// drags itself round with the lean it is riding on.
	const FRotator Facing = InViewInfo.Rotation;
	InViewInfo.Rotation += Lean;

	// And a little of the same motion spent as POSITION — right and up in the view's own frame, at the
	// same two off-rates. Small on purpose: the eye reads a moving horizon far sooner than it reads
	// being moved a couple of centimetres.
	const float SlideCm = LeanDegrees * ProsperitocracyShakeHandling::LocationCmPerDegree;
	InViewInfo.Location += Facing.RotateVector(FVector(
		0.0f,
		FMath::Sin(Turns * ProsperitocracyShakeHandling::YawFrequencyShare) * SlideCm,
		FMath::Sin(Turns * ProsperitocracyShakeHandling::RollFrequencyShare + Phase) * SlideCm));
}

void UProsperitocracyViewShakeSubsystem::AddShake(float ShakeDegrees)
{
	if (ShakeDegrees <= 0.0f)
	{
		return;
	}

	// MADE ON THE FIRST SHAKE THIS SCREEN IS EVER ASKED FOR. A man's screen has no shake until something
	// he does is worth one, so nothing is created to sit and write nought.
	if (!ViewShake.IsValid())
	{
		ULocalPlayer* LocalPlayer = GetLocalPlayer();
		APlayerController* PC = LocalPlayer ? Cast<APlayerController>(LocalPlayer->PlayerController) : nullptr;
		if (!PC)
		{
			return;
		}

		ViewShake = FSceneViewExtensions::NewExtension<FProsperitocracyViewShake>(PC);
	}

	// Said out loud once per burst rather than once per act: at a gun's rate the per-act line would be
	// ten lines a second, and what is worth seeing is that a shake started and what it is worth — the
	// per-act number is on the Verbose line. A shake nobody can see and a shake that never happened
	// look the same in the world, and this is what tells them apart.
	const bool bWasStill = !ViewShake->IsShaking();

	ViewShake->AddShake(ShakeDegrees);

	UE_LOG(LogProsperitocracy, Verbose, TEXT("[Shake] %s: the picture is leaned by %.2f deg this act"),
		*GetName(), ShakeDegrees);

	if (bWasStill)
	{
		UE_LOG(LogProsperitocracy, Log, TEXT("[Shake] the picture is jolted by %.2f deg — one thing's own Shake row, spent on the screen only"),
			ShakeDegrees);
	}
}
