// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility_HeavyCombo.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/ProsperitocracyCharacter.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyGameplayAbility_HeavyCombo)

/**
 * The heavy combo's own vocabulary: its four slices, and where in an attack the blade bites.
 *
 * These are the montage's OWN section names, read off the asset and NOT assumed from either move that
 * came before this one — the left-button combo runs `a`, `rec_a`, `b`, `rec_b`, `c`, `rec_c`, and the
 * dash runs `dash`, `dash_rec`, while this one runs `a`, `a_rec`, `b`, `b_rec`, `c`, `c_rec`, `d`,
 * `d_rec`. They are asked for BY NAME and never by seconds, which is what keeps a slice safe when Rate
 * moves: Rate re-times every part of the montage and would move every second-based boundary with it.
 *
 * The pairing IS the rule, and it is the same rule the combo runs on: slice n's attack is section 2n
 * and its recovery is 2n + 1, so a slice is an attack with the recovery behind it that says how long
 * the player's own time lasts. The difference between this and the combo is only the input: where the
 * combo waits for a PRESS to take the next attack, this walks on while the button is still DOWN.
 */
namespace ProsperitocracyBladeHeavyCombo
{
	/** The attack sections, in the order they are swung — four of them, `a` through `d`. */
	const TCHAR* const Attacks[] = { TEXT("a"), TEXT("b"), TEXT("c"), TEXT("d") };

	/** The recovery sections, in order: recovery n belongs to attack n and lasts the slice's tail. */
	const TCHAR* const Recoveries[] = { TEXT("a_rec"), TEXT("b_rec"), TEXT("c_rec"), TEXT("d_rec") };

	int32 NumSlices() { return UE_ARRAY_COUNT(Attacks); }

	/**
	 * Where in an attack the blade starts cutting — and, being the same number, where the body has
	 * finished covering its Range. ONE number, so the dash and the cut can never disagree: by the
	 * halfway point the body has arrived and the blade is through the arc, and the rest of the swing is
	 * spent holding that ground.
	 *
	 * It is derived from nothing and derives nothing: a Rate perk shortens the attack's real seconds and
	 * the dash shrinks with it, a Range perk raises the distance covered in that same time, and the
	 * fraction stays the fraction.
	 */
	constexpr float BiteStartsAtFraction = 0.5f;

	/** Range's own unit is meters (Design/stats.md); a body is placed, and a swing is sized, in cm. */
	constexpr float CentimetersPerMeter = 100.0f;
}

