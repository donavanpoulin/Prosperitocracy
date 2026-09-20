// Copyright Prosperitocracy. All Rights Reserved.

#include "CoreMinimal.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "Classes/ProsperitocracyClass.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

/**
 * The in-game dev sandbox — DEVELOPMENT ONLY. Four commands, one per thing you cannot try by walking
 * around: the gun you are holding, the weave you are wearing, the armour's colour, and which of your
 * class's three loadouts you are playing.
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
 * `Prosperitocracy.ArmorColor <1|2|3> <#RRGGBB|none>` paints one trim region of the armour you are
 * wearing — 1 the collar, 2 the shoulders and arms, 3 the legs, each its own colour, each one number.
 * It exists because the colour has no customizer yet, and because a colour is something you have to
 * SEE: it is the same call a customizer will make when it arrives, and the region repaints the moment
 * the row changes.
 *
 * `Prosperitocracy.Loadout <1|2|3>` plays one of your class's three loadouts, and what that loadout
 * carries comes with it — the armour and the guns. A class owns three (Design/loadout.md) and there is
 * no picker yet, so this is the only way to reach the second and the third; it is one call to the door
 * a switch goes through, the same call the picker will make.
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

/**
 * The look half of the sandbox: the trim colour of the armor you are wearing.
 *
 * One region at a time, because that is what the trim IS — three regions, three rows, one number each.
 * This command owns no colour of its own: it writes a region's row through the same call a customizer
 * will make, so what it does and what the menu will do cannot drift apart. The regions are spoken of
 * by the PLAYER's numbers — 1 the collar, 2 the shoulders and arms, 3 the legs — which is what the
 * menu will show too, because those are the parts the player can see.
 */
namespace ProsperitocracyDevArmorColor
{
	/** Say something in the log and on screen, as the COLOUR sandbox. LineIndex = which on-screen slot. */
	void Report(const FString& Message, int32 LineIndex = 0)
	{
		ProsperitocracyDev::Report(TEXT("DevColor"), /*OnScreenKey=*/ 0x9004 + LineIndex, Message);
	}

	/** The character's own numbers — where a piece of armour is painted from. Null outside PIE. */
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

	/** The region a word names ("1", "2", "3"), or null when it is not one. One list: the armour's. */
	const ProsperitocracyArmor::FPiece* FindPiece(const FString& Name)
	{
		for (const ProsperitocracyArmor::FPiece& Piece : ProsperitocracyArmor::Pieces)
		{
			if (Name.Equals(Piece.Name, ESearchCase::IgnoreCase))
			{
				return &Piece;
			}
		}
		return nullptr;
	}

	/**
	 * A colour as it is typed: six hex digits, with or without the '#'. Nothing else is a colour —
	 * a value we half-understood would land on the body as a colour nobody asked for.
	 */
	bool ParseHex(const FString& Text, int32& OutHex)
	{
		const int32 Start = (!Text.IsEmpty() && Text[0] == TEXT('#')) ? 1 : 0;
		const int32 Digits = Text.Len() - Start;
		if (Digits < 1 || Digits > 6)
		{
			return false;
		}

		int32 Value = 0;
		for (int32 Index = Start; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index];
			int32 Digit = 0;
			if (Char >= TEXT('0') && Char <= TEXT('9'))
			{
				Digit = Char - TEXT('0');
			}
			else if (Char >= TEXT('a') && Char <= TEXT('f'))
			{
				Digit = (Char - TEXT('a')) + 10;
			}
			else if (Char >= TEXT('A') && Char <= TEXT('F'))
			{
				Digit = (Char - TEXT('A')) + 10;
			}
			else
			{
				return false;
			}
			Value = (Value << 4) | Digit;
		}

