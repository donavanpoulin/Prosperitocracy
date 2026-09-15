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
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"InputCore"
			}
		);
	}
}
