// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyDamageReceiver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyDamageReceiver)

FProsperitocracyDamageProfile IProsperitocracyDamageReceiver::GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext) const
{
	// Default: no armor, no resists — a plain, unarmored target.
	FProsperitocracyDamageProfile Profile;
	Profile.Armor = 1;
	Profile.ImpactResist = 0.0f;
	Profile.PiercingResist = 0.0f;
	return Profile;
}

FProsperitocracyDamageProfile IProsperitocracyDamageReceiver::GetBodyDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext) const
{
	// Default: the target as a whole resists nothing. Armor 0, because nothing is being penetrated —
	// a line that carries no pen is never bounced.
	FProsperitocracyDamageProfile Profile;
	Profile.Armor = 0;
	Profile.ImpactResist = 0.0f;
	Profile.PiercingResist = 0.0f;
	return Profile;
}
