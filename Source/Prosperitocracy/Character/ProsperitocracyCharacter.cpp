// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyStatusComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyCharacter)

namespace
{
	/**
	 * This body's stat component — the one that owns its ability system and its own rows.
	 *
	 * Found rather than stored: the template's blueprint is what puts the component on the character, so
	 * C++ cannot hold a reference to it. Null when the body has none.
	 */
	const UProsperitocracyPlayerStatsComponent* GetStats(const AActor* Body)
	{
		return Body ? Body->FindComponentByClass<UProsperitocracyPlayerStatsComponent>() : nullptr;
	}
}

AProsperitocracyCharacter::AProsperitocracyCharacter()
{
	// The one place a status lives on this body. It is created here rather than in the blueprint so
	// that every body that can carry a status — this one and the damage targets — carries the same
	// component, and a status applied to either goes through the same code.
	Statuses = CreateDefaultSubobject<UProsperitocracyStatusComponent>(TEXT("Statuses"));

	// A live attack is driven per frame (see Tick), so this body has to tick. Asked for here rather
	// than left to whichever blueprint happens to have it switched on: the attack's own clock is
	// C++'s job, and an attack that never ticks is an attack that never happens.
	PrimaryActorTick.bCanEverTick = true;
}

AActor* AProsperitocracyCharacter::GetGunInHand_Implementation() const
{
	// There is no truth for this in C++: the rig decides which gun is in hand, and the rig is the
	// template's equip state in the blueprint. Returning null is the honest default — "nothing in
	// hand" — and every reader treats that as no drift rather than inventing a gun.
	return nullptr;
}

AProsperitocracyWeapon* AProsperitocracyCharacter::GetGunWeaponInHand() const
{
	// The rig answers with the actor it holds; this is where that becomes OUR gun. A rig holding
	// something that is not one of our weapons reads as no gun at all — no drift, plain camera aim.
	return Cast<AProsperitocracyWeapon>(GetGunInHand());
}

bool AProsperitocracyCharacter::MeleeWithGunInHand()
{
	// The bash is an ABILITY (UProsperitocracyGameplayAbility_Bash): this body does not swing anything
	// itself, it asks the character's ability system to run the bash — and the bash owns the swing, its
	// reach (its Range stat) and its damage. Nothing in hand is the ability's own business: it refuses
	// there, so "no gun" is answered in one place.
	//
	// The ability is addressed by its tag, not by a handle: the grant carries the tag (see
	// UProsperitocracyAbilitySet), so the body needs to know neither what the ability is nor where it
	// was granted — it asks the ability system for "the bash".
	UProsperitocracyAbilitySystemComponent* AbilitySystemComponent = Cast<UProsperitocracyAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(this));
	if (!AbilitySystemComponent)
	{
		return false;
	}

	return AbilitySystemComponent->TryActivateAbilityByInputTag(ProsperitocracyGameplayTags::InputTag_Bash);
}

bool AProsperitocracyCharacter::ReloadTheWeaponInHand()
{
	// The thing in hand answers for itself, and the body knows nothing else about it — not what it is,
	// not whether it has a magazine. A gun reloads; a melee's answer is nothing, because it has no
	// magazine to swap and never will.
	AProsperitocracyWeapon* Weapon = GetGunWeaponInHand();
	if (!Weapon)
	{
		return false;
	}

	Weapon->ReloadAction();
	return true;
}

bool AProsperitocracyCharacter::SecondaryActionTheWeaponInHand()
{
	// The right mouse button, on exactly the same terms as the reload above: the thing in hand answers
	// for itself. A gun's secondary action is its aim; the blade's is its dash; a thing with neither
	// answers with nothing at all. Nothing here learns which of them it asked.
	AProsperitocracyWeapon* Weapon = GetGunWeaponInHand();
	if (!Weapon)
	{
		return false;
	}

	Weapon->SecondaryAction();
	return true;
}

bool AProsperitocracyCharacter::SecondaryActionReleasedTheWeaponInHand()
{
	// The other half of the press above, and the reason a gun keeps its aim: aiming is a HOLD, so the
	// thing that started aiming is the thing that stops. The blade's answer to this is nothing at all
	// — its dash was spent on the press.
	AProsperitocracyWeapon* Weapon = GetGunWeaponInHand();
	if (!Weapon)
	{
		return false;
	}

	Weapon->SecondaryActionReleased();
	return true;
}

