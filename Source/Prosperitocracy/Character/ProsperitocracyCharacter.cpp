// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyStatusComponent.h"
#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"
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

	// A live attack is driven per frame (see Tick), so this body has to tick. Asked for here rather than
	// left to whichever blueprint happens to have it on: the attack's own clock is C++'s job, and an
	// attack that never ticks is an attack that never happens.
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

void AProsperitocracyCharacter::BeginAttack(const FVector& LineDirection, float DistanceCm, float Seconds)
{
	// The line is FLAT: an attack goes along the ground the player is standing on, whatever the camera was
	// doing with its pitch. It is taken once, here, and held for the whole attack — which is what keeps an
	// attack going straight while the player is still free to look wherever they like for the next one.
	AttackLine = LineDirection.GetSafeNormal2D();

	// No line to go along, nothing the attack is worth, or no time to take: that is not an attack, and the
	// honest answer is that there is none — not a state nobody can see.
	if (AttackLine.IsNearlyZero() || DistanceCm <= 0.0f || Seconds <= 0.0f)
	{
		EndAttack();
		return;
	}

	// Whether one was already live, because the facing below is taken from the controller only ONCE — a
	// press that passes through a recovery into the next attack REPLACES this state, and the yaw this body
	// normally uses must not be saved on top of a yaw the attack already took.
	const bool bWasLive = bAttackLive;

	// Where the body is now is what its number is measured from, and the attack's own length is its clock.
	AttackStartLocation = GetActorLocation();
	AttackDistanceCm = DistanceCm;
	AttackSeconds = Seconds;
	AttackElapsed = 0.0f;
	bAttackLive = true;

	// The FACING belongs to the attack for as long as it lasts. This body's yaw is normally the controller's
	// (bUseControllerRotationYaw), so that is what steps aside for the window and what comes back at the end
	// — which is why the body returns to the player's look by itself, with nothing to remember to do.
	if (!bWasLive)
	{
		bControllerYawBeforeAttack = bUseControllerRotationYaw;
		bUseControllerRotationYaw = false;
	}

	// Said out loud, with its numbers: an attack nobody can see and an attack that is not happening look
	// exactly the same in the world, and only one of them is a bug.
	UE_LOG(LogProsperitocracy, Log, TEXT("[Body] attack — %.0f cm over %.2fs along %s"),
		AttackDistanceCm, AttackSeconds, *AttackLine.ToCompactString());
}

void AProsperitocracyCharacter::EndAttack()
{
	if (!bAttackLive)
	{
		return;
	}

	// The attack is worth its NUMBER, not its clock: whatever the frame rate managed on the way, the body
	// finishes exactly the distance it was given, along its own line. This is the last thing an attack
	// does, so the number the player reads is where the body ends up.
	const FVector EndLocation = AttackStartLocation + AttackLine * AttackDistanceCm;

	bAttackLive = false;

	// The facing goes back to the controller's, which is what turns this body the rest of the time. The
	// player's own movement is his again for the same reason nothing here has to hand it back: the gate
	// that refuses it simply stops being told that an attack is live (see IsSwingLocked).
	bUseControllerRotationYaw = bControllerYawBeforeAttack;

	SetActorLocation(EndLocation, /*bSweep=*/ true);

	// MEASURED, not asserted: what the world actually allowed the body, in the stat's own unit, next to
	// what the attack was worth. A number that comes out short is the world having a say (a wall) — never
	// the clock, which cannot fall behind by construction.
	UE_LOG(LogProsperitocracy, Log, TEXT("[Body] attack done — moved %.0f cm of the %.0f cm it was worth"),
		FVector::Dist2D(AttackStartLocation, GetActorLocation()), AttackDistanceCm);
}

void AProsperitocracyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// A live attack is this body's own state and is driven here, in ONE place: the line, the distance, the
	// facing and the clock. Nothing else moves this body while it is live.
	if (bAttackLive)
	{
		TickAttack(DeltaSeconds);
	}
}

void AProsperitocracyCharacter::TickAttack(float DeltaSeconds)
{
	AttackElapsed += DeltaSeconds;

	// WHERE the body is comes off the attack's own clock and never off a speed the world can slow down:
	// the number is the authority, and the frames only decide how finely it is spread. The height is not
	// the attack's business — an attack travels the ground, and jumping or falling stays the world's.
	const float Progress = FMath::Clamp(AttackElapsed / AttackSeconds, 0.0f, 1.0f);
	FVector Target = AttackStartLocation + AttackLine * (AttackDistanceCm * Progress);
	Target.Z = GetActorLocation().Z;

	// Swept, so a wall still has a say: being stopped by the world is not the same as falling short.
	SetActorLocation(Target, /*bSweep=*/ true);

	// The body's movement is the attack's and nothing else's while it is live: no leftover velocity of the
	// player's, no friction eating the number, nothing left to fight the placement above. The walk, the run,
	// the crouch and the shot's push are untouched by this — they are simply not what is moving the body.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->Velocity = FVector::ZeroVector;
	}

	// ...and the body FACES its line for the whole of the attack — not the camera, not the way it happened
	// to be walking — which is the other half of what an attack is: it goes that way and it looks that way
	// until it is over. At the end the controller's yaw takes the body back (see EndAttack), so the return
	// to the player's look is a natural consequence and not a snap.
	SetActorRotation(FRotator(0.0f, AttackLine.Rotation().Yaw, 0.0f));

	if (AttackElapsed >= AttackSeconds)
	{
		EndAttack();
	}
}

bool AProsperitocracyCharacter::IsHoldingRangedWeapon() const
{
	// The weapon's own block answers, by the project's own rule: a gun is a weapon with a fire mode, and
	// a melee is a weapon without one. Nothing here names a weapon or reads a class.
	const AProsperitocracyWeapon* Weapon = GetGunWeaponInHand();
	const UProsperitocracyStatTable* Block = Weapon ? Weapon->GetStatBlock() : nullptr;
	return Block && Block->GetFireMode().IsValid();
}

bool AProsperitocracyCharacter::IsHoldingMeleeWeapon() const
{
	// The same question, answered once: a melee is a thing in hand with no fire mode. Note it is NOT the
	// negation of "ranged" — an EMPTY hand is neither, and a pose that must tell those apart needs both.
	const AProsperitocracyWeapon* Weapon = GetGunWeaponInHand();
	const UProsperitocracyStatTable* Block = Weapon ? Weapon->GetStatBlock() : nullptr;
	return Block && !Block->GetFireMode().IsValid();
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
