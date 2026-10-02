// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Explosions/ProsperitocracyExplosionStatics.h"

#include "AbilitySystem/Explosions/ProsperitocracyBlastVisual.h"
#include "AbilitySystem/Explosions/ProsperitocracyFireField.h"
#include "AbilitySystem/Explosions/ProsperitocracyFireStatTable.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySourceInterface.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "ProsperitocracyLogChannels.h"
#include "ProsperitocracyGameplayTags.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyExplosionStatics)

float ProsperitocracyExplosionHandling::ComputeAttenuation(const UProsperitocracyStatTable* Block, float DistanceCm, float RangeFinalMeters, float FalloffFinalMeters)
{
	// AN EXPLOSION'S RANGE IS THE WIDTH OF ITS BALL, so the two numbers the ONE falloff formula wants are
	// not the row itself: the damage reaches 0 at the ball's EDGE — half the Range — and the ramp starts
	// halfway out to there. Nothing is authored for either: the rule is the number, and both are worked
	// out here from the FINAL Range the caller just read, so a Range perk moves the ball and the ramp
	// together and no stored copy can go stale.
	if (Block && Block->IsExplosive())
	{
		// The ball's EDGE is half the Range, because the Range stat IS the ball's width: a sphere is built
		// from its radius, and that half is geometry rather than a number anyone carries, stores or shows.
		const float EdgeMeters = RangeFinalMeters * 0.5f;
		return UProsperitocracyStatSystemStatics::ComputeDistanceAttenuation(
			DistanceCm, EdgeMeters, EdgeMeters * FalloffStartsHalfwayOut);
	}

	// Everything else is a gun: its own Falloff row says where its ramp starts and its Range row where
	// the damage hits 0 — the rows as authored, read final. Absent rows = no falloff at all, exactly as
	// the formula has always behaved.
	return UProsperitocracyStatSystemStatics::ComputeDistanceAttenuation(DistanceCm, RangeFinalMeters, FalloffFinalMeters);
}

float ProsperitocracyExplosionHandling::ComputeShakeAttenuation(float DistanceCm, float ShakeRangeMeters)
{
	// The shake's own ramp over its own, wider range: full to halfway out, then 100% → 0% at the end of
	// it (Design/explosions.md). The same one formula and the same one rule as the damage above — only
	// the reach differs, because the shake reaches further than the ball.
	return UProsperitocracyStatSystemStatics::ComputeDistanceAttenuation(
		DistanceCm, ShakeRangeMeters, ShakeRangeMeters * FalloffStartsHalfwayOut);
}

void UProsperitocracyExplosionStatics::DetonateExplosion(IProsperitocracyAbilitySourceInterface* Explosive, AActor* Instigator, const FVector& Place, TSubclassOf<UGameplayEffect> DamageEffectClass)
{
// ---------------------------------------------------------------------------------------------------
// STAGE 1 — THE MARK. A thing's stat block plus the `Explosive` tag on it is the ENTIRE wiring
// (Design/explosions.md), so this is the whole of "is this thing an explosion", asked of the thing.
// ---------------------------------------------------------------------------------------------------
	const UProsperitocracyStatTable* Block = Explosive ? Explosive->GetStatBlock() : nullptr;
	if (!Block || !Block->IsExplosive())
	{
		// Said out loud, because it is an authoring bug and not a missing feature: something tried to
		// go off without the mark, so either the wrong block was handed over or the tag was never put on
		// the right one. A half-run pass here would be a bang nobody could account for.
		const FGameplayTag ExplosiveTag = ProsperitocracyGameplayTags::Explosive;
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Explosion] %s asked to go off, but its block carries no %s mark — there is no bang. An explosion is a thing whose block carries the mark; nothing else is one."),
			*GetNameSafe(Cast<UObject>(Explosive)), *ExplosiveTag.ToString());
		return;
	}

	UWorld* World = Instigator ? Instigator->GetWorld() : nullptr;
	if (!World)
	{
		// The pass needs a world to gather in and a man to credit: an explosion with neither is a bang
		// in a place that does not exist.
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Explosion] %s went off with no world — nothing happened."), *GetNameSafe(Block));
		return;
	}

