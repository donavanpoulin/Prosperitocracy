// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProsperitocracyGameplayEffectContext.h"

#include "AbilitySystem/ProsperitocracyAbilitySourceInterface.h"
#include "Engine/HitResult.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Serialization/GameplayEffectContextNetSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyGameplayEffectContext)

class FArchive;

FProsperitocracyGameplayEffectContext* FProsperitocracyGameplayEffectContext::ExtractEffectContext(struct FGameplayEffectContextHandle Handle)
{
	FGameplayEffectContext* BaseEffectContext = Handle.Get();
	if ((BaseEffectContext != nullptr) && BaseEffectContext->GetScriptStruct()->IsChildOf(FProsperitocracyGameplayEffectContext::StaticStruct()))
	{
		return (FProsperitocracyGameplayEffectContext*)BaseEffectContext;
	}

	return nullptr;
}

bool FProsperitocracyGameplayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);

	// Not serialized for post-activation use:
	// CartridgeID

	return true;
}

namespace UE::Net
{
	// Forward to FGameplayEffectContextNetSerializer
	// Note: If FProsperitocracyGameplayEffectContext::NetSerialize() is modified, a custom NetSerializer must be implemented as the current fallback will no longer be sufficient.
	UE_NET_IMPLEMENT_FORWARDING_NETSERIALIZER_AND_REGISTRY_DELEGATES(ProsperitocracyGameplayEffectContext, FGameplayEffectContextNetSerializer);
}

void FProsperitocracyGameplayEffectContext::SetAbilitySource(const IProsperitocracyAbilitySourceInterface* InObject, float InSourceLevel)
{
	AbilitySourceObject = MakeWeakObjectPtr(Cast<const UObject>(InObject));
	//SourceLevel = InSourceLevel;
}

const IProsperitocracyAbilitySourceInterface* FProsperitocracyGameplayEffectContext::GetAbilitySource() const
{
	return Cast<IProsperitocracyAbilitySourceInterface>(AbilitySourceObject.Get());
}

const UPhysicalMaterial* FProsperitocracyGameplayEffectContext::GetPhysicalMaterial() const
{
	if (const FHitResult* HitResultPtr = GetHitResult())
	{
		return HitResultPtr->PhysMaterial.Get();
	}
	return nullptr;
}

void FProsperitocracyGameplayEffectContext::AddDamageLine(const FGameplayTag& InType, uint8 InPenTier, float InAmount)
{
	FProsperitocracyDamageLine& Line = DamageLines.AddDefaulted_GetRef();
	Line.Type = InType;
	Line.PenTier = InPenTier;
	Line.Amount = InAmount;
}

