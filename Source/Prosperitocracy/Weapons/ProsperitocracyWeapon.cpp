// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyWeapon.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyWeaponBodyData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyWeapon)

const FName AProsperitocracyWeapon::MuzzleSocketName(TEXT("Muzzle"));

AProsperitocracyWeapon::AProsperitocracyWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	// A gun must never be shootable, never block the shot it fires, and never be culled out from
	// under the camera while it is the thing the player is looking along.
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->bOwnerNoSee = false;
	WeaponMesh->CastShadow = true;
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

void AProsperitocracyWeapon::InitializeFromStatBlock(UProsperitocracyStatTable* InStatBlock, APawn* InOwningPawn)
{
	StatBlock = InStatBlock;
	OwningPawn = InOwningPawn ? InOwningPawn : Cast<APawn>(GetOwner());

	if (!StatBlock)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Weapon] %s was initialized with no stat block — it has no numbers, no body and cannot fire."), *GetName());
		return;
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
		StatHost->InitializeFromStatBlock(StatBlock);
	}

	// The visuals + audio: the body this gun rides on.
	ApplyBody(StatBlock->GetWeaponBody());

	// Ammo. The loaded magazine is ONE OF the Capacity magazines, so the spare pool is
	// (Capacity x MagSize) minus the one in the gun. More magazines never makes a magazine bigger.
	const int32 MagSize = FMath::Max(0, FMath::RoundToInt(GetWeaponStat(EProsperitocracyStat::MagSize)));
	const int32 Capacity = FMath::Max(0, FMath::RoundToInt(GetWeaponStat(EProsperitocracyStat::Capacity)));
	MagazineAmmo = MagSize;
	SpareAmmo = FMath::Max(0, Capacity * MagSize - MagSize);
	LastShotTime = -BIG_NUMBER;

	UE_LOG(LogProsperitocracy, Log, TEXT("[Weapon] %s ready — body %s | mag %d spare %d | rate %.2f/s | %s"),
		*GetName(), *GetNameSafe(StatBlock->GetWeaponBody()), MagazineAmmo, SpareAmmo,
		GetSecondsBetweenShots() > 0.0f ? (1.0f / GetSecondsBetweenShots()) : 0.0f,
		IsFullAuto() ? TEXT("FullAuto") : TEXT("SemiAuto"));
}

void AProsperitocracyWeapon::ApplyBody(const UProsperitocracyWeaponBodyData* InBody)
{
	if (!InBody)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Weapon] %s: stat block %s has no WeaponBody — the gun will fire with no mesh, no anim, no sound and no FX."),
			*GetName(), *GetNameSafe(StatBlock));
		return;
	}

	if (USkeletalMesh* Mesh = InBody->Mesh.LoadSynchronous())
	{
		WeaponMesh->SetSkeletalMeshAsset(Mesh);
		// The template's gun blueprints drive the gun mesh with single-node anims (their
		// PlayAnimation calls), not with an anim blueprint — same thing here.
		WeaponMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
	else
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Weapon] %s: body %s has no mesh — the gun will fire invisibly."),
			*GetName(), *GetNameSafe(InBody));
	}

	GunFireAnim = InBody->GunFireAnim.LoadSynchronous();
	GunReloadAnim = InBody->GunReloadAnim.LoadSynchronous();
	CharacterFireMontage = InBody->CharacterFireMontage.LoadSynchronous();
	CharacterReloadMontage = InBody->CharacterReloadMontage.LoadSynchronous();
	CharacterDryFireMontage = InBody->CharacterDryFireMontage.LoadSynchronous();
	FireSound = InBody->FireSound.LoadSynchronous();
	DryFireSound = InBody->DryFireSound.LoadSynchronous();
	FireAttenuation = InBody->FireAttenuation.LoadSynchronous();
	FireConcurrency = InBody->FireConcurrency.LoadSynchronous();
	MuzzleVFX = InBody->MuzzleVFX.LoadSynchronous();
	TracerActorClass = InBody->TracerActorClass.LoadSynchronous();
	ImpactDecalClass = InBody->ImpactDecalClass.LoadSynchronous();
	ImpactDecalScale = InBody->ImpactDecalScale;
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
	return StatBlock && (StatBlock->GetFireMode() == ProsperitocracyGameplayTags::Weapon_FireMode_FullAuto);
}

