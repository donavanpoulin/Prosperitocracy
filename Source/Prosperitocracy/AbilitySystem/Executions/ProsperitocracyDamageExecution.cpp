// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyDamageExecution.h"

#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility_ContestedHealth.h"
#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyDamageReceiver.h"
#include "AbilitySystem/ProsperitocracyAbilitySourceInterface.h"
#include "ProsperitocracyGameplayTags.h"
#include "Engine/World.h"
#include "ProsperitocracyLogChannels.h"
#include "Weapons/ProsperitocracyBloodBlade.h"

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

	// Ask the receiver for the answer of the PART that was hit: its armour, and the resistance it has
	// against THE LINE'S TYPE (Design/damage.md). Something that doesn't implement the interface has no
	// armour and no resistance at all: an unarmoured target, which takes full from every pen (armor 0).
	const bool bHasReceiver = HitActor && Cast<IProsperitocracyDamageReceiver>(HitActor) != nullptr;

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
		//     resist, averaged across its parts (GetBodyResist).
		const bool bGated = (Line.PenTier > 0);
		float Gate = 1.0f;
		float Resist = 0.0f;
		int32 PartArmor = 0;

		if (bGated)
		{
			// The PART the hit landed on answers FOR THIS LINE'S TYPE: its armour, and the resistance it
			// has against that type. Asked per line, never once per hit — a hybrid hit asks two different
			// questions of the same part, and each line is answered on its own.
			const FProsperitocracyDamageProfile PartProfile = bHasReceiver
				? IProsperitocracyDamageReceiver::Execute_GetDamageProfile(HitActor, Spec.GetContext(), Line.Type)
				: FProsperitocracyDamageProfile();

			// The part's pen-gate threshold, clamped to the scale it lives on (Design/damage.md). The
			// clamp is what keeps the gate to 0-3 no matter what a part's number says, so there is no
			// fourth armour tier lurking in an authored value.
			PartArmor = FMath::Clamp(static_cast<int32>(PartProfile.Armor), 0, 3);
			// The pen gate, armour 0-3 against pen 1-4. ONE comparison, three outcomes:
			//   pen > armour  -> FULL      (the round over-pens the plate)
			//   pen == armour -> HALF      (a match half-penetrates)
			//   pen < armour  -> NOTHING   (it does not get through this part at all)
			//
			// Armour 0 is a real, unarmoured state, so every pen over-pens it and nothing is ever
			// halved or stopped by it. Armour 3 is the top of the scale: only pen 4 gets full
			// against it, pen 3 is halved, and pen 1 and 2 get nothing.
			//
			// A blocked line just does nothing, and the shot stops there. Ricochet — the round
			// carrying on to hit something else — is DESIGNED and NOT BUILT; until it is, a bounce
			// is the end of the shot.
			if (Line.PenTier < PartArmor)
			{
				UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] [%s] %s pen %d < armor %d -> BOUNCE (no damage, the shot stops there)"),
					*GetNameSafe(HitActor), *Line.Type.ToString(), Line.PenTier, PartArmor);
				continue;
			}
			const bool bReduced = (Line.PenTier == PartArmor);
			Gate = bReduced ? 0.5f : 1.0f;

			// Resist = the per-type efficiency axis, applied after the gate. This part's answer for THIS
			// type, and nothing else: a part built against another type resists this one not at all.
			Resist = PartProfile.Resist;
		}
		else
		{
			// No pen, so no part: the target AS A WHOLE answers, for THIS line's type — the resist
			// averaged across its parts, all parts weighing the same, final values. One type per ask, so
			// a target can never answer a Piercing line with its Impact resistance.
			Resist = bHasReceiver
				? IProsperitocracyDamageReceiver::Execute_GetBodyResist(HitActor, Spec.GetContext(), Line.Type)
				: 0.0f;
		}

		const float EffectiveAmount = Line.Amount * Gate;
		// The universal falloff multiplies the FINAL damage (never the base stat) at hit time.
		const float FinalAmount = FMath::Max(EffectiveAmount * (1.0f - Resist / 100.0f) * DistanceAttenuation, 0.0f);

		UE_LOG(LogProsperitocracy, Warning, TEXT("[Damage] [%s] %s %s | resist %.0f%% | falloff x%.2f (dist %.0fm) -> %.1f"),
			*GetNameSafe(HitActor), *Line.Type.ToString(),
			bGated
				? *FString::Printf(TEXT("pen %d vs armor %d -> %s"), Line.PenTier, PartArmor, (Gate < 1.0f) ? TEXT("50% REDUCED") : TEXT("FULL"))
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

		// And the two halves of contested health, at the one place damage becomes real
		// (Design/combat.md): what the TARGET just took becomes ITS contested pool — already lost, on the
		// clock, and winnable back — and the body that DEALT it wins a flat quarter of what it dealt back
		// as health. Credited to the INSTIGATOR, so whoever pulled the trigger is who it counts for,
		// teammate included: the game does not discriminate.
		UProsperitocracyGameplayAbility_ContestedHealth::NotifyDamageTaken(HitActor, TotalDamage);
		UProsperitocracyGameplayAbility_ContestedHealth::NotifyDamageDealt(Spec.GetContext().GetInstigator(), TotalDamage);

		// And the BLOOD — the second thing that follows real damage, told in the same breath and handed
		// the same number: the blade this body carries drinks a share of what it took off a target,
		// whichever weapon did the taking. Inside `TotalDamage` are the pen gate, the resists and the
		// falloff, because it is the number that actually came off the target.
		AProsperitocracyBloodBlade::NotifyDamageDealt(Spec.GetContext().GetInstigator(), TotalDamage);
	}
#endif // #if WITH_SERVER_CODE
}
