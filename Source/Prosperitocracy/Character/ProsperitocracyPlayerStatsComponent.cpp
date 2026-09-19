// Copyright Prosperitocracy. All Rights Reserved.

#include "Character/ProsperitocracyPlayerStatsComponent.h"

#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/Attributes/ProsperitocracyStatSet.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyPlayerStatsComponent)

UProsperitocracyPlayerStatsComponent::UProsperitocracyPlayerStatsComponent()
{
	// The component ticks every frame for two jobs: the shot's push, which has a life of its own and
	// nothing else in the engine ends, and the animation rate, which follows that push down and back
	// up — the ease back to normal has to keep running after the push itself has lapsed.
	//
	// WHERE it ticks in the frame is set up in BeginPlay, and it is not decoration: the push has to
	// land between the state writing its speed and the body moving at it, or it is never read.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UProsperitocracyPlayerStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	AbilitySystemComponent = Owner->FindComponentByClass<UProsperitocracyAbilitySystemComponent>();
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogProsperitocracy, Error,
			TEXT("%s on %s: no ability system component on this actor, so this character has no numbers. "
				 "Add the ability system component to the actor."),
			*GetName(), *Owner->GetName());
		return;
	}

	// A pawn-owned ability system: the pawn is both the owner and the avatar. (The readout component
	// asks for the same thing; it is idempotent.)
	AbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);

	// The character's stats and its health both live on the character's own ability system. Adding a
	// set that is already there returns the one that is there, so the readout and this component
	// cannot end up with two.
	AbilitySystemComponent->AddSet<UProsperitocracyStatSet>();
	AbilitySystemComponent->AddSet<UProsperitocracyHealthSet>();

	ApplyBaselineStats();
	ApplyToMovement();

	// The order the push needs to be felt at EVERY speed: the character's own tick first (that is
	// where the movement state writes its speed for the frame — and a sprint writes its speed every
	// single frame it is held), then this component (the push, on top of that number), then the
	// movement component, which reads the number and moves the body at it.
	//
	// Written in any other order the push is invisible while sprinting: the state's number lands
	// after it and the movement only ever reads the state's. That is exactly how it behaved — the walk
	// (where the state writes once, on entry) dragged, and the sprint, which writes every frame, did
	// not.
	if (UCharacterMovementComponent* Movement = GetMovementComponent())
	{
		AddTickPrerequisiteActor(Owner);
		Movement->AddTickPrerequisiteComponent(this);
	}

	// What the character OWNS as abilities comes up with it, once, through the same component that
	// brings up its numbers.
	GrantAbilities();
}

void UProsperitocracyPlayerStatsComponent::GrantAbilities()
{
	if (!AbilitySystemComponent || !AbilitySet)
	{
		return;
	}

	// The project's own ability set does the granting: each ability arrives with its input tag (which
	// is how a door addresses it — see AProsperitocracyCharacter::MeleeWithGunInHand) and the set
	// records the handles, so what this character owns can be handed back as one thing.
	AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedAbilityHandles, this);
}

void UProsperitocracyPlayerStatsComponent::ApplyBaselineStats()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!BaselineStats)
	{
		if (!bBaselineWarned)
		{
			bBaselineWarned = true;
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("%s on %s: no baseline stat block set — this character's stats are all zero, so it takes "
					 "template defaults for anything that reads them."),
				*GetName(), *GetNameSafe(GetOwner()));
		}
		return;
	}

	for (const FProsperitocracyStatTableEntry& Entry : BaselineStats->StatEntries)
	{
		const FGameplayAttribute Attribute = UProsperitocracyStatSystemStatics::GetAttributeForStat(Entry.Stat);
		if (!Attribute.IsValid())
		{
			continue;
		}

		// This component carries the CHARACTER's numbers. A thing stat (a gun's damage, a turret's
		// rate) belongs to that thing's own stat host, never here — pushing it would put a weapon
		// number on the player.
		const UClass* AttributeSetClass = Attribute.GetAttributeSetClass();
		if (AttributeSetClass != UProsperitocracyStatSet::StaticClass()
			&& AttributeSetClass != UProsperitocracyHealthSet::StaticClass())
		{
			continue;
		}

		AbilitySystemComponent->SetNumericAttributeBase(Attribute, Entry.BaseValue);
	}
}

float UProsperitocracyPlayerStatsComponent::GetStat(EProsperitocracyStat Stat) const
{
	if (!AbilitySystemComponent)
	{
		return 0.0f;
	}
	return UProsperitocracyStatSystemStatics::GetStatFinal(AbilitySystemComponent, Stat);
}

float UProsperitocracyPlayerStatsComponent::GetWalkSpeed() const
{
	return GetRunSpeed() * WalkSpeedMultiplier;
}

