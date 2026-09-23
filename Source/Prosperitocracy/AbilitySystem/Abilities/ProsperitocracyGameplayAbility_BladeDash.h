// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility.h"
#include "TimerManager.h"

#include "ProsperitocracyGameplayAbility_BladeDash.generated.h"

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
 * The blade's swappable right-click ability, as it is built today: the DASH.
 *
 * A SWORD's ability is an ABILITY, not a mode inside the blade: the blade carries a slot naming which
 * one it fires, so a sword throw — or anything else a sword should do on a press — is a new ability
 * and nothing about the blade changes. The numbers, the animation and the hit are all this ability's
 * own; from the body it is on it takes only the two doors it needs (is an attack live, and begin
 * this attack) and the one door every hit in the game travels.
 *
 * It is the blade's other move, with two differences from the combo and both are this ability's:
 *   - it cuts for the WHOLE attack, swept as it goes, rather than over a bite window — the body is
 *     travelling THROUGH what it cuts, so a window would let a man walk out of the far side unhurt;
 *   - it carries a COOLDOWN — the first thing in this game to have one.
 *
 * ONE Range row, FOUR readings. Range is the whole size of the move: how far the dash carries the
 * body, and how big the arc in front of it is — a FAN whose forward length is a THIRD of the Range,
 * whose width is HALF of it, and whose height is that same third. One number, so one Range perk
 * moves all four readings together.
 */
UCLASS()
class UProsperitocracyGameplayAbility_BladeDash : public UProsperitocracyGameplayAbility
{
	GENERATED_BODY()

public:
	UProsperitocracyGameplayAbility_BladeDash();

	/** Whether this ability's move is running on its owner right now. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	bool IsDashing() const { return bDashing; }

	/** Whether a dash is running on that body right now — the same question, asked from outside. */
	static bool IsDashingOn(AActor* Body);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * The dash's montage: its own file, with its own two sections, `dash` (the attack) and `dash_rec`
	 * (its recovery). Read BY NAME, never by seconds, so moving Rate — which re-times every part of
	 * the montage — cannot move a window that was never in seconds to begin with.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Blade")
	TObjectPtr<UAnimMontage> DashMontage;

private:
	/** The body's anim instance, which is where a move plays. Null while there is no body. */
	UAnimInstance* GetBodyAnimInstance() const;

	/**
	 * Start the move, or say why it will not start.
	 *
	 * Refused for every reason it can be, and each refusal is said out loud rather than being a press
	 * that does nothing: no body, no blade in hand, no block (so no Range, no Cooldown, no numbers),
	 * an attack already live, or the cooldown still running.
	 */
	bool StartTheDash(AProsperitocracyCharacter* Body, AProsperitocracyWeapon* Blade);

	/** One step of the move's own clock: the cut, the window, and the recovery's picture. */
	void DashStep();

	/** Stop the clock the move runs on. */
	void StopTheStepClock();

	/** The clock's own step length, in seconds — a rate, never a game number. */
	static constexpr float DashStepSeconds = 0.01f;

	/** How long the attack lasts in the world: its own section at the rate the player reads. */
	float GetAttackSeconds() const;

	/** How long the recovery lasts — its own length at its OWN speed, never the attack's rate. */
	float GetRecoverySeconds() const;

	/** What the move is played at: this ability's FINAL Rate over its own base Rate. */
	float GetPlayRate() const;

	/** The dash's own Cooldown, in seconds, read FINAL through this ability's own GAS home. */
	float GetCooldownSeconds() const;

	/** The arc's forward length in cm — one reading of the one Range row, in the unit a body moves in. */
	float GetReachCm() const;

	/** One sweep of the fan, at the moment it is made. Each enemy is cut once per dash. */
	void CutWhatTheDashIsThrough();

	/** Whether this dash has already been through that actor. */
	bool HasAlreadyBeenCut(const AActor* Actor) const;

	/** One hit, through the one damage pipeline, with this ability's own lines and its own source. */
	void ApplyDashToHit(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, const FHitResult& Hit, const AProsperitocracyWeapon& Blade);

	/** Whether the player is asking the body to move this frame. */
	bool HasMovementInput() const;

	/** Tell the body the move is over, so the facing it has been holding is let go and it turns back. */
	void ReleaseTheBodyFacing();

	/** Whether a move is running: true between the attack starting and the window closing. */
	bool bDashing = false;

	/**
	 * The direction the dash travels and faces, taken ONCE at the press and held for the whole move.
	 * FLAT, always: the dash goes along the ground the player is standing on, whatever the camera was
	 * doing with its pitch, and the body faces it until the move is over.
	 */
	FVector AttackLine = FVector::ZeroVector;

	/** The attack's own clock, kept here so the cut and the window know where in the move it is. */
	float AttackElapsed = 0.0f;
	float AttackSeconds = 0.0f;

	/**
	 * How long the move stays OPEN — the window, as its own clock: it opens when the attack's clock
	 * runs out and lasts the recovery's ORIGINAL length, whether or not its picture is still playing.
	 */
	float WindowRemaining = 0.0f;

	/** The world time this ability's Cooldown has run out at. Started on the press. */
	float ReadyAt = 0.0f;

	/** How many times this dash has been thrown, so its log lines mark each one. */
	int32 DashCount = 0;

	/** The clock that runs the move, and the body it runs it on. */
	FTimerHandle StepTimerHandle;
	TWeakObjectPtr<AActor> DashingOn;

	/** Whether the recovery's picture has already been dropped this move, so it is dropped once. */
	bool bPictureDropped = false;

	/** Everyone this dash has already been through, so one dash cuts a body once. */
	TArray<TWeakObjectPtr<AActor>> CutThisDash;
};
