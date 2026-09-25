// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyWeapon.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ProsperitocracyCharacter.h"
#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadout.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyWeapon)

AProsperitocracyWeapon::AProsperitocracyWeapon()
{
	// The gun's aim state (the trail, the spread, the settle) is a per-frame simulation, so the actor
	// ticks. Only a gun that has been dressed does any of it — see Tick.
	PrimaryActorTick.bCanEverTick = true;
}

bool AProsperitocracyWeapon::IsHeldInRig() const
{
	// The rig decides which weapon is out; this thing only asks. The same answer a shot is sent along.
	const AProsperitocracyCharacter* Character = Cast<AProsperitocracyCharacter>(OwningPawn.Get());
	return Character && Character->GetGunInHand() == this;
}

void AProsperitocracyWeapon::PrimaryAction_Implementation()
{
	// Nothing. A thing with no primary action has none, and saying so here is what lets the input
	// ask ANY weapon to do its job without being told what that weapon is. A gun's blueprint and a
	// melee's class each answer this for themselves; this is the answer for a thing that does not.
}

void AProsperitocracyWeapon::ReloadAction_Implementation()
{
	// A thing with no magazine has nothing to swap. A melee never reloads — not "reloads instantly",
	// never reloads.
}

void AProsperitocracyWeapon::SecondaryAction_Implementation()
{
	// THE BUTTON IS DOWN, and the thing it was pressed on is what knows it. That one line is the whole
	// of what a HOLD is, and it is recorded for every weapon before anything is decided: a move that is
	// held rather than pressed (the heavy combo) asks this every step, so nothing has to route the
	// release back to a running ability — the weapon it was pressed on answers.
	bSecondaryHeld = true;

	// THE DEFAULT ANSWER IS AIMING, and it belongs to a GUN: a firearm is aimed by holding the right
	// button, so the thing in the hand says "I am being aimed" and the body drives its camera,
	// crosshair and pose off that. The test is the project's own — a gun is a weapon with a fire mode
	// — so a melee gets no default here and answers for itself (the blade runs its own ability), and
	// nothing outside this weapon has to know which of them the player is holding.
	if (HasAFireMode())
	{
		bAiming = true;
	}
}

void AProsperitocracyWeapon::SecondaryActionReleased_Implementation()
{
	// The other half of the hold, recorded for every weapon for the same reason.
	bSecondaryHeld = false;

	// Aiming is a HOLD: the press starts it and this lets it go. A thing that never set it is not
	// harmed by clearing it — a melee's answer here is nothing at all.
	bAiming = false;
}

void AProsperitocracyWeapon::SetSecondPressAbility(TSubclassOf<UProsperitocracyGameplayAbility> InAbility)
{
	// HANDED OVER, never read off the weapon: the loadout that dressed this thing is the one that knows
	// what its carrier took in, and the weapon only holds what it was given. Said out loud, because a
	// right button that quietly does nothing and a right button that quietly does the WRONG thing look
	// exactly the same from the outside.
	SecondPressAbility = InAbility;

	UE_LOG(LogProsperitocracy, Log, TEXT("[Weapon] %s: the right button runs %s"),
		*GetName(), *GetNameSafe(InAbility.Get()));
}

bool AProsperitocracyWeapon::HasAFireMode() const
{
	// Read off the thing's own block: a weapon tagged FullAuto or SemiAuto is a gun, and a weapon with
	// no tag is not (Design/weapons.md). No block means no numbers and no mode.
	return StatBlockAsset && StatBlockAsset->GetFireMode().IsValid();
}

void AProsperitocracyWeapon::StandDownForANewMove()
{
	// Nothing. A gun holds no move of its own — its fire is a shot, and its aim is a state — so there
	// is nothing here to end. A weapon that DOES hold a move answers this for itself: the blade's
	// combo ends, because the body cannot wear two animations at once.
}

bool AProsperitocracyWeapon::SpendBlood(float Cost)
{
	// A thing with no blood owes none: true, and nothing taken. Only a thing whose whole use lives on
	// a pool answers this with real work (the blade), and a cost of nothing is free everywhere.
	return true;
}

