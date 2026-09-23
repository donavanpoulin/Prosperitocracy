// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include "ProsperitocracyBloodBlade.generated.h"

class UAnimInstance;
class UAnimMontage;
class UProsperitocracyGameplayAbility;

/**
 * The blade's swing: ONE number sizes it, and this is where in an attack it bites.
 *
 * Range is one row with THREE jobs — how far a swing CARRIES the body, how far it REACHES, and how
 * WIDE it is — so the swing carries no thickness of its own. There is no per-thing width here and no
 * universal one either: a thickness that ignored Range would be a second answer to a question Range
 * already answers, which is exactly the thing this project does not do.
 *
 * What is left to say is WHERE in an attack the blade cuts, and one rule says it: the blade cuts
 * from the SECOND HALF of the attack to its end. The wind-up is cocked back and hurts nothing, and
 * by the halfway point the blade is through the arc — so nothing is cut before the blade has gone
 * anywhere, and nothing walking into a long swing walks out of it unhurt. The same half of every
 * attack, so an attack's own length moves its bite with it and there is no per-attack number to
 * keep in step. All values [TUNE].
 */
namespace ProsperitocracyBladeHandling
{
	/** Where in an attack the blade starts cutting: halfway through it, and from there to the end. */
	constexpr float BiteStartsAtFraction = 0.5f;

	/** Range's own unit is meters (Design/stats.md); a body is placed, and a swing is sized, in cm. */
	constexpr float CentimetersPerMeter = 100.0f;
}

/**
 * AProsperitocracyBloodBlade
 *
 * The Reclaimer's blade as a thing: the COMBO, and the one number that sizes everything about it.
 *
 * It has NO FIRE MODE, and that is the whole of what makes it a melee — the project's own test
 * (Design/weapons.md: a gun is a weapon with a fire mode, a melee is a weapon without one). The
 * class's forced-melee primary and the pose both read that tag, so nothing here declares itself a
 * sword and nothing here needs to.
 *
 * The blade owns its COMBO — which attack is playing, and what a left press does — and it hands the
 * BODY three things and holds nothing: the LINE, the DISTANCE and the TIME. It never writes the
 * body's speed, its velocity or its facing; those belong to the body, and an attack that owned them
 * from two places is what made the last attempt unfixable.
 *
 * WHAT THE RIGHT BUTTON DOES IS NOT THE BLADE'S, AND THAT IS THE POINT. A sword's other move is an
 * ABILITY, and this blade carries a slot naming which one it fires (`SecondaryAbility`) — so a sword
 * throw, or anything else a sword should do on a press, is a new ability and nothing about this
 * class changes. The blade does not know what its right click is; it knows which ability to ask for,
 * and the ability owns its own numbers, its own animation and its own hit.
 */
UCLASS(BlueprintType)
class AProsperitocracyBloodBlade : public AProsperitocracyWeapon
{
	GENERATED_BODY()

public:
	AProsperitocracyBloodBlade();

	/**
	 * One press of the left mouse button, as the blade reads it.
	 *
	 * A press with nothing playing starts the combo at its first attack. A press inside a RECOVERY
	 * takes the combo straight to the next attack. A press inside an attack does nothing at all — it
	 * never stacks, and it never cancels a swing. False means this press was not an attack, so nothing
	 * is owed: no hit, no attack on the body.
	 *
	 * This is the weapon's own primary action, answered — `AProsperitocracyWeapon::PrimaryAction` —
	 * so the input asks the thing in the hand and never learns what it is holding.
	 */
	virtual void PrimaryAction_Implementation() override;

	/**
	 * One press of the right mouse button: run this blade's own ability.
	 *
	 * The blade carries the ability it fires rather than containing the move, so what a sword does on
	 * a right press is one asset away from being something else — a dash today, a sword throw later,
	 * anything the sword should do. Everything about what that move IS belongs to the ability: its
	 * Range, its Rate, its Cooldown, its damage, its animation, and the question of whether it may
	 * start at all.
	 *
	 * This is where the template's right-click AIM used to be. Aiming was never a thing this weapon
	 * did; the press belongs to the thing in the hand, exactly as the left button's does.
	 */
	virtual void SecondaryAction_Implementation() override;