UProsperitocracyGameplayAbility_HeavyCombo::UProsperitocracyGameplayAbility_HeavyCombo()
{
	// The move's state has to live BETWEEN the steps of its own clock — which slice, how far into it,
	// the window, whether the button is still down — so this ability is instanced per actor rather than
	// being a fresh object per activation. No activation policy: nothing presses this by a tag. The
	// weapon in hand runs it, by name, exactly as it runs the dash.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

int32 UProsperitocracyGameplayAbility_HeavyCombo::FindSection(const TCHAR* SectionName) const
{
	// BY NAME, off the montage's own table: this is what lets the section names be the asset's business
	// and the numbers be nobody's. A name this move asks for that the montage does not have answers
	// INDEX_NONE, and every reader of it treats that as "no such piece" rather than as a zero.
	if (!HeavyComboMontage || !SectionName)
	{
		return INDEX_NONE;
	}

	const int32 Section = HeavyComboMontage->GetSectionIndex(FName(SectionName));
	return HeavyComboMontage->IsValidSectionIndex(Section) ? Section : INDEX_NONE;
}

float UProsperitocracyGameplayAbility_HeavyCombo::GetAttackSeconds(int32 Slice) const
{
	if (Slice < 0 || Slice >= ProsperitocracyBladeHeavyCombo::NumSlices())
	{
		return 0.0f;
	}

	const int32 Section = FindSection(ProsperitocracyBladeHeavyCombo::Attacks[Slice]);
	if (Section == INDEX_NONE)
	{
		return 0.0f;
	}

	// A slice's attack, at the rate the player reads off Rate: its length in the world is its own length
	// over that rate, which is the same ratio its animation runs at.
	return HeavyComboMontage->GetSectionLength(Section) / FMath::Max(GetPlayRate(), KINDA_SMALL_NUMBER);
}

float UProsperitocracyGameplayAbility_HeavyCombo::GetRecoverySeconds(int32 Slice) const
{
	if (Slice < 0 || Slice >= ProsperitocracyBladeHeavyCombo::NumSlices())
	{
		return 0.0f;
	}

	const int32 Section = FindSection(ProsperitocracyBladeHeavyCombo::Recoveries[Slice]);
	if (Section == INDEX_NONE)
	{
		return 0.0f;
	}

	// The recovery's OWN length at its OWN speed: this is a length of time, not a picture, so the rate
	// the attack played at has nothing to do with it.
	return HeavyComboMontage->GetSectionLength(Section);
}

float UProsperitocracyGameplayAbility_HeavyCombo::GetPlayRate() const
{
	// This ability's own Rate row, read the way every rate in the game is read: what the stat says over
	// what it shipped as. The heavy combo's numbers are its own — the blade's rows are the left-button
	// combo's — so moving this one row re-times this move and nothing else.
	const float BaseRate = StatBlock ? StatBlock->GetBaseValue(EProsperitocracyStat::Rate) : 0.0f;
	const float FinalRate = GetStatFinalValue(EProsperitocracyStat::Rate);

	if (BaseRate <= 0.0f || FinalRate <= 0.0f)
	{
		// Nothing to scale by — no block, or a block with no Rate. The move plays at its own speed
		// rather than at a guess.
		return 1.0f;
	}

	return FinalRate / BaseRate;
}

float UProsperitocracyGameplayAbility_HeavyCombo::GetCooldownSeconds() const
{
	// FINAL through this ability's own GAS home, so a Cooldown perk reaches it like any other stat.
	return GetStatFinalValue(EProsperitocracyStat::Cooldown);
}

float UProsperitocracyGameplayAbility_HeavyCombo::GetRangeCm() const
{
	// The ONE Range row, in the unit a body is placed and a swing is sized in. It carries the body and
	// it sizes the swing — a Range deep, a Range wide — so there is no second number anywhere to keep
	// in step with it.
	return GetStatFinalValue(EProsperitocracyStat::Range) * ProsperitocracyBladeHeavyCombo::CentimetersPerMeter;
}

float UProsperitocracyGameplayAbility_HeavyCombo::GetTrueAttackRate() const
{
	// The four ATTACKS over their own lengths, untouched by any rate: this is what Rate IS for this move,
	// so it is the number the row holds. Logged rather than worked out on paper, because the montage's
	// section boundaries are the only place it can come from and paper has been wrong about them before.
	if (!HeavyComboMontage)
	{
		return 0.0f;
	}

	float Total = 0.0f;
	for (int32 Slice = 0; Slice < ProsperitocracyBladeHeavyCombo::NumSlices(); ++Slice)
	{
		const int32 Section = FindSection(ProsperitocracyBladeHeavyCombo::Attacks[Slice]);
		if (Section != INDEX_NONE)
		{
			Total += HeavyComboMontage->GetSectionLength(Section);
		}
	}

	return Total > KINDA_SMALL_NUMBER
		? static_cast<float>(ProsperitocracyBladeHeavyCombo::NumSlices()) / Total
		: 0.0f;
}

UAnimInstance* UProsperitocracyGameplayAbility_HeavyCombo::GetBodyAnimInstance() const
{
	const AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(RunningOn.Get());
	if (!Body)
	{
		return nullptr;
	}

	// The body's own anim instance, which is where a move plays. The visible body is a second mesh that
	// retargets this one, so this is where the player sees the combo from.
	const USkeletalMeshComponent* Mesh = Body->GetMesh();
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

bool UProsperitocracyGameplayAbility_HeavyCombo::HasMovementInput() const
{
	// MOVEMENT INPUT, not speed: the question is whether the player is ASKING the body to move, so a body
	// being pushed around by something else is not a man walking, and a player holding a key against a wall
	// is. The same read the left-button combo makes for the same rule.
	const APawn* Pawn = Cast<APawn>(RunningOn.Get());
	return Pawn && !Pawn->GetLastMovementInputVector().IsNearlyZero();
}

bool UProsperitocracyGameplayAbility_HeavyCombo::IsTheButtonStillDown() const
{
	// THE THING IN THE HAND KNOWS, and nothing else has to: the press and the release both land on the
	// weapon, so it is the one holder of "is the right button still down" — and this move asks it rather
	// than being told, which is what keeps the hold out of the input graph entirely.
	const AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(RunningOn.Get());
	const AProsperitocracyWeapon* Blade = Body ? Body->GetGunWeaponInHand() : nullptr;

	return Blade && Blade->IsSecondaryHeld();
}

void UProsperitocracyGameplayAbility_HeavyCombo::ReleaseTheBodyFacing()
{
	if (AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(RunningOn.Get()))
	{
		Body->ReleaseFacing();
	}
}

void UProsperitocracyGameplayAbility_HeavyCombo::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base class brings this ability's GAS home up, which is where every number it owns is
	// evaluated — its Range, its Rate, its Cooldown, its damage and its Pen.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(Avatar);
	AProsperitocracyWeapon* Blade = Body ? Body->GetGunWeaponInHand() : nullptr;

	if (!Body || !Blade || !StatBlock)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Heavy] %s: nothing runs — %s. This is a bug or an unauthored ability, not a missing feature."),
			*GetPathName(),
			(!Body ? TEXT("there is no body")
				: (!Blade ? TEXT("there is no weapon in hand")
					: TEXT("the ability has no stat block, so it has no Range, no Rate, no Cooldown and no damage"))));

		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	if (!StartTheHeavyCombo(Body, Blade))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	// THE MOVE'S OWN CLOCK, one step at a time. The TRAVEL is not here — the body owns it, and each slice
	// hands it over. What is here is the bite, the window, the recovery's picture and the question of
	// whether the button is still down, which are all this ability's own.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(StepTimerHandle, this, &UProsperitocracyGameplayAbility_HeavyCombo::ComboStep, StepSeconds, /*bLoop=*/ true);
	}
}