void AProsperitocracyWeapon::MeleeAction_Implementation()
{
	// THE DEFAULT IS THE BASH: a gun's melee is the bash ability, and the body's own door is what runs
	// it. The test is the project's own — a gun is a weapon with a fire mode — so a melee gets no
	// default here and answers for itself, and nothing outside this weapon has to know which of the
	// two the player is holding.
	if (HasAFireMode())
	{
		if (AProsperitocracyCharacter* Body = Cast<AProsperitocracyCharacter>(OwningPawn.Get()))
		{
			Body->RunTheBash();
		}
	}
}

void AProsperitocracyWeapon::BeginPlay()
{
	Super::BeginPlay();

	// Best effort: a gun already in the rig gets its numbers now — the ones that exist when the level
	// starts do. A gun the rig re-creates later (a slot switch) is not attached yet and gets them on
	// first use instead. See EnsureInitialized.
	EnsureInitialized();
}

void AProsperitocracyWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// An undressed gun has no numbers, so it has no feel: it neither trails nor settles. That is also
	// what keeps a gun the rig has thrown away from simulating anything.
	if (!bInitialized)
	{
		return;
	}

	UpdatePostureMultipliers(DeltaSeconds);
	UpdateDrift(DeltaSeconds);
}

bool AProsperitocracyWeapon::EnsureInitialized()
{
	if (bInitialized)
	{
		return true;
	}

	// The rig is the child actor component's owner, and it becomes reachable once the gun is attached.
	// Until then there is nobody to ask — and asking with a guess is how a gun ends up with no numbers.
	AActor* RigOwner = GetAttachParentActor();
	if (!RigOwner)
	{
		RigOwner = GetOwner();
	}
	if (!RigOwner)
	{
		// Not in the rig yet. Normal at BeginPlay for a gun the rig re-creates on a slot switch, and
		// the reason this door is asked again on first use. Not a failure, so it is not logged.
		DressResult = EProsperitocracyWeaponDressResult::Undressed;
		return false;
	}

	if (!OwningPawn)
	{
		OwningPawn = Cast<APawn>(RigOwner);
	}

	// What am I? The loadout that owns this rig's guns answers. The gun does not look itself up, and
	// cannot: the rig created it without saying which slot it is, and one body can be two guns.
	UProsperitocracyLoadoutComponent* LoadoutComponent = RigOwner->FindComponentByClass<UProsperitocracyLoadoutComponent>();
	if (!LoadoutComponent)
	{
		DressResult = EProsperitocracyWeaponDressResult::NoLoadout;
		LogDressFailureOnce(DressResult, RigOwner);
		return false;
	}

	DressResult = LoadoutComponent->DressGun(this);
	if (DressResult != EProsperitocracyWeaponDressResult::Dressed)
	{
		LogDressFailureOnce(DressResult, RigOwner);
	}

	return bInitialized;
}

void AProsperitocracyWeapon::LogDressFailureOnce(EProsperitocracyWeaponDressResult Result, const AActor* RigOwner)
{
	if (bDressFailureLogged)
	{
		return;
	}

	// Undressed means "not asked yet / not in the rig yet" and Dressed is the working case; neither is
	// a failure, and neither may burn the one warning this gun has.
	switch (Result)
	{
	case EProsperitocracyWeaponDressResult::NoLoadout:
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Weapon] %s: %s has no loadout component — this gun has no numbers and cannot fire."),
			*GetName(), *GetNameSafe(RigOwner));
		break;
	case EProsperitocracyWeaponDressResult::NoEntry:
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Weapon] %s: the loadout on %s carries nothing for this gun blueprint — it has no numbers and cannot fire."),
			*GetName(), *GetNameSafe(RigOwner));
		break;
	case EProsperitocracyWeaponDressResult::AmbiguousEntry:
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Weapon] %s: more than one slot in the loadout on %s carries this gun blueprint, so the blueprint cannot say which slot it is — the equip path has to tell it."),
			*GetName(), *GetNameSafe(RigOwner));
		break;
	case EProsperitocracyWeaponDressResult::NoStatBlock:
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Weapon] %s: the loadout entry for this blueprint carries no stat block — no numbers, so it cannot fire."),
			*GetName());
		break;
	case EProsperitocracyWeaponDressResult::SlotMismatch:
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Weapon] %s: the stat block's slot tag is not the slot it is carried in — one of the two is wrong, so this gun was not dressed."),
			*GetName());
		break;
	default:
		return;
	}

	bDressFailureLogged = true;
}

void AProsperitocracyWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The gun's GAS home only exists for as long as the gun does.
	if (StatHost)
	{
		StatHost->Destroy();
		StatHost = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool AProsperitocracyWeapon::ApplyLoadoutEntry(const FGameplayTag& InSlot, UProsperitocracyStatTable* InStatBlock, TSubclassOf<UGameplayEffect> InDamageEffectClass, UProsperitocracyLoadoutComponent* InOwnerLoadout, APawn* InOwningPawn)
{
	// Which slot this gun is, its numbers, what a shot applies, and where its ammo lives are one
	// decision, so they arrive together — handed over by the loadout, which is the only thing that
	// can say which slot this is.
	Slot = InSlot;
	ShotDamageEffectClass = InDamageEffectClass;
	if (InOwnerLoadout)
	{
		OwnerLoadout = InOwnerLoadout;
	}

	if (InStatBlock)
	{
		StatBlockAsset = InStatBlock;
	}
	if (InOwningPawn)
	{
		OwningPawn = InOwningPawn;
	}
	if (!OwningPawn)
	{
		OwningPawn = Cast<APawn>(GetAttachParentActor());
	}

	if (!StatBlockAsset)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Weapon] %s was given no stat block — it has no numbers and cannot fire."), *GetName());
		return false;
	}

	// No loadout means nowhere for this gun's ammo to live, and a gun with no ammo home is not a
	// working gun. Refuse rather than dress it into a state where it can never fire.
	if (!OwnerLoadout)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Weapon] %s was handed its numbers with no loadout to keep its ammo in — it cannot fire."), *GetName());
		return false;
	}

	// The gun's GAS home. Spawned once, owned by the pawn, and fed the block's (stat, base) pairs —
	// the same act as the character's health component feeding Health.
	if (!StatHost)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwningPawn;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		StatHost = GetWorld() ? GetWorld()->SpawnActor<AProsperitocracyStatHostActor>(
			AProsperitocracyStatHostActor::StaticClass(), GetActorTransform(), SpawnParams) : nullptr;
	}
	if (StatHost)
	{
		StatHost->InitializeFromStatBlock(StatBlockAsset);
	}

	// Ammo, in the owner's store for this slot. This first ask is what fills the magazine: the loaded
	// magazine is ONE OF the Capacity magazines, so the spare pool is (Capacity x MagSize) minus the
	// one in the gun — and more magazines never makes a magazine bigger.
	OwnerLoadout->GetOrCreateAmmoForSlot(Slot, MagazineSize(), MagazineCapacity());

	// A gun that has just come up starts from a clean OFFSET: no climb, no shove and no movement
	// inertia left over from the last time it was held. (The man's own aim is deliberately NOT reset —
	// it belongs to him and is continuous, so a gun coming up adds nothing to where he is pointing.)
	LastShotTime = -BIG_NUMBER;
	AimDriftDegrees = FVector2D::ZeroVector;
	CurrentPostureMultiplier = 1.0f;
	StanceMultiplier = 1.0f;
	JumpFallMultiplier = 1.0f;

	// And a gun that has just come up is not being aimed: the state belongs to a press, and there has
	// not been one yet.
	bAiming = false;

	bInitialized = true;

	UE_LOG(LogProsperitocracy, Log, TEXT("[Weapon] %s ready — slot %s | block %s | mag %d spare %d | rate %.2f/s | %s"),
		*GetName(), *Slot.ToString(), *GetNameSafe(StatBlockAsset), GetMagazineAmmo(), GetSpareAmmo(),
		GetSecondsBetweenShots() > 0.0f ? (1.0f / GetSecondsBetweenShots()) : 0.0f,
		IsFullAuto() ? TEXT("FullAuto") : TEXT("SemiAuto"));

	return true;
}