	/**
	 * Something else is taking the stage on this body: the combo stands down.
	 *
	 * A body wears ONE move at a time — two of our montages play in the SAME slot, so a move started
	 * without the last one ending does not replace it, it blends with it — and the body's own picture
	 * door is what swaps the animation. This is the other half: the combo's own state ends, so no
	 * press is an attack any more, nothing goes on biting, and no window is left open behind it.
	 *
	 * The thing that is starting asks for this through the weapon in hand (`StandDownForANewMove`), so
	 * it never has to know what it is taking over from.
	 */
	virtual void StandDownForANewMove() override;

	/** Whether this blade's combo is running right now. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	bool IsSwinging() const { return bSwinging; }

	/**
	 * Whether this blade is the thing in the player's hand right now.
	 *
	 * ASKED, never assumed: the rig decides which of a character's weapons is out, so a blade that
	 * is merely carried neither reaches anything nor swings.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	bool IsInHand() const;

	/**
	 * What the combo must be played at: this blade's FINAL Rate over its own base Rate.
	 *
	 * Rate IS the attack speed the player reads, so the animation runs at the ratio of what the stat
	 * says to what it shipped as — move the one row and the animation, the attack's length and the
	 * speed the body travels all move together, because they are all this one number. 1.0 until
	 * something moves the stat, and 1.0 rather than a guess when there is nothing to divide by.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	float GetComboPlayRate() const;

	/**
	 * The combo this blade carries: what the player sees when it swings.
	 *
	 * Its sections are `a`, `rec_a`, `b`, `rec_b`, `c`, `rec_c` — asked for BY NAME and never by
	 * seconds, so moving Rate (which re-times every part of the montage) cannot break a window. Set
	 * on the blade's blueprint: the blade carries the combo and the BODY plays it, so the animation
	 * is never this class's own to invent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Blade")
	TObjectPtr<UAnimMontage> ComboMontage;

	/**
	 * The ability this blade fires on the right mouse button.
	 *
	 * A SLOT, not a move: the blade names which ability its second press runs and knows nothing about
	 * it — not its numbers, not its animation, not what it does. That is what makes a sword's other
	 * move modular and interchangeable, and it is why the move itself is an ordinary ability with its
	 * own block rather than a mode hidden in this class.
	 *
	 * Null is a real answer: a blade with no ability simply does nothing on the right press.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Blade")
	TSubclassOf<UProsperitocracyGameplayAbility> SecondaryAbility;

protected:
	/**
	 * The blade is looked at and never bumped into.
	 *
	 * Its damage is its own swing, which ignores what already hangs off the swinger — and a
	 * collidable rod, the length of an arm, hanging off a body is a thing the world keeps resolving:
	 * it swallows every shot (a trace leaves the eye and meets the thing in the hand first) and it
	 * shoves the body it is attached to, because the world pushes the rod back out of whatever it is
	 * inside of. Off the moment the blade exists.
	 *
	 * Where it SITS is not this class's business at all: the character blueprint's equip path is what
	 * moves a weapon between the hand and the body, and it is what animates and sounds the move. This
	 * thing asks which weapon is held (`IsHeldInRig`) and never moves itself.
	 */
	virtual void BeginPlay() override;

	/**
	 * The blade's own tick, and the only two things it does.
	 *
	 * A RECOVERY IS THE PLAYER'S OWN TIME: it plays only while he is standing still, and any
	 * movement input ends it on the spot, straight back to normal locomotion. The blade watches that
	 * because the COMBO is the blade's — the body cannot see a montage — and it lets the body's
	 * facing go with it.
	 *
	 * And the BITE: from the second half of the attack to its end, the swing is swept and whatever
	 * is in it that has not been cut yet is cut. The body's half of an attack is not here at all —
	 * the line, the distance, the facing and the clock are the body's own state.
	 */
	virtual void Tick(float DeltaSeconds) override;

private:
	/** The body's anim instance, which is where the combo plays. Null while the blade is on no body. */
	UAnimInstance* GetBodyAnimInstance() const;

	/**
	 * The animation half of one press: start the combo at `a`, or take a live combo from a recovery
	 * straight to the next attack. True when an attack is playing — which is exactly when the
	 * numbers are owed — and false when the press landed inside an attack, where nothing happens.
	 */
	bool PlayOrAdvanceCombo();