bool UProsperitocracyGameplayAbility_HeavyCombo::StartTheHeavyCombo(AProsperitocracyCharacter* Body, AProsperitocracyWeapon* Blade)
{
	if (!Body || !Blade || bRunning)
	{
		return false;
	}

	// THE ONE QUESTION EVERY MOVE ASKS: an attack the player has committed to is never overwritten. The
	// BODY is the only thing that knows — it is the one holding the attack — so it is asked, rather than
	// any one move's clock being read.
	if (Body->IsSwingLocked())
	{
		UE_LOG(LogProsperitocracy, Verbose, TEXT("[Heavy] %s: an attack is live — nothing happens"), *GetPathName());
		return false;
	}

	// THE COOLDOWN, asked before anything is spent, and its answer is a refusal rather than a silence.
	UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < ReadyAt)
	{
		UE_LOG(LogProsperitocracy, Log, TEXT("[Heavy] %s: on cooldown — %.2fs of %.2fs left"),
			*GetPathName(), FMath::Max(0.0f, ReadyAt - World->GetTimeSeconds()), GetCooldownSeconds());
		return false;
	}

	// AND THE BLOOD: this ability's own use cost, out of the pool the thing in hand carries — its own
	// BloodCost row, read FINAL through this ability's GAS home. Paid BEFORE the move that is running is
	// stood down, so a heavy combo that cannot be paid for interrupts nothing.
	if (!Blade->SpendBlood(GetStatFinalValue(EProsperitocracyStat::BloodCost)))
	{
		UE_LOG(LogProsperitocracy, Log, TEXT("[Heavy] %s: not enough blood to run the heavy combo."), *GetPathName());
		return false;
	}

	// AND ONLY NOW DOES THE MOVE THAT WAS GOING STAND DOWN — the left-button combo included. This is the
	// order that matters: the checks come first, so a right press during the COMBO's attack interrupts
	// nothing at all, and the combo only ever gives way inside its own recovery.
	Blade->StandDownForANewMove();

	if (!Blade->GetDamageEffectClass())
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Heavy] %s: the blade in hand carries no damage effect — the combo will hit and hurt nothing."), *GetPathName());
	}

	if (!HeavyComboMontage)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Heavy] %s: the heavy combo has no montage set — it runs with no picture at all."), *GetPathName());
	}

	// The clock he watches starts the moment he spends it: the cooldown belongs to the move he asked for,
	// not to the animation finishing.
	if (World)
	{
		ReadyAt = World->GetTimeSeconds() + GetCooldownSeconds();
	}

	RunningOn = Body;
	bRunning = true;
	CurrentSlice = INDEX_NONE;

	// A NEW RUN, and the button is DOWN — that press is what started it — so nothing is latched away yet.
	bButtonCameUp = false;

	++HeavyCount;

	// The move's own numbers, said out loud once per use: each slice's attack and recovery in the world
	// at their own speeds, the four attacks' real rate (so the Rate row is SET from this rather than from
	// a boundary somebody worked out on paper), the one Range, the cooldown and what it costs in blood.
	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Heavy] %s: heavy #%d — attacks a %.4fs b %.4fs c %.4fs d %.4fs | recoveries %.4fs %.4fs %.4fs %.4fs | x%.3f, Range %.2fm | Rate base %.4f final %.4f — the row should hold %.6f | cooldown %.2fs | blood %.1f"),
		*GetPathName(), HeavyCount,
		GetAttackSeconds(0), GetAttackSeconds(1), GetAttackSeconds(2), GetAttackSeconds(3),
		GetRecoverySeconds(0), GetRecoverySeconds(1), GetRecoverySeconds(2), GetRecoverySeconds(3),
		GetPlayRate(), GetStatFinalValue(EProsperitocracyStat::Range),
		StatBlock ? StatBlock->GetBaseValue(EProsperitocracyStat::Rate) : 0.0f,
		GetStatFinalValue(EProsperitocracyStat::Rate), GetTrueAttackRate(),
		GetCooldownSeconds(), GetStatFinalValue(EProsperitocracyStat::BloodCost));

	// The first slice, and then the clock takes it from here.
	BeginTheSlice(0, Body);

	return bRunning;
}

