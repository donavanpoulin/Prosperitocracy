// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyStatusComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
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

	// THE YAW IS THE MAN'S OWN. The controller's rotation is only the direction he is TURNING TOWARD
	// (TickAimTurn), so the body must not be snapped onto it every frame — that snap is exactly what
	// made him instant and left only his arms pretending to be late. Nothing else turns him either
	// (bOrientRotationToMovement stays off), so his aim is the ONE writer of his yaw.
	bUseControllerRotationYaw = false;
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
	// THE MELEE KEY ASKS THE THING IN HAND, exactly as the fire, the reload and the second button do:
	// a gun answers with the bash, a blade with its blood-mode toggle, and the body never learns which
	// of them it asked.
	AProsperitocracyWeapon* Weapon = GetGunWeaponInHand();
	if (!Weapon)
	{
		return false;
	}

	Weapon->MeleeAction();
	return true;
}

bool AProsperitocracyCharacter::RunTheBash()
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

bool AProsperitocracyCharacter::IsTheWeaponInHandAGun() const
{
	// A gun is a weapon with a FIRE MODE and nothing else is — the project's own test (Design/weapons.md)
	// — so a melee answers false and the graph's gun-side chain stays shut for it. Asked of the thing in
	// the hand, never inferred from which hand it came out of.
	const AProsperitocracyWeapon* Weapon = GetGunWeaponInHand();
	return Weapon && Weapon->HasAFireMode();
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

	// The FACING belongs to the attack, and nothing has to step aside for it: the yaw is the AIM's own
	// (see Tick), so a live attack simply writes the aim to the blade's line, and the whole man — the
	// gun, the arms and the facing — goes down that line with the swing.
	bFacingHeld = true;

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

void AProsperitocracyCharacter::EndMovePicture()
{
	UAnimMontage* Last = MovePicture.Get();

	// Nothing to take off. A move that never put a picture on this body owes nothing here — and a move
	// that has already been taken over has had its picture swapped, so this is not its business.
	if (!Last)
	{
		return;
	}

	const USkeletalMeshComponent* Body = GetMesh();
	UAnimInstance* Anim = Body ? Body->GetAnimInstance() : nullptr;

	// THE MONTAGE'S OWN BLEND-OUT, never a zero cut. The animation says how it leaves, exactly as it does
	// when the player walks out of a recovery; a cut here is the snap that reads as the move breaking
	// rather than the move ending.
	if (Anim && Anim->Montage_IsPlaying(Last))
	{
		Anim->Montage_StopWithBlendOut(Last->GetBlendOutArgs(), Last);
	}

	// And this body is wearing nothing again, so the next move has nothing to take off — and a re-dress
	// cannot stop an animation that is no longer there.
	MovePicture = nullptr;

	UE_LOG(LogProsperitocracy, Log, TEXT("[Body] the move's picture is taken off — %s"), *GetNameSafe(Last));
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

	// The turn back to the look is THE AIM'S own, and it is always running (TickAimTurn) — so letting
	// the facing go needs nothing here at all. He comes round from the blade's line to wherever he is
	// looking at the rate his load allows: a turn, never a snap, with nothing special about the
	// handover.
}

void AProsperitocracyCharacter::ReleaseMovementLock()
{
	// Nothing to let go of: no live attack is refusing anything, so his legs are already his own.
	if (!bAttackLive)
	{
		return;
	}

	// HIS OWN MOVEMENT IS HIS AGAIN, from this frame on: the attack stops driving him and stops refusing his
	// input — the same switch that would have run out on its own when the attack's clock did.
	bAttackLive = false;

	// AND THE DISTANCE IS DELIBERATELY NOT SNAPPED. This is not the attack ENDING, it is the attack letting
	// go of him: the ground he has already covered is the ground he keeps, and the line and the facing are
	// left exactly where they are. (EndAttack is the other thing — it finishes the distance the attack was
	// worth, which is right for an attack that is over and wrong for one that is merely letting go.)
	//
	// Said out loud, because "he stopped early" and "he never started" look the same in a log that says
	// nothing, and the number is what tells them apart.
	const float MovedCm = FVector::Dist2D(AttackStartLocation, GetActorLocation());

	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Body] the attack let go of him — %.0f cm covered of the %.0f cm it was worth, and his legs are his own again"),
		MovedCm, AttackDistanceCm);
}

