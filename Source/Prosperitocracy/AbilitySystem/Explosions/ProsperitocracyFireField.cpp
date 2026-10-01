// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/Explosions/ProsperitocracyFireField.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/Explosions/ProsperitocracyExplosionStatics.h"
#include "AbilitySystem/Explosions/ProsperitocracyFireStatTable.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyFireField)

TArray<TWeakObjectPtr<AProsperitocracyFireField>> AProsperitocracyFireField::EveryFireBurning;
int32 AProsperitocracyFireField::HowManyFiresHaveBeenLaid = 0;

// THE TWO NUMBERS THE EFFECT IS FED, by the names its own user variables carry. One fire, one width and
// one rate: the width is handed over once when the ground is lit, the rate is handed over again whenever
// it really moves, and between them they are the whole of what our code has to say about the look.
namespace ProsperitocracyFireVariables
{
	static const FName TheWidth(TEXT("User.PatchRadius"));
	static const FName TheRate(TEXT("User.FlameRate"));
}

AProsperitocracyFireField::AProsperitocracyFireField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// A fire is a place on the ground, not an obstacle: the CIRCLE catches bodies, and this actor is
	// only the home the fire hangs on. Nothing bumps into it.
	TheRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TheRoot"));
	RootComponent = TheRoot;
	SetActorEnableCollision(false);
}

void AProsperitocracyFireField::LightTheGround(UProsperitocracyFireStatTable* InFireBlock,
	AActor* TheOneWhoLeftIt_, TSubclassOf<UGameplayEffect> InDamageEffectClass)
{
	UWorld* World = GetWorld();
	if (!World || !InFireBlock)
	{
		return;
	}

	FireBlock = InFireBlock;
	TheOneWhoLeftIt = TheOneWhoLeftIt_;
	DamageEffectClass = InDamageEffectClass;

	// THE FIRE'S OWN HOME. Its rows are read FINAL through the one aggregator, exactly as a gun's or a
	// status's are, so a perk or a buff that ever reaches a fire moves its circle and its clock with it.
	MyOwnHome = World->SpawnActor<AProsperitocracyStatHostActor>();
	if (MyOwnHome)
	{
		MyOwnHome->InitializeFromStatBlock(FireBlock);
	}

	DurationSeconds = GetMyStat(EProsperitocracyStat::Duration);
	const float RangeMeters = GetMyStat(EProsperitocracyStat::Range);
	if (DurationSeconds <= 0.0f || RangeMeters <= 0.0f)
	{
		// An authoring bug, and said out loud as one: a fire with no clock or no circle is a fire that
		// burns nothing. Half of it running would be a fire nobody could account for.
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s was left with no %s in its block — nothing is burning. A fire IS its circle and its clock."),
			*GetNameSafe(FireBlock), DurationSeconds <= 0.0f ? TEXT("Duration") : TEXT("Range"));
		TheClockIsOut();
		return;
	}

	// THE CIRCLE IS THE FIRE: its Range is the WIDTH of that circle, like every other Range in this
	// feature, and a circle is built from its radius — geometry, not a second number anybody carries.
	RadiusCm = RangeMeters * 100.0f * 0.5f;
	EndsAt = World->GetTimeSeconds() + DurationSeconds;
	NextCatchesAt = World->GetTimeSeconds();

	EveryFireBurning.Add(this);

	// THIS FIRE'S PLACE IN THE QUEUE, so "newer" is a number and never a guess: a fire laid later owns
	// the ground it covers, and every fire here knows which fires came after it.
	LaidOrder = ++HowManyFiresHaveBeenLaid;

	LayTheFire();

	// A NEW FIRE HAS TAKEN SOMEONE'S GROUND. Every fire burning reads its own share back — this one too,
	// though nothing newer exists yet — so the ground that changed hands stops being asked for twice.
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		if (AProsperitocracyFireField* Fire = EveryFireBurning[Index].Get())
		{
			Fire->ReReadHowMuchGroundItStillOwns();
		}
	}

	UE_LOG(LogProsperitocracy, Log,
		TEXT("[Fire] fire laid at %s — %.1fm across, asking for %.0f flames a second (about %.0f alight), %.1fs on the clock, and the ground catches for all of it."),
		*GetActorLocation().ToCompactString(), RangeMeters, BaseFlameRate,
		BaseFlameRate * 0.8f, DurationSeconds);
}