void UProsperitocracyGameplayAbility_HeavyCombo::BeginTheSlice(int32 Slice, AProsperitocracyCharacter* Body)
{
	if (!Body || Slice < 0 || Slice >= ProsperitocracyBladeHeavyCombo::NumSlices())
	{
		EndTheMove();
		return;
	}

	// The slice's picture, through the BODY's own door: it takes the last move's picture off with THAT
	// montage's own blend-out first, so two of our animations can never blend into a pose that is half of
	// each — and the new one comes on with its own blend-in, so a slice reads as the next beat of one
	// move rather than as a jump.
	if (HeavyComboMontage)
	{
		Body->BeginMovePicture(HeavyComboMontage, FName(ProsperitocracyBladeHeavyCombo::Attacks[Slice]), GetPlayRate());
	}

	// A NEW SLICE, so its own clock starts, the window behind it closes, its picture has not been dropped
	// and a body cut by the last slice may be cut again by this one.
	CurrentSlice = Slice;
	AttackSeconds = GetAttackSeconds(Slice);
	AttackElapsed = 0.0f;
	WindowRemaining = 0.0f;
	bWindowOpened = false;
	bPictureDropped = false;
	CutThisSlice.Reset();

	// The LINE, taken for THIS slice and taken FLAT: the chain takes a new line at every beat, exactly as
	// a new press does in the left-button combo, so holding the button through a swing he walks around
	// still cuts where he is looking when each slice lands.
	AttackLine = Body->GetControlRotation().Vector().GetSafeNormal2D();

	// THE BODY'S HALF, handed over and then let go. The body takes TWO times, and they are not the same:
	//
	//   - the DASH is the first half of the ATTACK, so the body covers its Range by the moment the blade
	//     bites and holds that ground for the rest of the swing;
	//   - the LOCK is the WHOLE BEAT — the attack AND the recovery behind it — so while the key is down
	//     nothing he does with his legs interrupts the chain and the animation plays whole.
	//
	// The lock is the BEAT and not the attack, and that is the root of the whole difference: with the lock
	// on the attack, the recovery behind it is a gap of unlocked time he can walk out of mid-chain — which
	// is precisely what the left-button combo wants and precisely what this one must not have. It is handed
	// back the instant he lets go (see ComboStep), and only from then on is the recovery his own time.
	const float TheDash = AttackSeconds * ProsperitocracyBladeHeavyCombo::BiteStartsAtFraction;
	const float TheBeat = AttackSeconds + GetRecoverySeconds(Slice);

	Body->BeginAttack(AttackLine, GetRangeCm(), TheDash, TheBeat);
}

