// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ProsperitocracyPerformanceSubsystem.generated.h"

/**
 * THE SESSION'S FRAMES, MEASURED ONCE AND RECORDED (his ask, 2026-09-30).
 *
 * He plays; the number that decides whether a feature is worth having is whether the game still runs. So
 * the frames are counted for the whole session, by one small thing that does nothing else:
 *   - every frame adds its own length to a sum, so the SESSION AVERAGE is a real average of real frames
 *     and not a guess or an eyeball;
 *   - the lowest and highest frames are kept, because an average hides the stutter that is felt;
 *   - and "right now" is a smoothed reading rather than one frame, which is what `Prosperitocracy.FPS`
 *     shows on screen along with the session's own numbers.
 *
 * The session average is logged when the session ends, so a play test leaves the number in the log
 * whether anybody remembered to ask for it or not — that line is what goes into PROGRESS.md (README).
 *
 * It measures and reports. It never changes the game: no cap, no throttle, no level of detail — a
 * measurement that changed what it measured would be telling him a story about a game he did not play.
 */
UCLASS()
class UProsperitocracyPerformanceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Only a running game has frames worth counting: not the editor, not an asset preview. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** The session is over: its own summary goes into the log, whether anybody asked for it or not. */
	virtual void Deinitialize() override;

	/** The whole session, in frames per second. 0 until the first second has been measured. */
	float GetSessionAverageFps() const;

	/** The smoothed reading right now — what the counter shows. */
	float GetFpsNow() const { return FpsNow; }

	/** The worst and best frames of the session, once it is warm. 0 until it is. */
	float GetLowestFps() const { return LowestFps; }
	float GetHighestFps() const { return HighestFps; }

	/** How much of the session has been counted, and out of how many frames. */
	float GetSecondsCounted() const { return SecondsCounted; }
	int64 GetFramesCounted() const { return FramesCounted; }

	/** The one line the log and the screen both carry — the session's own summary. */
	FString DescribeTheSession() const;

private:
	/** [TUNE] MINE — a frame shorter than this (longer than 1/X seconds) never counts: it is a hitch in
	 *  loading, not the game running, and one of them would poison a whole session's average. */
	static constexpr float SlowestFrameThatStillCountsSeconds = 0.5f;

	/** [TUNE] MINE — how long the session runs before it is measured at all: the first second is
	 *  startup, and a session's average should be the session, not the shader compile. */
	static constexpr float TheSessionStartsWithSeconds = 1.0f;

	/** How much of a new reading the smoothed one takes — small, so "now" is steady enough to read. */
	static constexpr float HowMuchOfANewReadingShowsNow = 0.05f;

	/** Every frame counted, in seconds. */
	double SumOfFrameSeconds = 0.0;

	/** The frames counted. */
	int64 FramesCounted = 0;

	/** How much session has been counted. */
	float SecondsCounted = 0.0f;

	/** The worst and best of the counted frames. 0 = nothing counted yet. */
	float LowestFps = 0.0f;
	float HighestFps = 0.0f;

	/** The smoothed reading. */
	float FpsNow = 0.0f;

	/** The clock has started: the first frame of the session is what starts it. */
	bool bCounting = false;
};