float UProsperitocracyPlayerStatsComponent::GetCarriedWeightLbs() const
{
	const AActor* Owner = GetOwner();
	const UProsperitocracyLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UProsperitocracyLoadoutComponent>() : nullptr;
	return Loadout ? Loadout->GetCarriedWeightLbs() : 0.0f;
}

float UProsperitocracyPlayerStatsComponent::GetWeightSpeedMultiplier() const
{
	const float Lbs = GetCarriedWeightLbs();
	const float Multiplier = 1.0f - (Lbs * WeightPenaltyPercentPerLb * 0.01f);

	// A multiplier below zero would run and jump the body BACKWARDS, which is never wanted — so the
	// penalty bottoms out at a standstill. Where that floor should really sit (a weight past which
	// nothing moves, or one that merely crawls) is a tuning question for heavy builds, and one worth
	// asking rather than inventing: this clamps the arithmetic, nothing more.
	return FMath::Clamp(Multiplier, 0.0f, 1.0f);
}

UCharacterMovementComponent* UProsperitocracyPlayerStatsComponent::GetMovementComponent() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
}

UCharacterMovementComponent* UProsperitocracyPlayerStatsComponent::GetMovingBody(int32& OutSlot) const
{
	UCharacterMovementComponent* Movement = GetMovementComponent();
	OutSlot = (Movement && Movement->IsCrouching()) ? 1 : 0;
	return Movement;
}

void UProsperitocracyPlayerStatsComponent::ApplyToMovement()
{
	PushWalkSpeed();
	ApplyJumpVelocity();

	const UCharacterMovementComponent* Movement = GetMovementComponent();
	UE_LOG(LogProsperitocracy, Log,
		TEXT("%s on %s: movement from stats — walk %.0f | run %.0f | crouch %.0f | jump %.0f | carried %.1f lbs (weight x%.2f)"),
		*GetName(), *GetNameSafe(GetOwner()), Movement ? Movement->MaxWalkSpeed : 0.0f, GetRunSpeed(),
		Movement ? Movement->MaxWalkSpeedCrouched : 0.0f, Movement ? Movement->JumpZVelocity : 0.0f,
		GetCarriedWeightLbs(), GetWeightSpeedMultiplier());
}

void UProsperitocracyPlayerStatsComponent::ApplyJumpVelocity()
{
	UCharacterMovementComponent* Movement = GetMovementComponent();
	if (!Movement)
	{
		return;
	}

	const float JumpVelocity = GetJumpVelocity();
	if (JumpVelocity > 0.0f)
	{
		Movement->JumpZVelocity = JumpVelocity;
	}
}

void UProsperitocracyPlayerStatsComponent::PushWalkSpeed()
{
	UCharacterMovementComponent* Movement = GetMovementComponent();
	if (!Movement)
	{
		return;
	}

	// A stat that is not set must never zero the body out: movement keeps whatever it had until there
	// is a number to give it.
	if (GetStat(EProsperitocracyStat::MoveSpeed) <= 0.0f)
	{
		return;
	}

	// Every speed the body moves at on the ground is this one number: the walk/run number, and the
	// crouch number, which IS the walk number — a crouch is not faster than a walk, so there is no
	// third fraction to keep in step with the stat. (Crouch speed used to be the engine's own default
	// with no stat behind it at all: it is the one speed Move Speed never reached, and now it is.)
	//
	// Floored at a standstill: a shot's push can take the last of the character's speed away, but it
	// can never walk them backwards — the same floor the weight penalty keeps, for the same reason.
	const float WalkSpeed = FMath::Max(0.0f, GetWalkSpeed());
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = WalkSpeed;
	ShotPushLastWritten[0] = Movement->MaxWalkSpeed;
	ShotPushLastWritten[1] = Movement->MaxWalkSpeedCrouched;
}

float UProsperitocracyPlayerStatsComponent::GetAnimationRate() const
{
	// NORMAL, unless a shot is pushing the body. Nothing else slows the animation: not walking, not a
	// heavy load, not crouching. That is deliberate — a body that is permanently a little slower would
	// slow everything it plays, and a reload or a mantle is not something the gun in their hands gets
	// to re-time. The one thing that moves it is a shot's push, which is already moving the body and
	// is already fading on its own.
	if (!bShotPushActive)
	{
		return 1.0f;
	}

	int32 Slot = 0;
	const UCharacterMovementComponent* Movement = GetMovingBody(Slot);
	if (!Movement)
	{
		return 1.0f;
	}

	const float BodySpeed = ShotPushBaseSpeed[Slot];
	if (BodySpeed <= 0.0f)
	{
		return 1.0f;
	}

	// The push's own share, in the same percent form the movement gets: the speed the push is worth
	// this frame over the speed the body would have without it. Moving forward it is negative and the
	// legs slow; backpedalling it is positive and they cycle faster; across the shot's line, or
	// standing still, there is no push to ride and the animation is left at normal.
	const float PushShare = GetShotPushDrag() / BodySpeed;
	return FMath::Clamp(1.0f + PushShare, MinAnimationRate, 2.0f);
}