float AProsperitocracyWeapon::GetSecondsBetweenShots() const
{
	const float Rate = GetWeaponStat(EProsperitocracyStat::Rate);
	return (Rate > 0.0f) ? (1.0f / Rate) : 0.0f;
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

bool AProsperitocracyWeapon::Fire()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Too soon for the next round: not a dry fire, just the Rate cadence refusing the shot.
	if (!CanFireNow())
	{
		return false;
	}

	if (MagazineAmmo <= 0)
	{
		// The dry fire is also cadence-gated, so holding the trigger on an empty gun clicks at the
		// gun's rate instead of every frame.
		LastShotTime = World->GetTimeSeconds();
		PlayDryFire();
		return false;
	}

	--MagazineAmmo;
	LastShotTime = World->GetTimeSeconds();

	FVector MuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
	FVector TraceEnd = MuzzleLocation;
	const FHitResult Hit = TraceShot(MuzzleLocation, TraceEnd);

	PlayShotFeedback(Hit, MuzzleLocation, TraceEnd);
	ApplyDamage(Hit);
	return true;
}

bool AProsperitocracyWeapon::Reload()
{
	if (SpareAmmo <= 0)
	{
		return false;
	}

	const int32 MagSize = FMath::Max(0, FMath::RoundToInt(GetWeaponStat(EProsperitocracyStat::MagSize)));

	// A mag swap, not a top-up: the rounds left in this magazine are thrown away. They are NOT
	// returned to the pool (Design/combat.md), and a partial magazine is loaded when the pool has
	// less than a full one left.
	MagazineAmmo = 0;
	const int32 Loaded = FMath::Min(MagSize, SpareAmmo);
	MagazineAmmo = Loaded;
	SpareAmmo -= Loaded;

	PlayCharacterMontage(CharacterReloadMontage);
	if (GunReloadAnim)
	{
		WeaponMesh->PlayAnimation(GunReloadAnim, false);
	}

	UE_LOG(LogProsperitocracy, Log, TEXT("[Weapon] %s reloaded — mag %d spare %d"), *GetName(), MagazineAmmo, SpareAmmo);
	return true;
}

FHitResult AProsperitocracyWeapon::TraceShot(FVector& OutMuzzleLocation, FVector& OutTraceEnd) const
{
	FHitResult Result;

	UWorld* World = GetWorld();
	OutMuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
	OutTraceEnd = OutMuzzleLocation + GetActorForwardVector() * MaxShotRangeCm;
	if (!World)
	{
		return Result;
	}

	// 1. Where the player is aiming, with this gun's drift added. The reticle circle draws the SAME
	//    drift, so the shot lands where the circle points. (Drift is zero until the aim-feel pass;
	//    the trace already reads it so the two never drift apart later.)
	FVector ViewLocation = OutMuzzleLocation;
	FRotator ViewRotation = GetActorRotation();
	if (const APlayerController* PlayerController = OwningPawn ? Cast<APlayerController>(OwningPawn->GetController()) : nullptr)
	{
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	ViewRotation.Pitch += AimDriftDegrees.Y;
	ViewRotation.Yaw += AimDriftDegrees.X;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProsperitocracyWeaponShot), /*bTraceComplex=*/false, OwningPawn ? static_cast<const AActor*>(OwningPawn) : this);
	Params.bReturnPhysicalMaterial = true;

	const FVector AimPoint = ViewLocation + ViewRotation.Vector() * MaxShotRangeCm;
	FHitResult CameraHit;
	const bool bHitAimLine = World->LineTraceSingleByChannel(CameraHit, ViewLocation, AimPoint, ECC_Visibility, Params);

	// 2. The bullet itself: from the muzzle to that aim point. This is the line that decides what is
	//    hit — it starts at the barrel, not at the eye, so the gun cannot shoot through a wall it is
	//    pressed against.
	OutTraceEnd = bHitAimLine ? CameraHit.ImpactPoint : AimPoint;
	World->LineTraceSingleByChannel(Result, OutMuzzleLocation, OutTraceEnd, ECC_Visibility, Params);
	if (!Result.bBlockingHit)
	{
		Result.TraceStart = OutMuzzleLocation;
		Result.TraceEnd = OutTraceEnd;
		Result.Location = OutTraceEnd;
		Result.ImpactPoint = OutTraceEnd;
	}
	return Result;
}

