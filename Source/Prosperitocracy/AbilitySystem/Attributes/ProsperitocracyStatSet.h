// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ProsperitocracyAttributeSet.h"
#include "NativeGameplayTags.h"

#include "ProsperitocracyStatSet.generated.h"

#define UE_API PROSPERITOCRACY_API

class UObject;
struct FFrame;

/**
 * UProsperitocracyStatSet
 *
 * The GAS home for the ACTOR-facing stats of the universal stat table
 * (Design/stats.md — Character stats). Health already lives in UProsperitocracyHealthSet
 * and is NOT duplicated here. Every value here is evaluated by the GAS attribute
 * aggregator — the one evaluator: (base + Σflat) × Σpercent.
 *
 * Thing stats (weapons, abilities, turrets, deployables) are NOT owned by an actor's
 * ability system component; they get their own storage when items/deployables are built
 * (later phase), but resolve through the SAME stat vocabulary + evaluator.
 */
UCLASS(MinimalAPI, BlueprintType)
class UProsperitocracyStatSet : public UProsperitocracyAttributeSet
{
	GENERATED_BODY()

public:
	UE_API UProsperitocracyStatSet();

	ATTRIBUTE_ACCESSORS(UProsperitocracyStatSet, MoveSpeed);
	ATTRIBUTE_ACCESSORS(UProsperitocracyStatSet, JumpVelocity);
	ATTRIBUTE_ACCESSORS(UProsperitocracyStatSet, DiveDistance);
	ATTRIBUTE_ACCESSORS(UProsperitocracyStatSet, PackCapacity);
	ATTRIBUTE_ACCESSORS(UProsperitocracyStatSet, WeightCapacity);
	ATTRIBUTE_ACCESSORS(UProsperitocracyStatSet, ImpactResist);
	ATTRIBUTE_ACCESSORS(UProsperitocracyStatSet, PiercingResist);

protected:
	UFUNCTION()
	UE_API void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_JumpVelocity(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_DiveDistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_PackCapacity(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_WeightCapacity(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_ImpactResist(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_PiercingResist(const FGameplayAttributeData& OldValue);

private:
	// Move speed of the owner — the RUN speed; walking is half of it, one universal constant.
	// (Receiver of perks/attachments.) [TUNE]
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MoveSpeed;

	// cm/s — the upward velocity a jump launches with. [TUNE]
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JumpVelocity, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData JumpVelocity;

	// Dive distance of the owner. [TUNE]
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DiveDistance, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData DiveDistance;

	// Max blood / pack capacity. [TUNE]
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PackCapacity, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData PackCapacity;

	// Max weight the owner can carry. [TUNE]
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeightCapacity, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData WeightCapacity;

	// % less Impact damage taken. Negative = weakness. Receiver-side, per type.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ImpactResist, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ImpactResist;

	// % less Piercing damage taken. Negative = weakness. Receiver-side, per type.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PiercingResist, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData PiercingResist;
};

#undef UE_API