		OutHex = Value;
		return true;
	}

	/** One region's colour as it is spoken of: the hex, or the paint it ships with. */
	FString DescribeColor(int32 Hex)
	{
		return (Hex == ProsperitocracyArmor::NoColor)
			? FString(TEXT("its own paint"))
			: FString::Printf(TEXT("#%06X"), Hex);
	}

	/** The armour's three trim regions and what each is painted right now — one line each on screen. */
	void ListColors(const UProsperitocracyPlayerStatsComponent* Stats)
	{
		int32 Line = 0;
		Report(TEXT("Prosperitocracy.ArmorColor <1|2|3> <#RRGGBB|none> — the armor's trim:"), Line++);

		for (const ProsperitocracyArmor::FPiece& Piece : ProsperitocracyArmor::Pieces)
		{
			const int32 Hex = Stats ? Stats->GetArmorColor(Piece.Color) : ProsperitocracyArmor::NoColor;
			Report(FString::Printf(TEXT("  Trim %-4s %s"), Piece.Name, *DescribeColor(Hex)), Line++);
		}

		if (!Stats)
		{
			Report(TEXT("  (no character — this reads, and paints, only while the game is running)"), Line++);
		}
	}

	void HandleArmorColorCommand(const TArray<FString>& Args, UWorld* World)
	{
		UProsperitocracyPlayerStatsComponent* Stats = FindStats(World);

		// No region named: say what can be typed, by saying what the armour has and what it is painted.
		if (Args.Num() < 2 || Args[0].IsEmpty() || Args[1].IsEmpty())
		{
			ListColors(Stats);
			return;
		}

		const ProsperitocracyArmor::FPiece* Piece = FindPiece(Args[0]);
		if (!Piece)
		{
			Report(FString::Printf(TEXT("'%s' is not one of the armor's trim regions — 1, 2 or 3."), *Args[0]));
			ListColors(Stats);
			return;
		}

		if (!Stats)
		{
			Report(TEXT("no character to paint — this only works while the game is running (PIE)."));
			return;
		}

		// "none" is how a region goes back to the paint it ships with — not a colour, and not black.
		int32 Hex = ProsperitocracyArmor::NoColor;
		if (!Args[1].Equals(TEXT("none"), ESearchCase::IgnoreCase) && !ParseHex(Args[1], Hex))
		{
			Report(FString::Printf(
				TEXT("'%s' is not a colour — give six hex digits (like #FF0000), or 'none' to go back to its own paint."),
				*Args[1]));
			return;
		}

		// The ONE door, the same call a customizer will make: the region's row is written, and the body
		// paints it on the spot (it listens to that row). Nothing is painted from here.
		if (!Stats->SetArmorColor(Piece->Color, Hex))
		{
			Report(FString::Printf(TEXT("could not paint trim %s — see the log for why."), Piece->Name));
			return;
		}

		Report(FString::Printf(TEXT("trim %s: %s."), Piece->Name, *DescribeColor(Hex)));
	}

	// A plain console command like the gun and armour ones, so it needs no cheat manager, no
	// PlayerController subclass and no exec routing — type it in the Output Log's Cmd box during PIE.
	static FAutoConsoleCommandWithWorldAndArgs ArmorColorCommand(
		TEXT("Prosperitocracy.ArmorColor"),
		TEXT("Paint one trim region of the armor you are wearing, one colour per region: 1 the collar, ")
		TEXT("2 the shoulders and arms, 3 the legs — each given six hex digits (like #FF0000) or 'none' ")
		TEXT("for the paint it ships with. It writes the region's own colour row through the same call a ")
		TEXT("customizer will make, so the change shows on the body the moment you press enter. No ")
		TEXT("arguments list the regions and their colours."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleArmorColorCommand));
}

/**
 * The loadout half of the sandbox: which of your class's three loadouts you are playing.
 *
 * A class owns three (Design/loadout.md) and a player has both classes, so "which loadout am I on" is
 * real state the body carries — and with no picker yet, this is the only way to reach loadouts 2 and 3.
 * It is one call to the door a switch goes through (SelectLoadout), which is the same call the picker
 * will make, so switching from here cannot dress a body differently from the way the picker will.
 */
namespace ProsperitocracyDevLoadout
{
	/** Say something in the log and on screen, as the LOADOUT sandbox. LineIndex = which on-screen slot. */
	void Report(const FString& Message, int32 LineIndex = 0)
	{
		ProsperitocracyDev::Report(TEXT("DevLoadout"), /*OnScreenKey=*/ 0x9005 + LineIndex, Message);
	}

	/** The character's loadout component — the thing that knows which of the three is being played. */
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