void UProsperitocracyPlayerStatsComponent::PushAnimationRate(float DeltaSeconds)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	// The body plays its animation at the rate the shot is moving it by: dragged, the legs cycle
	// slower; carried, faster; and back to normal the moment nothing is pushing them. It is read every
	// frame so the push's own decay is what brings it back, not a second timer in here.
	const float Target = GetAnimationRate();
	CurrentAnimationRate = (DeltaSeconds > 0.0f)
		? FMath::FInterpTo(CurrentAnimationRate, Target, DeltaSeconds, AnimationRateEase)
		: Target;

	// Written only when it actually moves, so an unchanged rate costs no churn on the mesh.
	if (!FMath::IsNearlyEqual(Mesh->GlobalAnimRateScale, CurrentAnimationRate, 0.0005f))
	{
		Mesh->GlobalAnimRateScale = CurrentAnimationRate;
	}
}

//~ The shot's push on the body -------------------------------------------------------------------

void UProsperitocracyPlayerStatsComponent::NotifyShotFired(float DragPercent, float CarryPercent, const FVector& ShotDirection)
{
	// A shot fired straight up or straight down has no line on the ground to be pushed along, and
	// guessing one would be inventing the shot's direction. Nothing to do.
	const FVector ShotForward = ShotDirection.GetSafeNormal2D();
	if (ShotForward.IsNearlyZero())
	{
		return;
	}

	const bool bWasIdle = !bShotPushActive;

	// ONE slot for the push. A shot while a push is already running replaces what is in it — the
	// newest gun's two numbers and the newest line — and restarts the window AND the decay. Two shots
	// can never add up into two pushes, which is what "never stacking" means here, and it is why a gun
	// that fires faster than the window (any gun faster than one shot a second) holds one steady push
	// near full strength for as long as the trigger is held.
	ShotPushBackwardAxis = -ShotForward;
	ShotPushDragPercent = DragPercent;
	ShotPushCarryPercent = CarryPercent;
	ShotPushEndTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + ShotPushSeconds;
	bShotPushActive = true;

	// The shot is felt on the frame it happened, not on the next tick.
	ApplyShotPushToWalkSpeed();

	if (bWasIdle)
	{
		UE_LOG(LogProsperitocracy, Log,
			TEXT("%s on %s: shot push — %.0f%% drag / %.0f%% carry of speed back along the shot's line, decaying over %.2fs | walk %.0f | anim x%.2f"),
			*GetName(), *GetNameSafe(GetOwner()), ShotPushDragPercent, ShotPushCarryPercent, ShotPushSeconds, GetWalkSpeed(), GetAnimationRate());
	}
}

float UProsperitocracyPlayerStatsComponent::GetShotPushDecay() const
{
	if (!bShotPushActive || ShotPushSeconds <= 0.0f)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	// The window runs from (end - ShotPushSeconds) to the end, and the push is worth FULL at the top
	// of it and nothing at the bottom: one straight ramp, the same decay for every gun. Every shot
	// puts it back to the top, so a burst of fire keeps it near full and letting go eases it away.
	const float Elapsed = World->GetTimeSeconds() - (ShotPushEndTime - ShotPushSeconds);
	return FMath::Clamp(1.0f - (Elapsed / ShotPushSeconds), 0.0f, 1.0f);
}

float UProsperitocracyPlayerStatsComponent::GetShotPushDrag() const
{
	if (!bShotPushActive || (ShotPushDragPercent <= 0.0f && ShotPushCarryPercent <= 0.0f))
	{
		return 0.0f;
	}

	int32 Slot = 0;
	UCharacterMovementComponent* Movement = GetMovingBody(Slot);
	if (!Movement)
	{
		return 0.0f;
	}

	FVector MoveDirection = Movement->Velocity;
	MoveDirection.Z = 0.0f;
	const float Speed = MoveDirection.Size();
	if (Speed <= UE_KINDA_SMALL_NUMBER)
	{
		// Standing still: the push is along a line the character is not moving along, so there is
		// nothing to fight and nothing to ride. It is also why firing on the spot never slides
		// anybody anywhere.
		return 0.0f;
	}
	MoveDirection /= Speed;

	// The whole mechanic, in one dot product: moving back along the shot's line rides the push
	// (positive — faster), moving forward fights it (negative — slower), moving across it is neither.
	// There is no branch anywhere for "walking backwards" or "walking forwards": the character's own
	// movement direction picks between the two, which is why it is the same one push either way.
	const float Along = FMath::Clamp(FVector::DotProduct(MoveDirection, ShotPushBackwardAxis), -1.0f, 1.0f);

	// Which of the gun's two numbers this is worth is decided by which way the character is spending
	// it: moving forward it is Drag (you are fighting it), moving back it is Carry (you are riding it,
	// and Carry is Drag at half). No branch on "walking backwards", no second mechanic.
	const float PushPercent = (Along >= 0.0f) ? ShotPushCarryPercent : ShotPushDragPercent;

	// And it is that percent OF THE SPEED THE BODY IS AT — the number the movement state put on it for
	// this frame, standing or crouched, walking, running or sprinting — so the same gun drags the same
	// at any of them. Then the decay: full on the shot, nothing by the end of the window.
	const float PushSpeed = ShotPushBaseSpeed[Slot] * (PushPercent * 0.01f);
	return PushSpeed * Along * GetShotPushDecay();
}

