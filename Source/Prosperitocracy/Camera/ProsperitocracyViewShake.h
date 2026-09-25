// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "ProsperitocracyViewShake.generated.h"

class APlayerController;

/**
 * The SHAPE of the shake — every number the wobble is made of, in one place, all of them [TUNE].
 *
 * None of these is a stat, and that is the whole distinction: the universal stat table holds the one
 * number a THING is worth on the screen (its `Shake` row, in degrees, derived from its damage), and
 * these are how that number is spent — how fast it wobbles, how it leans on each axis, how quickly it
 * runs off. A thing never tunes the shape; the shape is the game's, and every thing in it is shaken by
 * the same one.
 */
namespace ProsperitocracyShakeHandling
{
	/**
	 * degrees — the most the picture is ever leaned by at once.
	 *
	 * A CEILING FOR A BURST, NOT A CAP ON A HIT: what several acts add up to is stopped here, so a fast
	 * gun settles into one steady rumble instead of stacking a shake per bullet. A single act worth more
	 * than this is never cut down to it — see AddShake.
	 */
	constexpr float CeilingDegrees = 0.5f;

	/**
	 * degrees per second — how fast the lean runs off by itself. The only thing that ends a shake: it is
	 * what makes one jolt a jolt rather than a lean that stays, and it is what a burst is fighting.
	 */
	constexpr float DecayDegreesPerSecond = 12.0f;

	/** Hz — how fast the picture wobbles while it is leaned. The single driver of the shape. */
	constexpr float FrequencyHz = 14.0f;

	/**
	 * The other two axes, as shares of the pitch, and their own rates as shares of the one above.
	 *
	 * Off-rates on purpose: three sines at the same rate would lean the picture back and forward in one
	 * flat line, which reads as a camera being slid about rather than a picture being jolted. The shares
	 * keep the pitch leading — the axis a shake is actually felt on — and the roll smallest, because a
	 * leaning horizon is the first thing that reads as wrong rather than as impact.
	 */
	constexpr float YawShare = 0.65f;
	constexpr float RollShare = 0.35f;
	constexpr float YawFrequencyShare = 1.37f;
	constexpr float RollFrequencyShare = 1.71f;

	/**
	 * cm per degree — how much of the same motion is spent as POSITION rather than as lean.
	 *
	 * A camera that only turns reads as a twitch and one that only slides reads as a drift; a little of
	 * both is what a jolt looks like. Deliberately small — the eye is far more sensitive to a moving
	 * horizon than to being moved a couple of centimetres.
	 */
	constexpr float LocationCmPerDegree = 1.0f;
}

/**
 * FProsperitocracyViewShake — THE PICTURE'S SHAKE. The one place in the game that moves the screen for
 * feel, and the only one.
 *
 * WHY IT IS NOT ON THE CAMERA. The chain this game runs on is `mouse -> view -> him -> his gun -> the
 * ring`: the player's look reaches the camera, the MAN then comes round to it at his own rate, his aim
 * is what the shot, the reticle and the pose all read, and his yaw is written from that aim. Anything
 * added to the camera is therefore added to the aim a moment later — it comes back out of the barrel,
 * turns the man, and moves the ring. A shake on the camera is a shake the player shoots with.
 *
 * So the shake is written onto the VIEW THE FRAME IS DRAWN FROM and not onto the camera the game
 * reads: the point of view is worked out for the render (`ULocalPlayer::GetViewPoint`), this leans it
 * there, and the camera manager's own rotation — which is what the aim chases, what the shot flies
 * along and what the ring is projected from — is never touched. That is what makes it LOOK and nothing
 * else: the world is jolted on screen, the barrel does not move a hair, and the ring keeps telling the
 * truth about where the next bullet goes.
 *
 * ONE DOOR IN, ONE PLACE OUT: everything that shakes asks its own `Shake` row (read FINAL off its own
 * GAS home, derived from its damage) and hands the number to AddShake — a gun on a shot, a blade on a
 * swing, and anything added later without this file changing. Nothing else in the game writes a
 * camera offset for feel, and nothing else may.
 *
 * LOCAL, ALWAYS: it is owned by one local player's own shake subsystem (see below), and it moves that
 * man's screen only, off his own controller. A shake is not replicated, does not exist on a server and
 * is never applied to another man's view.
 */
class FProsperitocracyViewShake : public FSceneViewExtensionBase
{
public:
	FProsperitocracyViewShake(const FAutoRegister& AutoReg, APlayerController* InOwner);

	/**
	 * The picture is jolted by this many degrees — one act's worth, off the thing's own Shake row.
	 *
	 * Called on the game thread, once per act that has one: per committed shot of a gun, per attack of a
	 * blade. It adds to what is already there and never stacks past the ceiling, so a burst is one
	 * sustained jolt rather than a sum of them.
	 */
	void AddShake(float ShakeDegrees);

	/** Whether the picture is leaning right now — what the log's "a burst started" reads. */
	bool IsShaking() const { return LeanDegrees > 0.0f; }

	//~ Begin FSceneViewExtensionBase
	/**
	 * The render's own point of view, leaned before it is culled and drawn.
	 *
	 * The one moment the picture can be moved without the game reading it: the camera manager's rotation
	 * was already handed out to the aim this frame, and nothing downstream of this asks the view point
	 * for anything but drawing.
	 */
	virtual void SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InViewInfo) override;
	//~ End FSceneViewExtensionBase

private:
	/** Whose screen this is. Never touched for any other man's view — see SetupViewPoint. */
	TWeakObjectPtr<APlayerController> Owner;

	/** How hard the picture is being leaned right now, in degrees. What decays. */
	float LeanDegrees = 0.0f;

	/** Where in the wave this act's wobble starts, re-rolled per act so two acts never paint the same one. */
	float Phase = 0.0f;

	/** World time at the last ask, so the lean runs off by TIME and not by ticks. */
	float LastTimeSeconds = 0.0f;

	/** Whether the clock above has been started — the first ask has no time behind it to decay by. */
	bool bClockStarted = false;
};

/**
 * UProsperitocracyViewShakeSubsystem — the screen this body's shakes land on, one per LOCAL PLAYER.
 *
 * The home of the picture's shake, and the reason a shake can only ever reach the screen it belongs to:
 * this exists for a local player or not at all, so a dedicated server, a body driven from another
 * machine and a spectator have nowhere to shake and are asked for nothing. The extension inside is made
 * on the first shake a screen is ever asked for, so a body that never fires nothing into the world's
 * view waiting for a number that never comes.
 */
UCLASS()
class UProsperitocracyViewShakeSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * THE DOOR: whatever just did its thing hands over its own Shake row, read FINAL.
	 *
	 * One call per act, from the same per-act hook the act's other numbers hang off, so no thing carries
	 * code of its own for this and everything that acts gets it for free.
	 */
	void AddShake(float ShakeDegrees);

private:
	/** The picture's shake for this screen, or null until the first one. */
	TSharedPtr<FProsperitocracyViewShake, ESPMode::ThreadSafe> ViewShake;
};
