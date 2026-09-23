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

	// A gun that has just come up starts from a clean aim: no trail or climb left over from the last
	// time it was held, and no swing measured across the gap (a gun that was put away for a minute
	// would otherwise read that whole minute as one enormous camera swing on its first tick back).
	LastShotTime = -BIG_NUMBER;
	AimDriftDegrees = FVector2D::ZeroVector;
	CurrentAccuracyMultiplier = 1.0f;
	StandingStillMultiplier = 1.0f;
	JumpFallMultiplier = 1.0f;
	CrouchingMultiplier = 1.0f;
	LastControlRotation = FRotator::ZeroRotator;
	bHasLastControlRotation = false;
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
	// The aim the player steers, plus what this gun adds to it. This is the one direction the bullet
	// flies along and the one the reticle circle is projected along, so the two cannot disagree. The
	// pose adds the same drift to the same aim in AProsperitocracyCharacter::GetBaseAimRotation.
	//
	// Camera rotation while a camera manager exists; the pawn's own control rotation otherwise; an
	// unheld gun falls back to its own forward, which is a state it is never fired in.
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
	// Recoil climbs the CIRCLE, never the screen (Design/ui.md): each shot pushes the aim up, and the
	// Weight-driven return in UpdateDrift brings it back down once the firing stops. Recoil is its own
	// axis — it never touches Accuracy, and Accuracy never scales it.
	AimDriftDegrees.Y += GetRecoil();

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

	// Weight is the whole feel of the sway: in this file it is the ONLY stat that scales the trail,
	// the displacement and the settle rate. It is read through this gun's own GAS home like every
	// other number, so a perk or attachment that moves it moves the feel.
	const float Weight = FMath::Max(0.0f, GetWeaponStat(EProsperitocracyStat::Weight));

	// --- The handling trail: the circle lags behind a camera swing, heavier lags further ---
	const FRotator CurrentRotation = PC->GetControlRotation();
	if (bHasLastControlRotation)
	{
		const FRotator Delta = (CurrentRotation - LastControlRotation).GetNormalized();
		const float LagTime = ProsperitocracyWeaponHandling::LagTimeBase + Weight * ProsperitocracyWeaponHandling::LagTimePerWeight;

		// Opposite the motion: swing right and the circle sits left of centre, swing up and it sits
		// below. Stop moving and the settle below pulls it back.
		AimDriftDegrees.X += -Delta.Yaw * LagTime;
		AimDriftDegrees.Y += -Delta.Pitch * LagTime;
	}
	LastControlRotation = CurrentRotation;
	bHasLastControlRotation = true;

	// --- The movement trail: the aim is an inert mass, so it lags its own motion ---
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
	UCharacterMovementComponent* Movement = OwningPawn ? OwningPawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr;

	// Standing still steadies: full bonus at a standstill, gone once the character is genuinely moving.
	const float Speed = OwningPawn ? OwningPawn->GetVelocity().Size() : 0.0f;
	const float StandingStillTarget = FMath::GetMappedRangeValueClamped(
		/*InputRange=*/ FVector2D(ProsperitocracyWeaponHandling::StandingStillSpeedThreshold, ProsperitocracyWeaponHandling::StandingStillSpeedThreshold + ProsperitocracyWeaponHandling::StandingStillToMovingSpeedRange),
		/*OutputRange=*/ FVector2D(ProsperitocracyWeaponHandling::PostureMultiplier_StandingStill, 1.0f),
		/*Alpha=*/ Speed);
	StandingStillMultiplier = FMath::FInterpTo(StandingStillMultiplier, StandingStillTarget, DeltaSeconds, ProsperitocracyWeaponHandling::TransitionRate_Posture);

	// Crouching steadies more; being in the air is sloppier. Neither has a stat: they are posture.
	const bool bCrouching = Movement && Movement->IsCrouching();
	CrouchingMultiplier = FMath::FInterpTo(CrouchingMultiplier, bCrouching ? ProsperitocracyWeaponHandling::PostureMultiplier_Crouching : 1.0f, DeltaSeconds, ProsperitocracyWeaponHandling::TransitionRate_Posture);

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

	// One multiplier on Accuracy. Posture never touches the recoil climb and never touches the trail.
	CurrentAccuracyMultiplier = AimingMultiplier * StandingStillMultiplier * CrouchingMultiplier * JumpFallMultiplier;
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