bool AProsperitocracyWeapon::TryConsumeRound()
{
	// A gun that has not met its loadout yet has no magazine; the answer is no.
	if (!EnsureInitialized() || !OwnerLoadout)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Too soon for the next round: not a dry fire, just the Rate cadence refusing the shot. The gun's
	// graph must not run its dry-fire branch on this — that is what IsMagazineEmpty is for.
	if (!CanFireNow())
	{
		return false;
	}

	FProsperitocracyWeaponAmmo& Ammo = OwnerLoadout->GetOrCreateAmmoForSlot(Slot, MagazineSize(), MagazineCapacity());
	if (Ammo.Magazine <= 0)
	{
		// Empty. The gun's own graph plays its dry-fire sound and montage.
		LastShotTime = World->GetTimeSeconds();
		return false;
	}

	--Ammo.Magazine;
	LastShotTime = World->GetTimeSeconds();
	return true;
}

bool AProsperitocracyWeapon::ReloadFromStats()
{
	if (!EnsureInitialized() || !OwnerLoadout)
	{
		return false;
	}

	const int32 MagSize = MagazineSize();
	if (MagSize <= 0)
	{
		return false;
	}

	FProsperitocracyWeaponAmmo& Ammo = OwnerLoadout->GetOrCreateAmmoForSlot(Slot, MagSize, MagazineCapacity());
	if (Ammo.Spare <= 0)
	{
		return false;
	}

	// A mag swap, not a top-up: the rounds left in this magazine are thrown away. They are NOT
	// returned to the pool (Design/combat.md), and a partial magazine is loaded when the pool has
	// less than a full one left.
	const int32 Loaded = FMath::Min(MagSize, Ammo.Spare);
	Ammo.Magazine = Loaded;
	Ammo.Spare -= Loaded;

	UE_LOG(LogProsperitocracy, Log, TEXT("[Weapon] %s (%s) reloaded — mag %d spare %d"),
		*GetName(), *Slot.ToString(), Ammo.Magazine, Ammo.Spare);
	return true;
}

bool AProsperitocracyWeapon::IsMagazineEmpty() const
{
	// Undressed means no magazine yet, which is empty as far as the gun's own dry-fire branch is
	// concerned — the same answer the gun gave when the magazine count lived on it.
	const FProsperitocracyWeaponAmmo* Ammo = FindAmmo();
	return !Ammo || Ammo->Magazine <= 0;
}

int32 AProsperitocracyWeapon::GetMagazineAmmo() const
{
	const FProsperitocracyWeaponAmmo* Ammo = FindAmmo();
	return Ammo ? Ammo->Magazine : 0;
}

int32 AProsperitocracyWeapon::GetSpareAmmo() const
{
	const FProsperitocracyWeaponAmmo* Ammo = FindAmmo();
	return Ammo ? Ammo->Spare : 0;
}

const FProsperitocracyWeaponAmmo* AProsperitocracyWeapon::FindAmmo() const
{
	// The ammo is the slot's, not the gun's: read it out of the owner's store through the one key
	// this gun has — the slot it was told it is.
	return (OwnerLoadout && Slot.IsValid()) ? OwnerLoadout->FindAmmoForSlot(Slot) : nullptr;
}

USkeletalMeshComponent* AProsperitocracyWeapon::GetWeaponMesh() const
{
	// The gun blueprint's OWN mesh component: the one its fire and reload anims play on and the one
	// that carries the Muzzle socket. This class deliberately does not create a second one.
	return FindComponentByClass<USkeletalMeshComponent>();
}

