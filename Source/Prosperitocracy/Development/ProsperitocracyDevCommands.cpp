// Copyright Prosperitocracy. All Rights Reserved.

#include "CoreMinimal.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyDamageStatics.h"
#include "AbilitySystem/ProsperitocracyGameplayEffectContext.h"
#include "AbilitySystem/ProsperitocracyStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "Classes/ProsperitocracyClass.h"
#include "ProsperitocracyGameplayTags.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadout.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

/**
 * The in-game dev sandbox — DEVELOPMENT ONLY. Six commands, one per thing you cannot try by walking
 * around: the gun you are holding, the weave you are wearing, the armour's colour, which of your
 * class's three loadouts you are playing, the Stun status you cannot otherwise put on yourself, and the
 * damage you cannot otherwise take (Prosperitocracy.Hurt — so a weave's resist and contested health can
 * both be felt while nothing in the game attacks you).
 *
 * `Prosperitocracy.Gun <name>` puts one of our six guns in your hands without touching what the
 * character spawns with and without a pickup system.
 *
 * It is this small because of how a gun is built: four guns are TWO bodies (pistol, rifle) x FOUR stat
 * blocks, and a body carries NO numbers — so a gun is a body AND a block, and picking one is writing
 * that pair into a slot of the loadout you are playing. That write goes through the SAME door the
 * picker will use (SetWeaponInSlot), which is what checks the class's access rules and then dresses the
 * body from the loadout — so what this command leaves behind is what a switch and a menu will read.
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
		const TCHAR* BodyPath;
		const TCHAR* BlockPath;
		const TCHAR* BlockName;
	};

	/** The two gun bodies: every gun we have is built on one of them. */
	const TCHAR* const RifleBodyPath  = TEXT("/Game/ThirdPerson/Blueprints/WeaponBP/weaponChild/BP_Rifle.BP_Rifle_C");
	const TCHAR* const PistolBodyPath = TEXT("/Game/ThirdPerson/Blueprints/WeaponBP/weaponChild/BP_Pistol.BP_Pistol_C");

	// The six guns we have, each as the body it is built on AND the block that is its numbers — both
	// halves, because both halves are what a gun is and a slot carries the pair. The incendiary rifle
	// and the stun pistol are the two that carry a STATUS: the block each one names applies it, and the
	// status's own block is where its numbers are.
	const FGunEntry Guns[] =
	{
		{ TEXT("Pistol"),      PistolBodyPath, TEXT("/Game/Weapons/StatBlocks/STB_Pistol.STB_Pistol"),                   TEXT("STB_Pistol")           },
		{ TEXT("SMG"),         PistolBodyPath, TEXT("/Game/Weapons/StatBlocks/STB_SMG.STB_SMG"),                         TEXT("STB_SMG")              },
		{ TEXT("Auto"),        RifleBodyPath,  TEXT("/Game/Weapons/StatBlocks/STB_RifleAuto.STB_RifleAuto"),             TEXT("STB_RifleAuto")        },
		{ TEXT("Semi"),        RifleBodyPath,  TEXT("/Game/Weapons/StatBlocks/STB_RifleSemi.STB_RifleSemi"),             TEXT("STB_RifleSemi")        },
		{ TEXT("Incendiary"),  RifleBodyPath,  TEXT("/Game/Weapons/StatBlocks/STB_IncendiaryRifle.STB_IncendiaryRifle"), TEXT("STB_IncendiaryRifle")  },
		{ TEXT("StunPistol"),  PistolBodyPath, TEXT("/Game/Weapons/StatBlocks/STB_StunPistol.STB_StunPistol"),           TEXT("STB_StunPistol")       },
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
		UClass* Body = LoadClass<AActor>(nullptr, Wanted->BodyPath);
		if (!Block || !Body)
		{
			Report(FString::Printf(TEXT("could not load the gun: %s"), Block ? Wanted->BodyPath : Wanted->BlockPath));
			return;
		}

		// The gun, as a loadout carries it: the body it is built on, and the block that is its numbers.
		FProsperitocracyWeaponSlot Entry;
		Entry.BodyClass = Body;
		Entry.StatBlock = Block;

		// The slot is the block's own tag, and the loadout is what checks it — along with the class's
		// access rules — so nothing here decides what may go where. One line comes back either way: the
		// reason it was refused, or what was put where.
		FString Message;
		Loadout->SetWeaponInSlot(Block->GetSlot(), Entry, Message);
		Report(Message);
	}

	// The command itself: a plain console command, so it needs no cheat manager, no PlayerController
	// subclass and no exec routing — type it in the Output Log's Cmd box during PIE, or in the
	// in-game console.
	static FAutoConsoleCommandWithWorldAndArgs GunCommand(
		TEXT("Prosperitocracy.Gun"),
		TEXT("Equip one of our six guns by name, with a full magazine and a full spare pool: ")
		TEXT("Pistol, SMG, Auto, Semi, Incendiary, StunPistol. Puts that gun's stat block into the slot its own tag says it ")
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

	/** The character's loadout — the thing a weave is chosen IN. Null outside PIE, or before a pawn. */
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

		UProsperitocracyLoadoutComponent* Loadout = FindLoadout(World);
		if (!Loadout)
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

		// The ONE door a weave is chosen through: it is written into the loadout being played, and the
		// body is dressed from there — the same dress a loadout switch performs. Nothing about the body
		// is touched from here, which is why the choice survives a switch and the shipped default is
		// never written.
		FString Message;
		Loadout->SetWeave(Block, Message);
		Report(FString::Printf(TEXT("%s (%s) — watch the run and the jump."), *Message, Wanted->BlockName));
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

	/** The character's loadout — where a colour is chosen. Null outside PIE, or before a pawn. */
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

	/** The three colour regions of the armour and the colour the playing loadout holds for each. */
	void ListColors(const UProsperitocracyLoadoutComponent* LoadoutComponent)
	{
		int32 Line = 0;
		Report(TEXT("Prosperitocracy.ArmorColor <1|2|3> <#RRGGBB|none> — the armor's colours:"), Line++);

		// The colours live in the loadout being played, so that is what is read — the same holder a
		// loadout switch reads, which is exactly why a colour set here is still there after one.
		const UProsperitocracyLoadout* Playing = LoadoutComponent ? LoadoutComponent->GetLoadout() : nullptr;
		const int32 Trims[] =
		{
			Playing ? Playing->Trim1 : ProsperitocracyArmor::NoColor,
			Playing ? Playing->Trim2 : ProsperitocracyArmor::NoColor,
			Playing ? Playing->Trim3 : ProsperitocracyArmor::NoColor,
		};

		for (int32 Index = 0; Index < ProsperitocracyArmor::PieceCount; ++Index)
		{
			Report(FString::Printf(TEXT("  Trim %-4s %s"), ProsperitocracyArmor::Pieces[Index].Name,
				*DescribeColor(Trims[Index])), Line++);
		}

		if (!LoadoutComponent)
		{
			Report(TEXT("  (no character — this reads, and sets, only while the game is running)"), Line++);
		}
	}

	void HandleArmorColorCommand(const TArray<FString>& Args, UWorld* World)
	{
		UProsperitocracyLoadoutComponent* Loadout = FindLoadout(World);

		// No region named: say what can be typed, by saying what the armour has and what each holds.
		if (Args.Num() < 2 || Args[0].IsEmpty() || Args[1].IsEmpty())
		{
			ListColors(Loadout);
			return;
		}

		const ProsperitocracyArmor::FPiece* Piece = FindPiece(Args[0]);
		if (!Piece)
		{
			Report(FString::Printf(TEXT("'%s' is not one of the armor's trim regions — 1, 2 or 3."), *Args[0]));
			ListColors(Loadout);
			return;
		}

		if (!Loadout)
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

		// The ONE door a colour is chosen through: it is written into the loadout being played — one
		// number, one holder — and the body is painted from it on the spot. Nothing is painted from here,
		// which is why the colour survives a loadout switch and the shipped default is never written.
		FString Message;
		if (!Loadout->SetTrim(Piece->Color, Hex, Message))
		{
			Report(FString::Printf(TEXT("could not set trim %s — %s"), Piece->Name, *Message));
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
 * The class half of the sandbox: which of our classes this character is playing.
 *
 * A class is what a character plays FROM — its three loadouts, its palette and its own access rules
 * (the Reclaimer's Primary is its melee, and it carries no Special) — and with no picker yet this is
 * the only way to be the Reclaimer at all. It is one call to the door a class is chosen through
 * (SelectClass), the same call the picker will make, so choosing from here cannot dress a body
 * differently from the way the picker will: the class's FIRST loadout is played and the body is dressed
 * from it, this character's own copies are made from the class's three, and the class ASSET is never
 * written.
 *
 * A NUMBER picks a class — 1 is the first of the list — and so does its name. NO argument plays class
 * 1, because the command you reach for should work with nothing typed after it.
 */
namespace ProsperitocracyDevClass
{
	/** One class as you type it, and the class asset that IS that class. */
	struct FClassEntry
	{
		const TCHAR* Name;
		const TCHAR* ClassPath;
	};

	/** The classes we have, in the order their numbers go: 1 is the first. */
	const FClassEntry Classes[] =
	{
		{ TEXT("Reclaimer"), TEXT("/Game/Classes/DA_Class_Reclaimer.DA_Class_Reclaimer") },
		{ TEXT("Anchor"),    TEXT("/Game/Classes/DA_Class_Anchor.DA_Class_Anchor")       },
	};

	/** How many classes we have — the ceiling of a typed number. */
	int32 NumClasses() { return UE_ARRAY_COUNT(Classes); }

	/** Say something in the log and on screen, as the CLASS sandbox. LineIndex = which on-screen slot. */
	void Report(const FString& Message, int32 LineIndex = 0)
	{
		ProsperitocracyDev::Report(TEXT("DevClass"), /*OnScreenKey=*/ 0x9009 + LineIndex, Message);
	}

	/** The character's loadout component — the thing that knows which class is being played. */
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

	/** The classes, numbered, with the slots each carries and which one is being played. */
	void ListClasses(const UProsperitocracyLoadoutComponent* LoadoutComponent)
	{
		int32 Line = 0;
		Report(TEXT("Prosperitocracy.Class [1|2|name] — the classes we have:"), Line++);

		const UProsperitocracyClass* Playing = LoadoutComponent ? LoadoutComponent->Class : nullptr;

		for (int32 Index = 0; Index < NumClasses(); ++Index)
		{
			const FClassEntry& Entry = Classes[Index];
			const UProsperitocracyClass* Class = LoadObject<UProsperitocracyClass>(nullptr, Entry.ClassPath);
			const FString Slots = Class ? Class->UsableSlots.ToStringSimple() : FString(TEXT("CLASS MISSING"));

			Report(FString::Printf(TEXT("  %d %-10s %s%s"), Index + 1, Entry.Name, *Slots,
				(Class && Class == Playing) ? TEXT("   <- playing") : TEXT("")), Line++);
		}
	}

	void HandleClassCommand(const TArray<FString>& Args, UWorld* World)
	{
		UProsperitocracyLoadoutComponent* LoadoutComponent = FindLoadout(World);

		// WHICH class this is: a number from 1, or a name. NOTHING typed plays class 1 — the first of
		// the list — and it is the same call a number or a name makes.
		const FString Typed = (Args.Num() > 0) ? Args[0].TrimStartAndEnd() : FString();

		int32 Index = INDEX_NONE;
		if (Typed.IsEmpty())
		{
			Index = 0;
		}
		else if (Typed.IsNumeric())
		{
			const int32 Wanted = FCString::Atoi(*Typed) - 1;
			if (Wanted >= 0 && Wanted < NumClasses())
			{
				Index = Wanted;
			}
		}
		else
		{
			for (int32 Candidate = 0; Candidate < NumClasses(); ++Candidate)
			{
				if (Typed.Equals(Classes[Candidate].Name, ESearchCase::IgnoreCase))
				{
					Index = Candidate;
					break;
				}
			}
		}

		if (Index == INDEX_NONE)
		{
			Report(FString::Printf(TEXT("'%s' is not one of our classes."), *Typed));
			ListClasses(LoadoutComponent);
			return;
		}

		if (!LoadoutComponent)
		{
			Report(TEXT("no character to change — this only works while the game is running (PIE)."));
			return;
		}

		UProsperitocracyClass* Class = LoadObject<UProsperitocracyClass>(nullptr, Classes[Index].ClassPath);
		if (!Class)
		{
			Report(FString::Printf(TEXT("could not load %s"), Classes[Index].ClassPath));
			return;
		}

		// The ONE door a class is chosen through: that class's three loadouts are what this character
		// now plays, this character's own copies are made from them, and the first of the three is
		// played and dressed on the spot. Nothing here decides what a class carries.
		LoadoutComponent->SelectClass(Class);
		Report(FString::Printf(TEXT("now playing the %s — class %d of %d, its loadout 1 of three."),
			*Class->DisplayName.ToString(), Index + 1, NumClasses()));
	}

	// A plain console command like the others, so it needs no cheat manager, no PlayerController
	// subclass and no exec routing — type it in the Output Log's Cmd box during PIE.
	static FAutoConsoleCommandWithWorldAndArgs ClassCommand(
		TEXT("Prosperitocracy.Class"),
		TEXT("Play as one of our classes — a number (1 Reclaimer, 2 Anchor) or a name. NO argument plays ")
		TEXT("class 1. It goes in through the same door a class is chosen through, so that class's own ")
		TEXT("rules (the Reclaimer's Primary is its melee, and it carries no Special) hold from then on."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleClassCommand));
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

		// What THIS character holds at each index — its own copy, which is what it would actually play and
		// therefore what anything changed in play shows up in. Not the asset the class ships.
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Report(FString::Printf(TEXT("  %d. %s%s"), Index + 1, *Describe(LoadoutComponent->GetLoadoutAt(Index)),
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

/**
 * The status half of the sandbox: the Stun you cannot otherwise put on yourself.
 *
 * Burn shows itself — you set a target alight and watch it burn down. A stun shows itself on the only
 * body that can be stopped dead today: the player. Nothing can damage the player yet, so no round can
 * ever land a stun on the one body it would be felt on, and this command is that missing door.
 *
 * It reads the stun pistol's OWN block for both halves of the status — the tag it stamps and the block
 * that is its Duration — so what lands on you is exactly what a stun pistol round puts on a target.
 * This file states no status tag and no duration of its own: change the pistol's block in the editor
 * and this command changes with it, because both read the same pairing.
 */
namespace ProsperitocracyDevStatus
{
	/** The stun pistol's block — the thing that NAMES its status. Read, never restated. */
	const TCHAR* const StunPistolBlockPath = TEXT("/Game/Weapons/StatBlocks/STB_StunPistol.STB_StunPistol");

	/** Say something in the log and on screen, as the STATUS sandbox. LineIndex = which on-screen slot. */
	void Report(const FString& Message, int32 LineIndex = 0)
	{
		ProsperitocracyDev::Report(TEXT("DevStatus"), /*OnScreenKey=*/ 0x9006 + LineIndex, Message);
	}

	void HandleStunCommand(const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!Pawn)
		{
			Report(TEXT("no character to stun — this only works while the game is running (PIE)."));
			return;
		}

		UAbilitySystemComponent* SourceAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
		UProsperitocracyStatusComponent* Statuses = Pawn->FindComponentByClass<UProsperitocracyStatusComponent>();
		const UProsperitocracyStatTable* PistolBlock = LoadObject<UProsperitocracyStatTable>(nullptr, StunPistolBlockPath);
		if (!SourceAbilitySystemComponent || !Statuses || !PistolBlock)
		{
			Report(TEXT("cannot stun — the character has no status component, or the stun pistol's block is missing."));
			return;
		}

		// The status the pistol names, and the block that is ITS OWN numbers: the same pairing a round
		// from that gun applies, read from the same place.
		for (const FProsperitocracyAppliedEffect& Applied : PistolBlock->AppliedEffects)
		{
			if (Applied.StatusTag != ProsperitocracyGameplayTags::Status_Stun)
			{
				continue;
			}

			const UProsperitocracyStatTable* StatusBlock = Applied.StatBlock.LoadSynchronous();
			if (!StatusBlock)
			{
				break;
			}

			// The effect the ONE damage pipeline runs on comes from the loadout, exactly as a gun's
			// shot takes it — one asset for the game, never a second one minted for a dev command.
			const UProsperitocracyLoadoutComponent* LoadoutComponent = Pawn->FindComponentByClass<UProsperitocracyLoadoutComponent>();
			const UProsperitocracyLoadout* Playing = LoadoutComponent ? LoadoutComponent->GetLoadout() : nullptr;
			const TSubclassOf<UGameplayEffect> DamageEffectClass = Playing ? Playing->GunDamageEffectClass : nullptr;
			if (!DamageEffectClass)
			{
				Report(TEXT("cannot stun — the loadout names no damage effect to carry a status."));
				return;
			}

			// The same door a hit goes through: the one place a status is put on a body.
			Statuses->ApplyStatus(Applied.StatusTag, StatusBlock, SourceAbilitySystemComponent, DamageEffectClass, FHitResult());
			Report(TEXT("stun applied to you — for its Duration you cannot walk, jump, fire or use anything."));
			return;
		}

		Report(TEXT("the stun pistol's block names no Stun status — nothing applied."));
	}

	// A plain console command like the other four, so it needs no cheat manager, no PlayerController
	// subclass and no exec routing — type it in the Output Log's Cmd box during PIE.
	static FAutoConsoleCommandWithWorldAndArgs StunCommand(
		TEXT("Prosperitocracy.Stun"),
		TEXT("Put Stun on yourself, using the stun pistol's own status block — the duration it lands with is ")
		TEXT("that block's own, so it is the same stun a stun pistol round puts on a target. Nothing can ")
		TEXT("damage you yet, so this is the only way to feel a stun on the one body it stops."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleStunCommand));
}

namespace ProsperitocracyDevHurt
{
	/** Say something in the log and on screen, as the other sandboxes do. LineIndex = on-screen slot. */
	void Report(const FString& Message, int32 LineIndex = 0)
	{
		ProsperitocracyDev::Report(TEXT("DevHurt"), /*OnScreenKey=*/ 0x9007 + LineIndex, Message);
	}

	/**
	 * Prosperitocracy.Hurt <amount> <impact|piercing>
	 *
	 * Take that much damage OF A NAMED TYPE, through the ONE pipeline — everything in this game is a
	 * damage line and a damage line is always its type, so the type is the one thing the command asks for
	 * and it has no default.
	 *
	 * **No pen, and the command does not ask for one.** Pen is what a PLAYER'S ATTACK carries to get
	 * through ARMOUR, and a player has no armour — you answer a hit with your weave's resists, so there is
	 * no gate for a pen to bite on. A wound from nowhere carries none, exactly like a burn does.
	 *
	 * That is what makes it the way to feel a weave: run it once as piercing and once as impact against
	 * the same weave and the two resists answer for themselves.
	 *
	 * And it carries NO dealer either: nothing dealt this to you, so it wins you back nothing — but it
	 * DOES hand you contested health, because contested comes from damage you TAKE. So: hurt yourself,
	 * then shoot a dummy and watch the health come back.
	 */
	void HandleHurtCommand(const TArray<FString>& Args, UWorld* World)
	{
		const auto SayUsage = [](const TCHAR* Why)
		{
			Report(FString::Printf(TEXT("%s A damage line is a type: Prosperitocracy.Hurt <amount> <impact|piercing> — e.g. Prosperitocracy.Hurt 30 piercing."), Why));
		};

		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!Pawn)
		{
			Report(TEXT("no character — this only works while the game is running (PIE)."));
			return;
		}

		// The amount AND the type: the type is half of every damage line, so there is nothing to default it
		// to and no such thing as hurting someone by an untyped number.
		if (Args.Num() < 2)
		{
			SayUsage(TEXT("a damage line is an amount and a type."));
			return;
		}

		UAbilitySystemComponent* SourceAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
		const UProsperitocracyLoadoutComponent* LoadoutComponent = Pawn->FindComponentByClass<UProsperitocracyLoadoutComponent>();
		const UProsperitocracyLoadout* Playing = LoadoutComponent ? LoadoutComponent->GetLoadout() : nullptr;

		// The effect the ONE damage pipeline runs on comes from the loadout, exactly as a shot takes it —
		// one asset for the game, never a second one minted for a dev command.
		const TSubclassOf<UGameplayEffect> DamageEffectClass = Playing ? Playing->GunDamageEffectClass : nullptr;
		if (!SourceAbilitySystemComponent || !DamageEffectClass)
		{
			Report(TEXT("cannot hurt you — the character has no ability system, or the loadout names no damage effect."));
			return;
		}

		const float Amount = FMath::Max(0.0f, FCString::Atof(*Args[0]));

		FGameplayTag Type;
		if (Args[1].Equals(TEXT("impact"), ESearchCase::IgnoreCase))
		{
			Type = ProsperitocracyGameplayTags::Damage_Type_Impact;
		}
		else if (Args[1].Equals(TEXT("piercing"), ESearchCase::IgnoreCase))
		{
			Type = ProsperitocracyGameplayTags::Damage_Type_Piercing;
		}
		else
		{
			Report(FString::Printf(TEXT("'%s' is not a damage type — the two are impact and piercing."), *Args[1]));
			return;
		}

		// The same context a shot builds, with the line on the context where the execution reads it. NO pen
		// (a body has no armour) and no instigator: nothing fired, and no one is credited for it.
		FGameplayEffectContextHandle Context = SourceAbilitySystemComponent->MakeEffectContext();
		if (FProsperitocracyGameplayEffectContext* TypedContext = FProsperitocracyGameplayEffectContext::ExtractEffectContext(Context))
		{
			TypedContext->AddDamageLine(Type, /*InPenTier=*/ 0, Amount);
		}
		else
		{
			Report(TEXT("cannot hurt you — the effect context is not ours. Check AbilitySystemGlobalsClassName in DefaultGame.ini."));
			return;
		}

		UProsperitocracyDamageStatics::ApplyDamageEffectToHit(Context, Pawn, SourceAbilitySystemComponent, DamageEffectClass);

		Report(FString::Printf(TEXT("took %.0f %s — the ONE pipeline ran it, and the log says what your weave let through."), Amount, *Type.ToString()), /*LineIndex=*/ 1);
	}

	static FAutoConsoleCommandWithWorldAndArgs HurtCommand(
		TEXT("Prosperitocracy.Hurt"),
		TEXT("Take a damage line of your own naming, through the ONE pipeline, so a worn weave's two resists ")
		TEXT("and contested health can all be felt: Prosperitocracy.Hurt <amount> <impact|piercing>. No pen — ")
		TEXT("a player has no armour for one to bite on. Run it both ways against one weave to hear each ")
		TEXT("resist answer for itself."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleHurtCommand));
}