void UProsperitocracyGameplayAbility_HeavyCombo::ComboStep()
{
	if (!bRunning || !RunningOn.IsValid() || CurrentSlice == INDEX_NONE)
	{
		StopTheStepClock();
		return;
	}

	UAnimInstance* Anim = GetBodyAnimInstance();
	const bool bPlaying = Anim && HeavyComboMontage && Anim->Montage_IsPlaying(HeavyComboMontage);

	// THE BUTTON'S STATE IS LATCHED, NEVER SAMPLED — and this is the whole of what "you must HOLD it"
	// means. The question is asked of EVERY step and the answer is remembered, so the moment the button
	// comes up anywhere in the run, the run is SPENT: no later step can carry the chain on, whatever the
	// button is doing by then.
	//
	// Sampling it only when a beat's window ran out is what made taps play the whole thing: a tap that
	// happened to land on that one boundary carried the chain, so the move was being driven by PRESSES —
	// which is the left-button combo's rule, and not this one's.
	if (!IsTheButtonStillDown())
	{
		if (!bButtonCameUp)
		{
			// LETTING GO HANDS HIS LEGS BACK, and it is the other half of the lock being the whole beat.
			// While the key is down he is committed — his own movement is refused right through the recovery,
			// so nothing he does with his legs can walk the chain apart. The instant it comes up, this beat's
			// recovery is HIS OWN TIME again: his legs are his, and if he walks, the recovery stands down
			// into the walk (see the drop below). That is the one case in this whole move where anything
			// gives way to movement, and it cannot happen while the key is still holding.
			if (AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(RunningOn.Get()))
			{
				Body->ReleaseMovementLock();
			}
		}

		bButtonCameUp = true;
	}

	const bool bInRecovery = bPlaying
		&& Anim->Montage_GetCurrentSection(HeavyComboMontage) == FName(ProsperitocracyBladeHeavyCombo::Recoveries[CurrentSlice]);

	// THE BITE, from the second half of the attack to its end — the wind-up is cocked back and hurts
	// nothing, and by the halfway point the blade is through the arc. Asked on this slice's own clock and
	// never on the montage's position, so a dropped picture cannot take the hit with it.
	if (AttackElapsed >= AttackSeconds * ProsperitocracyBladeHeavyCombo::BiteStartsAtFraction
		&& AttackElapsed <= AttackSeconds)
	{
		CutWhatTheSwingIsThrough();
	}

	// THE ATTACK'S OWN CLOCK. Whether the attack is still happening is a question about TIME, which is
	// what lets the picture be cut without the slice's own state being cut with it.
	const bool bAttackLive = AttackElapsed < AttackSeconds;
	if (bAttackLive)
	{
		AttackElapsed += StepSeconds;
	}

	// The picture's rate follows whichever half of the beat is playing: the attack at the rate the player
	// reads, and the recovery always at its own.
	if (bPlaying)
	{
		Anim->Montage_SetPlayRate(HeavyComboMontage, bInRecovery ? 1.0f : GetPlayRate());
	}

	// A RECOVERY'S PICTURE IS DROPPED IN EXACTLY ONE CASE, and this is the only difference there is
	// between this move and the left-button combo: the key is UP **and** he is asking to move.
	//
	//   - HOLDING: nothing he does with his legs takes the beat away. The key being down IS "keep going",
	//     so a direction held through a recovery does not cancel it — the recovery plays, whole, and the
	//     move carries on to the next beat.
	//   - LET GO **AND** WALKING: that is a man who has left the move, and the recovery stands down into
	//     his walk — with the montage's OWN blend-out, never a zero cut.
	//   - LET GO AND STANDING STILL: the recovery still plays. Letting go ends the RUN, never the beat.
	//
	// The left-button combo has no key to be still holding — a PRESS is what says keep going — so there,
	// walking out of a recovery is simply how that combo is left. That is the whole of the difference.
	if (bPlaying && bInRecovery && !bPictureDropped && !IsTheButtonStillDown() && HasMovementInput())
	{
		Anim->Montage_StopWithBlendOut(HeavyComboMontage->GetBlendOutArgs(), HeavyComboMontage);
		ReleaseTheBodyFacing();
		bPictureDropped = true;

		UE_LOG(LogProsperitocracy, Log, TEXT("[Heavy] %s: he let go and walked — the beat stands down into the walk"), *GetPathName());
	}

	// THE SLICE'S TAIL. When the attack's clock is out the window opens — ONCE — for that recovery's OWN
	// length, and the slice is over when the window has run out. It runs whether or not its picture is
	// still playing, because it is a length of time and not an animation playing; and it opens once,
	// because a window read as empty every step would be refilled every step and no slice would ever end.
	if (!bAttackLive || AttackSeconds <= 0.0f)
	{
		if (!bWindowOpened)
		{
			WindowRemaining = GetRecoverySeconds(CurrentSlice);
			bWindowOpened = true;
		}

		WindowRemaining -= StepSeconds;

		if (WindowRemaining <= 0.0f)
		{
			// THE SLICE IS OVER, and there are exactly two things it can mean.
			//
			// THE RUN IS NOT SPENT: the next slice begins. That is the whole of what holding is — the key
			// has stayed down for the entire run so far, and it is why nothing has to press anything.
			//
			// THE RUN IS SPENT (the button came up at some point, see the latch above): the move is over.
			// This is what letting go means, and it is why letting go during an attack is not a cancel —
			// the attack he committed to finishes, the recovery behind it plays out, and only then does
			// the move end. Letting go in a recovery means the same thing, because the slice always
			// finishes itself. A press arriving after this does NOT revive the run: it is a new run, at
			// the cooldown's mercy, and this one is done.
			const int32 NextSlice = CurrentSlice + 1;
			if (!bButtonCameUp && NextSlice < ProsperitocracyBladeHeavyCombo::NumSlices())
			{
				BeginTheSlice(NextSlice, Cast<AProsperitocracyCharacter>(RunningOn.Get()));
			}
			else
			{
				// How far the run got, said out loud, because a chain that stopped early and a chain that
				// finished look exactly the same in a log that only says "it ended" — and the difference
				// is the whole question of whether the hold worked.
				UE_LOG(LogProsperitocracy, Log, TEXT("[Heavy] %s: the chain is done after %d of %d slices — %s"),
					*GetPathName(), NextSlice, ProsperitocracyBladeHeavyCombo::NumSlices(),
					bButtonCameUp ? TEXT("the button came up") : TEXT("the last slice"));
				EndTheMove();
			}
		}
	}
}