float AProsperitocracyFireField::TheGroundTwoFiresShare(float OneRadiusCm, float OtherRadiusCm,
	const FVector& OneCentre, const FVector& OtherCentre)
{
	// TWO CIRCLES ON THE GROUND, and the ground they have in common — the textbook figure: the two
	// circular segments, minus the triangle their chord corners make. Flat, because a fire on the ground
	// is flat: a body overhead is not standing in it either (see CatchWhateverStandsInIt).
	const float Distance = FVector::Dist2D(OneCentre, OtherCentre);
	if (Distance >= OneRadiusCm + OtherRadiusCm)
	{
		return 0.0f;
	}

	// One circle entirely inside the other: the whole of the smaller one changed hands.
	if (Distance <= FMath::Abs(OneRadiusCm - OtherRadiusCm))
	{
		const float Smaller = FMath::Min(OneRadiusCm, OtherRadiusCm);
		return PI * Smaller * Smaller;
	}

	const float OneSquared = OneRadiusCm * OneRadiusCm;
	const float OtherSquared = OtherRadiusCm * OtherRadiusCm;
	const float DistanceSquared = Distance * Distance;

	const float OneAngle = FMath::Acos((DistanceSquared + OneSquared - OtherSquared) / (2.0f * Distance * OneRadiusCm));
	const float OtherAngle = FMath::Acos((DistanceSquared + OtherSquared - OneSquared) / (2.0f * Distance * OtherRadiusCm));

	const float Chord = FMath::Sqrt(FMath::Max(0.0f,
		(-Distance + OneRadiusCm + OtherRadiusCm) * (Distance + OneRadiusCm - OtherRadiusCm)
		* (Distance - OneRadiusCm + OtherRadiusCm) * (Distance + OneRadiusCm + OtherRadiusCm)));

	return OneSquared * OneAngle + OtherSquared * OtherAngle - 0.5f * Chord;
}

void AProsperitocracyFireField::ReReadHowMuchGroundItStillOwns()
{
	if (RadiusCm <= 0.0f)
	{
		return;
	}

	// THE NEWER FIRES OWN THE GROUND THEY COVER, and only that ground: a circle that takes another's
	// corner takes the corner, never the whole circle. What this fire has left is what it is still
	// asking for fire over — so nothing is replaced, nothing pops, and the ground that did not change
	// hands keeps burning exactly as it was (his call, 2026-10-01).
	const float OwnGround = PI * RadiusCm * RadiusCm;
	float Taken = 0.0f;
	for (const TWeakObjectPtr<AProsperitocracyFireField>& OtherWeak : EveryFireBurning)
	{
		const AProsperitocracyFireField* Other = OtherWeak.Get();
		if (!Other || Other == this || Other->LaidOrder <= LaidOrder || Other->RadiusCm <= 0.0f)
		{
			continue;
		}

		Taken += TheGroundTwoFiresShare(RadiusCm, Other->RadiusCm, GetActorLocation(), Other->GetActorLocation());
	}

	const float ShareWas = ShareOfItsOwnGround;
	ShareOfItsOwnGround = FMath::Clamp(1.0f - Taken / OwnGround, 0.0f, 1.0f);

	// Only when it really moved: this is called on every fire laid and every fire gone, and a fire whose
	// ground nobody touched has nothing to say.
	if (!FMath::IsNearlyEqual(ShareWas, ShareOfItsOwnGround, 0.02f))
	{
		UE_LOG(LogProsperitocracy, Log,
			TEXT("[Fire] a newer fire took this ground over — this fire still owns %.0f%% of its circle, so it thins itself to %.0f flames a second and no ground is asked for twice. Nothing was replaced."),
			ShareOfItsOwnGround * 100.0f, BaseFlameRate * ShareOfItsOwnGround);
	}
}