void AProsperitocracyCharacter::TickAimTurn(float DeltaSeconds)
{
	// THE MAN COMES ROUND HIMSELF. Where he is LOOKING is instant — the controller's rotation is the
	// player's own input and nothing here slows it down — but his BODY gets there at its own rate, so
	// the whole man is late rather than his arms being bent to pretend. This is the lag: the aim behind
	// the look, in degrees, and it is the same lag the circle sits off the dot by (see GetAimRotation).
	const AController* OwningController = GetController();
	if (!OwningController)
	{
		return;
	}

	// WHAT HE CHASES IS WHAT THE PLAYER IS LOOKING THROUGH — the camera's own rotation, never the raw
	// mouse. The view is a smoothed copy of the mouse (the body's camera has rotation lag on), so a man
	// chasing the mouse keeps up with the INPUT while the PICTURE is still coming round: he arrives
	// before the view does, which reads as the gun swinging the wrong way — leading instead of lagging.
	// The chain must only ever run one way: mouse → view → him → his gun → the ring. Nothing measures
	// itself against the mouse except the view.
	const APlayerController* PC = Cast<APlayerController>(OwningController);
	const FRotator Look = (PC && PC->PlayerCameraManager)
		? PC->PlayerCameraManager->GetCameraRotation()
		: OwningController->GetControlRotation();

	// The first frame this body has a controller there is nothing to turn FROM: he is not late, he has
	// simply not been anywhere yet, and starting at nought would swing the whole man round from the
	// north. Snap once, here, and never again.
	if (!bAimInitialized)
	{
		AimRotation = Look;
		bAimInitialized = true;
		SetActorRotation(FRotator(0.0f, AimRotation.Yaw, 0.0f));
		return;
	}

	// Where he is going is the LOOK ITSELF — there is no clamp on how far behind he is allowed to be.
	// A clamp is a dead zone: inside it the aim does not move at all, and past it the aim mirrors the
	// look one-for-one, which is the opposite of a turn. He comes round at the rate his LOAD allows,
	// never faster, and that rate is the whole of what weight does to the turn
	// (GetTurnRateDegreesPerSecond). His lateness is therefore a rate, not a distance: the circle
	// trails the dot while he is coming round and lands on it when he arrives.
	const FRotator Wanted = Look.GetNormalized();
	const float TurnRate = GetTurnRateDegreesPerSecond();

	AimRotation = (DeltaSeconds > 0.0f && TurnRate > 0.0f)
		? FMath::RInterpConstantTo(AimRotation, Wanted, DeltaSeconds, TurnRate).GetNormalized()
		: Wanted;

	// His YAW is the aim's — one writer, so the man, the gun in his hands and the animation all agree
	// about which way he is facing. His pitch is NOT written to the actor: a man cannot lean his whole
	// body up, so the up-and-down of the aim reaches the pose through the aim itself
	// (GetBaseAimRotation, and the gun's own climb inside GetAimRotation).
	const FRotator Facing(0.0f, AimRotation.Yaw, 0.0f);
	if (!GetActorRotation().Equals(Facing, 0.01f))
	{
		SetActorRotation(Facing);
	}

	// Said out loud, throttled, while he is actually behind: the look, the aim, how far behind he is,
	// how fast he can come round and what he is carrying. A turn nobody can see and a turn that is not
	// happening look the same in the world, and this is what tells them apart.
	const float TrailDegrees = FMath::Abs(FMath::FindDeltaAngleDegrees(AimRotation.Yaw, Look.Yaw));
	if (!FMath::IsNearlyZero(TrailDegrees, 1.0f))
	{
		AimLogCooldown -= DeltaSeconds;
		if (AimLogCooldown <= 0.0f)
		{
			AimLogCooldown = ProsperitocracyBodyAimHandling::AimLogIntervalSeconds;
			const UProsperitocracyPlayerStatsComponent* Stats = GetStats(this);
			// WHAT is in his hands belongs on this line as much as the load does: the rate is two
			// layers, and a line that printed only one of them could not tell a slow man from a light
			// gun.
			const AProsperitocracyWeapon* Held = GetGunWeaponInHand();
			UE_LOG(LogProsperitocracy, Log,
				TEXT("[Aim] look %.1f° → aim %.1f° (%.1f° behind, turn %.0f°/s, load %.1f lb, %s x%.2f)"),
				Look.Yaw, AimRotation.Yaw, TrailDegrees, TurnRate,
				Stats ? Stats->GetCarriedWeightLbs() : 0.0f,
				Held ? *Held->GetName() : TEXT("empty hand"),
				GetEquippedWeightMultiplier());
		}
	}
	else
	{
		AimLogCooldown = 0.0f;
	}
}

void AProsperitocracyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// WHERE HE IS FACING IS THE AIM'S, always, and the aim comes round at his own rate. The one thing
	// that takes it over is a live attack: a swing goes along its line and looks along it until it is
	// over, so the aim wears the blade's line for the whole of it — and the gun, the arms and the
	// facing all go down that line together.
	if (bAttackLive)
	{
		AimRotation.Yaw = AttackLine.Rotation().Yaw;
		TickAttack(DeltaSeconds);
		return;
	}

	if (bFacingHeld)
	{
		// The attack has stopped TRAVELLING but the combo is not over — a recovery is playing — so he
		// goes on facing his line while the player's legs are his own again. Only the travel stopped.
		AimRotation.Yaw = AttackLine.Rotation().Yaw;
		SetActorRotation(FRotator(0.0f, AimRotation.Yaw, 0.0f));
		return;
	}

	TickAimTurn(DeltaSeconds);
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

FRotator AProsperitocracyCharacter::GetAimRotation() const
{
	FRotator Aim = AimRotation;

	// The gun's own numbers ride ON his aim and never beside it: the climb from Recoil, the shove from
	// Accuracy and the movement's inertia are the gun's, and they are read HERE by everyone who needs
	// to know where the next bullet goes. No gun in hand = no offset, and the aim is the plain turn.
	if (const AProsperitocracyWeapon* Gun = GetGunWeaponInHand())
	{
		const FVector2D Offset = Gun->GetAimDriftDegrees();
		Aim.Yaw += Offset.X;
		Aim.Pitch += Offset.Y;
	}

	return Aim.GetNormalized();
}

float AProsperitocracyCharacter::GetTurnRateDegreesPerSecond() const
{
	// TWO LAYERS, AND BOTH OF THEM WEIGHT. What he CARRIES sets the rate he comes round at with his
	// hands empty — his unarmed turn, and the quickest this body moves. The thing IN his hands then
	// multiplies it off its own Weight, so the same kit turns differently with a pistol out than with
	// a rifle out, and putting everything away drops him back to the plain carried rate.
	//
	// The whole load counts and not just the gun in hand: a man in armour still turns slower than a
	// naked one, and a gun on his back is still on his back. Nothing here is per-gun — the constants
	// are the same for every thing in the game, which is the whole of what weight does to the turn.
	const UProsperitocracyPlayerStatsComponent* Stats = GetStats(this);
	const float Carried = Stats ? FMath::Max(0.0f, Stats->GetCarriedWeightLbs()) : 0.0f;
	const float CarriedRate = ProsperitocracyBodyAimHandling::TurnRateBase
		- Carried * ProsperitocracyBodyAimHandling::TurnRatePerLb;

	// ONE floor, at the end, and it is the last word: however the two layers compose, the man turns.
	return FMath::Clamp(
		CarriedRate * GetEquippedWeightMultiplier(),
		ProsperitocracyBodyAimHandling::TurnRateMin,
		ProsperitocracyBodyAimHandling::TurnRateBase);
}

float AProsperitocracyCharacter::GetEquippedWeightMultiplier() const
{
	// The thing in his hands, off its OWN Weight — read FINAL through the one evaluator, so a weight
	// perk or an attachment on the gun moves his turn with it. An empty hand multiplies nothing, which
	// is what makes putting a gun away a real change rather than a relabelling.
	const AProsperitocracyWeapon* Weapon = GetGunWeaponInHand();
	if (!Weapon)
	{
		return 1.0f;
	}

	return FMath::Clamp(
		1.0f - Weapon->GetWeaponStat(EProsperitocracyStat::Weight)
			* ProsperitocracyBodyAimHandling::EquippedWeightMultiplierPerLb,
		ProsperitocracyBodyAimHandling::EquippedWeightMultiplierMin,
		1.0f);
}

FRotator AProsperitocracyCharacter::GetBaseAimRotation() const
{
	// The animation is handed THE MAN'S AIM and nothing else — the same rotation the bullet flies
	// along and the reticle circle sits on — so the pose can never disagree with the shot. Nothing is
	// bent here to make the arms match a number: the arms are given the number.
	return GetAimRotation();
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