void UProsperitocracyGameplayAbility_HeavyCombo::EndTheMove()
{
	CurrentSlice = INDEX_NONE;
	bRunning = false;

	StopTheStepClock();

	// THE MOVE TAKES ITS OWN PICTURE OFF. A run that is let go in slice `a` used to stop its own state
	// while its ANIMATION carried on to the end of the montage — swings going on with nothing behind them,
	// which reads as the move never having ended. The body is told here, through its own door, and the
	// animation leaves with its OWN blend-out rather than a cut.
	//
	// It happens HERE and not in EndAbility, because EndAbility ALSO runs when a new move takes the stage
	// (the left-button combo cancelling this one) — and there the new move's own BeginMovePicture does the
	// handover, so taking the picture off here would kill the picture the new move has just put on.
	if (AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(RunningOn.Get()))
	{
		Body->EndMovePicture();
	}

	// The facing the move has been holding is let go, so the body turns back to where the player is
	// looking instead of snapping to it.
	ReleaseTheBodyFacing();

	// The move is over, so the ability is done with. Ending it here is what lets the same move be pressed
	// again rather than the ability latching on.
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
	}
}

void UProsperitocracyGameplayAbility_HeavyCombo::StopTheStepClock()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimerHandle);
	}
}

void UProsperitocracyGameplayAbility_HeavyCombo::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopTheStepClock();
	bRunning = false;
	CurrentSlice = INDEX_NONE;

	// The facing the slice has been holding is let go, so the body turns back to where the player is
	// looking instead of snapping to it. Ending the ability from anywhere — a cancel, a stand-down, the
	// chain finishing — goes through here, so nothing of it keeps working underneath the next move.
	ReleaseTheBodyFacing();

	RunningOn = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UProsperitocracyGameplayAbility_HeavyCombo::CutWhatTheSwingIsThrough()
{
	APawn* Pawn = Cast<APawn>(RunningOn.Get());
	UWorld* World = GetWorld();

	const float RangeCm = GetRangeCm();
	const FVector Forward = AttackLine;

	if (!Pawn || !World || RangeCm <= 0.0f || Forward.IsNearlyZero())
	{
		return;
	}

	// THE SHAPE IS THE RANGE, read exactly as the left-button combo reads it, because this IS that move:
	// a cube one Range on a side sitting with its back face at the player's eye and its far face a Range
	// in front of him — it reaches a Range, it is a Range WIDE, and there is nothing behind him at all,
	// because a swing is a swing and not a circle. One number, three jobs, no thickness of its own.
	const FVector Eye = Pawn->GetPawnViewLocation();
	const FVector Centre = Eye + Forward * (RangeCm * 0.5f);
	const FQuat Facing = FRotator(0.0f, Forward.Rotation().Yaw, 0.0f).Quaternion();
	const FCollisionShape Swing = FCollisionShape::MakeBox(FVector(RangeCm * 0.5f));

	// The same channel and the same ignores as a shot: never the swinger, never what hangs off him.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BladeHeavyCombo), /*bTraceComplex=*/ false, Pawn);
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Centre, Facing, ECC_Visibility, Swing, Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Through = Overlap.GetActor();

		// EACH ENEMY ONCE PER SLICE: the slice remembers who it has already been through, so a body
		// sitting in the blade for ten steps is cut one time — and a body an EARLIER slice cut may be cut
		// by this one, because a new slice is a new swing.
		if (!Through || HasAlreadyBeenCut(Through))
		{
			continue;
		}

		CutThisSlice.Add(Through);

		// A wall has no ability system: the swing has been through it and that is the end of it.
		UAbilitySystemComponent* Target = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Through);
		if (!Target)
		{
			continue;
		}

		const AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(RunningOn.Get());
		const AProsperitocracyWeapon* Blade = Body ? Body->GetGunWeaponInHand() : nullptr;
		if (!Blade)
		{
			continue;
		}

		const FHitResult Hit(Through, Overlap.GetComponent(), Eye, -Forward);
		ApplyHeavyHitTo(CurrentSpecHandle, CurrentActorInfo, Hit, *Blade);

		UE_LOG(LogProsperitocracy, Log, TEXT("[Heavy] %s: slice %d caught %s (%s)"),
			*GetPathName(), CurrentSlice, *GetNameSafe(Through), *GetNameSafe(Overlap.GetComponent()));
	}
}