void AProsperitocracyFireField::LayTheFire()
{
	UWorld* World = GetWorld();
	if (!World || !FireBlock || RadiusCm <= 0.0f || bTheFireIsLaid)
	{
		return;
	}
	bTheFireIsLaid = true;

	// WHAT A FIRE LOOKS LIKE IS ITS OWN DATA, asked of the PATH and never of the loaded object — a
	// question a soft pointer answers "is it loaded", which is not the same question (2026-09-30, and it
	// cost a whole play test). A fire that names no effect says so out loud; the circle, the Burn and the
	// clock run regardless, because a fire that hurts you and shows nothing is still a fire.
	UNiagaraSystem* TheLook = nullptr;
	if (!FireBlock->FireLook.IsNull())
	{
		TheLook = FireBlock->FireLook.LoadSynchronous();
		if (!TheLook)
		{
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("[Fire] %s names %s as its fire and it could not be loaded — the ground burns with nothing to see."),
				*GetNameSafe(FireBlock), *FireBlock->FireLook.ToString());
		}
	}
	else
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("[Fire] %s names no fire effect — the ground is burning with nothing to see. The circle, the Burn and the clock are all running."),
			*GetNameSafe(FireBlock));
	}

	// WHAT THE FIRE LIES ON. A bang happens at the surface it hit, and the fire lies on that surface,
	// flat against it — the floor today; a wall's own patch is a later thing because a flat circle is
	// what the ground is. Asked by OBJECT TYPE and never by a channel, for the same reason the ball is.
	FVector GroundPoint = GetActorLocation();
	FVector SurfaceNormal = FVector::UpVector;
	{
		// [TUNE] MINE — how far the search looks, up and down from where the bang happened.
		const float SearchUpCm = 200.0f;
		const float SearchDownCm = 2000.0f;

		FCollisionObjectQueryParams ObjectTypes;
		ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(FireOnTheGround), /*bTraceComplex=*/ false);
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActor(TheOneWhoLeftIt);

		FHitResult Ground;
		if (World->LineTraceSingleByObjectType(Ground, GetActorLocation() + FVector::UpVector * SearchUpCm,
			GetActorLocation() - FVector::UpVector * SearchDownCm, ObjectTypes, Params))
		{
			GroundPoint = Ground.ImpactPoint;
			SurfaceNormal = Ground.ImpactNormal;
		}
		else
		{
			// A bang with no ground under it leaves nothing burning, and says so: a fire laid in mid-air
			// would be a circle on nothing that still lights people up.
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("[Fire] %s had no ground under it at %s — nothing caught fire."),
				*GetNameSafe(FireBlock), *GetActorLocation().ToCompactString());
			return;
		}
	}

	const FRotator SurfaceRotation = FRotationMatrix::MakeFromZ(SurfaceNormal).Rotator();

	// HOW MUCH FIRE THIS GROUND WANTS — the whole of it, until the burning down begins. The block's own
	// density does the sizing: flames a second per square metre, so a wider circle asks for more fire off
	// one number and nothing anywhere authors "how many". Two ceilings then hold it — the fire's own, and
	// what the world is already burning — and past either the ground is covered THINNER, because the look
	// is what gives under load and never the fire.
	{
		const float RadiusMeters = RadiusCm / 100.0f;
		const float TheWholeCircle = PI * RadiusMeters * RadiusMeters;
		const float WidthOfTheBlock = (float)FMath::Max(1, FireBlock->MostFlamesASecond);

		float Wanted = FireBlock->FlamesASquareMetreASecond * TheWholeCircle;

		// What every OTHER fire is already asking for, and what is left of the world's ceiling after it.
		const float OthersAreBurning = FMath::Max(0.0f, FlamesASecondBurningInTheWorld() - BaseFlameRate);
		const float RoomInTheWorld = FMath::Max(0.0f, MostFlamesASecondInTheWholeWorld - OthersAreBurning);

		BaseFlameRate = FMath::Clamp(Wanted, 0.0f, FMath::Min(WidthOfTheBlock, RoomInTheWorld));
		if (BaseFlameRate < Wanted)
		{
			BaseFlameRate = FMath::Max(BaseFlameRate, FewestFlamesASecondAFireMayAsk);
			UE_LOG(LogProsperitocracy, Log,
				TEXT("[Fire] this ground asked for %.0f flames a second and gets %.0f — the fire is laid thinner, and nothing else about it changes."),
				Wanted, BaseFlameRate);
		}
	}

	if (!TheLook)
	{
		return;
	}

	// THE FADE IS THE LAST PART OF THE DURATION AND INSIDE IT (his rule): the fire starts thinning itself
	// here and is asking for nothing by the time its clock runs out.
	const float FadeSeconds = FMath::Clamp(FireBlock->FadeSeconds, 0.0f, DurationSeconds);
	FadeStartsAt = EndsAt - FadeSeconds;

	// ONE PICTURE FOR THE WHOLE PATCH, placed on the surface and sunk into it along its own normal — the
	// effect's origin is not its middle, so this is what puts the flames IN the ground instead of
	// floating over it. It turns with the surface, so a slope's fire lies on the slope.
	TheFire = NewObject<UNiagaraComponent>(this);
	TheFire->SetAsset(TheLook);
	TheFire->SetupAttachment(TheRoot);
	TheFire->RegisterComponent();
	TheFire->SetWorldLocationAndRotation(GroundPoint - SurfaceNormal * FireBlock->SinkCm, SurfaceRotation);

	// The FIRE says when it stops, never the effect: this is a fire on the ground, and it burns down when
	// the block's clock says so.
	TheFire->SetAutoDestroy(false);

	// THE TWO NUMBERS, handed over before the first particle is spawned: how wide the circle is, and how
	// much fire the ground is asking for. Everything else about the look is the effect's own business.
	TheFire->SetVariableFloat(ProsperitocracyFireVariables::TheWidth, RadiusCm);
	TheFire->SetVariableFloat(ProsperitocracyFireVariables::TheRate, BaseFlameRate);
	RateSent = BaseFlameRate;

	TheFire->Activate(true);
}