float AProsperitocracyWeapon::GetWeaponStat(EProsperitocracyStat Stat) const
{
	if (!StatHost)
	{
		return 0.0f;
	}
	return UProsperitocracyStatSystemStatics::GetStatFinal(StatHost->GetProsperitocracyAbilitySystemComponent(), Stat);
}

bool AProsperitocracyWeapon::IsFullAuto() const
{
	return StatBlockAsset && (StatBlockAsset->GetFireMode() == ProsperitocracyGameplayTags::Weapon_FireMode_FullAuto);
}

float AProsperitocracyWeapon::GetSecondsBetweenShots() const
{
	const float Rate = GetWeaponStat(EProsperitocracyStat::Rate);
	return (Rate > 0.0f) ? (1.0f / Rate) : 0.0f;
}

int32 AProsperitocracyWeapon::MagazineSize() const
{
	return FMath::Max(0, FMath::RoundToInt(GetWeaponStat(EProsperitocracyStat::MagSize)));
}

int32 AProsperitocracyWeapon::MagazineCapacity() const
{
	return FMath::Max(0, FMath::RoundToInt(GetWeaponStat(EProsperitocracyStat::Capacity)));
}

bool AProsperitocracyWeapon::CanFireNow() const
{
	const float Interval = GetSecondsBetweenShots();
	if (Interval <= 0.0f)
	{
		return true;
	}
	const UWorld* World = GetWorld();
	return World && ((World->GetTimeSeconds() - LastShotTime) >= Interval);
}

void AProsperitocracyWeapon::ApplyShotDamage(const FHitResult& Hit)
{
	// A ranged shot carries no lines of its own: this gun's stat host answers with its Impact and/or
	// Piercing lines and its Penetration (see AProsperitocracyStatHostActor::GetDamageLines).
	ApplyDamageToHit(Hit);
}

void AProsperitocracyWeapon::ApplyDamageToHit(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return;
	}

	// A wall has no ability system: nothing to damage, and its impact FX has already been played by
	// the gun's own graph.
	UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetAbilitySystemComponent)
	{
		return;
	}

	UAbilitySystemComponent* SourceAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
	if (!SourceAbilitySystemComponent)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] %s fired at %s but the firer has no ability system — nothing was applied."), *GetName(), *GetNameSafe(HitActor));
		return;
	}

	if (!ShotDamageEffectClass)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] %s has no shot damage effect set — hit %s and applied nothing. This is a bug, not a missing feature."), *GetName(), *GetNameSafe(HitActor));
		return;
	}

	// The effect context carries the hit (so the execution can read the impact distance for falloff,
	// and the part that was hit for its armor + resists) and points at this gun's stat host as the
	// ABILITY SOURCE — which is where the execution gets the damage lines (Penetration + the weapon's
	// damage stat) and the falloff (Range/Falloff) from, through the ONE evaluator.
	FGameplayEffectContextHandle Context = SourceAbilitySystemComponent->MakeEffectContext();
	Context.AddInstigator(OwningPawn, this);
	Context.AddHitResult(Hit, /*bReset=*/ true);

	if (FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context))
	{
		TypedContext->SetAbilitySource(StatHost, 1.0f);
	}
	else
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] %s: the effect context is not ours — no lines or falloff will resolve. Check AbilitySystemGlobalsClassName in DefaultGame.ini."), *GetName());
	}

	// The one place a built context becomes damage: the same applier the bash reaches the execution
	// through, so a shot and a bash cannot drift apart in how the shared pen-gate → resist path runs.
	UProsperitocracyDamageStatics::ApplyDamageEffectToHit(Context, HitActor, SourceAbilitySystemComponent, ShotDamageEffectClass);

	// And the statuses this gun's block NAMES go on with the hit — through the same one door, so a gun
	// that sets things alight or stops them dead carries no firing code of its own: it lists the
	// status, and each status's own block is where its numbers are (Design/abilities.md).
	UProsperitocracyDamageStatics::ApplyEffectsToHit(Context, HitActor, SourceAbilitySystemComponent, GetStatBlock(), ShotDamageEffectClass);
}

