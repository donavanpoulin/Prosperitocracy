// Copyright Prosperitocracy. All Rights Reserved.

#include "Projectiles/ProsperitocracyProjectile.h"

#include "AbilitySystem/Explosions/ProsperitocracyExplosionStatics.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "ProsperitocracyLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyProjectile)

AProsperitocracyProjectile::AProsperitocracyProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// This actor IS the thing that goes off: the stat host's own ASC is already here (see the base), so its
	// block answers the mark, the damage lines, the Pen, the Range and the falloff — the explosion pass asks
	// IT and nothing else. Nothing about the bang is decided in this class.
	PrimaryActorTick.bCanEverTick = false;

	Hull = CreateDefaultSubobject<USphereComponent>(TEXT("Hull"));
	SetRootComponent(Hull);
	Hull->InitSphereRadius(ProsperitocracyProjectileHandling::HullRadiusCm);
	Hull->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Hull->SetNotifyRigidBodyCollision(true);
	Hull->OnComponentHit.AddDynamic(this, &AProsperitocracyProjectile::OnItHitSomething);

	Missile = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Missile"));
	Missile->SetupAttachment(Hull);
	// The model is the BLUEPRINT's (the plan names it, and the 75%-smaller scale is part of the look): this
	// component only says where the look hangs, never which look it is. It never collides — the Hull does
	// that, so the thing that hits and the thing you see can never disagree.
	Missile->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Flight = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Flight"));
	Flight->SetUpdatedComponent(Hull);
	// A POWERED missile, not a lobbed rock: it flies AT the place it was sent to, so there is no ballistics
	// to solve and no arcing to tune — where it is aimed is where it lands.
	Flight->ProjectileGravityScale = 0.0f;
	Flight->bRotationFollowsVelocity = true;
	Flight->bInitialVelocityInLocalSpace = false;
	// Nothing moves it until a launch says so.
	Flight->InitialSpeed = 0.0f;
	// A powered missile: its ceiling is the speed it is sent at, read off the property below when it launches.
	Flight->MaxSpeed = FlightSpeedMetersPerSecond * 100.0f;
}

void AProsperitocracyProjectile::LaunchAt(const FVector& LandingPlace, AActor* InInstigator, TSubclassOf<UGameplayEffect> InDamageEffectClass)
{
	TheOneWhoCalledIt = InInstigator;
	DamageEffectClass = InDamageEffectClass;

	// ITS OWN ANGLE: the azimuth is drawn per strike, so two strikes on one place never come in the same
	// way, and every strike arrives at the place it was sent to. Only the LANDING POINT has to be inside the
	// ability's circle — where it came in from is nobody's business but its own.
	const FVector Approach = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector();
	const FVector Start = LandingPlace
		+ (Approach * EntryDistanceMeters * 100.0f)
		+ (FVector::UpVector * EntryHeightMeters * 100.0f);

	SetActorLocation(Start);
	SetActorRotation((LandingPlace - Start).Rotation());

	if (Flight)
	{
		// The ceiling is read again here, off the property as the thing is authored — a blueprint that sends
		// its strikes faster is not capped by the class's own default.
		Flight->MaxSpeed = FlightSpeedMetersPerSecond * 100.0f;
		Flight->Velocity = (LandingPlace - Start).GetSafeNormal() * FlightSpeedMetersPerSecond * 100.0f;
	}

	// WHAT IS ACTUALLY IN ITS WAY, said once per strike as it leaves: whether this body can collide at all,
	// and whether the world between the entry and the landing stops a trace — asked on BOTH channels, because
	// the call's own placement is found with a Visibility trace while a projectile's body sweeps on its own
	// object channel, and a surface that answers one and not the other is a strike called onto nothing.
	{
		UWorld* World = GetWorld();
		FHitResult OnVisibility;
		FHitResult OnObjectChannel;
		FCollisionQueryParams ProbeParams(SCENE_QUERY_STAT(StrikeLaunchProbe), /*bTraceComplex=*/ false, this);

		// Past the landing by three metres, not stopping on it: a surface exactly at the end of a trace is not
		// reported, and "nothing there" would be a lie about a floor the landing sits on.
		const FVector Past = LandingPlace - (FVector::UpVector * 300.0f);
		const bool bVisibility = World && World->LineTraceSingleByChannel(OnVisibility, Start, Past, ECC_Visibility, ProbeParams);
		const bool bObject = World && World->LineTraceSingleByChannel(OnObjectChannel, Start, Past, ECC_WorldDynamic, ProbeParams);

		UE_LOG(LogProsperitocracy, Log,
			TEXT("[Strike] %s: hull collision %s, actor collision %s — down to %s, Visibility %s, WorldDynamic %s."),
			*GetName(),
			(Hull && Hull->IsCollisionEnabled()) ? TEXT("on") : TEXT("OFF"),
			GetActorEnableCollision() ? TEXT("on") : TEXT("OFF"),
			*LandingPlace.ToCompactString(),
			bVisibility ? *FString::Printf(TEXT("hits at %s"), *FVector(OnVisibility.ImpactPoint).ToCompactString()) : TEXT("hits nothing"),
			bObject ? *FString::Printf(TEXT("hits at %s"), *FVector(OnObjectChannel.ImpactPoint).ToCompactString()) : TEXT("hits nothing"));
	}

	// A strike that hits NOTHING is the one case that would otherwise fly forever: it gives up after its own
	// stretch of time, and it says so, because a strike vanishing without a bang is a thing to be able to
	// read. [TUNE]
	GetWorldTimerManager().SetTimer(GiveUpTimerHandle, this, &AProsperitocracyProjectile::GiveUpWithoutABang,
		ProsperitocracyProjectileHandling::StrikeLifeSeconds, /*bLoop=*/ false);

	UE_LOG(LogProsperitocracy, Log, TEXT("[Strike] %s is on its way to %s from %s (%.0f m/s)"),
		*GetName(), *LandingPlace.ToCompactString(), *Start.ToCompactString(), FlightSpeedMetersPerSecond);
}

void AProsperitocracyProjectile::GiveUpWithoutABang()
{
	if (bGoneOff)
	{
		return;
	}

	bGoneOff = true;

	UE_LOG(LogProsperitocracy, Warning, TEXT("[Strike] %s flew its whole life without hitting anything and gave up at %s — a strike that hits nothing never goes off."),
		*GetName(), *GetActorLocation().ToCompactString());

	Destroy();
}

void AProsperitocracyProjectile::OnItHitSomething(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	// ONE THING GOES OFF ONCE, however many things it touches in the frame it lands.
	if (bGoneOff)
	{
		return;
	}
	bGoneOff = true;

	// The blast is centred where the body actually touched — a strike landing on a body explodes on the
	// body, one landing on a wall explodes on the wall. A hit with no usable point falls back to the body's
	// own origin rather than exploding at the world origin.
	const FVector Place = Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint);

	// WHEN IS MINE; WHAT HAPPENS IS THE PASS'S. This is the whole trigger: a projectile's "now" is the
	// moment it hits something, and the bang is handed over unsaid — the pass asks this thing's own block
	// whether it is an explosion at all, answers for itself, and puts up the look that block names.
	UProsperitocracyExplosionStatics::DetonateExplosion(this, TheOneWhoCalledIt, Place, DamageEffectClass);

	// And then it is gone — AFTER the bang, because the pass resolves the damage and the falloff off this
	// thing's own home and a dead source answers nothing.
	Destroy();
}