bool UProsperitocracyGameplayAbility_HeavyCombo::HasAlreadyBeenCut(const AActor* Actor) const
{
	for (const TWeakObjectPtr<AActor>& Cut : CutThisSlice)
	{
		if (Cut.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

void UProsperitocracyGameplayAbility_HeavyCombo::ApplyHeavyHitTo(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, const FHitResult& Hit, const AProsperitocracyWeapon& Blade)
{
	UAbilitySystemComponent* SourceAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent)
	{
		return;
	}

	// The context carries the hit — the part that was hit for its armour and its resists — and points at
	// THIS ABILITY as the ability source, which is where the one execution reads the damage lines from:
	// this ability's own Piercing Damage and its own Penetration, never the blade's and never the dash's.
	FGameplayEffectContextHandle Context = MakeEffectContext(Handle, ActorInfo);
	Context.AddHitResult(Hit, /*bReset=*/ true);

	// The lines also travel ON the context as data, so a hit resolving later than its instance still
	// carries the right numbers. One evaluator answers both ways.
	AddDamageLinesToContext(Context);

	// And then the same effect and the same pen-gate -> resist pipeline a shot travels, with the effect
	// the weapon carries: the loadout owns the one damage effect every weapon's damage travels, and a
	// heavy combo is weapon damage exactly as the bash and the dash are.
	UProsperitocracyDamageStatics::ApplyDamageEffectToHit(Context, Hit.GetActor(), SourceAbilitySystemComponent, Blade.GetDamageEffectClass());
}
