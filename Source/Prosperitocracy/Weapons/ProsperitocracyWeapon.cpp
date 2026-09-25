// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyWeapon.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Character/ProsperitocracyCharacter.h"
#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadout.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyWeapon)

/** The barrel's mouth, on every gun body — the socket the tracer is drawn from, and the shot's start. */
const FName AProsperitocracyWeapon::MuzzleSocketName(TEXT("Muzzle"));

AProsperitocracyWeapon::AProsperitocracyWeapon()
{
	// The gun's steadiness (how still the body is holding it) is read per frame, so the actor ticks.
	// Only a gun that has been dressed does any of it — see Tick.
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

	// An undressed gun has no numbers, so it has no feel: nothing about how steady it is can be read.
	// That is also what keeps a gun the rig has thrown away from simulating anything.
	if (!bInitialized)
	{
		return;
	}

	UpdatePostureMultipliers(DeltaSeconds);
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

	// A gun that has just come up starts from a clean slate: no cadence left over from the last time it
	// was held, and its steadiness read fresh rather than whatever it was when the gun went away. There
	// is NO aim state on a gun to reset — the aim is the body's, and it never stopped being his.
	LastShotTime = -BIG_NUMBER;
	CurrentSteadiness = 1.0f;
	AimingMultiplier = 1.0f;
	StandingStillMultiplier = 1.0f;
	WalkingMultiplier = 1.0f;
	JumpFallMultiplier = 1.0f;
	CrouchingMultiplier = 1.0f;

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

FVector AProsperitocracyWeapon::GetMuzzleLocation() const
{
	// The barrel's mouth: the gun's OWN 'Muzzle' socket, which is the same point the blueprint draws
	// its tracer from — so the bullet and the tracer start in the same place. A gun whose mesh carries
	// no such socket falls back to its own origin rather than inventing a point in the world.
	if (const USkeletalMeshComponent* GunMesh = GetWeaponMesh())
	{
		if (GunMesh->DoesSocketExist(MuzzleSocketName))
		{
			return GunMesh->GetSocketLocation(MuzzleSocketName);
		}
	}
	return GetActorLocation();
}

FVector AProsperitocracyWeapon::GetAimDirection() const
{
	// THE MAN'S OWN AIM — and this is the only thing a bullet's direction has ever been allowed to come
	// from. The muzzle socket below gives the shot its START and nothing else: an aim-offset blend and a
	// fire montage can only ever be roughly the right way, so a direction read back out of the pose
	// inherits the pose's error, its blends and its punch — the gun ends up aiming by animation, and the
	// shot lands wherever the animation happened to be.
	//
	// A gun held by no character has no aim of its own, so it answers with the way it is facing.
	if (const AProsperitocracyCharacter* Character = GetOwnerCharacter())
	{
		return Character->GetAimRotation().Vector();
	}
	return GetActorForwardVector();
}

bool AProsperitocracyWeapon::GetShotLine(FVector& OutStart, FVector& OutEnd, FHitResult& OutHit)
{
	OutStart = GetMuzzleLocation();

	const FVector Direction = GetAimDirection();

	// How far the bullet may travel: the gun's OWN Range (its stat, in metres), and a long default when
	// the block carries none — so a gun with no range still has a line, and cover still snaps onto it.
	float ReachCm = 10000.0f;
	if (const float RangeMeters = GetWeaponStat(EProsperitocracyStat::Range); RangeMeters > 0.0f)
	{
		ReachCm = RangeMeters * 100.0f;
	}

	OutEnd = OutStart + Direction * ReachCm;

	UWorld* World = GetWorld();
	if (!World)
	{
		OutHit = FHitResult();
		return false;
	}

	// ONE trace, and it is the SHOT's own: the channel the bullet travels, and the shooter plus
	// everything attached to him ignored — never his own body, never the gun in his hands. Every reader
	// (the trigger, the circle, the tracer) is handed THIS, so none of them can disagree.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponShotLine), /*bTraceComplex=*/ false, OwningPawn.Get());
	if (OwningPawn)
	{
		TArray<AActor*> AttachedActors;
		OwningPawn->GetAttachedActors(AttachedActors);
		Params.AddIgnoredActors(AttachedActors);
	}

	const bool bBlocked = World->LineTraceSingleByChannel(OutHit, OutStart, OutEnd, ECC_Visibility, Params) && OutHit.bBlockingHit;
	if (bBlocked)
	{
		OutEnd = OutHit.ImpactPoint;
	}

	return bBlocked;
}

void AProsperitocracyWeapon::ApplyShotFeel()
{
	// THE KICK AND THE SHOVE LAND ON THE AIM ITSELF, because the aim is a real thing now: a gun that has
	// just fired is off its point, and the man is off his with it. The body's own turn rate is what walks
	// it back — nothing here brings it home, and nothing clamps it: it cannot leave the man.
	if (AProsperitocracyCharacter* OwnerCharacter = GetOwnerCharacter())
	{
		const float Steadiness = FMath::Max(0.25f, CurrentSteadiness);

		// The climb: one Recoil's worth of degrees up, divided by how steady the body is holding the gun
		// — so the same burst climbs least crouched and planted, and most at a run.
		const float ClimbDegrees = GetRecoil() / Steadiness;

		// And the spread: one random shove in any direction around the point, through that same
		// steadiness. That displacement IS the spread — hold the trigger and it dances.
		const float ShoveDegrees = ProsperitocracyWeaponHandling::SpreadShoveBaseDegrees / FMath::Max(0.25f, GetEffectiveAccuracy());
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);

		OwnerCharacter->PushAim(FMath::Cos(Angle) * ShoveDegrees, ClimbDegrees + FMath::Sin(Angle) * ShoveDegrees);
	}

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

			BodyStats->NotifyShotFired(GetWeaponStat(EProsperitocracyStat::Drag), GetWeaponStat(EProsperitocracyStat::Carry), GetAimDirection());
		}
	}
}

