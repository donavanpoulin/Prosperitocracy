// Copyright Prosperitocracy. All Rights Reserved.

#include "CoreMinimal.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

/**
 * The in-game gun sandbox — DEVELOPMENT ONLY.
 *
 * `Prosperitocracy.Gun <name>` puts one of our four guns in your hands without touching what the
 * character spawns with and without a pickup system.
 *
 * It is this small because of how a gun is built: four guns are TWO bodies (pistol, rifle) x FOUR
 * stat blocks, and a body carries NO numbers. You already carry both bodies — the rifle in Primary,
 * the pistol in Secondary — and the template's own keys 1 and 2 already switch between them. So the
 * only thing a different gun needs is a different BLOCK, which is exactly what the loadout already
 * does at spawn. This command asks it to do that again, at runtime.
 *
 * Every gun arrives with a fresh gun's ammo: a full magazine and a full spare pool.
 *
 * Nothing here writes a value, spawns an actor, or adds a rule: it calls the one function that
 * already hands a gun its numbers, and deleting this file would change no number in the game.
 */

namespace ProsperitocracyDevGuns
{
	/** One gun as you type it, the stat block asset that IS that gun, and that asset's short name. */
	struct FGunEntry
	{
		const TCHAR* Name;
		const TCHAR* BlockPath;
		const TCHAR* BlockName;
	};

	// The four guns we have. The block — not a body — is what makes a gun, so this table names blocks.
	const FGunEntry Guns[] =
	{
		{ TEXT("Pistol"), TEXT("/Game/Weapons/StatBlocks/STB_Pistol.STB_Pistol"),       TEXT("STB_Pistol")    },
		{ TEXT("SMG"),    TEXT("/Game/Weapons/StatBlocks/STB_SMG.STB_SMG"),             TEXT("STB_SMG")       },
		{ TEXT("Auto"),   TEXT("/Game/Weapons/StatBlocks/STB_RifleAuto.STB_RifleAuto"), TEXT("STB_RifleAuto") },
		{ TEXT("Semi"),   TEXT("/Game/Weapons/StatBlocks/STB_RifleSemi.STB_RifleSemi"), TEXT("STB_RifleSemi") },
	};

	/** The character's loadout — the thing that owns what it carries. Null outside PIE, or before a pawn. */
	UProsperitocracyLoadoutComponent* FindLoadout(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		const APlayerController* PlayerController = World->GetFirstPlayerController();
		APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UProsperitocracyLoadoutComponent>() : nullptr;
	}

	/** Say something both in the log (the Output Log, where the command was typed) and on screen. */
	void Report(const FString& Message)
	{
		UE_LOG(LogProsperitocracy, Display, TEXT("[DevGun] %s"), *Message);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(/*Key=*/ 0x9002, /*TimeToDisplay=*/ 6.0f, FColor::Green, TEXT("[DevGun] ") + Message);
		}
	}

	/** List the four guns and the slot each one's block belongs in. */
	void ListGuns()
	{
		Report(TEXT("Prosperitocracy.Gun <name> — the guns we have:"));

		for (const FGunEntry& Gun : Guns)
		{
			const UProsperitocracyStatTable* Block = LoadObject<UProsperitocracyStatTable>(nullptr, Gun.BlockPath);
			const FString Slot = Block ? Block->GetSlot().ToString() : TEXT("BLOCK MISSING");

			Report(FString::Printf(TEXT("  %-6s -> %s  (%s)"), Gun.Name, *Slot, Gun.BlockName));
		}
	}

	void HandleGunCommand(const TArray<FString>& Args, UWorld* World)
	{
		// No argument: say what can be typed. Handy enough that nobody has to remember the four names.
		if (Args.Num() < 1 || Args[0].IsEmpty())
		{
			ListGuns();
			return;
		}

		const FGunEntry* Wanted = nullptr;
		for (const FGunEntry& Gun : Guns)
		{
			if (Args[0].Equals(Gun.Name, ESearchCase::IgnoreCase))
			{
				Wanted = &Gun;
				break;
			}
		}

		if (!Wanted)
		{
			Report(FString::Printf(TEXT("'%s' is not one of our guns."), *Args[0]));
			ListGuns();
			return;
		}

		UProsperitocracyLoadoutComponent* Loadout = FindLoadout(World);
		if (!Loadout)
		{
			Report(TEXT("no character to give it to — this only works while the game is running (PIE)."));
			return;
		}

		UProsperitocracyStatTable* Block = LoadObject<UProsperitocracyStatTable>(nullptr, Wanted->BlockPath);
		if (!Block)
		{
			Report(FString::Printf(TEXT("could not load %s"), Wanted->BlockPath));
			return;
		}

		// One line either way: the reason it could not be installed, or the whole story of the install
		// (which slot, and whether the gun that was already there was re-dressed on the spot).
		FString Message;
		Loadout->DevEquipStatBlock(Block, Message);
		Report(Message);
	}

	// The command itself: a plain console command, so it needs no cheat manager, no PlayerController
	// subclass and no exec routing — type it in the Output Log's Cmd box during PIE, or in the
	// in-game console.
	static FAutoConsoleCommandWithWorldAndArgs GunCommand(
		TEXT("Prosperitocracy.Gun"),
		TEXT("Equip one of our four guns by name, with a full magazine and a full spare pool: ")
		TEXT("Pistol, SMG, Auto, Semi. Puts that gun's stat block into the slot its own tag says it ")
		TEXT("belongs in and re-dresses the gun already there, so it changes in hand. No argument lists them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleGunCommand));
}