void AProsperitocracyWeapon::PlayShotFeedback(const FHitResult& Hit, const FVector& MuzzleLocation, const FVector& TraceEnd)
{
	// The character's half of the action — the template's montage, on the character mesh.
	PlayCharacterMontage(CharacterFireMontage);

	// The gun's half — the template's own anim on the gun mesh's single node.
	if (GunFireAnim)
	{
		WeaponMesh->PlayAnimation(GunFireAnim, false);
	}

	// The muzzle flash, at the Muzzle socket. These systems are hand-triggered: they emit nothing
	// until User.Trigger is true, and setting it after activation is overwritten on the init frame —
	// so spawn inactive, arm the trigger, then activate.
	if (MuzzleVFX)
	{
		if (UNiagaraComponent* MuzzleComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleVFX, WeaponMesh, MuzzleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, /*bAutoDestroy=*/ true,
			/*bAutoActivate=*/ false, ENCPoolMethod::None,
			/*bPreCullCheck=*/ false))
		{
			MuzzleComponent->SetNiagaraVariableBool(TEXT("User.Trigger"), true);
			MuzzleComponent->Activate(/*bReset=*/ true);
		}
	}

	// The shot sound: one path for every gun, the body's own sound with its attenuation and its
	// concurrency limit, played at the muzzle.
	if (FireSound)
	{
		UGameplayStatics::SpawnSoundAttached(
			FireSound, WeaponMesh, MuzzleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget,
			/*bStopWhenAttachedToDestroyed=*/ true,
			/*VolumeMultiplier=*/ 1.0f, /*PitchMultiplier=*/ 1.0f, /*StartTime=*/ 0.0f,
			FireAttenuation, FireConcurrency, /*bAutoDestroy=*/ true);
	}

	// The tracer: the template's own tracer actor, spawned muzzle-to-landing-point exactly where its
	// blueprint spawned it.
	if (TracerActorClass)
	{
		const FRotator TracerRotation = (TraceEnd - MuzzleLocation).Rotation();
		GetWorld()->SpawnActor<AActor>(TracerActorClass, MuzzleLocation, TracerRotation);
	}

	// The impact decal at the landing point, on the surface normal. A miss has no normal to stick to.
	if (ImpactDecalClass && Hit.bBlockingHit && !Hit.ImpactNormal.IsNearlyZero())
	{
		const FTransform DecalTransform(Hit.ImpactNormal.Rotation(), Hit.ImpactPoint, ImpactDecalScale);
		GetWorld()->SpawnActor<AActor>(ImpactDecalClass, DecalTransform);
	}
}

void AProsperitocracyWeapon::PlayDryFire()
{
	PlayCharacterMontage(CharacterDryFireMontage);

	if (DryFireSound)
	{
		UGameplayStatics::SpawnSoundAttached(
			DryFireSound, WeaponMesh, MuzzleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget,
			/*bStopWhenAttachedToDestroyed=*/ true,
			/*VolumeMultiplier=*/ 1.0f, /*PitchMultiplier=*/ 1.0f, /*StartTime=*/ 0.0f,
			FireAttenuation, FireConcurrency, /*bAutoDestroy=*/ true);
	}
}

void AProsperitocracyWeapon::PlayCharacterMontage(UAnimMontage* Montage) const
{
	if (!Montage)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(OwningPawn);
	if (!Character)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	UAnimInstance* AnimInstance = CharacterMesh ? CharacterMesh->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		AnimInstance->Montage_Play(Montage);
	}
}

void AProsperitocracyWeapon::ApplyDamage(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return;
	}

	// A wall has no ability system: nothing to damage, and the decal has already been placed.
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

	if (!DamageEffectClass)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] %s has no DamageEffectClass set — hit %s and applied nothing. This is a bug, not a missing feature."), *GetName(), *GetNameSafe(HitActor));
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

	const FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystemComponent);
	}
}
