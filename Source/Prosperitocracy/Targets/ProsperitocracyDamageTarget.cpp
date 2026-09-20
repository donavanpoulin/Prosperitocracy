// Copyright Prosperitocracy. All Rights Reserved.

#include "Targets/ProsperitocracyDamageTarget.h"

#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatusComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialInterface.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Targets/ProsperitocracyBodyPart.h"
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

	// The parts' numbers are NOT here. Each part wears a BLOCK (HeadBlock / ChestBlock) and both of its
	// numbers — its armour and its one resistance or none — are read FINAL off that block's own GAS home.
	// A part left wearing no block answers with no armour and no resistance, which is a real answer.
	// Presence is scope, and a number on this actor would be a second home for one number.

	// Give it a health pool + ability system so the pipeline can actually apply damage.
	AbilitySystemComponent = CreateDefaultSubobject<UProsperitocracyAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false);

	HealthSet = CreateDefaultSubobject<UProsperitocracyHealthSet>(TEXT("HealthSet"));

	// And the one place a STATUS lives on a body. A target can be set alight, so it carries the status
	// component — the same component every body that can burn or be stunned carries, which is why the
	// burn needs no code here and no code in the gun that set it.
	Statuses = CreateDefaultSubobject<UProsperitocracyStatusComponent>(TEXT("Statuses"));
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

	// The body's own numbers first — its health — then the parts, so a body is never standing there with
	// its parts dressed and no health of its own.
	DressBody();
	SpawnParts();

	if (HealthSet)
	{
		HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
	}
}

void AProsperitocracyDamageTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A body that goes away takes its parts' GAS homes with it.
	for (const TObjectPtr<AProsperitocracyBodyPart>& Part : PartHomes)
	{
		if (Part)
		{
			Part->Destroy();
		}
	}
	PartHomes.Reset();

	Super::EndPlay(EndPlayReason);
}

void AProsperitocracyDamageTarget::DressBody()
{
	// An enemy's health is the universal Health row — the same row players and vehicles use — so it is
	// authored in a block and written onto this body's own rows, through the same door the player's
	// baseline block comes in through. Nothing numeric is authored on this actor.
	//
	// Through the one evaluator like everything else: the number is a row, a perk or a debuff can move it,
	// and the death handler reads it back off the attribute rather than off a copy.
	if (!HealthBlock)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Enemy] %s has no body block — it is standing with whatever its health set came up with, not with an authored Health row."), *GetName());
		return;
	}

	UProsperitocracyStatSystemStatics::ApplyBlockBodyRows(AbilitySystemComponent, HealthBlock, /*bBare=*/ false);
}

void AProsperitocracyDamageTarget::SpawnParts()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// One home per part. The BODY names the mesh and hands over the block; the part does the rest, so
	// nothing here knows what a part's numbers are or what they mean.
	const auto AddPart = [this, World](UPrimitiveComponent* PartMesh, UProsperitocracyStatTable* PartBlock)
	{
		if (!PartMesh)
		{
			return;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AProsperitocracyBodyPart* Part = World->SpawnActor<AProsperitocracyBodyPart>(
			AProsperitocracyBodyPart::StaticClass(), GetActorTransform(), SpawnParams);
		if (!Part)
		{
			return;
		}

		Part->InitializePart(PartMesh, PartBlock);
		PartHomes.Add(Part);

		if (!PartBlock)
		{
			// A part with no block is a real state — it answers with no armour and no resistance — but it
			// is usually an unfinished body, so it is said out loud.
			UE_LOG(LogProsperitocracy, Warning, TEXT("[Part] %s wears no block on %s — it answers with no armour and no resistance. Give the part a block."),
				*GetNameSafe(PartMesh), *GetName());
		}
	};

	// The two parts this body has. Head first or chest first does not matter: a part is found by the mesh
	// a hit landed on.
	AddPart(ChestMesh, ChestBlock);
	AddPart(HeadMesh, HeadBlock);
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

FProsperitocracyDamageProfile AProsperitocracyDamageTarget::GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const
{
	// The answer comes off the PART the hit landed on and from nowhere else: each part wears its own
	// block, and reads its armour and its one resistance FINAL off its own GAS home.
	if (const FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(EffectContext))
	{
		if (const FHitResult* HitResult = TypedContext->GetHitResult())
		{
			if (const AProsperitocracyBodyPart* Part = FindPartFor(HitResult->GetComponent()))
			{
				return Part->GetProfile(DamageType);
			}
		}
	}

	// A hit that landed on no part of this body has nothing to be priced against. Said out loud, because
	// every hit a body takes is supposed to land on one of its parts.
	UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] %s was hit where it has no part — answered with no armour and no resistance."), *GetName());
	return FProsperitocracyDamageProfile();
}

float AProsperitocracyDamageTarget::GetBodyResist_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const
{
	// The body AS A WHOLE, for ONE damage type — the question a line with no pen (burn) asks, because
	// such a line strikes no part.
	//
	// The plain mean across this body's parts, all parts weighing the same (the user's rule): a part
	// that does not carry a resistance of this type counts as 0, because a row a part does not carry is
	// not a row it has. Every number is the part's FINAL value off its own home, so a perk or a debuff
	// that moves a part's resistance moves this answer too.
	if (PartHomes.Num() == 0)
	{
		return 0.0f;
	}

	float Total = 0.0f;
	for (const TObjectPtr<AProsperitocracyBodyPart>& Part : PartHomes)
	{
		if (Part)
		{
			Total += Part->GetResistAgainst(DamageType);
		}
	}

	return Total / static_cast<float>(PartHomes.Num());
}

AProsperitocracyBodyPart* AProsperitocracyDamageTarget::FindPartFor(const UPrimitiveComponent* HitComponent) const
{
	if (!HitComponent)
	{
		return nullptr;
	}

	for (const TObjectPtr<AProsperitocracyBodyPart>& Part : PartHomes)
	{
		if (Part && Part->GetPartMesh() == HitComponent)
		{
			return Part;
		}
	}

	return nullptr;
}