// ---------------------------------------------------------------------------------------------------
// STAGE 2 — THE BALL IS AS WIDE AS THE THING'S RANGE SAYS, and that stat is the only size there is:
// no radius row, no converted number stored or shown, nothing derived off it that becomes a second size.
// A sphere is built from its radius, so the ball is made at half the Range — geometry, and the same
// halving the falloff read below does for the ball's edge. The Range comes off the thing's own GAS home,
// read FINAL like every other number in this game.
// ---------------------------------------------------------------------------------------------------
	const float RangeMeters = Explosive->GetStatFinalValue(EProsperitocracyStat::Range);
	if (RangeMeters <= 0.0f)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Explosion] %s is an explosion whose Range is 0 — a ball with no size catches nobody and deals to nobody. Give the block its Range; an explosive IS its ball."),
			*GetNameSafe(Block));
		return;
	}

	const float RadiusCm = RangeMeters * 100.0f * 0.5f;

	TArray<FProsperitocracyExplosionTarget> Caught;
	GatherTheBall(World, Place, RadiusCm, Cast<AActor>(Explosive), Caught);

// ---------------------------------------------------------------------------------------------------
// STAGE 3 — THE DAMAGE, through the ONE pipeline, and the statuses that ride a hit that hurt.
//
// The thing's OWN lines travel: the context points at the THING as the ability source, so
// UProsperitocracyDamageExecution reads its Impact/Piercing, its Pen and its falloff off the thing's own
// GAS home — every body answered on its own, with the pen gate, its own resists and the distance the
// ball caught it at. Nothing about the damage is worked out here.
// ---------------------------------------------------------------------------------------------------
	UAbilitySystemComponent* SourceAbilitySystemComponent = Instigator
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator)
		: nullptr;
	if (!SourceAbilitySystemComponent || !DamageEffectClass)
	{
		// The bang happened, but there is nothing to deal it with: an explosion whose caller brought no
		// ability system (or no effect) would silently deal nothing at all, which is a bug worth saying.
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Explosion] %s went off with no %s — it dealt nothing. Wire the caller; this is a bug, not a missing feature."),
			*GetNameSafe(Block), SourceAbilitySystemComponent ? TEXT("damage effect") : TEXT("ability system"));
		return;
	}

	// The thing that went off, named as the damage's CAUSER: it is the thing the damage came out of, in
	// the same breath the firer is the instigator of a shot.
	AActor* EffectCauser = Cast<AActor>(Explosive);

	int32 ShovedCount = 0;
	for (const FProsperitocracyExplosionTarget& Target : Caught)
	{
		// A wall has no ability system: the ball caught it honestly and there is nothing to damage —
		// the same answer a shot's own impact already gets.
		UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target.Actor);
		if (!TargetAbilitySystemComponent)
		{
			continue;
		}

		// THE HIT THE BALL IS. There is no bullet hole on a body in a ball, so the hit is built from the
		// only place a blast touched it: the nearest point to the middle. Its DISTANCE is what the one
		// falloff formula is measured at (a trace would have set exactly this number), and its point is
		// where a damage number comes up. The normal points back at the middle, because that is the
		// surface the blast hit.
		FHitResult Hit(Target.Actor, Target.Part, Target.Point, -Target.Direction);
		Hit.TraceStart = Place;
		Hit.Distance = Target.DistanceCm;

		FGameplayEffectContextHandle Context = SourceAbilitySystemComponent->MakeEffectContext();
		Context.AddInstigator(Instigator, EffectCauser);
		Context.AddHitResult(Hit, /*bReset=*/ true);

		FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context);
		if (!TypedContext)
		{
			UE_LOG(LogProsperitocracy, Warning, TEXT("[Explosion] %s: the effect context is not ours — no lines or falloff will resolve. Check AbilitySystemGlobalsClassName in DefaultGame.ini."), *GetNameSafe(Block));
			continue;
		}

		// The thing's own rows, through the thing's own home — the same door a gun points a shot at.
		TypedContext->SetAbilitySource(Explosive, 1.0f);

		UProsperitocracyDamageStatics::ApplyDamageEffectToHit(Context, Target.Actor, SourceAbilitySystemComponent, DamageEffectClass);

		// And what the thing's block NAMES goes on with the hit, through the same one door a gun's
		// statuses go through: the strike's burn lands only on a body this actually hurt, and each
		// status's numbers are its own block's.
		UProsperitocracyDamageStatics::ApplyEffectsToHit(Context, Target.Actor, SourceAbilitySystemComponent, Block, DamageEffectClass);

