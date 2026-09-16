// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Prosperitocracy : ModuleRules
{
	public Prosperitocracy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Includes stay relative to the module root (e.g. #include "Character/..."), the same shape as the
		// project we port from, so ported files keep their existing include paths.
		PublicIncludePaths.AddRange(
			new string[] {
				"Prosperitocracy"
			}
		);

		// Deliberately minimal while the module is empty. Dependencies are added as each system is ported,
		// so the container never carries a link to a plugin the game does not actually use yet.
		// GAS landed with the attribute sets: it is the ONE evaluator every stat resolves through
		// (Design/stats.md), so nothing numeric can work without it. GameplayTasks is GAS's own
		// dependency; GameplayTags is required by NativeGameplayTags.h.
		// PhysicsCore landed with the ability layer: UPhysicalMaterialWithTags derives from UPhysicalMaterial,
		// which lives there (Physics/PhysicalMaterialWithTags.h). The linker named it — unresolved
		// UPhysicalMaterial ctor/dtor/vtable in PhysicalMaterialWithTags.cpp.obj — not a guess.
		// Niagara landed with the weapon: AProsperitocracyWeapon spawns the template's own muzzle-flash
		// system per shot (UNiagaraFunctionLibrary / UNiagaraComponent / UNiagaraSystem live there).
		// The plugin itself is enabled by default in 5.8 and the project already ships Niagara assets.
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"PhysicsCore",
				"GameplayAbilities",
				"GameplayTasks",
				"GameplayTags",
				"Niagara"
			}
		);
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore"
			}
		);

		// Iris support landed with the ability layer, and it is what the second failed link was about:
		// ProsperitocracyGameplayEffectContext.cpp carries a net serializer, and its registry symbols
		// (UE::Net::FNetSerializerRegistryDelegates, FNetSerializerConfig, FPropertyNetSerializerInfoRegistry)
		// live in the IrisCore module — checked in the engine at Runtime/Net/Iris/. NetCore was my first
		// guess and did not fix it; the old project calls this same helper. It adds IrisCore plus
		// UE_WITH_IRIS=1, so the ported code sees Iris exactly as it did on the other side.
		SetupIrisSupport(Target);
	}
}
