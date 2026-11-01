// Copyright Prosperitocracy. All Rights Reserved.

#include "AbilitySystem/ProsperitocracyDamageStatics.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatusComponent.h"
#include "GameplayEffect.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyDamageStatics)

void UProsperitocracyDamageStatics::AddDamageLine(FGameplayEffectContextHandle Context, FGameplayTag DamageType, uint8 PenTier, float Amount)
{
	if (FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context))
	{
		TypedContext->AddDamageLine(DamageType, PenTier, Amount);
	}
}

void UProsperitocracyDamageStatics::ApplyDamageEffectToHit(FGameplayEffectContextHandle Context, AActor* HitActor, UAbilitySystemComponent* SourceAbilitySystemComponent, TSubclassOf<UGameplayEffect> DamageEffectClass)
{
	if (!Context.IsValid() || !SourceAbilitySystemComponent || !DamageEffectClass)
	{
		return;
	}

	// A wall has no ability system: there is nothing to damage. Not a failure — the caller's impact
	// FX has already played on the surface.
	UAbilitySystemComponent* TargetAbilitySystemComponent = HitActor
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor)
		: nullptr;
	if (!TargetAbilitySystemComponent)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystemComponent);
	}
}

bool UProsperitocracyDamageStatics::IsContextValid(FGameplayEffectContextHandle Context)
{
	return Context.IsValid();
}

void UProsperitocracyDamageStatics::ApplyEffectsToHit(FGameplayEffectContextHandle Context, AActor* HitActor,
	UAbilitySystemComponent* SourceAbilitySystemComponent, const UProsperitocracyStatTable* SourceStatBlock,
	TSubclassOf<UGameplayEffect> DamageEffectClass)
{
	if (!HitActor || !SourceAbilitySystemComponent || !SourceStatBlock || SourceStatBlock->AppliedEffects.Num() == 0)
	{
		return;
	}

	// The statuses go on the body that was caught, through the one component that owns statuses on a body
	// — so a body carries them the same way whether a shot hit it or fire touched it.
	UProsperitocracyStatusComponent* Statuses = HitActor->FindComponentByClass<UProsperitocracyStatusComponent>();
	if (!Statuses)
	{
		return;
	}

	// THE HIT, IF THERE WAS ONE. A valid context IS a hit and brings both of its facts with it: whether the
	// damage LANDED — read from where the gate decided it, never worked out a second time here where it
	// could disagree — and the part that was struck, so a status that works on a part keeps working on the
	// part that caught it. No context is a CONTACT, which is the fire's case: a fire is not a hit and has no
	// damage to bounce, so standing in it is the whole of the reason, and the status goes on the body as a
	// whole with no gate and no part.
	FHitResult Hit;
	if (Context.IsValid())
	{
		const FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context);
		if (!TypedContext || !TypedContext->LandedAnyDamage())
		{
			return;
		}

		if (const FHitResult* HitResultPtr = TypedContext->GetHitResult())
		{
			Hit = *HitResultPtr;
		}
	}

	for (const FProsperitocracyAppliedEffect& Applied : SourceStatBlock->AppliedEffects)
	{
		const UProsperitocracyStatTable* StatusBlock = Applied.StatBlock.LoadSynchronous();
		if (!StatusBlock)
		{
			// A thing that names a status but carries no block for it has no numbers to apply. Said out
			// loud, because it is an authoring bug rather than a missing feature.
			UE_LOG(LogProsperitocracy, Warning, TEXT("[Status] %s names the status %s with no block — nothing applied."),
				*GetNameSafe(SourceStatBlock), *Applied.StatusTag.ToString());
			continue;
		}

		Statuses->ApplyStatus(Applied.StatusTag, StatusBlock, SourceAbilitySystemComponent, DamageEffectClass, Hit);
	}
}