bool AProsperitocracyCharacter::IsTheWeaponInHandAiming() const
{
	// The INTENT is the weapon's; this is only the read. Nothing in hand answers false, which is the
	// same answer a melee gives — so the body's camera is driven by the thing in the hand and never by
	// the key that asked it.
	const AProsperitocracyWeapon* Weapon = GetGunWeaponInHand();
	return Weapon && Weapon->IsAiming();
}

bool AProsperitocracyCharacter::SetSlotOut(FGameplayTag Slot, bool bOut)
{
	// WHICH channel carries that slot, asked of the things themselves: a weapon knows the slot the
	// loadout handed it when it was dressed, so a channel is found by asking and not by a list of
	// component names kept here — a list like that is a second place that has to agree with the rig,
	// and the rig belongs to the blueprint.
	UChildActorComponent* Channel = nullptr;
	AProsperitocracyWeapon* Weapon = nullptr;

	TArray<UChildActorComponent*> Channels;
	GetComponents(Channels);
	for (UChildActorComponent* Candidate : Channels)
	{
		AProsperitocracyWeapon* InIt = Candidate ? Cast<AProsperitocracyWeapon>(Candidate->GetChildActor()) : nullptr;
		if (InIt && InIt->GetSlotTag() == Slot)
		{
			Channel = Candidate;
			Weapon = InIt;
			break;
		}
	}

	if (!Channel || !Weapon)
	{
		// Nothing in that slot, so there is nothing to bring out and nothing to animate. A key with
		// nothing behind it does nothing at all.
		UE_LOG(LogProsperitocracy, Log, TEXT("[Body] slot %s: nothing carries it — no key to answer"),
			*Slot.ToString());
		return false;
	}

	// WHERE it sits is the WEAPON's answer: its own socket, on the body's own mesh. The body does not
	// pick the socket and neither does the key press — a rifle is held where a rifle is held and a
	// sword where a sword is, and each of them says so for itself.
	const FName Socket = bOut ? Weapon->HandSocket : Weapon->AwaySocket;
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Channel->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetIncludingScale, Socket);

		// The two facts that settle a weapon sitting at the body's feet instead of in its hand: whether
		// the socket the WEAPON named is on this mesh at all, and where the channel actually ended up.
		// An attach to a socket that is not there does not fail — it quietly lands at the parent's
		// origin, which is between the legs, and says nothing.
		UE_LOG(LogProsperitocracy, Log, TEXT("[Body] socket %s: %s — the channel now sits at '%s'"),
			*Socket.ToString(), Body->DoesSocketExist(Socket) ? TEXT("found") : TEXT("NOT FOUND"),
			*Channel->GetAttachSocketName().ToString());
	}

	// And whether coming out or going away PLAYS anything is the weapon's answer too. A gun plays its
	// own draw and its own holster; a melee has neither and plays nothing, so it simply appears in the
	// hand and simply leaves it. The body's animation is the body's business — all this does is ask
	// for the montage the weapon says is its own.
	if (UAnimMontage* Montage = bOut ? Weapon->DrawMontage : Weapon->StowMontage)
	{
		PlayAnimMontage(Montage);
	}

	// AND THE STANCE, on the same terms as every other answer here: the thing that came out names what
	// the body should hold. A rifle names a rifle's hold, a pistol a pistol's, and a melee names
	// NOTHING — the body goes on running the anims it was already running, which is what a sword wants.
	// Putting it away puts the body back to bare, which is what the old chain's unequip did too.
	//
	// It crosses into the blueprint because the stance the ANIMATION reads is a blueprint value on this
	// body: the weapon decides, the body holds, and this call is the one place the two meet.
	OnStanceChosen(bOut ? Weapon->Stance : EProsperitocracyStance::Unarmed);

	// Said out loud, because "the stick is at my feet" and "the stick is in my hand" are the same
	// silence in a log that says nothing: what was found, which socket it was sent to, and what it was
	// asked to play.
	UE_LOG(LogProsperitocracy, Log, TEXT("[Body] slot %s %s — %s at socket %s (draw %s, stow %s)"),
		*Slot.ToString(), bOut ? TEXT("out") : TEXT("away"), *GetNameSafe(Weapon), *Socket.ToString(),
		*GetNameSafe(Weapon->DrawMontage), *GetNameSafe(Weapon->StowMontage));

	return true;
}

