// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "ProsperitocracyGameplayEffectContext.generated.h"

class AActor;
class FArchive;
class IProsperitocracyAbilitySourceInterface;
class UObject;
class UPhysicalMaterial;

/**
 * FProsperitocracyDamageLine
 *
 * One damage value in a damage instance: exactly ONE type (Impact or Piercing), a pen tier (1-4),
 * and the amount. A normal hit carries one line; a hybrid attack (e.g. a railgun/railcannon that
 * slams AND pierces) carries SEVERAL lines, each resolved through the same pen-gate → resist pipeline.
 */
USTRUCT(BlueprintType)
struct FProsperitocracyDamageLine
{
	GENERATED_BODY()

	// Exactly one of Damage.Type.Impact / Damage.Type.Piercing.
	UPROPERTY()
	FGameplayTag Type;

	// Pen tier 1-4 (light/medium/heavy/anti-tank) — the gate value.
	UPROPERTY()
	uint8 PenTier = 1;

	// The amount of this damage value.
	UPROPERTY()
	float Amount = 0.0f;
};

USTRUCT()
struct FProsperitocracyGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	FProsperitocracyGameplayEffectContext()
		: FGameplayEffectContext()
	{
	}

	FProsperitocracyGameplayEffectContext(AActor* InInstigator, AActor* InEffectCauser)
		: FGameplayEffectContext(InInstigator, InEffectCauser)
	{
	}

	/** Returns the wrapped FProsperitocracyGameplayEffectContext from the handle, or nullptr if it doesn't exist or is the wrong type */
	static PROSPERITOCRACY_API FProsperitocracyGameplayEffectContext* ExtractEffectContext(struct FGameplayEffectContextHandle Handle);

	/** Sets the object used as the ability source */
	void SetAbilitySource(const IProsperitocracyAbilitySourceInterface* InObject, float InSourceLevel);

	/** Returns the ability source interface associated with the source object. Only valid on the authority. */
	const IProsperitocracyAbilitySourceInterface* GetAbilitySource() const;

	virtual FGameplayEffectContext* Duplicate() const override
	{
		FProsperitocracyGameplayEffectContext* NewContext = new FProsperitocracyGameplayEffectContext();
		*NewContext = *this;
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FProsperitocracyGameplayEffectContext::StaticStruct();
	}

	/** Overridden to serialize new fields */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

	/** Returns the physical material from the hit result if there is one */
	const UPhysicalMaterial* GetPhysicalMaterial() const;

public:
	// --- Damage instance payload (0.4): one OR MORE damage lines, each {Type, PenTier, Amount}. ---
	// A normal hit has one line. A hybrid hit (e.g. a railgun/railcannon that slams AND pierces)
	// carries several lines — each resolved through the same pen-gate → resist pipeline. Each line is
	// exactly one of the two types. Set by whoever produces the damage; the execution reads them.
	// NOTE: server-authoritative (the execution runs WITH_SERVER_CODE); not net-serialized yet — if we
	// replicate damage specs directly later, add DamageLines to NetSerialize + a custom net serializer.

	void AddDamageLine(const FGameplayTag& InType, uint8 InPenTier, float InAmount);
	void ResetDamageLines() { DamageLines.Reset(); }
	const TArray<FProsperitocracyDamageLine>& GetDamageLines() const { return DamageLines; }

public:
	/** ID to allow the identification of multiple bullets that were part of the same cartridge */
	UPROPERTY()
	int32 CartridgeID = -1;

protected:
	/** Ability Source object (should implement IProsperitocracyAbilitySourceInterface). NOT replicated currently */
	UPROPERTY()
	TWeakObjectPtr<const UObject> AbilitySourceObject;

	// The damage lines carried by this damage instance.
	UPROPERTY()
	TArray<FProsperitocracyDamageLine> DamageLines;
};

template<>
struct TStructOpsTypeTraits<FProsperitocracyGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FProsperitocracyGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

