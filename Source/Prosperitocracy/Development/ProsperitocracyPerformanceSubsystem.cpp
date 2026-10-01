// Copyright Prosperitocracy. All Rights Reserved.

#include "Development/ProsperitocracyPerformanceSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProsperitocracyLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyPerformanceSubsystem)

bool UProsperitocracyPerformanceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// A RUNNING GAME has frames. The editor's own world, an asset preview and an inactive world do not,
	// and counting them would put a number in the log that no one ever played.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UProsperitocracyPerformanceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UProsperitocracyPerformanceSubsystem, STATGROUP_Tickables);
}

void UProsperitocracyPerformanceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// A frame that took half a second is the world hitching — a shader compile, a hitch on load — and not
	// the game running. Counting it would poison a whole session's average with something that is not a
	// fact about the game.
	if (DeltaTime <= 0.0f || DeltaTime > SlowestFrameThatStillCountsSeconds)
	{
		return;
	}

	const float Fps = 1.0f / DeltaTime;

	// "RIGHT NOW" IS SMOOTHED, because one frame is noise and he is reading a number off the screen.
	FpsNow = (FpsNow <= 0.0f) ? Fps : FMath::Lerp(FpsNow, Fps, HowMuchOfANewReadingShowsNow);

	const UWorld* World = GetWorld();
	const float SessionTime = World ? World->GetTimeSeconds() : 0.0f;

	// The session's first second is startup: shaders, the level, the first spawn. A session's average
	// should be the session, so nothing is counted until the game has actually been running a moment.
	if (SessionTime < TheSessionStartsWithSeconds)
	{
		return;
	}

	// A REAL AVERAGE OF REAL FRAMES: the frames and their lengths are summed, and the average is the one
	// divided by the other. Nothing here is sampled, guessed or estimated.
	++FramesCounted;
	SumOfFrameSeconds += (double)DeltaTime;
	SecondsCounted += DeltaTime;

	LowestFps = (LowestFps <= 0.0f) ? Fps : FMath::Min(LowestFps, Fps);
	HighestFps = FMath::Max(HighestFps, Fps);
}

float UProsperitocracyPerformanceSubsystem::GetSessionAverageFps() const
{
	return (SumOfFrameSeconds > 0.0) ? (float)((double)FramesCounted / SumOfFrameSeconds) : 0.0f;
}

FString UProsperitocracyPerformanceSubsystem::DescribeTheSession() const
{
	if (FramesCounted == 0)
	{
		return TEXT("[Perf] nothing counted yet — this session has not been running a second.");
	}

	return FString::Printf(
		TEXT("[Perf] session: average %.1f fps over %.0f s (%lld frames), lowest %.1f, highest %.1f, right now %.1f"),
		GetSessionAverageFps(), SecondsCounted, FramesCounted, LowestFps, HighestFps, FpsNow);
}

void UProsperitocracyPerformanceSubsystem::Deinitialize()
{
	// THE NUMBER A SESSION LEAVES BEHIND. Logged whether anybody remembered to ask or not, so a play
	// test always ends with its own average in the log — that line is what a PROGRESS.md entry carries.
	UE_LOG(LogProsperitocracy, Log, TEXT("%s"), *DescribeTheSession());

	Super::Deinitialize();
}

/**
 * `Prosperitocracy.FPS` — the session's frames, on the screen and in the log.
 *
 * The one command a play test needs: what the game is running at now, what it has averaged, and its
 * worst and best frames. Nothing about the game changes when it runs — it measures and says.
 */
static void ReportTheSessionsFrames(UWorld* World)
{
	if (!World)
	{
		return;
	}

	const UProsperitocracyPerformanceSubsystem* Frames = World->GetSubsystem<UProsperitocracyPerformanceSubsystem>();
	if (!Frames)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Perf] this world has no performance measuring (it is not a running game) — there is nothing to report."));
		return;
	}

	const FString Line = Frames->DescribeTheSession();
	UE_LOG(LogProsperitocracy, Log, TEXT("%s"), *Line);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(/*Key=*/ 0x9510, /*TimeToDisplay=*/ 10.0f, FColor(0xFF, 0x3B, 0x00), Line);
	}
}

static FAutoConsoleCommandWithWorld ReportFramesCommand(
	TEXT("Prosperitocracy.FPS"),
	TEXT("the session's average frames, its worst and best, and what it runs at right now — the number a PROGRESS entry carries"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&ReportTheSessionsFrames));