void AProsperitocracyCharacter::BeginAttack(const FVector& LineDirection, float DistanceCm, float TravelSeconds, float Seconds)
{
	// The line is FLAT: an attack goes along the ground the player is standing on, whatever the camera
	// was doing with its pitch. It is taken once, here, and held for the whole attack — which is what
	// keeps an attack straight while he is still free to look wherever he likes for the next one.
	AttackLine = LineDirection.GetSafeNormal2D();

	// No line to go along, nothing the attack is worth, or no time to take: that is not an attack, and
	// the honest answer is that there is none — not a state nobody can see.
	if (AttackLine.IsNearlyZero() || DistanceCm <= 0.0f || Seconds <= 0.0f)
	{
		EndAttack();
		return;
	}

	// Whether this body already OWES its yaw to something: the facing was the last attack's, or the
	// turn back from it is still under way. The yaw is taken from the controller only ONCE, so a press
	// that passes through a recovery into the next attack must not save the value this body already
	// set — saving it there would save "off", and the controller would never get the body back.
	const bool bTheBodyAlreadyOwesItsYaw = bFacingHeld || bTurningToLook;

	// Where the body is now is what its number is measured from, and the attack's own length is its clock.
	AttackStartLocation = GetActorLocation();
	AttackDistanceCm = DistanceCm;
	AttackSeconds = Seconds;

	// The DASH is its own length, and never longer than the attack it lives in: a thing that handed
	// over no travel time at all travels for the whole attack, which is the old behaviour and the
	// honest reading of "you told me nothing".
	AttackTravelSeconds = (TravelSeconds > 0.0f) ? FMath::Min(TravelSeconds, Seconds) : Seconds;
	AttackElapsed = 0.0f;
	bAttackLive = true;

	// The FACING belongs to the attack. This body's yaw is normally the controller's
	// (bUseControllerRotationYaw), so that is what steps aside for the window and what comes back at
	// the end of the turn that follows it (see ReleaseFacing, TickFacingTurn).
	if (!bTheBodyAlreadyOwesItsYaw)
	{
		bControllerYawBeforeFacing = bUseControllerRotationYaw;
		bUseControllerRotationYaw = false;
	}

	bFacingHeld = true;

	// A live attack always wins over a turn that was under way: the newest line and clock are the ones
	// that count, which is what a press through a recovery into the next attack means.
	bTurningToLook = false;

	// Said out loud, with its numbers: an attack nobody can see and an attack that is not happening
	// look exactly the same in the world, and only one of them is a bug.
	UE_LOG(LogProsperitocracy, Log, TEXT("[Body] attack — %.0f cm over %.2fs of a %.2fs attack along %s"),
		AttackDistanceCm, AttackTravelSeconds, AttackSeconds, *AttackLine.ToCompactString());
}

void AProsperitocracyCharacter::BeginMovePicture(UAnimMontage* Montage, FName Section, float PlayRate)
{
	if (!Montage)
	{
		return;
	}

	const USkeletalMeshComponent* Body = GetMesh();
	UAnimInstance* Anim = Body ? Body->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}

	// THE LAST MOVE'S PICTURE COMES OFF FIRST, and with ITS OWN blend-out: two of our montages play in
	// the same slot, so a second one started without this would BLEND with the first instead of
	// replacing it — half of each clip, which reads as the animation itself being wrong.
	if (const UAnimMontage* Last = MovePicture.Get())
	{
		if (Last != Montage && Anim->Montage_IsPlaying(Last))
		{
			Anim->Montage_StopWithBlendOut(Last->GetBlendOutArgs(), Last);
		}
	}

	// And the new one comes on with its OWN blend-in, at the rate the move the player reads says.
	if (!Anim->Montage_IsPlaying(Montage))
	{
		Anim->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::MontageLength,
			/*InTimeToStartMontageAt=*/ 0.0f, /*bStopAllMontages=*/ false);
	}

	Anim->Montage_JumpToSection(Section, Montage);

	// This is the picture this body is wearing now, so the NEXT move knows what to take off.
	MovePicture = Montage;
}