void AProsperitocracyWeapon::UpdatePostureMultipliers(float DeltaSeconds)
{
	UCharacterMovementComponent* Movement = OwningPawn ? OwningPawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr;

	// What the body is DOING, as its own numbers: how fast it is actually going, and whether it is
	// crouched or in the air. Nothing here is a copy of a speed — the walk speed the tiers compare
	// against is read off the body's own stats.
	const float Speed = OwningPawn ? OwningPawn->GetVelocity().Size() : 0.0f;

	// Standing still steadies most; the bonus fades over a short band, so easing off the stick settles
	// the gun rather than switching it.
	const float StandingStillTarget = FMath::GetMappedRangeValueClamped(
		/*InputRange=*/ FVector2D(ProsperitocracyWeaponHandling::StandingStillSpeedThreshold, ProsperitocracyWeaponHandling::StandingStillSpeedThreshold + ProsperitocracyWeaponHandling::StandingStillToMovingSpeedRange),
		/*OutputRange=*/ FVector2D(ProsperitocracyWeaponHandling::Steadiness_StandingStill, 1.0f),
		/*Alpha=*/ Speed);
	StandingStillMultiplier = FMath::FInterpTo(StandingStillMultiplier, StandingStillTarget, DeltaSeconds, ProsperitocracyWeaponHandling::TransitionRate_Posture);

	// Walking is steadier than running, and the change happens over a band above the body's OWN walk
	// speed — so walking into a run loosens the gun, and no threshold here has to be kept in step with
	// a stat by hand.
	float WalkSpeed = 0.0f;
	if (const UProsperitocracyPlayerStatsComponent* Stats = OwningPawn ? OwningPawn->FindComponentByClass<UProsperitocracyPlayerStatsComponent>() : nullptr)
	{
		WalkSpeed = Stats->GetWalkSpeed();
	}
	const float WalkingTarget = FMath::GetMappedRangeValueClamped(
		/*InputRange=*/ FVector2D(WalkSpeed, WalkSpeed + ProsperitocracyWeaponHandling::WalkingToRunningSpeedRange),
		/*OutputRange=*/ FVector2D(ProsperitocracyWeaponHandling::Steadiness_Walking, 1.0f),
		/*Alpha=*/ Speed);
	WalkingMultiplier = FMath::FInterpTo(WalkingMultiplier, WalkingTarget, DeltaSeconds, ProsperitocracyWeaponHandling::TransitionRate_Posture);

	// Crouching steadies more; being in the air is sloppier. Neither has a stat: they are posture.
	const bool bCrouching = Movement && Movement->IsCrouching();
	CrouchingMultiplier = FMath::FInterpTo(CrouchingMultiplier, bCrouching ? ProsperitocracyWeaponHandling::Steadiness_Crouching : 1.0f, DeltaSeconds, ProsperitocracyWeaponHandling::TransitionRate_Posture);

	const bool bFalling = Movement && Movement->IsFalling();
	JumpFallMultiplier = FMath::FInterpTo(JumpFallMultiplier, bFalling ? ProsperitocracyWeaponHandling::Steadiness_Airborne : 1.0f, DeltaSeconds, ProsperitocracyWeaponHandling::TransitionRate_Posture);

	// Aiming steadies most of all — and THE GUN'S OWN aiming state is what says so, eased at the same
	// rate as every other part of it. Nothing here reads the camera's own ADS blend: how steady the gun
	// is, is the gun's business, so changing the camera could never change how it shoots.
	const float AimingTarget = bAiming ? ProsperitocracyWeaponHandling::Steadiness_Aiming : 1.0f;
	AimingMultiplier = FMath::FInterpTo(AimingMultiplier, AimingTarget, DeltaSeconds, ProsperitocracyWeaponHandling::TransitionRate_Posture);

	// ONE number, and it sizes BOTH of this gun's own feel stats: the per-shot shove (Accuracy) and the
	// up-climb (Recoil). Nothing here touches how fast the man turns — the turn is his, not the gun's.
	CurrentSteadiness = AimingMultiplier * StandingStillMultiplier * WalkingMultiplier * CrouchingMultiplier * JumpFallMultiplier;
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