AProsperitocracyCharacter* AProsperitocracyWeapon::GetOwnerCharacter() const
{
	return Cast<AProsperitocracyCharacter>(OwningPawn);
}

APlayerController* AProsperitocracyWeapon::GetOwningPlayerController() const
{
	return OwningPawn ? Cast<APlayerController>(OwningPawn->GetController()) : nullptr;
}

FVector AProsperitocracyWeapon::GetShotDirection() const
{
	// THE MAN'S AIM IS THE SHOT. With a body holding this gun the direction is that body's aim — the
	// same rotation the reticle circle is projected along and the pose is handed, so the bullet, the
	// circle and the arms cannot disagree about where the gun is pointing. This gun adds nothing here:
	// its own numbers are already part of that aim (the body reads GetAimDriftDegrees), and adding
	// them a second time is how one offset becomes two.
	if (const AProsperitocracyCharacter* Character = GetOwnerCharacter())
	{
		return Character->GetAimRotation().Vector();
	}

	// No body steering this gun (a dummy, an unattended gun): the plain camera aim plus this gun's own
	// offset, which is what its numbers mean when nobody is turning for it.
	const APlayerController* PC = GetOwningPlayerController();
	const FRotator AimRotation = (PC && PC->PlayerCameraManager)
		? PC->PlayerCameraManager->GetCameraRotation()
		: (OwningPawn ? OwningPawn->GetControlRotation() : GetActorRotation());

	const FVector2D Drift = GetAimDriftDegrees();
	FRotator ShotRotation = AimRotation;
	ShotRotation.Pitch += Drift.Y;
	ShotRotation.Yaw += Drift.X;

	return ShotRotation.Vector();
}

void AProsperitocracyWeapon::ApplyShotFeel()
{
	// Recoil climbs THE MAN'S AIM, never the screen (Design/ui.md): each shot pushes the aim up, and
	// the Weight-driven return in UpdateDrift brings it back down once the firing stops. Recoil is
	// still its own axis — Accuracy never scales it — but HOW MUCH of it a shot has is posture's to
	// say, exactly as posture says how wide the shove below is: planted and crouched climbs less than
	// the same shot taken at a run.
	AimDriftDegrees.Y += GetRecoil() / FMath::Max(0.05f, CurrentPostureMultiplier);

	// Then the spread: a random shove in any direction around the aim point. That displacement IS the
	// spread — hold the trigger and the circle dances.
	ApplySpreadShove();

	ClampDrift();

	// And the shot's push on the BODY: the gun hands over its own Drag and Carry — its push, the stat
	// derived from its Weight and damage on its stat host — and the line the shot went down, to the
	// character's stats component, which is the one place a number reaches the body. Walking forward
	// that push is speed you spend fighting it, walking back it carries you — the character's own
	// movement direction decides, so the gun has no rule of its own about it and every gun gets the
	// same one.
	if (AProsperitocracyCharacter* OwnerCharacter = GetOwnerCharacter())
	{
		if (UProsperitocracyPlayerStatsComponent* BodyStats = OwnerCharacter->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
		{
			// The push is worked out HERE, as it is handed over — not once when the gun came up. Its
			// inputs are this gun's Weight and damage, and either can move mid-fight (a perk, a proc,
			// an attachment), so a push derived back at equip time would hand the body a number priced
			// at the weight the gun had then. The sway never had this hole because it reads Weight
			// every frame; this is the push reading its own inputs at the moment it is used. The two
			// stay stat bases, so perks still resolve on top of them through the aggregator.
			if (StatHost)
			{
				StatHost->ApplyDerivedStats();
			}

			BodyStats->NotifyShotFired(GetWeaponStat(EProsperitocracyStat::Drag), GetWeaponStat(EProsperitocracyStat::Carry), GetShotDirection());
		}
	}
}

void AProsperitocracyWeapon::ApplySpreadShove()
{
	// Accuracy is the only driver, and higher is tighter: the displacement shrinks as the effective
	// Accuracy grows. The floor keeps a nonsensical value from inverting the curve.
	const float EffectiveAccuracy = GetEffectiveAccuracy();
	const float ShoveMagnitude = ProsperitocracyWeaponHandling::SpreadShoveBaseDegrees / FMath::Max(0.25f, EffectiveAccuracy);
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);

	AimDriftDegrees.X += FMath::Cos(Angle) * ShoveMagnitude;
	AimDriftDegrees.Y += FMath::Sin(Angle) * ShoveMagnitude;

	ClampDrift();
}