void AProsperitocracyCharacter::EndAttack()
{
	if (!bAttackLive)
	{
		return;
	}

	// MEASURED before the last snap, not asserted: what the world actually let the body move, in the
	// stat's own unit, next to what the attack was worth. A short number is the world having a say (a
	// wall) — never the clock, which cannot fall behind by construction.
	const float MovedCm = FVector::Dist2D(AttackStartLocation, GetActorLocation());

	bAttackLive = false;

	// The attack is worth its NUMBER, not its clock: whatever the frame rate managed on the way, the
	// body finishes exactly the distance it was given, along its own line.
	SetActorLocation(AttackStartLocation + AttackLine * AttackDistanceCm, /*bSweep=*/ true);

	UE_LOG(LogProsperitocracy, Log, TEXT("[Body] attack done — moved %.0f cm of the %.0f cm it was worth"),
		MovedCm, AttackDistanceCm);

	// The FACING is deliberately NOT let go here. The attack has stopped TRAVELLING; the recovery behind
	// it is still the attack's as far as where this body is looking, and the thing that swung is the one
	// that knows when the combo is actually over (see ReleaseFacing).
}

void AProsperitocracyCharacter::ReleaseFacing()
{
	if (!bFacingHeld)
	{
		return;
	}

	bFacingHeld = false;

	// The TURN begins here; the controller does not get this body's yaw back until the body has actually
	// arrived at the look (see TickFacingTurn). That delay is the whole difference between a turn and a
	// snap — hand the yaw back now and a body looking ninety degrees away jumps.
	bTurningToLook = true;
}

void AProsperitocracyCharacter::FinishFacingTurn()
{
	bTurningToLook = false;
	bUseControllerRotationYaw = bControllerYawBeforeFacing;
}

void AProsperitocracyCharacter::TickFacingTurn(float DeltaSeconds)
{
	// Named for what it is rather than `Controller`: APawn already has a member by that name, and a
	// local shadowing it hides the body's own controller from every read in this function.
	const AController* OwningController = GetController();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();

	// Nothing to turn towards, or no rate on this body to turn at: the honest answer is to hand the
	// facing straight back rather than invent a number to turn it by.
	const float TurnRate = Movement ? Movement->RotationRate.Yaw : 0.0f;
	if (!OwningController || TurnRate <= 0.0f)
	{
		FinishFacingTurn();
		return;
	}

	// The body turns to where the player is looking at the rate it ALREADY turns at — the movement
	// component's own yaw rate, the number this body is configured with — so the look can be right round
	// behind him and the body walks its way round instead of snapping to it.
	const float TargetYaw = OwningController->GetControlRotation().Yaw;
	const float NewYaw = FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, TurnRate * DeltaSeconds);
	SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));

	// Arrived. From here the controller's yaw IS this body's yaw, which is what turns it the rest of the
	// time — so the turn hands over to nothing at all, and there is no snap at either end of it.
	if (FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(NewYaw, TargetYaw), 0.1f))
	{
		FinishFacingTurn();
	}
}

void AProsperitocracyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bAttackLive)
	{
		// A live attack is this body's own state and is driven here, in ONE place: the line, the
		// distance, the facing and the clock. Nothing else moves this body while it is live.
		TickAttack(DeltaSeconds);
		return;
	}

	if (bFacingHeld)
	{
		// The attack's clock is out but the combo is NOT over — a recovery is playing — so the body goes
		// on facing its line while the player's legs are his own again. Only the travel stopped.
		SetActorRotation(FRotator(0.0f, AttackLine.Rotation().Yaw, 0.0f));
		return;
	}

	if (bTurningToLook)
	{
		TickFacingTurn(DeltaSeconds);
	}
}

