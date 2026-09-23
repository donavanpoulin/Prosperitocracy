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
 * The blade's HEAVY combo: the same move the left button makes, HELD instead of pressed.
 *
 * It is the combo's own shape, four times over: the body covers its Range over the first half of each
 * slice and holds that ground while the blade bites through the second half, each enemy is cut once per
 * slice, and every slice has a recovery behind it that the player's own legs can walk out of. What is
 * different is the INPUT and nothing else — the left button is a press per attack, and this is a HOLD
 * that walks the whole chain for him:
 *
 *   - HELD: the whole thing plays, `a` through `d`, each slice running its attack and its recovery
 *     before the next one begins. Nothing is asked of the player but the button.
 *   - RELEASED: the slice he is in FINISHES — its attack, and the recovery behind it — and then the
 *     move is over. Letting go in the wind-up does not cut the swing he committed to, and letting go in
 *     the recovery still plays that recovery out.
 *
 * FOUR slices, not three, and its OWN section names: the asset says `a`, `b`, `c`, `d` with `a_rec`,
 * `b_rec`, `c_rec`, `d_rec` behind them — which is NOT the naming style of either move that came
 * before it (the left-button combo is `a`, `rec_a`, `b`, ... and the dash is `dash`, `dash_rec`). So
 * this file names its own sections and reads them BY NAME off the montage, never by seconds, which is
 * what keeps a re-timed montage from moving anything but the speed.
 *
 * Its numbers are its own, every one of them, on its own block — Range, Rate, its damage, its
 * Penetration, its Cooldown and its blood cost — so the blade's rows stay the left-button combo's and
 * nothing else's. ONE Range row is all the shape it has, read exactly as the combo reads it: it carries
 * the body, it is how far the swing reaches, and it is how wide the swing is.
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

	/** Whether the player is asking the body to move this frame. */
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

	/** The world time this ability's Cooldown has run out at. Started on the press. */
	float ReadyAt = 0.0f;

	/** How many times this move has been thrown, so its log lines mark each one. */
	int32 HeavyCount = 0;

	/** The clock that runs the move, and the body it runs it on. */
	FTimerHandle StepTimerHandle;
	TWeakObjectPtr<AActor> RunningOn;

	/** Whether this slice's recovery picture has already been dropped, so it is dropped once. */
	bool bPictureDropped = false;

	/** Everyone this slice has already been through, so one slice cuts a body once. */
	TArray<TWeakObjectPtr<AActor>> CutThisSlice;
};