void AProsperitocracyWeapon::UpdateDrift(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	APawn* Pawn = OwningPawn;
	APlayerController* PC = GetOwningPlayerController();
	if (!Pawn || !PC)
	{
		return;
	}

	// Weight is the whole feel of the movement: in this file it is the ONLY stat that scales the
	// displacement and the settle rate. It is read through this gun's own GAS home like every other
	// number, so a perk or attachment that moves it moves the feel. (The TURN's own weight is the
	// body's, and is what the man carries — see AProsperitocracyCharacter::GetTurnRateDegreesPerSecond.)
	const float Weight = FMath::Max(0.0f, GetWeaponStat(EProsperitocracyStat::Weight));

	// --- The movement's inertia: the gun is an inert mass, so it lags its own motion ---
	const FVector Velocity2D(Pawn->GetVelocity().X, Pawn->GetVelocity().Y, 0.0f);
	const FVector CameraForward = PC->GetControlRotation().Vector();
	const FVector Forward2D = FVector(CameraForward.X, CameraForward.Y, 0.0f).GetSafeNormal();
	const FVector Right2D(-Forward2D.Y, Forward2D.X, 0.0f);
	const float ForwardSpeed = FVector::DotProduct(Velocity2D, Forward2D);
	const float RightSpeed = FVector::DotProduct(Velocity2D, Right2D);

	// dt-scaled, so the resting offset the trail settles at (speed x displacement / return rate) is the
	// same at any framerate. Run right and the aim lags left; run forward and it rises — the vertical
	// is a fraction of the sidelong shift, which is what makes the movement oval a wide one.
	const float MoveDisplace = ProsperitocracyWeaponHandling::MoveDisplaceBase + Weight * ProsperitocracyWeaponHandling::MoveDisplacePerWeight;
	AimDriftDegrees.X += -RightSpeed * MoveDisplace * DeltaSeconds;
	AimDriftDegrees.Y += ForwardSpeed * MoveDisplace * ProsperitocracyWeaponHandling::MoveVerticalFraction * DeltaSeconds;

	// --- Back to centre: the same Weight makes the return slower, so heavy gear stays unsettled ---
	const float ReturnRate = FMath::Max(
		ProsperitocracyWeaponHandling::ReturnRateMin,
		ProsperitocracyWeaponHandling::ReturnRateBase - Weight * ProsperitocracyWeaponHandling::ReturnRatePerWeight);
	AimDriftDegrees *= FMath::Max(0.0f, 1.0f - ReturnRate * DeltaSeconds);

	ClampDrift();
}