void AProsperitocracyCharacter::TickAttack(float DeltaSeconds)
{
	AttackElapsed += DeltaSeconds;

	// WHERE the body is comes off the attack's own clock and never off a speed the world can slow down:
	// the number is the authority, and the frames only decide how finely it is spread. The height is not
	// the attack's business — an attack travels the ground, and jumping or falling stays the world's.
	//
	// The dash runs to its OWN length and stops there: once it has covered its Range the body holds
	// that ground for the rest of the attack, which is what puts the cut at the end of the dash
	// instead of at the end of the swing.
	const float Progress = FMath::Clamp(AttackElapsed / FMath::Max(AttackTravelSeconds, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	FVector Target = AttackStartLocation + AttackLine * (AttackDistanceCm * Progress);
	Target.Z = GetActorLocation().Z;

	// Swept, so a wall still has a say: being stopped by the world is not the same as falling short.
	SetActorLocation(Target, /*bSweep=*/ true);

	// This body's movement is the attack's and nothing else's while it is live, which is what refuses the
	// player's own: no velocity of his survives to fight the placement above, and the movement component
	// starts each frame from nothing instead of running him off the line.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->Velocity = FVector::ZeroVector;
	}

	// ...and the body FACES its line for the whole of the attack — not the camera, not the way it
	// happened to be walking — which is the other half of what an attack is: it goes that way and it
	// looks that way until it is over.
	SetActorRotation(FRotator(0.0f, AttackLine.Rotation().Yaw, 0.0f));

	if (AttackElapsed >= AttackSeconds)
	{
		EndAttack();
	}
}

float AProsperitocracyCharacter::GetAimingAlpha_Implementation() const
{
	// Same story: the template's Aim_Smooth timeline owns the ADS blend, so the blueprint reports it.
	// Hipfire is the honest default.
	return 0.0f;
}

USkeletalMeshComponent* AProsperitocracyCharacter::GetBodyMesh_Implementation() const
{
	// Nothing in C++ can say which of the character's meshes is the one on screen — the rig decided
	// that when the visible body was put on it, and a mesh being visible is not a statement about
	// what it is. Null is the honest default: nothing drawn, so nothing painted, and whoever asked
	// says so (see UProsperitocracyPlayerStatsComponent::ApplyArmorColors) instead of painting a
	// guess. The blueprint overrides this with the body it actually draws.
	return nullptr;
}

FRotator AProsperitocracyCharacter::GetBaseAimRotation() const
{
	FRotator AimRotation = Super::GetBaseAimRotation();

	// The animation's aim pose must follow the reticle circle: the drift (handling trail from Weight,
	// movement trail, per-shot spread from Accuracy, recoil climb) is the same value the bullet flies
	// along and the circle sits on, so adding it here is what makes the character's gun and arms point
	// where the bullet will land. No gun in hand = no drift, and the pose keeps the plain camera aim.
	if (const AProsperitocracyWeapon* Gun = GetGunWeaponInHand())
	{
		const FVector2D Drift = Gun->GetAimDriftDegrees();
		AimRotation.Pitch += Drift.Y;
		AimRotation.Yaw += Drift.X;
	}

	return AimRotation;
}

//~ Being damageable -------------------------------------------------------------------------------

UAbilitySystemComponent* AProsperitocracyCharacter::GetAbilitySystemComponent() const
{
	// The body's ability system lives on the stats component, so this is where a hit finds it — and
	// finding it is what makes this body damageable at all (see AProsperitocracyWeapon::ApplyDamageToHit:
	// no ability system on the hit actor means the hit has nowhere to land).
	const UProsperitocracyPlayerStatsComponent* Stats = GetStats(this);
	return Stats ? Stats->GetAbilitySystemComponent() : nullptr;
}

FProsperitocracyDamageProfile AProsperitocracyCharacter::GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const
{
	// The player's ONE part answers, and it carries NO armour: armour is the pen gate's number on enemy
	// parts, and a player answers with resists. So every pen over-pens this body — armour 0, always full
	// on the gate — and the rest of the answer is the body's resist for the type of the line that hit it.
	//
	// Plain English: wearing nothing means nothing is ever halved or stopped on you, and what a worn weave
	// does is take a share off the damage you DO take.
	FProsperitocracyDamageProfile Profile;
	Profile.Armor = 0;
	Profile.Resist = GetBodyResist_Implementation(EffectContext, DamageType);
	return Profile;
}

float AProsperitocracyCharacter::GetBodyResist_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const
{
	// What this body resists AS A WHOLE, for one damage type. A player has a single part — the weave — so
	// the average across parts IS that one part's number, and there is nothing else to weigh.
	//
	// The number is the body's OWN row for this type, read FINAL through the one evaluator: a worn weave's
	// two resists land on these rows (that is what wearing one does), so a resist perk, an item or a
	// weakening debuff reaches this answer exactly like it reaches any other stat. Bare means no weave, so
	// both rows are 0 and every line lands at full.
	const UProsperitocracyPlayerStatsComponent* Stats = GetStats(this);
	if (!Stats)
	{
		return 0.0f;
	}

	return Stats->GetStat(UProsperitocracyStatSystemStatics::GetResistStatForDamageType(DamageType));
}