	/** The numbers half: the line, the distance and the time, handed to the BODY and then let go. */
	void BeginTheBodyAttack(int32 AttackIndex);

	/**
	 * End whatever the RIGHT button's ability is running, if it is running.
	 *
	 * The blade knows which ability its own slot names, and a move started by the left button ends it
	 * by name — the ability's own EndAbility stops its clock, its window, its sweep and its facing, so
	 * nothing of it keeps working underneath the combo.
	 */
	void EndTheOtherMove();

	/**
	 * One sweep of the swing, at the moment it is made.
	 *
	 * The shape is the RANGE, and nothing else: a cube one Range on a side, sitting right in front of
	 * the player's eye — so it reaches a Range, it is a Range wide, and there is nothing at all
	 * behind him, because a swing is a swing and not a circle. Every enemy in it is cut, and each of
	 * them only ONCE per swing: the swing remembers who it has already been through, so a body that
	 * sits in the blade for ten frames is cut one time.
	 */
	void CutWhatTheSwingIsThrough();

	/** Whether this swing has already been through that actor. */
	bool HasAlreadyBeenCut(const AActor* Actor) const;

	/** What the player was looking at, flat — the direction the swing is made along. */
	FVector GetSwingDirection() const;

	/** Range in meters — the stat's own unit, read FINAL through this blade's own GAS home. */
	float GetRangeMeters() const;

	/** The same number in the unit the body is moved and a swing is sized in. One conversion, here. */
	float GetRangeCm() const;

	/**
	 * How long attack n lasts in the world: that attack's OWN piece of the combo at the rate the
	 * player reads. An attack is one of `a` / `b` / `c`, never a recovery, and its piece is its
	 * section's start to the next section's start — never the whole clip its piece lives in.
	 */
	float GetAttackSeconds(int32 AttackIndex) const;

	/** Which recovery a section is, or INDEX_NONE when it is not a recovery at all. */
	int32 FindRecoveryIndex(const FName& Section) const;

	/** Whether the combo is currently sitting in a recovery. */
	bool IsInARecovery() const;

	/**
	 * How long an attack's recovery lasts — its OWN length at its OWN speed, never the rate the
	 * attacks play at. Rate buys faster attacks and must not quietly buy faster recoveries too.
	 */
	float GetRecoverySeconds(int32 AttackIndex) const;

	/**
	 * The number the Rate ROW should hold: the three attacks over their own lengths, at their own
	 * speed — `3 / (a + b + c)` in attacks a second. Logged so the row can be set to exactly this
	 * rather than to a number somebody worked out on paper.
	 */
	float GetTrueAverageAttackRate() const;

	/** The combo is over: nothing is live any more, and the facing it held is let go. */
	void EndTheCombo();

	/** Whether the player is asking the body to move this frame. */
	bool HasMovementInput() const;

	/** Tell the body the combo is over, so the facing it has been holding is let go and it turns back. */
	void ReleaseTheBodyFacing();

	/** Whether this blade's combo is running: true between an attack starting and the combo ending. */
	bool bSwinging = false;

	/** Which attack the combo is on — `a`, `b` or `c` by index — while it is swinging. */
	int32 CurrentAttack = INDEX_NONE;

	/** The attack's own clock, kept here so the BITE knows where in the attack the blade is. */
	float AttackElapsed = 0.0f;
	float AttackSeconds = 0.0f;

	/**
	 * How long the combo stays OPEN for the next attack — the window, as its own clock.
	 *
	 * This is deliberately NOT "is the recovery playing". The recovery is a picture, and the picture
	 * is dropped the moment the player moves — which used to drop the window with it, so a man
	 * holding a direction could swing once and never continue. The window is TIME: it opens when an
	 * attack's own clock runs out, it lasts the recovery's ORIGINAL length, and it runs whether the
	 * player is standing in a recovery or walking away from one.
	 */
	float WindowRemaining = 0.0f;

	/** How many times the combo has been started, so a log line marks each one. */
	int32 ComboStarts = 0;

	/** Everyone this swing has already been through, so one swing cuts a body once. */
	TArray<TWeakObjectPtr<AActor>> CutThisSwing;
};