// ---------------------------------------------------------------------------------------------------
// STAGE 4 — THE SHOVE. It is the pipeline's OWN number, reused (his rule): what came off that body,
// with the gate and the falloff already inside it, turned into a speed and capped so a big blast cannot
// send you to space. The outer ball does less damage, so it shoves you less far, by construction.
//
// THE BODY DOES THE THROWING: this hands over a speed and the place the ball touched, and a body
// answers by going limp and being thrown by it (Design/explosions.md -> "Bodies thrown"). What a BODY
// is worth is the body's own — its weight damps the throw inside that door — so nothing here is per-body
// and nothing here is per-explosive.
// ---------------------------------------------------------------------------------------------------
		const float DamageThatCameOff = TypedContext->GetLandedDamage();
		const float ShoveSpeed = FMath::Min(DamageThatCameOff * ProsperitocracyExplosionHandling::ShoveSpeedPerDamagePoint,
			ProsperitocracyExplosionHandling::MaxShoveSpeedCmS);
		if (ShoveSpeed > 0.0f)
		{
			if (UProsperitocracyPlayerStatsComponent* BodyStats = Target.Actor->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
			{
				BodyStats->NotifyBlastShove(Target.Direction * ShoveSpeed, Target.Point);
				++ShovedCount;
			}
		}
	}

// ---------------------------------------------------------------------------------------------------
// STAGE 5 — THE SHAKE. It reaches ITS OWN range — the thing's final Range times the universal
// multiplier — over its own ramp, and it goes to everything in that reach whether or not the ball
// touched them, because a bang is heard and felt through the ground as well as by what it hits (his
// correction: "The shake isnt just to apply to those who get hit"). Its strength is the thing's own
// Shake row, derived off the thing's own damage, and what falls off is that strength.
//
// THE SOUND IS PARKED, and its shape is already fixed by this stage: the same reach, the same ramp, used
// as volume — not one blast that quietly gets quieter, which is what he refused. There is no boom asset
// in the project at all (Design/explosions.md), so there is nothing to hook up yet.
// ---------------------------------------------------------------------------------------------------
	const float ShakeDegrees = Explosive->GetStatFinalValue(EProsperitocracyStat::Shake);
	const float ShakeRangeMeters = RangeMeters * ProsperitocracyExplosionHandling::ShakeAndSoundRangeMultiplier;
	if (ShakeDegrees > 0.0f && ShakeRangeMeters > 0.0f)
	{
		TArray<FProsperitocracyExplosionTarget> InTheShake;
		GatherTheBall(World, Place, ShakeRangeMeters * 100.0f, EffectCauser, InTheShake);

		for (const FProsperitocracyExplosionTarget& Target : InTheShake)
		{
			if (UProsperitocracyPlayerStatsComponent* BodyStats = Target.Actor->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
			{
				BodyStats->NotifyViewShake(ShakeDegrees * ProsperitocracyExplosionHandling::ComputeShakeAttenuation(Target.DistanceCm, ShakeRangeMeters));
			}
		}
	}

// ---------------------------------------------------------------------------------------------------
// STAGE 6 — THE LOOK. The bang's picture is the THING'S OWN, named on its block beside its marks and its
// statuses — the same way an armour's colour rows are a look carried on a block. Some explosions are firey,
// some are shrapnel and shatter, and which one a thing is is the thing's answer: the pass puts up whatever
// the block points at and never learns which it is, so a new explosive chooses its bang's look in the same
// place it chooses its numbers.
//
// It is sized from the thing's own Range — what it reads FINAL against what its block ships, with the look
// taking half of that growth (that rule lives in AProsperitocracyBlastVisual, where the picture is).
// ---------------------------------------------------------------------------------------------------
if (Block->BlastVisual)
{
	FActorSpawnParameters VisualParams;
	VisualParams.Owner = Instigator;
	VisualParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProsperitocracyBlastVisual* Visual = World->SpawnActor<AProsperitocracyBlastVisual>(
		Block->BlastVisual, Place, FRotator::ZeroRotator, VisualParams);
	if (Visual)
	{
		// The thing's Range FINAL is what the picture is sized from; the block's authored Range is the
		// reference it grows against — the same shape the blade's combo uses to read a rate against its base.
		Visual->ShowTheBang(RangeMeters, Block->GetBaseValue(EProsperitocracyStat::Range));
	}
	else
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Blast] %s names a bang's look that could not be spawned — the explosion happened with nothing to see."),
			*GetNameSafe(Block));
	}
}