void AProsperitocracyWeapon::UpdatePostureMultipliers(float DeltaSeconds)
{
	// ONE number, out of everything the SHOOTER is doing: aiming down sights, what he is doing on his
	// feet (standing still, walking, running), whether he is crouched, and whether he is in the air.
	// It scales BOTH of this gun's numbers — the shove it divides and the climb it divides — so a
	// planted, crouched, aiming shot is tighter AND climbs less than the same shot taken at a run.
	UCharacterMovementComponent* Movement = OwningPawn ? OwningPawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
	const float Speed = OwningPawn ? OwningPawn->GetVelocity().Size() : 0.0f;

	// THE STANCE TIERS. Crouching is steadier than anything on his feet; standing still is steadier
	// than walking, and running on his feet is the loosest. A crouch is never a run — crouch speed IS
	// the walk speed — so crouching is one tier whatever his legs are doing.
	//
	// Walking and running are told apart by the body's OWN walking number and never by a guess: up to
	// his walk speed he is walking, past it he is running. That number is read through the one
	// evaluator, so a load that moves his speeds moves where the changeover sits.
	float StanceTarget = ProsperitocracyWeaponHandling::PostureMultiplier_Walking;
	if (Movement && Movement->IsCrouching())
	{
		StanceTarget = ProsperitocracyWeaponHandling::PostureMultiplier_Crouching;
	}
	else if (Speed <= ProsperitocracyWeaponHandling::StandingStillSpeedThreshold)
	{
		StanceTarget = ProsperitocracyWeaponHandling::PostureMultiplier_StandingStill;
	}
	else
	{
		const AProsperitocracyCharacter* Character = GetOwnerCharacter();
		const UProsperitocracyPlayerStatsComponent* Stats = Character ? Character->FindComponentByClass<UProsperitocracyPlayerStatsComponent>() : nullptr;
		const float WalkSpeed = Stats ? Stats->GetWalkSpeed() : 0.0f;
		StanceTarget = (WalkSpeed > 0.0f && Speed > WalkSpeed)
			? ProsperitocracyWeaponHandling::PostureMultiplier_Running
			: ProsperitocracyWeaponHandling::PostureMultiplier_Walking;
	}

	StanceMultiplier = FMath::FInterpTo(StanceMultiplier, StanceTarget, DeltaSeconds, ProsperitocracyWeaponHandling::TransitionRate_Posture);

	// In the air is sloppier, and it is posture like the rest of them — no stat carries it.
	const bool bFalling = Movement && Movement->IsFalling();
	JumpFallMultiplier = FMath::FInterpTo(JumpFallMultiplier, bFalling ? ProsperitocracyWeaponHandling::PostureMultiplier_JumpingOrFalling : 1.0f, DeltaSeconds, ProsperitocracyWeaponHandling::TransitionRate_Posture);

	// Aiming steadies most of all, and it eases in with the camera: the very same ADS blend the
	// reticle circle fades in with, so the tighter group and the visible aid arrive together.
	const AProsperitocracyCharacter* Character = GetOwnerCharacter();
	const float AimingAlpha = Character ? Character->GetAimingAlpha() : 0.0f;
	const float AimingMultiplier = FMath::GetMappedRangeValueClamped(
		/*InputRange=*/ FVector2D(0.0f, 1.0f),
		/*OutputRange=*/ FVector2D(1.0f, ProsperitocracyWeaponHandling::PostureMultiplier_Aiming),
		/*Alpha=*/ AimingAlpha);

	// ONE multiplier on both of this gun's numbers. The TURN is the body's, and posture says nothing
	// about it: crouching does not make a man's whole body come round faster.
	CurrentPostureMultiplier = AimingMultiplier * StanceMultiplier * JumpFallMultiplier;
}

void AProsperitocracyWeapon::ClampDrift()
{
	constexpr float MaxDrift = ProsperitocracyWeaponHandling::MaxDriftDegrees;
	AimDriftDegrees.X = FMath::Clamp(AimDriftDegrees.X, -MaxDrift, MaxDrift);
	AimDriftDegrees.Y = FMath::Clamp(AimDriftDegrees.Y, -MaxDrift, MaxDrift);
}

float AProsperitocracyWeapon::GetAccuracy() const
{
	// Presence-is-scope: an absent Accuracy evaluates to 0 through GAS, and here that means "not
	// specified" rather than "hopelessly inaccurate", so it falls back to the baseline 1.0.
	const float Accuracy = GetWeaponStat(EProsperitocracyStat::Accuracy);
	return (Accuracy > 0.0f) ? Accuracy : 1.0f;
}

float AProsperitocracyWeapon::GetRecoil() const
{
	// Presence-is-scope: no Recoil stat means no climb at all.
	return GetWeaponStat(EProsperitocracyStat::Recoil);
}
