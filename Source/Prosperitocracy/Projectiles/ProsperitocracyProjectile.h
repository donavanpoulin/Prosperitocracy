// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"

#include "ProsperitocracyProjectile.generated.h"

class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * What a projectile is, in numbers shared by EVERY projectile (Design/explosions.md, the plan's strike).
 *
 * Universal means universal, and all of it `[TUNE]`: a projectile's own body, how fast it comes, and how
 * long one that hits nothing is allowed to live. Nothing here is per-strike config — a strike's own
 * numbers are its block's, and the one thing the plan leaves open ("where a strike enters from — how high
 * and from what direction — is not decided") is the entry below.
 */
namespace ProsperitocracyProjectileHandling
{
	/** How big the projectile's own BODY is (not its blast — the blast is the block's Range). `[TUNE]` */
	constexpr float HullRadiusCm = 25.0f;

	/** A strike that hits nothing at all gives up after this long, so nothing flies forever. `[TUNE]` */
	constexpr float StrikeLifeSeconds = 10.0f;
}

/**
 * AProsperitocracyProjectile
 *
 * A real projectile (Design/explosions.md, the plan): it flies, and it hits whatever it lands on — floor,
 * wall, enemy, prop. It is never placed on the ground.
 *
 * It IS the thing that goes off, so it is a thing's GAS home (AProsperitocracyStatHostActor): the block
 * handed to it answers the explosion mark, its damage lines, its Pen, its Range (the ball's width) and the
 * falloff, all through the ONE evaluator. Nothing about the bang is decided here — this class owns WHEN,
 * and the pass owns what happens then.
 */
UCLASS()
class AProsperitocracyProjectile : public AProsperitocracyStatHostActor
{
	GENERATED_BODY()

public:
	AProsperitocracyProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * SEND IT AT A PLACE.
	 *
	 * LandingPlace — the point it is aimed at, on the ground or on whatever is standing there.
	 * InInstigator — whose strike it is: the bang is credited to him and the shake is his screen's.
	 * InDamageEffectClass — the one effect the damage pipeline runs on, handed over by whoever called it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Projectile")
	void LaunchAt(const FVector& LandingPlace, AActor* InInstigator, TSubclassOf<UGameplayEffect> InDamageEffectClass);

	/**
	 * WHERE IT COMES IN FROM — how high, how far back along its own line, and how fast, in metres.
	 *
	 * Every strike draws its own azimuth and comes in from the same height and distance, so it arrives at its
	 * own angle at the place it was sent to. These are numbers of FEEL and they live here so they can be moved
	 * without a rebuild: higher only ever means the strike arrives out of view instead of appearing in it.
	 * `[TUNE]`
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Projectile")
	float EntryHeightMeters = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Projectile")
	float EntryDistanceMeters = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Projectile")
	float FlightSpeedMetersPerSecond = 60.0f;

protected:
	/**
	 * IT HIT SOMETHING — a projectile's own "now", and the whole of its trigger.
	 *
	 * The design gives the pass no trigger of its own and this is why: a thing owns when it goes off, and a
	 * projectile's when is the moment it lands on something. A grenade's fuze and a barrel's death are the
	 * same shape with a different when, and none of them touch the pass.
	 */
	UFUNCTION()
	void OnItHitSomething(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	/** The one case a projectile needs a clock of its own for: it hits nothing, and gives up saying so. */
	void GiveUpWithoutABang();

	/** The projectile's own body: the thing that flies and the thing that hits. */
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Projectile")
	TObjectPtr<USphereComponent> Hull;

	/**
	 * The model. THE MESH AND ITS SCALE ARE THE BLUEPRINT'S, never set here — the cruise missile ships big
	 * and this is not a massive strike, so a strike uses it at a quarter of its size (the plan's 75%
	 * smaller). A mesh is a look, and a look belongs on the asset that carries it.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Projectile")
	TObjectPtr<UStaticMeshComponent> Missile;

	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Projectile")
	TObjectPtr<UProjectileMovementComponent> Flight;

	/** Who called it: the bang is credited to him and the shake lands on his screen. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> TheOneWhoCalledIt;

	/** The one effect the damage pipeline runs on, handed over on the launch. */
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

private:
	/** One thing goes off once, however many things it hits in the frame it lands. */
	bool bGoneOff = false;

	/** The clock that gives up on a strike that hits nothing. */
	FTimerHandle GiveUpTimerHandle;
};