// ---------------------------------------------------------------------------------------------------
// STAGE 7 — THE FIRE IT LEAVES. A bang leaves fire on the ground by NAMING a fire on its block, in the
// same breath as naming its look — so a firey explosive burns the ground and a shrapnel one does not, and
// neither needs a line of code. Everything a fire then does is the fire's own block's business: its
// circle, its clock, the Burn it applies and what it looks like (PLANS/incendiary-strike.md §4).
//
// A thing with no fire named here is not a thing that failed to leave one: presence is scope, exactly as
// it is for a mark, a status or a bang's look.
//
// ASKED OF THE PATH, NEVER OF THE OBJECT (2026-09-30, and it cost a whole play test): `if (Block->Fire)`
// reads `TSoftObjectPtr::operator bool`, which is `IsValid()` — "is the object LOADED" — not "is a fire
// named". Nothing loads a fire block until something asks for it, so that test was false for a fire that
// was named all along and the whole stage was skipped WITHOUT ONE WORD IN THE LOG, because a thing that
// names no fire is silent by design. Presence is `IsNull()` — the path — and the load is the next step,
// where a path that cannot be loaded says so out loud.
// ---------------------------------------------------------------------------------------------------
	if (!Block->Fire.IsNull())
	{
		UProsperitocracyFireStatTable* FireBlock = Block->Fire.LoadSynchronous();
		if (FireBlock)
		{
			FActorSpawnParameters FireParams;
			FireParams.Owner = Instigator;
			FireParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AProsperitocracyFireField* TheFire = World->SpawnActor<AProsperitocracyFireField>(
				AProsperitocracyFireField::StaticClass(), Place, FRotator::ZeroRotator, FireParams);
			if (TheFire)
			{
				TheFire->LightTheGround(FireBlock, Instigator, DamageEffectClass);
			}
			else
			{
				UE_LOG(LogProsperitocracy, Warning,
					TEXT("[Fire] %s names a fire that could not be spawned — the bang left no ground burning."),
					*GetNameSafe(Block));
			}
		}
		else
		{
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("[Fire] %s names a fire block that could not be loaded — the bang left no ground burning."),
				*GetNameSafe(Block));
		}
	}

	// One line per bang, and it lists what the ball caught: a horde in the ball is how a blast is read
	// from outside the game, since one ball can catch many bodies at once.
	FString CaughtNames;
	for (const FProsperitocracyExplosionTarget& Target : Caught)
	{
		CaughtNames += FString::Printf(TEXT("%s%s (%.1fm)"), CaughtNames.IsEmpty() ? TEXT("") : TEXT(", "),
			*GetNameSafe(Target.Actor), Target.DistanceCm / 100.0f);
	}

	UE_LOG(LogProsperitocracy, Log, TEXT("[Explosion] %s went off at %s — the ball is %.1fm wide and it caught %d %s: %s. %d of them were shoved."),
		*GetNameSafe(Block), *Place.ToCompactString(), RangeMeters, Caught.Num(),
		Caught.Num() == 1 ? TEXT("body") : TEXT("bodies"),
		CaughtNames.IsEmpty() ? TEXT("nothing") : *CaughtNames, ShovedCount);
}

