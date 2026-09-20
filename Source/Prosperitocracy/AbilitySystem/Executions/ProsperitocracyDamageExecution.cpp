// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyDamageExecution.h"

#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyDamageReceiver.h"
#include "AbilitySystem/ProsperitocracyAbilitySourceInterface.h"
#include "ProsperitocracyGameplayTags.h"
#include "Engine/World.h"
#include "ProsperitocracyLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyDamageExecution)

UProsperitocracyDamageExecution::UProsperitocracyDamageExecution()
{
	// No attribute capture: the damage amount (and its Type + PenTier) is carried on the effect
	// context as damage lines (FProsperitocracyGameplayEffectContext::AddDamageLine). This is the ONE
	// damage pipeline: pen gate → resist, for every line. There is no separate generic-BaseDamage path.
}

void UProsperitocracyDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Spec.GetContext());
	check(TypedContext);

	// The avatar we actually hit (from the hit result), falling back to the target ASC's avatar.
	AActor* HitActor = nullptr;
	const FHitResult* HitResult = TypedContext->GetHitResult();
	if (HitResult)
	{
		HitActor = HitResult->HitObjectHandle.FetchActor();
	}
	if (!HitActor)
	{
		UAbilitySystemComponent* TargetAbilitySystemComponent = ExecutionParams.GetTargetAbilitySystemComponent();
		HitActor = TargetAbilitySystemComponent ? TargetAbilitySystemComponent->GetAvatarActor_Direct() : nullptr;
	}

	// Ask the receiver for the armor + resists of the part that was hit. Something that doesn't
	// implement the interface is an unarmored, unresisted target (armor 1, no resists).
	FProsperitocracyDamageProfile Profile;
	// And for the SECOND question, the one a burn asks: what does this target resist AS A WHOLE? Only
	// read by a line that carries no pen (see below) — the part profile is the normal answer.
	FProsperitocracyDamageProfile BodyProfile;
	if (HitActor)
	{
		if (Cast<IProsperitocracyDamageReceiver>(HitActor))
		{
			// Interface events must be dispatched via the generated Execute_ thunk, not called directly.
			Profile = IProsperitocracyDamageReceiver::Execute_GetDamageProfile(HitActor, Spec.GetContext());
			BodyProfile = IProsperitocracyDamageReceiver::Execute_GetBodyDamageProfile(HitActor, Spec.GetContext());
		}
	}

	// Prosperitocracy has no teams and friendly fire is always on (see combat.md), so damage is
	// never gated by a team check.
	constexpr float DamageInteractionAllowedMultiplier = 1.0f;

	// Run EVERY damage line through the SAME shared gate → resist path. A normal hit has one line,
	// a hybrid hit (e.g. railgun/railcannon) has several. Lines are read off the context; if the
	// source added none, fall back to the ability source's damage lines (a weapon, ability, or stat
	// host — all resolved from GAS, the ONE evaluator). An unwired source deals 0 and logs a warning:
	// per design, an unwired damage source is a bug, not a missing feature — there is no silent
	// fallback to raw data.
	const IProsperitocracyAbilitySourceInterface* AbilitySource = TypedContext->GetAbilitySource();
	TArray<FProsperitocracyDamageLine> DamageLines = TypedContext->GetDamageLines();
	if (DamageLines.Num() == 0 && AbilitySource)
	{
		AbilitySource->GetDamageLines(DamageLines);
	}

	if (DamageLines.Num() == 0)
	{
		UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] %s hit by an unwired damage source (no context lines, no ability source) — deals 0. Wire the source; this is a bug."), *GetNameSafe(HitActor));
	}

	// The ONE universal falloff (Design/damage.md): the source's Range/Falloff stats scale the
	// FINAL damage by the shot/explosion distance — the hit result's trace distance (cm). Guns and
	// explosions, one formula; perks raise the stats and the formula reads finals automatically.
	const float HitDistance = HitResult ? HitResult->Distance : 0.0f;
	const float DistanceAttenuation = AbilitySource ? AbilitySource->GetDistanceAttenuation(HitDistance, nullptr, nullptr) : 1.0f;

	float TotalDamage = 0.0f;
	for (const FProsperitocracyDamageLine& Line : DamageLines)
	{
		// Presence is scope, and it is what decides WHICH question this line asks:
		//
		//   - a line that CARRIES a pen strikes a part, so it faces that part's armor;
		//   - a line that carries NO pen (0) is not an attack that penetrates anything — it reaches the
		//     target ITSELF. Burn is the case: any burn burns a big guy whatever plates he wears, so
		//     there is no gate to run and no part to bounce off. What answers it is the target's own
		//     resist, averaged across its parts (GetBodyDamageProfile).
		const bool bGated = (Line.PenTier > 0);
		const bool bIsImpact = (Line.Type == ProsperitocracyGameplayTags::Damage_Type_Impact);
		float Gate = 1.0f;
		float Resist = 0.0f;

		if (bGated)
		{
			// Pen gate (full / 50% reduced / ricochet): pen > armor -> full; pen == armor -> 50% reduced;
			// pen < armor -> bounces/ricochets OFF this target. A bounce is not "no event" — the projectile
			// keeps flying (handled at the projectile level), but this target takes no damage from this line.
			if (Line.PenTier < Profile.Armor)
			{
				UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] [%s] %s pen %d < armor %d -> BOUNCE (no damage)"),
					*GetNameSafe(HitActor), *Line.Type.ToString(), Line.PenTier, Profile.Armor);
				continue;
			}
			const bool bReduced = (Line.PenTier == Profile.Armor);
			Gate = bReduced ? 0.5f : 1.0f;

			// Resist = the per-type efficiency axis, applied after the gate. Weakness = negative resist.
			Resist = bIsImpact ? Profile.ImpactResist : Profile.PiercingResist;
		}
		else
		{
			// No pen, so no part: the target as a whole is what this line answers to.
			Resist = bIsImpact ? BodyProfile.ImpactResist : BodyProfile.PiercingResist;
		}

		const float EffectiveAmount = Line.Amount * Gate;
		// The universal falloff multiplies the FINAL damage (never the base stat) at hit time.
		const float FinalAmount = FMath::Max(EffectiveAmount * (1.0f - Resist / 100.0f) * DistanceAttenuation, 0.0f);

		UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] [%s] %s %s | resist %.0f%% | falloff x%.2f (dist %.0fm) -> %.1f"),
			*GetNameSafe(HitActor), *Line.Type.ToString(),
			bGated
				? *FString::Printf(TEXT("pen %d vs armor %d -> %s"), Line.PenTier, Profile.Armor, (Gate < 1.0f) ? TEXT("50% REDUCED") : TEXT("FULL"))
				: TEXT("no pen -> the target itself"),
			Resist, DistanceAttenuation, HitDistance / 100.0f, FinalAmount);

		TotalDamage += FinalAmount;
	}
	TotalDamage *= DamageInteractionAllowedMultiplier;

	// What this hit actually dealt, said once, where the gate was decided — so anything that follows the
	// damage (a gun's burn, which may only land on a hit that got through) reads the answer instead of
	// working the gate out again and risking a different one.
	TypedContext->SetLandedDamage(TotalDamage);

	if (TotalDamage > 0.0f)
	{
		// Apply the combined damage as the Damage meta-attribute, which the HealthSet maps to -Health.
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UProsperitocracyHealthSet::GetDamageAttribute(), EGameplayModOp::Additive, TotalDamage));
	}
#endif // #if WITH_SERVER_CODE
}