float AProsperitocracyFireField::TheRateThisGroundWants(float Now) const
{
	// THE GROUND THIS FIRE STILL OWNS IS WHAT IT ASKS FOR. Flames over ground a newer fire has taken
	// would be the same ground burning twice, which is what an overlap used to cost — so the rate is
	// this fire's own, scaled to its share, and a fire whose ground nobody took asks for all of it.
	const float ForTheGroundItOwns = BaseFlameRate * ShareOfItsOwnGround;

	// Before the fade it is that, unchanging. Through the fade it is less and less fire, reaching nothing
	// exactly as the clock runs out — the fire thinning ITSELF, so the ground goes dark as one fire
	// rather than as a field of little fires stopping one at a time. Nothing about who gets lit reads
	// this: the ground catches people the whole way down, because the fade is inside the Duration (his
	// rule).
	const float FadeSeconds = EndsAt - FadeStartsAt;
	if (FadeSeconds <= 0.0f || Now <= FadeStartsAt)
	{
		return ForTheGroundItOwns;
	}

	const float BurnedDown = FMath::Clamp((Now - FadeStartsAt) / FadeSeconds, 0.0f, 1.0f);
	return ForTheGroundItOwns * (1.0f - BurnedDown);
}

void AProsperitocracyFireField::SetTheRate(float FlamesASecond)
{
	if (!TheFire)
	{
		return;
	}

	// A rate that has barely moved is not worth a write: the effect keeps what it has and nothing is
	// pushed at it every frame for the sake of a number nobody could see change.
	const float WorthSending = FMath::Max(1.0f, FMath::Abs(RateSent) * RateChangeWorthSending);
	if (RateSent >= 0.0f && FMath::Abs(FlamesASecond - RateSent) < WorthSending)
	{
		return;
	}

	TheFire->SetVariableFloat(ProsperitocracyFireVariables::TheRate, FlamesASecond);
	RateSent = FlamesASecond;
}

void AProsperitocracyFireField::CatchWhateverStandsInIt()
{
	UWorld* World = GetWorld();
	if (!World || !FireBlock || !TheOneWhoLeftIt)
	{
		return;
	}

	// The Burn's ticks are dealt by whoever left the fire, so a fire that kills contests health and shows
	// a number exactly as his own shot would. His ability system gone means nothing to credit: no burn.
	UAbilitySystemComponent* Source = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TheOneWhoLeftIt);
	if (!Source)
	{
		return;
	}

	// WHO IS STANDING IN IT. The body-folding is the ball's, reused rather than written a second time:
	// an overlap answers per PART and a fire catches a BODY once, whoever it belongs to.
	TArray<FProsperitocracyExplosionTarget> Standing;
	UProsperitocracyExplosionStatics::GatherTheBall(World, GetActorLocation(), RadiusCm, this, Standing);

	const FVector Middle = GetActorLocation();
	for (const FProsperitocracyExplosionTarget& Target : Standing)
	{
		if (!Target.Actor)
		{
			continue;
		}

		// "IN IT" IS THE CIRCLE, and a circle on the ground is FLAT: a body overhead or on a ledge above
		// the fire is not standing in it. One flat distance, asked of the nearest point the ball reached.
		if (FVector::Dist2D(Target.Point, Middle) > RadiusCm)
		{
			continue;
		}

		// NO HIT, NO GATE, NO PEN QUESTION (his spec): the fire's door asks nothing about damage, because
		// a fire has none. It applies its Burn again, every beat, for as long as the body stands here —
		// and a body that walks out keeps the Burn it has, running down on its own clock.
		UProsperitocracyDamageStatics::ApplyStatusesOnContact(Target.Actor, Source, FireBlock, DamageEffectClass);
	}
}