void UProsperitocracyExplosionStatics::GatherTheBall(UWorld* World, const FVector& Place, float RadiusCm, const AActor* TheExplosiveItself, TArray<FProsperitocracyExplosionTarget>& OutCaught)
{
	OutCaught.Reset();

	if (!World || RadiusCm <= 0.0f)
	{
		return;
	}

	// ASKED BY OBJECT TYPE, NEVER BY A CHANNEL — and that difference is the whole reason a blast could catch
	// the floor and the test dummies and never the man standing in the middle of it. A channel is answered by
	// whoever BLOCKS it, and a player's own capsule answers Visibility with IGNORE (it ships as the "Pawn"
	// profile), so a sphere asked on that channel is blind to every player in the game while the floor and a
	// dummy answer Block and are caught. A blast wants BODIES and the world, so it asks for the object types
	// they are: what a thing IS, not how it responds to somebody else's question.
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectTypes.AddObjectTypesToQuery(ECC_Destructible);

	// The ignore list is built BEFORE the query: the thing that went off is never its own target.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExplosionBall), /*bTraceComplex=*/ false);

	// THE THING THAT WENT OFF IS NEVER ITS OWN TARGET: a grenade's own body and its own ability system
	// are not things its blast catches. Everything else in the ball IS — the man who called it in
	// included, because an explosion is friendly fire like everything else here (Design/explosions.md).
	if (TheExplosiveItself)
	{
		Params.AddIgnoredActor(TheExplosiveItself);
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Place, FQuat::Identity, ObjectTypes, FCollisionShape::MakeSphere(RadiusCm), Params);

	// ONE ENTRY PER BODY. A body is many parts and an overlap answers per part, so the parts are folded
	// into the body they belong to and the nearest part to the middle is the one that stands for it.
	TMap<AActor*, int32> IndexOfBody;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		UPrimitiveComponent* Part = Overlap.GetComponent();
		if (!Actor || !Part)
		{
			continue;
		}

		// Where the ball actually touched this part — the nearest point on it to the middle, and how far
		// away that is. An overlap answers "you are inside the sphere"; this answers WHERE, and that
		// point is what the falloff is measured at and what the body is shoved away from. It is also the
		// honest answer to "which part did the blast catch" on a body built from parts.
		FVector PointOnIt = Place;
		const float DistanceCm = Part->GetClosestPointOnCollision(Place, PointOnIt);
		if (DistanceCm < 0.0f)
		{
			// No collision to measure against: nothing here to catch.
			continue;
		}

		const int32* Existing = IndexOfBody.Find(Actor);
		if (Existing && OutCaught[*Existing].DistanceCm <= DistanceCm)
		{
			// This body is already in, caught by a part nearer the middle. A body is caught once.
			continue;
		}

		FProsperitocracyExplosionTarget& Target = Existing ? OutCaught[*Existing] : OutCaught.AddDefaulted_GetRef();
		if (!Existing)
		{
			IndexOfBody.Add(Actor, OutCaught.Num() - 1);
		}

		Target.Actor = Actor;
		Target.Part = Part;
		Target.Point = PointOnIt;
		Target.DistanceCm = DistanceCm;

		// The direction it was HIT from: off the ball's surface at the target, and never flatly out from
		// the middle — caught at the side of the ball it is shoved sideways, caught above the middle it
		// is shoved up and out (Design/explosions.md).
		Target.Direction = (PointOnIt - Place).GetSafeNormal();

		// A body standing exactly in the middle has no surface to be shoved off, and it must still be
		// thrown somewhere rather than being the one thing a blast leaves standing: straight up, off the
		// ball's own shape rather than out of a direction nobody could name.
		if (Target.Direction.IsNearlyZero())
		{
			Target.Direction = ((Actor->GetActorLocation() - Place).GetSafeNormal());
			if (Target.Direction.IsNearlyZero())
			{
				Target.Direction = FVector::UpVector;
			}
		}
	}
}