	/** One loadout as a line: the armour it names, then the gun each of its slots carries. */
	FString Describe(const UProsperitocracyLoadout* Loadout)
	{
		if (!Loadout)
		{
			return TEXT("nothing at all");
		}

		// The armour first — it is the piece you FEEL, because it is what the weight comes from.
		const UProsperitocracyStatTable* Weave = Loadout->Weave;
		FString Text = Weave ? Weave->GetName() : FString(TEXT("no armour"));

		// Then the guns, in slot order, by the block each slot carries. A slot that names nothing is
		// simply not carried, which is exactly what an empty loadout is.
		const FProsperitocracyWeaponSlot* const Slots[] =
		{
			&Loadout->Primary, &Loadout->Secondary, &Loadout->Special, &Loadout->Grenade
		};
		for (const FProsperitocracyWeaponSlot* Slot : Slots)
		{
			if (const UProsperitocracyStatTable* Block = Slot->StatBlock.LoadSynchronous())
			{
				Text += FString::Printf(TEXT(" + %s"), *Block->GetName());
			}
		}

		return Text;
	}

	/** The class's three loadouts and which one is being played — one line per on-screen slot. */
	void ListLoadouts(const UProsperitocracyLoadoutComponent* LoadoutComponent)
	{
		int32 Line = 0;

		if (!LoadoutComponent)
		{
			Report(TEXT("Prosperitocracy.Loadout <1|2|3> — no character yet: this works while the game is running (PIE)."), Line++);
			return;
		}

		const UProsperitocracyClass* Class = LoadoutComponent->Class;
		if (!Class)
		{
			Report(TEXT("Prosperitocracy.Loadout <1|2|3> — no class is chosen on this character, so it plays the one loadout it was authored with."), Line++);
			return;
		}

		Report(FString::Printf(TEXT("Prosperitocracy.Loadout <1|2|3> — %s:"), *Class->DisplayName.ToString()), Line++);

		const UProsperitocracyLoadout* Three[] = { Class->Loadout1, Class->Loadout2, Class->Loadout3 };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Report(FString::Printf(TEXT("  %d. %s%s"), Index + 1, *Describe(Three[Index]),
				(LoadoutComponent->SelectedLoadout == Index) ? TEXT("   <- playing") : TEXT("")), Line++);
		}
	}

	void HandleLoadoutCommand(const TArray<FString>& Args, UWorld* World)
	{
		UProsperitocracyLoadoutComponent* LoadoutComponent = FindLoadout(World);

		// No argument: say what the three are, and which one is being played.
		if (Args.Num() < 1 || Args[0].IsEmpty())
		{
			ListLoadouts(LoadoutComponent);
			return;
		}

		if (!LoadoutComponent)
		{
			Report(TEXT("no character to change — this only works while the game is running (PIE)."));
			return;
		}

		// Spoken of as 1, 2 and 3 — the way the player counts a class's three loadouts.
		const int32 Index = FCString::Atoi(*Args[0]) - 1;
		if (Index < 0 || Index > 2)
		{
			Report(FString::Printf(TEXT("'%s' is not one of the three — give 1, 2 or 3."), *Args[0]));
			ListLoadouts(LoadoutComponent);
			return;
		}

		// The ONE door a switch goes through, the same call the picker will make. Everything a switch
		// does — the armour, and the guns in hand — happens in there, so nothing is dressed from here.
		if (!LoadoutComponent->SelectLoadout(Index))
		{
			Report(TEXT("could not change loadout — see the log for why."));
			return;
		}

		Report(FString::Printf(TEXT("playing loadout %d — %s"), Index + 1, *Describe(LoadoutComponent->GetLoadout())));
	}

	// A plain console command like the other three, so it needs no cheat manager, no PlayerController
	// subclass and no exec routing — type it in the Output Log's Cmd box during PIE.
	static FAutoConsoleCommandWithWorldAndArgs LoadoutCommand(
		TEXT("Prosperitocracy.Loadout"),
		TEXT("Play one of your class's three loadouts: 1, 2 or 3. What that loadout carries comes with it ")
		TEXT("— the armour and the guns — through the same call a switch goes through, so the change ")
		TEXT("shows in hand. No argument lists the three and marks the one being played."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleLoadoutCommand));
}