void AProsperitocracyFireField::TheClockIsOut()
{
	if (bTheClockIsOut)
	{
		return;
	}
	bTheClockIsOut = true;

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	// NOTHING CATCHES ANYTHING FROM HERE. The circle is dead the moment the clock is; the fire that is
	// still alight is only burning down, and the ground it is on is nobody's any more.
	if (TheFire)
	{
		// Told to stop, and nothing else: no new fire is made and the flames already alight finish their
		// own lives at the effect's own pace — a real burn-out, with no size trick and nothing re-created.
		// The rate is already nothing by now, because the burning down ends with the clock.
		SetTheRate(0.0f);
		TheFire->OnSystemFinished.AddDynamic(this, &AProsperitocracyFireField::TheFireHasBurnedOut);
		TheFire->Deactivate();
	}

	// A picture that never reports an end must not leave a dead fire standing in the world forever.
	GraceEndsAt = Now + LongestAFireMayOutliveItsClockSeconds;

	if (!TheFire)
	{
		Destroy();
	}
}

void AProsperitocracyFireField::TheFireHasBurnedOut(UNiagaraComponent* Finished)
{
	// The last of the fire is gone and the clock is out: nothing of this fire is left in the world.
	if (bTheClockIsOut)
	{
		Destroy();
	}
}

void AProsperitocracyFireField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World || !FireBlock)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	// THE GROUND CATCHES FOR THE WHOLE OF ITS LIFE, the fade included, because the fade is part of the
	// Duration (his rule). Nothing about who gets lit changes at any point in this fire's clock.
	if (!bTheClockIsOut && Now >= NextCatchesAt)
	{
		NextCatchesAt = Now + CatchEverySeconds;
		CatchWhateverStandsInIt();
	}

	// THE FIRE BURNS DOWN, and it burns down as ONE fire: one number, thinning the whole patch evenly.
	if (!bTheClockIsOut)
	{
		SetTheRate(TheRateThisGroundWants(Now));

		if (Now >= EndsAt)
		{
			TheClockIsOut();
			return;
		}
	}

	// The clock is out and the fire has outlived even the grace it was given: it goes.
	if (bTheClockIsOut && Now >= GraceEndsAt)
	{
		Destroy();
	}
}

float AProsperitocracyFireField::GetMyStat(EProsperitocracyStat Stat) const
{
	// Presence is scope, asked on the block: a block that does not carry the row does not have it, and a
	// "0" from the evaluator would not say that. And it is the FINAL value, through the one aggregator.
	if (!FireBlock || !MyOwnHome || !FireBlock->Carries(Stat))
	{
		return 0.0f;
	}

	return UProsperitocracyStatSystemStatics::GetStatFinal(MyOwnHome->GetProsperitocracyAbilitySystemComponent(), Stat);
}

float AProsperitocracyFireField::FlamesASecondBurningInTheWorld()
{
	float Burning = 0.0f;
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		const AProsperitocracyFireField* Fire = EveryFireBurning[Index].Get();
		if (!Fire)
		{
			EveryFireBurning.RemoveAt(Index);
			continue;
		}

		// What a fire is actually asking for: its own rate, over the ground it still owns. A fire whose
		// ground a newer one took is asking for little or nothing, and the world's ceiling is a count of
		// fire in the world, not a count of fires.
		Burning += Fire->BaseFlameRate * Fire->ShareOfItsOwnGround;
	}

	return Burning;
}

void AProsperitocracyFireField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The fire's home is an actor of its own, so it does not die with its owner unless it is told to.
	if (MyOwnHome)
	{
		MyOwnHome->Destroy();
		MyOwnHome = nullptr;
	}

	EveryFireBurning.Remove(this);

	// THE GROUND GOES BACK. This fire's circle stops being covered the moment it is gone, so every fire
	// still burning reads its own share again — an older fire gets back whatever this one was sitting on.
	for (int32 Index = EveryFireBurning.Num() - 1; Index >= 0; --Index)
	{
		if (AProsperitocracyFireField* Fire = EveryFireBurning[Index].Get())
		{
			Fire->ReReadHowMuchGroundItStillOwns();
		}
	}

	Super::EndPlay(EndPlayReason);
}
