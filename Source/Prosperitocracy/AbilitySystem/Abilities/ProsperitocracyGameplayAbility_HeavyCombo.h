// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility.h"
#include "TimerManager.h"

#include "ProsperitocracyGameplayAbility_HeavyCombo.generated.h"

class AProsperitocracyCharacter;
class AProsperitocracyWeapon;
class UAnimInstance;
class UAnimMontage;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilityActivationInfo;
struct FGameplayAbilitySpecHandle;
struct FGameplayEventData;
struct FHitResult;

/**
 * The blade's HEAVY combo: ONE move, split into beats, and HELD instead of pressed.
 *
 * This is NOT the left-button combo with a different key, and the difference is the whole reason this file
 * exists separately. The left-button combo is a series of swings, each with its own real recovery — a gap
 * of the player's own time between one swing and the next, which he is free to walk out of, and walking out
 * of it is a legitimate end to the combo. THIS move is one continuous attack chopped into sections, and the
 * sections are its BEATS: each one is where the body travels and where the blade bites, and the recovery
 * behind it is the rest of that same motion, not a gap between two moves.
 *
 * THE ROOT OF EVERYTHING DIFFERENT ABOUT IT IS ONE LENGTH: how long the body refuses the player's own
 * movement. The left-button combo hands over the ATTACK, so the recovery behind each swing is a gap of
 * unlocked time he can walk out of — which is right for that combo, because its recoveries ARE his own time
 * and walking out of one is how it is left. THIS move hands over the WHOLE BEAT while the key is down, so he
 * is committed from the first swing of a beat to the last moment of the recovery behind it and nothing he
 * does with his legs can walk the chain apart. The moment the key comes up the lock is handed back
 * (`ReleaseMovementLock`), and from then on that recovery is his own time — which is where walking out of it
 * becomes possible.
 *
 * From that one length every other rule follows:
 *
 *   1. WHILE THE KEY IS DOWN NOTHING CANCELS A RECOVERY — he cannot move at all, so there is nothing to
 *      cancel it with. The recovery plays whole and the move carries on to the next beat. Let go and stand
 *      still and the recovery still plays too: letting go ends the RUN, never the beat, which is also why an
 *      attack he has committed to is never cut mid-swing.
 *   2. A RECOVERY'S PICTURE IS DROPPED IN EXACTLY ONE CASE — the key is UP and he is asking to move. That is
 *      the only case in the whole move where anything gives way to his legs.
 *   3. HELD, IT WALKS THE BEATS ITSELF. `a` through `d`, each beat running its attack and the recovery
 *      behind it before the next one begins; nothing is asked of the player but the button.
 *
 * AND THAT LEAVES THE INPUT AS THE ONLY OUTWARD DIFFERENCE: this advances while the key is HELD where the
 * left-button combo advances on a PRESS. Everything else — the bite, the travel landing with the cut, each
 * recovery playing at its own speed, the window as TIME, each enemy cut once per swing — is the same move.
 *
 * Letting go ends the RUN, never the beat: the beat he is in finishes — its attack, and the recovery behind
 * it — and then the move is over. That is why letting go is never a cancel: an attack he committed to is
 * never overwritten mid-swing, which is the same rule every other move in the game obeys.
 *
 * FOUR beats, not three, and its OWN section names: the asset says `a`, `b`, `c`, `d` with `a_rec`, `b_rec`,
 * `c_rec`, `d_rec` behind them — which is NOT the naming style of either move that came before it (the
 * left-button combo is `a`, `rec_a`, `b`, ... and the dash is `dash`, `dash_rec`). So this file names its
 * own sections and reads them BY NAME off the montage, never by seconds, which is what keeps a re-timed
 * montage from moving anything but the speed.
 *
 * Its numbers are its own, every one of them, on its own block — Range, Rate, its damage, its Penetration,
 * its Cooldown and its blood cost — so the blade's rows stay the left-button combo's and nothing else's.
 * ONE Range row is all the shape it has, read exactly as the combo reads it: it carries the body, it is how
 * far the swing reaches, and it is how wide the swing is.
 */
UCLASS()
class UProsperitocracyGameplayAbility_HeavyCombo : public UProsperitocracyGameplayAbility
{
	GENERATED_BODY()

public:
	UProsperitocracyGameplayAbility_HeavyCombo();

	/** Whether this move is running on its owner right now. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	bool IsRunningTheHeavyCombo() const { return bRunning; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * The heavy combo's montage: its own file (`Sword_Heavy`), its own eight sections, and the four
	 * slices' own piece of it. Set on this ability's blueprint — the blade carries the pictures of the
	 * moves it makes and this ability is one of them, so the animation is never the C++ class's to
	 * invent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Blade")
	TObjectPtr<UAnimMontage> HeavyComboMontage;

private:
	/** The body's anim instance, which is where a move plays. Null while there is no body. */
	UAnimInstance* GetBodyAnimInstance() const;

	/**
	 * Start the move, or say why it will not start.
	 *
	 * Refused for every reason it can be, each said out loud rather than being a press that does
	 * nothing: no body, no blade in hand, no block (so no Range, no Cooldown and no numbers), an
	 * attack already live, the cooldown still running, or a pool that cannot pay for it.
	 */
	bool StartTheHeavyCombo(AProsperitocracyCharacter* Body, AProsperitocracyWeapon* Blade);

	/** One step of the move's own clock: the bite, the window, the recovery's picture, and the slice's end. */
	void ComboStep();

