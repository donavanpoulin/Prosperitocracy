// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyDamageReceiver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyDamageReceiver)

FProsperitocracyDamageProfile IProsperitocracyDamageReceiver::GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const
{
	// Default: a part with no armour and nothing to say about any damage type. Armor is 0 and not 1:
	// unarmoured is a real state on the 0-3 scale, and it means every pen over-pens this part and takes
	// full. Nothing about a target that says no armour should quietly halve a light round.
	FProsperitocracyDamageProfile Profile;
	Profile.Armor = 0;
	Profile.Resist = 0.0f;
	return Profile;
}

float IProsperitocracyDamageReceiver::GetBodyResist_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const
{
	// Default: the target as a whole resists nothing. No armour is involved — a line that carries no
	// pen is never gated.
	return 0.0f;
}