void UProsperitocracyPlayerStatsComponent::AdoptBodySpeedNumbers()
{
	UCharacterMovementComponent* Movement = GetMovementComponent();
	if (!Movement)
	{
		return;
	}

	// Both of the numbers the push can ride, taken as the movement state's own — unless a number is
	// what this component last wrote there, in which case it is the push and not the state's.
	if (!FMath::IsNearlyEqual(Movement->MaxWalkSpeed, ShotPushLastWritten[0]))
	{
		ShotPushBaseSpeed[0] = Movement->MaxWalkSpeed;
	}
	if (!FMath::IsNearlyEqual(Movement->MaxWalkSpeedCrouched, ShotPushLastWritten[1]))
	{
		ShotPushBaseSpeed[1] = Movement->MaxWalkSpeedCrouched;
	}
}

void UProsperitocracyPlayerStatsComponent::ApplyShotPushToWalkSpeed()
{
	int32 Slot = 0;
	UCharacterMovementComponent* Movement = GetMovingBody(Slot);
	if (!Movement)
	{
		return;
	}

	// The state's own numbers are taken first — both of them, so the push can move between standing
	// and crouched without ever riding on one of its own values.
	AdoptBodySpeedNumbers();

	// Then the push goes onto whichever number the body is actually moving at. The share is always
	// taken from the state's number, never from an already-pushed one, so it can never compound with
	// itself.
	float& BodySpeed = (Slot == 1) ? Movement->MaxWalkSpeedCrouched : Movement->MaxWalkSpeed;

	// Floored at a standstill, exactly as the stat's own number is: the push can stop the character
	// but it can never walk them backwards.
	BodySpeed = FMath::Max(0.0f, ShotPushBaseSpeed[Slot] + GetShotPushDrag());
	ShotPushLastWritten[Slot] = BodySpeed;
}

void UProsperitocracyPlayerStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The animation rate is read every frame, shot or no shot: the push's own decay moves it while a
	// shot runs, and the ease back to normal keeps going after the push has lapsed.
	PushAnimationRate(DeltaTime);

	if (!bShotPushActive)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() >= ShotPushEndTime)
	{
		// The window lapsed. The body gets its own numbers back — the push never leaves a slower (or
		// faster) body behind it, crouched or standing — and this component goes quiet until the next
		// shot.
		if (UCharacterMovementComponent* Movement = GetMovementComponent())
		{
			Movement->MaxWalkSpeed = ShotPushBaseSpeed[0];
			Movement->MaxWalkSpeedCrouched = ShotPushBaseSpeed[1];
			ShotPushLastWritten[0] = Movement->MaxWalkSpeed;
			ShotPushLastWritten[1] = Movement->MaxWalkSpeedCrouched;
		}

		bShotPushActive = false;
		ShotPushDragPercent = 0.0f;
		ShotPushCarryPercent = 0.0f;
		ShotPushBackwardAxis = FVector::ZeroVector;

		// The tick stays ON: it is what eases the animation back to normal after the push lapses. The
		// push's own work is gated on bShotPushActive, so a body that is not firing pays only for the
		// animation rate.

		const UCharacterMovementComponent* Movement = GetMovementComponent();
		UE_LOG(LogProsperitocracy, Log,
			TEXT("%s on %s: shot push done — walk speed back to %.0f | crouch %.0f"),
			*GetName(), *GetNameSafe(GetOwner()), Movement ? Movement->MaxWalkSpeed : 0.0f,
			Movement ? Movement->MaxWalkSpeedCrouched : 0.0f);
		return;
	}

	// Still alive: the push is re-applied every frame — the decay has moved it down, the character's
	// own movement may have turned under it, and the movement state may have written a fresh number
	// for the frame that this has to ride on top of.
	ApplyShotPushToWalkSpeed();
}
