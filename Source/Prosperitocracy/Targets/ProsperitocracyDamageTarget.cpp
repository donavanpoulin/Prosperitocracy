// Copyright Prosperitocracy. All Rights Reserved.

#include "Targets/ProsperitocracyDamageTarget.h"

#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialInterface.h"
#include "ProsperitocracyLogChannels.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyDamageTarget)

AProsperitocracyDamageTarget::AProsperitocracyDamageTarget()
{
	// The target is a solid, standable test dummy: a cylinder body + a cube weakspot on top.
	ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));
	SetRootComponent(ChestMesh);
	ChestMesh->SetMobility(EComponentMobility::Movable);
	ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChestMesh->SetCollisionObjectType(ECC_WorldStatic);
	ChestMesh->SetCollisionResponseToAllChannels(ECR_Block); // block the weapon trace channel so bullets hit it

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(RootComponent);
	HeadMesh->SetMobility(EComponentMobility::Movable);
	HeadMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	HeadMesh->SetRelativeScale3D(FVector(0.5f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadMesh->SetCollisionObjectType(ECC_WorldStatic);
	HeadMesh->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ChestMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (ChestMeshAsset.Succeeded())
	{
		ChestMesh->SetStaticMesh(ChestMeshAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (HeadMeshAsset.Succeeded())
	{
		HeadMesh->SetStaticMesh(HeadMeshAsset.Object);
	}

	// A simple solid material with a "BaseColor" parameter, so the target can be recolored/flashed
	// at runtime (the engine basic-shape material has no color parameter).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TargetMaterial(TEXT("/Game/Materials/M_DamageTarget.M_DamageTarget"));
	if (TargetMaterial.Succeeded())
	{
		ChestMesh->SetMaterial(0, TargetMaterial.Object);
		HeadMesh->SetMaterial(0, TargetMaterial.Object);
	}

	// Per-part profiles. Head = no armor (always full). Chest = armor 1 (pen 1 -> 50%, pen 2+ -> full).
	HeadProfile.Armor = 0;
	ChestProfile.Armor = 1;

	// Give it a health pool + ability system so the pipeline can actually apply damage.
	AbilitySystemComponent = CreateDefaultSubobject<UProsperitocracyAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false);

	HealthSet = CreateDefaultSubobject<UProsperitocracyHealthSet>(TEXT("HealthSet"));
}

void AProsperitocracyDamageTarget::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void AProsperitocracyDamageTarget::BeginPlay()
{
	Super::BeginPlay();

	ApplyBaseColor();

	if (HealthSet)
	{
		HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
	}
}

void AProsperitocracyDamageTarget::ApplyBaseColor()
{
	// Called once at BeginPlay to create the dynamic material instances and set the base color.
	auto ApplyColor = [this](UStaticMeshComponent* Mesh)
	{
		if (!Mesh) { return; }
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
			DynamicMaterials.Add(MID);
		}
	};

	ApplyColor(ChestMesh);
	ApplyColor(HeadMesh);
}

void AProsperitocracyDamageTarget::ResetFlashColor()
{
	for (UMaterialInstanceDynamic* MID : DynamicMaterials)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
		}
	}
}

void AProsperitocracyDamageTarget::FlashTarget()
{
	// Death feedback uses a longer, brighter flash (reset a short while after).
	for (UMaterialInstanceDynamic* MID : DynamicMaterials)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), BaseColor * 4.0f);
		}
	}

	GetWorldTimerManager().SetTimer(FlashTimerHandle, this, &ThisClass::ResetFlashColor, 0.4f, false);
}

void AProsperitocracyDamageTarget::HandleOutOfHealth(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec, float EffectMagnitude, float OldValue, float NewValue)
{
	UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] %s KILLED (flash + reset to full health)."), *GetName());

	// Death feedback: flash (a bit longer than a hit flash), then reset to full so it stays for testing.
	FlashTarget();

	if (AbilitySystemComponent && HealthSet)
	{
		const float MaxHealth = HealthSet->GetMaxHealth();
		AbilitySystemComponent->ApplyModToAttribute(UProsperitocracyHealthSet::GetMaxHealthAttribute(), EGameplayModOp::Override, MaxHealth);
		AbilitySystemComponent->ApplyModToAttribute(UProsperitocracyHealthSet::GetHealthAttribute(), EGameplayModOp::Override, MaxHealth);
	}
}

UAbilitySystemComponent* AProsperitocracyDamageTarget::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FProsperitocracyDamageProfile AProsperitocracyDamageTarget::GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext) const
{
	// Return the profile of the part that was hit. If we can't tell which part, default to the chest.
	if (const FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(EffectContext))
	{
		if (const FHitResult* HitResult = TypedContext->GetHitResult())
		{
			if (const UPrimitiveComponent* HitComponent = HitResult->GetComponent())
			{
				if (HitComponent == HeadMesh)
				{
					return HeadProfile;
				}
				if (HitComponent == ChestMesh)
				{
					return ChestProfile;
				}
			}
		}
	}
	return ChestProfile;
}
