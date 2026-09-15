// Fill out your copyright notice in the Description page of Project Settings.

#include "Prosperitocracy.h"

#include "Modules/ModuleManager.h"

/**
 * The game module: the container for Prosperitocracy's own code.
 *
 * Empty on purpose. This is box 1.3 - the project gets a code side and keeps running. Our systems are
 * ported into their own folders under this module, and the dependencies they need (GAS, EnhancedInput,
 * the stat modules, ...) are added to Build.cs as each one lands.
 */
class FProsperitocracyGameModule : public FDefaultGameModuleImpl
{
public:

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FProsperitocracyGameModule, Prosperitocracy, "Prosperitocracy");
