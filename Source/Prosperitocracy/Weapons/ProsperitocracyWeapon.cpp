// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyWeapon.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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
	PrimaryActorTick.bCanEverTick = false;
}

void AProsperitocracyWeapon::BeginPlay()
{
	Super::BeginPlay();

	// Best effort: a gun already in the rig gets its numbers now — the ones that exist when the level
	// starts do. A gun the rig re-creates later (a slot switch) is not attached yet and gets them on
	// first use instead. See EnsureInitialized.
	EnsureInitialized();
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

	LastShotTime = -BIG_NUMBER;
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

	const FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystemComponent->MakeOutgoingSpec(ShotDamageEffectClass, 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystemComponent);
	}
}
