// Copyright Prosperitocracy. All Rights Reserved.

#include "CoreMinimal.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

/**
 * The in-game dev sandbox — DEVELOPMENT ONLY. Two commands, one per thing you cannot try by walking
 * around: the gun you are holding, and the weave you are wearing.
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
 * `Prosperitocracy.Armor <name>` wears one of our five weaves — the same call the character makes at
 * spawn with the weave it ships wearing, asked for again. It exists because an armor has no pickup
 * system yet and no customizer yet, and because a weave's weight is something you have to FEEL: wear
 * a heavy one and run, wear a light one and run.
 *
 * Nothing here writes a value, spawns an actor, or adds a rule: each calls the one function that
 * already puts that thing on the body, and deleting this file would change no number in the game.
 */

namespace ProsperitocracyDev
{
	/**
	 * Say something both in the log (the Output Log, where the command was typed) and on screen.
	 *
	 * One message said in two places, once — every dev command speaks through here. The tag says which
	 * sandbox is talking, and the key is the on-screen SLOT the line takes: the same key replaces the
	 * line already sitting in that slot (so a command's usual chatter never stacks up six seconds deep),
	 * while a LISTING gives every line its own slot — otherwise each line of a list lands on top of the
	 * one before it and all you can read is the last entry.
	 */
	void Report(const TCHAR* Tag, int32 OnScreenKey, const FString& Message)
	{
		UE_LOG(LogProsperitocracy, Display, TEXT("[%s] %s"), Tag, *Message);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(OnScreenKey, /*TimeToDisplay=*/ 6.0f, FColor::Green,
				FString::Printf(TEXT("[%s] "), Tag) + Message);
		}
	}
}

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

	/** Say something in the log and on screen, as the GUN sandbox. LineIndex = which on-screen slot. */
	void Report(const FString& Message, int32 LineIndex = 0)
	{
		ProsperitocracyDev::Report(TEXT("DevGun"), /*OnScreenKey=*/ 0x9002 + LineIndex, Message);
	}

	/** List the four guns and the slot each one's block belongs in — one line per on-screen slot. */
	void ListGuns()
	{
		int32 Line = 0;
		Report(TEXT("Prosperitocracy.Gun <name> — the guns we have:"), Line++);

		for (const FGunEntry& Gun : Guns)
		{
			const UProsperitocracyStatTable* Block = LoadObject<UProsperitocracyStatTable>(nullptr, Gun.BlockPath);
			const FString Slot = Block ? Block->GetSlot().ToString() : TEXT("BLOCK MISSING");

			Report(FString::Printf(TEXT("  %-6s -> %s  (%s)"), Gun.Name, *Slot, Gun.BlockName), Line++);
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

/**
 * The armor half of the sandbox: the weave you are wearing.
 *
 * It is this small for the same reason the gun one is. An armor is a block of numbers, and what a body
 * wears is one of those blocks — so wearing a different weave is one call to the door the character
 * already wears its starting weave through. No pickup system, no customizer, no second path.
 */
namespace ProsperitocracyDevArmor
{
	/** One weave as you type it, the stat block asset that IS that weave, and that asset's short name. */
	struct FWeaveEntry
	{
		const TCHAR* Name;
		const TCHAR* BlockPath;
		const TCHAR* BlockName;
	};

	// The five weaves we have, lightest to heaviest. A weave — not a mesh and not a colour — is what an
	// armor IS right now: the whole armor we have today is these five blocks.
	const FWeaveEntry Weaves[] =
	{
		{ TEXT("Nanoweave"),    TEXT("/Game/Armor/StatBlocks/STB_Nanoweave.STB_Nanoweave"),                 TEXT("STB_Nanoweave")         },
		{ TEXT("Graphene"),     TEXT("/Game/Armor/StatBlocks/STB_GrapheneWeave.STB_GrapheneWeave"),         TEXT("STB_GrapheneWeave")     },
		{ TEXT("BoronCarbide"), TEXT("/Game/Armor/StatBlocks/STB_BoronCarbideWeave.STB_BoronCarbideWeave"), TEXT("STB_BoronCarbideWeave") },
		{ TEXT("Carborundum"),  TEXT("/Game/Armor/StatBlocks/STB_CarborundumWeave.STB_CarborundumWeave"),   TEXT("STB_CarborundumWeave")  },
		{ TEXT("Tungsten"),     TEXT("/Game/Armor/StatBlocks/STB_TungstenWeave.STB_TungstenWeave"),         TEXT("STB_TungstenWeave")     },
	};

	/** Say something in the log and on screen, as the ARMOR sandbox. LineIndex = which on-screen slot. */
	void Report(const FString& Message, int32 LineIndex = 0)
	{
		ProsperitocracyDev::Report(TEXT("DevArmor"), /*OnScreenKey=*/ 0x9003 + LineIndex, Message);
	}

	/** The character's own numbers — the component a weave is worn through. Null outside PIE, or before a pawn. */
	UProsperitocracyPlayerStatsComponent* FindStats(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		const APlayerController* PlayerController = World->GetFirstPlayerController();
		APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UProsperitocracyPlayerStatsComponent>() : nullptr;
	}

	/** List the five weaves, lightest to heaviest — one line per on-screen slot, so all five are readable. */
	void ListWeaves()
	{
		int32 Line = 0;
		Report(TEXT("Prosperitocracy.Armor <name> — the weaves we have, lightest to heaviest:"), Line++);

		for (const FWeaveEntry& Weave : Weaves)
		{
			Report(FString::Printf(TEXT("  %-12s -> %s"), Weave.Name, Weave.BlockName), Line++);
		}
	}

	void HandleArmorCommand(const TArray<FString>& Args, UWorld* World)
	{
		// No argument: say what can be typed. Handy enough that nobody has to remember the five names.
		if (Args.Num() < 1 || Args[0].IsEmpty())
		{
			ListWeaves();
			return;
		}

		const FWeaveEntry* Wanted = nullptr;
		for (const FWeaveEntry& Weave : Weaves)
		{
			if (Args[0].Equals(Weave.Name, ESearchCase::IgnoreCase))
			{
				Wanted = &Weave;
				break;
			}
		}

		if (!Wanted)
		{
			Report(FString::Printf(TEXT("'%s' is not one of our weaves."), *Args[0]));
			ListWeaves();
			return;
		}

		UProsperitocracyPlayerStatsComponent* Stats = FindStats(World);
		if (!Stats)
		{
			Report(TEXT("no character to wear it — this only works while the game is running (PIE)."));
			return;
		}

		UProsperitocracyStatTable* Block = LoadObject<UProsperitocracyStatTable>(nullptr, Wanted->BlockPath);
		if (!Block)
		{
			Report(FString::Printf(TEXT("could not load %s"), Wanted->BlockPath));
			return;
		}

		// The ONE door: the same call this character already made at spawn with the weave it shipped
		// wearing. Everything a weave does to the body happens in there — nothing is done from here, so
		// this command cannot dress a body differently from the way the game dresses it.
		Stats->WearWeave(Block);

		Report(FString::Printf(TEXT("wearing %s (%s) — watch the run and the jump."), Wanted->Name, Wanted->BlockName));
	}

	// The command itself: a plain console command like the gun one, so it needs no cheat manager, no
	// PlayerController subclass and no exec routing — type it in the Output Log's Cmd box during PIE.
	static FAutoConsoleCommandWithWorldAndArgs ArmorCommand(
		TEXT("Prosperitocracy.Armor"),
		TEXT("Wear one of our five weaves by name — Nanoweave, Graphene, BoronCarbide, Carborundum, ")
		TEXT("Tungsten (lightest to heaviest). It goes on through the same door a weave is worn through ")
		TEXT("at spawn, so it takes effect on the spot. No argument lists them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleArmorCommand));
}