	/** Stop the clock the move runs on. */
	void StopTheStepClock();

	/** The clock's own step length, in seconds — a rate, never a game number. */
	static constexpr float StepSeconds = 0.01f;

	/** The montage's section index for a name this move asked for, or INDEX_NONE when it is not there. */
	int32 FindSection(const TCHAR* SectionName) const;

	/** How long one slice's ATTACK lasts in the world: its own section at the rate the player reads. */
	float GetAttackSeconds(int32 Slice) const;

	/** How long one slice's recovery lasts — its own length at its OWN speed, never the attack's rate. */
	float GetRecoverySeconds(int32 Slice) const;

	/** What the move is played at: this ability's FINAL Rate over its own base Rate. */
	float GetPlayRate() const;

	/** This ability's own Cooldown, in seconds, read FINAL through its own GAS home. */
	float GetCooldownSeconds() const;

	/** The Range in cm — the one number that carries the body and sizes the swing. */
	float GetRangeCm() const;

	/**
	 * The number the Rate ROW should hold: the four slices' attacks over their own lengths, at their own
	 * speed — `4 / (a + b + c + d)` in attacks a second. Logged so the row is set from this rather than
	 * from a boundary somebody worked out on paper.
	 */
	float GetTrueAttackRate() const;

	/**
	 * Put the next slice on: a NEW line taken at that moment, its picture through the body's own door,
	 * and the numbers handed over — the line, the Range, and the TWO times, so the dash lands with the
	 * cut.
	 */
	void BeginTheSlice(int32 Slice, AProsperitocracyCharacter* Body);

	/** The move is over: nothing is live any more, and the facing it held is let go. */
	void EndTheMove();

	/** One sweep of the swing, at the moment it is made. Each enemy is cut once per slice. */
	void CutWhatTheSwingIsThrough();

	/** Whether this slice has already been through that actor. */
	bool HasAlreadyBeenCut(const AActor* Actor) const;

	/** One hit, through the one damage pipeline, with this ability's own lines and its own source. */
	void ApplyHeavyHitTo(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, const FHitResult& Hit, const AProsperitocracyWeapon& Blade);

	/**
	 * Whether the right mouse button is STILL DOWN — asked of the thing in the player's hand, which is
	 * where the press and the release both land. That one read is the whole of what a hold is: nothing
	 * has to tell this ability when the button came up, because the weapon it was pressed on already
	 * knows.
	 */
	bool IsTheButtonStillDown() const;

	/**
	 * Whether the player is asking the body to move this frame — the other half of the one condition that
	 * drops a recovery's picture.
	 */
	bool HasMovementInput() const;

	/** Tell the body the move is over, so the facing it has been holding is let go and it turns back. */
	void ReleaseTheBodyFacing();

	/** Whether a move is running: true between the first slice starting and the last one finishing. */
	bool bRunning = false;

	/** Which slice is playing — `a` 0 through `d` 3 — while the move runs. */
	int32 CurrentSlice = INDEX_NONE;

	/**
	 * The direction the slice travels and faces, taken ONCE at that slice's start and held for it.
	 * FLAT, always: the swing goes along the ground the player is standing on, whatever the camera was
	 * doing with its pitch — and a new slice takes a new line, exactly as a new press does.
	 */
	FVector AttackLine = FVector::ZeroVector;

	/** The slice's attack clock, kept here so the bite and the window know where in the slice it is. */
	float AttackElapsed = 0.0f;
	float AttackSeconds = 0.0f;

	/**
	 * How long the slice stays open behind its attack — the window, as its own clock: it opens when the
	 * attack's clock runs out and lasts that recovery's ORIGINAL length, whether or not its picture is
	 * still playing. When it runs out the slice is over, and the next one begins if the button is still
	 * held.
	 */
	float WindowRemaining = 0.0f;

	/**
	 * Whether this slice's window has already been opened.
	 *
	 * The window opens ONCE, at the end of the attack, and then only ticks down: without this, a window
	 * read as "empty" every step would be refilled every step and the slice would never end.
	 */
	bool bWindowOpened = false;

	/**
	 * Whether the button has come UP at any point since this run started.
	 *
	 * THE RUN IS SPENT THE MOMENT THE BUTTON COMES UP, and this is what remembers it. The button's state
	 * is LATCHED rather than sampled: reading it only at a slice's own boundary would let a tap that
	 * happens to land on that boundary carry the chain on, and a chain carried by taps is a PRESS driving
	 * the move — which is the left-button combo, not this one. This move is a HOLD: the only thing that
	 * carries it on is a key that has stayed down, so one release anywhere in the run ends it at the
	 * slice it is in — and no later press can resurrect it, because the run it belonged to is over.
	 */
	bool bButtonCameUp = false;

	/** The world time this ability's Cooldown has run out at. Started on the press. */
	float ReadyAt = 0.0f;

	/** How many times this move has been thrown, so its log lines mark each one. */
	int32 HeavyCount = 0;

	/** The clock that runs the move, and the body it runs it on. */
	FTimerHandle StepTimerHandle;
	TWeakObjectPtr<AActor> RunningOn;

	/**
	 * Whether this beat's recovery picture has already been dropped, so it is dropped once.
	 *
	 * Needed because a montage still counts as playing while it blends out: without this the drop would
	 * re-fire on every step of the blend.
	 */
	bool bPictureDropped = false;

	/** Everyone this slice has already been through, so one slice cuts a body once. */
	TArray<TWeakObjectPtr<AActor>> CutThisSlice;
};
