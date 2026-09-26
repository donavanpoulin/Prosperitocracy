// Copyright Prosperitocracy. All Rights Reserved.

#include "Character/ProsperitocracyPlayerStatsComponent.h"

#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/Attributes/ProsperitocracyStatSet.h"
#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Camera/ProsperitocracyViewShake.h"
#include "Character/ProsperitocracyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Weapons/ProsperitocracyLoadout.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyPlayerStatsComponent)

/** The parameter the body's materials take their colour through — the pack's own name for it. */
const FName UProsperitocracyPlayerStatsComponent::ArmorColorParameterName(TEXT("Color"));

namespace
{
	/** Whether a stat is one of the armor's colour rows. The pieces are the only ones that are. */
	bool IsArmorColorPiece(EProsperitocracyStat Stat)
	{
		for (const ProsperitocracyArmor::FPiece& Piece : ProsperitocracyArmor::Pieces)
		{
			if (Piece.Color == Stat)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * A colour's hex (0xRRGGBB) as the colour the material is handed: the number unpacked back into
	 * red / green / blue. This is a CONVERSION at the moment of use, not a second thing stored — the
	 * row carries the one number and nothing else.
	 */
	FLinearColor ColorFromHex(int32 Hex)
	{
		const uint8 R = static_cast<uint8>((Hex >> 16) & 0xFF);
		const uint8 G = static_cast<uint8>((Hex >> 8) & 0xFF);
		const uint8 B = static_cast<uint8>(Hex & 0xFF);

		// A colour is picked in the numbers every colour picker speaks, so it is handed over as the
		// colour it reads as — not a darkened or brightened version of it.
		return FLinearColor::FromSRGBColor(FColor(R, G, B, 255));
	}
}

UProsperitocracyPlayerStatsComponent::UProsperitocracyPlayerStatsComponent()
{
	// The component ticks every frame for two jobs: the shot's push, which has a life of its own and
	// nothing else in the engine ends, and the animation rate, which follows that push down and back
	// up — the ease back to normal has to keep running after the push itself has lapsed.
	//
	// WHERE it ticks in the frame is set up in BeginPlay, and it is not decoration: the push has to
	// land between the state writing its speed and the body moving at it, or it is never read.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UProsperitocracyPlayerStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	AbilitySystemComponent = Owner->FindComponentByClass<UProsperitocracyAbilitySystemComponent>();
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogProsperitocracy, Error,
			TEXT("%s on %s: no ability system component on this actor, so this character has no numbers. "
				 "Add the ability system component to the actor."),
			*GetName(), *Owner->GetName());
		return;
	}

	// A pawn-owned ability system: the pawn is both the owner and the avatar. (The readout component
	// asks for the same thing; it is idempotent.)
	AbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);

	// The character's stats and its health both live on the character's own ability system. Adding a
	// set that is already there returns the one that is there, so the readout and this component
	// cannot end up with two.
	AbilitySystemComponent->AddSet<UProsperitocracyStatSet>();
	AbilitySystemComponent->AddSet<UProsperitocracyHealthSet>();

	ApplyBaselineStats();
	ApplyToMovement();

	// The order the push needs to be felt at EVERY speed: the character's own tick first (that is
	// where the movement state writes its speed for the frame — and a sprint writes its speed every
	// single frame it is held), then this component (the push, on top of that number), then the
	// movement component, which reads the number and moves the body at it.
	//
	// Written in any other order the push is invisible while sprinting: the state's number lands
	// after it and the movement only ever reads the state's. That is exactly how it behaved — the walk
	// (where the state writes once, on entry) dragged, and the sprint, which writes every frame, did
	// not.
	if (UCharacterMovementComponent* Movement = GetMovementComponent())
	{
		AddTickPrerequisiteActor(Owner);
		Movement->AddTickPrerequisiteComponent(this);
	}

	// The body now LISTENS to the numbers its movement is built from, and it does so before anything
	// writes one: the first write below is already a change the body should hear, and so is every write
	// after it — a weave, a gun, a perk, a re-tune. That is what makes the movement follow its own
	// inputs instead of being set once and forgotten.
	BindMovementStatListeners();

	// What the character is carrying, as its own number, written before anything reads a weight. The
	// loadout writes this row too, every time it dresses a gun; this is the answer for a body nothing
	// has been dressed onto yet, so there is never a frame where the row is missing and the weight
	// reads as nothing.
	SyncCarriedWeight();

	// What the character OWNS as abilities comes up with it, once, through the same component that
	// brings up its numbers.
	GrantAbilities();

	// And what it is WEARING comes up with it too — DRESSED FROM THE LOADOUT it is playing, because
	// the armour is part of what you take in (Design/loadout.md). One call, the same one a loadout
	// swap makes, so a swap can never dress a body differently from the way it spawns — and the
	// armour's weight reaches the legs through the row it writes, not through a movement rule.
	//
	// A body with no loadout component (or no loadout) is dressed from nothing, which is a bare body:
	// no armour, and so no armour numbers. That is a real state, not a failure.
	UProsperitocracyLoadout* Playing = nullptr;
	if (const UProsperitocracyLoadoutComponent* LoadoutComponent = Owner->FindComponentByClass<UProsperitocracyLoadoutComponent>())
	{
		Playing = LoadoutComponent->GetLoadout();
	}
	DressFromLoadout(Playing);

	// And what it OWNS as abilities comes from the same loadout, through the same kind of call — so the
	// four a loadout took in are granted here at spawn exactly as a swap grants them later. ONE door, so
	// a swap can never own a different set from the one a spawn owns.
	//
	// A body with no loadout component (or one naming no abilities) owns none of them, which is a real
	// state: an empty ability slot is a legal build (Design/loadout.md), not a failure.
	DressAbilitiesFromLoadout(Playing);
}

void UProsperitocracyPlayerStatsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The notifications go with the body: a listener outliving the thing it was bound to is a dangling
	// pointer waiting for a perk to fire.
	UnbindMovementStatListeners();

	// The colour rows' notifications go the same way, for the same reason.
	UnbindArmorColorListeners();

	// The armor's GAS home only exists for as long as the body does, exactly as a gun's does.
	if (ArmorHost)
	{
		ArmorHost->Destroy();
		ArmorHost = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UProsperitocracyPlayerStatsComponent::BindMovementStatListeners()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// The three rows the movement is built FROM. A change to any of them re-prices every speed the body
	// moves at — whoever caused it and through whatever door: a weave, a perk proc, an attachment, a
	// re-tune. Nothing here needs to know who did it, which is the whole point of doing it this way.
	const EProsperitocracyStat MovementInputs[] =
	{
		EProsperitocracyStat::MoveSpeed,
		EProsperitocracyStat::JumpVelocity,
		EProsperitocracyStat::CarriedWeight,
	};

	for (const EProsperitocracyStat Input : MovementInputs)
	{
		const FGameplayAttribute Attribute = UProsperitocracyStatSystemStatics::GetAttributeForStat(Input);
		if (!Attribute.IsValid())
		{
			continue;
		}

		// A lambda through the one evaluator's own notification: the body is told by the NUMBER, not by
		// whoever moved it. The handle is kept so the binding can be let go with the body.
		MovementStatChangeHandles.Add(
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddLambda([this](const FOnAttributeChangeData&) { ApplyToMovement(); }));
	}
}

void UProsperitocracyPlayerStatsComponent::UnbindMovementStatListeners()
{
	if (!AbilitySystemComponent)
	{
		MovementStatChangeHandles.Reset();
		return;
	}

	const EProsperitocracyStat MovementInputs[] =
	{
		EProsperitocracyStat::MoveSpeed,
		EProsperitocracyStat::JumpVelocity,
		EProsperitocracyStat::CarriedWeight,
	};

	// Removed by the handle it was added with — the bindings are lambdas, so the delegate cannot find
	// them by object.
	int32 HandleIndex = 0;
	for (const EProsperitocracyStat Input : MovementInputs)
	{
		const FGameplayAttribute Attribute = UProsperitocracyStatSystemStatics::GetAttributeForStat(Input);
		if (!Attribute.IsValid())
		{
			continue;
		}

		if (MovementStatChangeHandles.IsValidIndex(HandleIndex))
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute)
				.Remove(MovementStatChangeHandles[HandleIndex]);
		}
		++HandleIndex;
	}

	MovementStatChangeHandles.Reset();
}

void UProsperitocracyPlayerStatsComponent::SyncCarriedWeight()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// What the body carries, as ONE number: the loadout's sum, written onto the row everything else
	// reads. This is the only writer of that base anywhere in the game, so a perk that moves it (flat
	// or percent) resolves on top of it through the aggregator, and nothing holds a second copy of the
	// total. Writing it here is what makes the movement follow — the component is already listening.
	const FGameplayAttribute Attribute = UProsperitocracyStatSystemStatics::GetAttributeForStat(EProsperitocracyStat::CarriedWeight);
	if (Attribute.IsValid())
	{
		AbilitySystemComponent->SetNumericAttributeBase(Attribute, GetCarriedWeightLbs());
	}
}

void UProsperitocracyPlayerStatsComponent::GrantAbilities()
{
	if (!AbilitySystemComponent || !AbilitySet)
	{
		return;
	}

	// The project's own ability set does the granting: each ability arrives with its input tag (which
	// is how a door addresses it — see AProsperitocracyCharacter::MeleeWithGunInHand) and the set
	// records the handles, so what this character owns can be handed back as one thing.
	AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedAbilityHandles, this);
}

void UProsperitocracyPlayerStatsComponent::ApplyBaselineStats()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!BaselineStats)
	{
		if (!bBaselineWarned)
		{
			bBaselineWarned = true;
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("%s on %s: no baseline stat block set — this character's stats are all zero, so it takes "
					 "template defaults for anything that reads them."),
				*GetName(), *GetNameSafe(GetOwner()));
		}
		return;
	}

	// The block's BODY rows and nothing else — the same act, through the same guard, that a weave goes
	// in through when it is worn.
	UProsperitocracyStatSystemStatics::ApplyBlockBodyRows(AbilitySystemComponent, BaselineStats, /*bBare=*/ false);
}

void UProsperitocracyPlayerStatsComponent::DressFromLoadout(UProsperitocracyLoadout* Loadout)
{
	// 1. The weave. It is worn FIRST because everything below it lives ON the armour: a colour has
	//    nowhere to be carried until an armour is on the body. Null is passed straight through —
	//    a loadout that names no weave is a bare body, and WearWeave's own door takes the last one off.
	UProsperitocracyStatTable* Worn = nullptr;
	if (Loadout)
	{
		Worn = Loadout->Weave;
	}
	WearWeave(Worn);

	// 2. The paint. A bare body has nowhere to carry a colour, so there is nothing to write; and the
	//    rows live on the armour, so taking the armour off takes the paint with it.
	if (!GetWornWeave())
	{
		return;
	}

	// Written whether the loadout names a colour or not: "nothing chosen" is a value (-1) and it puts
	// the region back to the paint it ships with — which is what makes a SWAP land exactly the same
	// as a spawn, on the rows the last loadout painted.
	//
	// The loadout's three trims line up with the body's three regions by ORDER (1 the collar, 2 the
	// shoulders and arms, 3 the legs — ProsperitocracyArmor::Pieces), so the numbering lives in that
	// one list and is never written down a second time.
	for (int32 Index = 0; Index < ProsperitocracyArmor::PieceCount; ++Index)
	{
		int32 Hex = ProsperitocracyArmor::NoColor;

		if (Loadout)
		{
			switch (Index)
			{
			case 0: Hex = Loadout->Trim1; break;
			case 1: Hex = Loadout->Trim2; break;
			case 2: Hex = Loadout->Trim3; break;
			default: break;
			}
		}

		SetArmorColor(ProsperitocracyArmor::Pieces[Index].Color, Hex);
	}
}

void UProsperitocracyPlayerStatsComponent::DressAbilitiesFromLoadout(UProsperitocracyLoadout* Loadout)
{
	if (!AbilitySystemComponent)
	{
		// Nothing to grant to yet. Said plainly rather than silently, because a body that owns no
		// abilities and a body whose grant never ran look the same in the world.
		UE_LOG(LogProsperitocracy, Log,
			TEXT("%s on %s: the loadout's abilities cannot be granted yet — this body has no ability system up."),
			*GetName(), *GetNameSafe(GetOwner()));
		return;
	}

	// THE LAST LOADOUT'S ABILITIES GO FIRST, and this is the half that makes a swap a SWAP: an ability
	// the loadout being played does not name is taken away here, with the very spec handle it was granted
	// with, so it cannot be pressed afterwards. Clearing the list is what ends it — an ability outliving
	// the loadout that chose it is exactly the accumulation this door exists to prevent, and it would
	// make a loadout's pick mean nothing at all.
	//
	// It is why the loadout's grants are kept in their OWN list: this takes back what a LOADOUT gave, and
	// never the bash or the contested health the body owns by itself, whatever order the two are called in.
	LoadoutAbilityHandles.TakeFromAbilitySystem(AbilitySystemComponent);

	if (!Loadout)
	{
		return;
	}

	// The four slots, in bar order — four fields and never a list, because four is the design's number
	// (Design/abilities.md) and a fifth cannot be authored. Listing them here is what makes this the one
	// place that knows a loadout's four ability slots are a set.
	const TSubclassOf<UProsperitocracyGameplayAbility> Slots[] =
	{
		Loadout->Ability1,
		Loadout->Ability2,
		Loadout->Ability3,
		Loadout->Ability4
	};

	int32 Granted = 0;

	for (const TSubclassOf<UProsperitocracyGameplayAbility>& Slot : Slots)
	{
		// Presence is scope: an empty slot is a slot this loadout did not fill, which is a legal build
		// (Design/loadout.md) and not a missing entry.
		if (!Slot)
		{
			continue;
		}

		UProsperitocracyGameplayAbility* AbilityCDO = Slot->GetDefaultObject<UProsperitocracyGameplayAbility>();
		if (!AbilityCDO)
		{
			// A slot holding something that is not one of our abilities is a misconfiguration worth
			// naming: it would otherwise be granted and never work, which reads as a broken loadout.
			UE_LOG(LogProsperitocracy, Warning,
				TEXT("%s on %s: a loadout's ability slot holds %s, which is not one of our abilities — it is skipped."),
				*GetName(), *GetNameSafe(GetOwner()), *GetNameSafe(Slot.Get()));
			continue;
		}

		// GRANTED THE SAME SHAPE the character's own abilities are granted in: a spec whose level is 1,
		// with this component as the source object. It carries NO input tag, and that is deliberate — a
		// loadout's abilities are not pressed BY a tag: the thing in the player's hand runs its own by
		// name (the weapon's second-press slot), and a tag here would be a second way to address them.
		FGameplayAbilitySpec AbilitySpec(AbilityCDO, /*AbilityLevel=*/ 1);
		AbilitySpec.SourceObject = this;

		LoadoutAbilityHandles.AddAbilitySpecHandle(AbilitySystemComponent->GiveAbility(AbilitySpec));
		++Granted;
	}

	// With its count, and with which loadout: "the abilities did nothing" and "the loadout names no
	// abilities" are the same silence, and only one of them is a bug.
	UE_LOG(LogProsperitocracy, Log, TEXT("%s on %s: %s grants %d of its four ability slots."),
		*GetName(), *GetNameSafe(GetOwner()), *GetNameSafe(Loadout), Granted);
}

void UProsperitocracyPlayerStatsComponent::WearWeave(UProsperitocracyStatTable* Weave)
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("%s on %s: no ability system component, so there are no numbers for a weave to be worn on."),
			*GetName(), *GetNameSafe(GetOwner()));
		return;
	}

	// 1. What the LAST weave gave the body comes back off first. A weave's rows are this body's while
	// it is worn, so a row the new block does not carry must read as bare — never as the old weave's
	// number sitting there dressed as the new one's.
	if (ArmorWeave)
	{
		UProsperitocracyStatSystemStatics::ApplyBlockBodyRows(AbilitySystemComponent, ArmorWeave, /*bBare=*/ true);
	}

	ArmorWeave = Weave;

	if (Weave)
	{
		// 2. The armor's OWN home for its numbers — the same home a gun's numbers get, because an
		// armor is a thing the same way a gun is. Its Weight is a real attribute there, resolved by the
		// one evaluator, so anything that modifies an armor's weight is counted. That host's own guard
		// keeps the body rows off it, so each number of this block lives in exactly one place.
		if (!ArmorHost)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner();
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			const AActor* Owner = GetOwner();
			ArmorHost = GetWorld()
				? GetWorld()->SpawnActor<AProsperitocracyStatHostActor>(
					AProsperitocracyStatHostActor::StaticClass(),
					Owner ? Owner->GetActorTransform() : FTransform::Identity, SpawnParams)
				: nullptr;
		}
		if (ArmorHost)
		{
			ArmorHost->InitializeFromStatBlock(Weave);

			// And from here on the body listens to the armour's colour rows: a colour written by
			// anyone repaints the piece on the spot, with nothing in between. A weave carries no
			// colours, so wearing a different one keeps the colours the body already has — the look
			// is detached from the weave (Design/armor.md).
			BindArmorColorListeners();
		}

		// 3. And the weave's BODY rows — its two resists — onto this character, through the same door
		// the baseline block comes in through. That is what makes them the player's own numbers: the
		// one evaluator resolves them, and a resist perk lands on top of them like any other stat.
		UProsperitocracyStatSystemStatics::ApplyBlockBodyRows(AbilitySystemComponent, Weave, /*bBare=*/ false);
	}
	else if (ArmorHost)
	{
		// Bare: the armor's own numbers go away with it — and its colours with it, so the pieces go
		// back to the paint they ship with. The listener is let go BEFORE the host, so nothing fires
		// off a dead armour.
		UnbindArmorColorListeners();
		ArmorHost->Destroy();
		ArmorHost = nullptr;
	}

	// 4. What the body CARRIES just changed, so its Carried Weight row is written again — and that is
	// ALL this step has to do. The movement numbers follow that row by themselves (this component is
	// listening to it), so an armor needs no movement rule of its own and nothing has to remember which
	// numbers a weight change touched. One number, one writer — and every speed the body moves at is
	// re-priced, walk, run, crouch and jump, keeping the speed the body is already in.
	SyncCarriedWeight();

	// 5. And the look: every piece painted from its own colour row, read through the one evaluator.
	// A piece with no colour chosen goes back to the paint it ships with — which is also what a body
	// wearing no armour shows. One call and one log line, the same ones a colour change makes.
	ApplyArmorColors();

	// One line with the whole story — which weave, its three numbers as the ONE evaluator resolves
	// them, what the body now carries, and the speeds it moves at with it on.
	const UAbilitySystemComponent* ArmorASC = ArmorHost ? ArmorHost->GetProsperitocracyAbilitySystemComponent() : nullptr;
	UE_LOG(LogProsperitocracy, Log,
		TEXT("%s on %s: worn %s — impact resist %.1f%% | piercing resist %.1f%% | armor weight %.1f lbs | carried %.1f lbs (weight x%.2f) | run %.0f | walk %.0f | jump %.0f"),
		*GetName(), *GetNameSafe(GetOwner()), Weave ? *GetNameSafe(Weave) : TEXT("nothing"),
		GetStat(EProsperitocracyStat::ImpactResist), GetStat(EProsperitocracyStat::PiercingResist),
		UProsperitocracyStatSystemStatics::GetStatFinal(ArmorASC, EProsperitocracyStat::Weight),
		GetCarriedWeightLbs(), GetWeightSpeedMultiplier(), GetRunSpeed(), GetWalkSpeed(), GetJumpVelocity());
}

//~ The armor's trim (Design/armor.md) -------------------------------------------------------------------
//
// A trim colour is ONE number per region, carried on the armour's own GAS home beside its Weight.
// Setting it writes that number and nothing else; painting it reads that number and nothing else. The
// two halves meet at the row — which is why a colour set from a dev command today and a colour set
// from a customizer later are the same act, and why neither has to know about the other.

int32 UProsperitocracyPlayerStatsComponent::GetArmorColor(EProsperitocracyStat PieceColor) const
{
	if (!IsArmorColorPiece(PieceColor) || !ArmorHost)
	{
		return ProsperitocracyArmor::NoColor;
	}

	// The row's FINAL value through the one evaluator — never a raw base read.
	const UAbilitySystemComponent* ArmorASC = ArmorHost->GetProsperitocracyAbilitySystemComponent();
	return ArmorASC
		? FMath::RoundToInt(UProsperitocracyStatSystemStatics::GetStatFinal(ArmorASC, PieceColor))
		: ProsperitocracyArmor::NoColor;
}

bool UProsperitocracyPlayerStatsComponent::SetArmorColor(EProsperitocracyStat PieceColor, int32 Hex)
{
	if (!IsArmorColorPiece(PieceColor))
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("%s on %s: that is not one of the armor's colour rows, so there is no piece to paint."),
			*GetName(), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!ArmorHost)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("%s on %s: no armor worn, so there is nowhere for a colour to be carried."),
			*GetName(), *GetNameSafe(GetOwner()));
		return false;
	}

	// ONE number onto the armour's own row, through the thing's own door onto the evaluator. Nothing
	// else happens here: the row's own notification paints the piece, so a colour set from anywhere
	// lands the same way.
	ArmorHost->SetStatBase(PieceColor, static_cast<float>(Hex));
	return true;
}

USkeletalMeshComponent* UProsperitocracyPlayerStatsComponent::GetBodyMesh() const
{
	// The rig's answer, through the one place that can give it: the character. Never worked out from
	// here — C++ cannot tell the mesh on screen from the mesh the body is animated from, and painting
	// a guess is worse than painting nothing (the character's GetBodyMesh says exactly that).
	const AProsperitocracyCharacter* Character = Cast<AProsperitocracyCharacter>(GetOwner());
	return Character ? Character->GetBodyMesh() : nullptr;
}

int32 UProsperitocracyPlayerStatsComponent::FindArmorColorSlot(const USkeletalMeshComponent* Body, const TCHAR* SlotName) const
{
	if (!Body || !SlotName)
	{
		return INDEX_NONE;
	}

	// A piece is found by the NAME its slot carries on the mesh — the name the mesh was built with —
	// never by an index: slot order is the mesh's business, and the head's colour must not be able to
	// land on the legs because the slots were re-ordered or re-imported.
	const FString Wanted(SlotName);
	const TArray<FName> SlotNames = Body->GetMaterialSlotNames();
	for (int32 Slot = 0; Slot < SlotNames.Num(); ++Slot)
	{
		if (SlotNames[Slot].ToString().Contains(Wanted, ESearchCase::IgnoreCase))
		{
			return Slot;
		}
	}

	return INDEX_NONE;
}

void UProsperitocracyPlayerStatsComponent::EnsureArmorColorMIDs()
{
	if (bArmorColorMIDsMade)
	{
		return;
	}

	USkeletalMeshComponent* Body = GetBodyMesh();
	if (!Body)
	{
		// Not a failure to shout about, and not something to retry forever either: nothing has said
		// which mesh the body is DRAWN with, so there is nothing to paint. Leaving this un-made means
		// the moment the rig answers, the paint happens (a body can come up before its mesh is set).
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("%s on %s: nothing has said which mesh the body is drawn with, so the armor's colours have "
				 "nowhere to go. The character's GetBodyMesh must answer with the visible body."),
			*GetName(), *GetNameSafe(GetOwner()));
		return;
	}

	ArmorColorMIDs.SetNumZeroed(ProsperitocracyArmor::PieceCount);
	OriginalArmorColors.SetNumZeroed(ProsperitocracyArmor::PieceCount);

	for (int32 Index = 0; Index < ProsperitocracyArmor::PieceCount; ++Index)
	{
		const ProsperitocracyArmor::FPiece& Piece = ProsperitocracyArmor::Pieces[Index];

		const int32 Slot = FindArmorColorSlot(Body, Piece.SlotName);
		if (Slot == INDEX_NONE)
		{
			// Say which slots the mesh DOES have — a name is the whole fix, and hunting for it is the
			// only other way to find out.
			const TArray<FName> SlotNames = Body->GetMaterialSlotNames();
			TArray<FString> Names;
			for (const FName& Name : SlotNames)
			{
				Names.Add(Name.ToString());
			}

			UE_LOG(LogProsperitocracy, Warning,
				TEXT("%s on %s: the body mesh (%s) has no material slot for trim %s (looked for a slot name "
					 "containing '%s'; the mesh's slots are: %s)."),
				*GetName(), *GetNameSafe(GetOwner()), *GetNameSafe(Body->GetSkeletalMeshAsset()),
				Piece.Name, Piece.SlotName, *FString::Join(Names, TEXT(", ")));
			continue;
		}

		// The material COPY that makes a colour possible at all: the material asset is a file and
		// cannot be repainted while the game runs, but a copy of it can be, as often as we like — and
		// every player gets their own, which is why one player's colour is never another's. Made FROM
		// the material already on the slot, so the piece's own textures and paint stay underneath and
		// only the colour is ours.
		UMaterialInstanceDynamic* MID = Body->CreateDynamicMaterialInstance(Slot);
		if (!MID)
		{
			continue;
		}

		ArmorColorMIDs[Index] = MID;

		// What the piece looked like BEFORE we touched it: "nothing chosen" puts this back, so a piece
		// with no colour is the piece the pack shipped, never a black we invented.
		OriginalArmorColors[Index] = MID->K2_GetVectorParameterValue(ArmorColorParameterName);
	}

	bArmorColorMIDsMade = true;
}

void UProsperitocracyPlayerStatsComponent::ApplyArmorColors()
{
	EnsureArmorColorMIDs();

	// The colours live on the ARMOUR, so they are read from the armour's own ability system — the
	// same home its Weight is evaluated on. No armour, no colours (nothing chosen, all three).
	const UAbilitySystemComponent* ArmorASC = ArmorHost ? ArmorHost->GetProsperitocracyAbilitySystemComponent() : nullptr;

	TArray<FString> Pieces;
	Pieces.Reserve(ProsperitocracyArmor::PieceCount);

	for (int32 Index = 0; Index < ProsperitocracyArmor::PieceCount; ++Index)
	{
		const ProsperitocracyArmor::FPiece& Piece = ProsperitocracyArmor::Pieces[Index];

		// Read at the moment of painting, through the one evaluator: a colour that moved for any
		// reason is painted, with nobody having to remember to ask.
		const float Final = ArmorASC
			? UProsperitocracyStatSystemStatics::GetStatFinal(ArmorASC, Piece.Color)
			: static_cast<float>(ProsperitocracyArmor::NoColor);

		// Nothing chosen is not a colour: the piece goes back to the paint it ships with.
		const bool bChosen = Final >= 0.0f;
		const int32 Hex = bChosen ? FMath::Clamp(FMath::RoundToInt(Final), 0, 0xFFFFFF) : ProsperitocracyArmor::NoColor;

		UMaterialInstanceDynamic* MID = ArmorColorMIDs.IsValidIndex(Index) ? ArmorColorMIDs[Index] : nullptr;
		if (MID)
		{
			const FLinearColor Colour = bChosen ? ColorFromHex(Hex) : OriginalArmorColors[Index];
			MID->SetVectorParameterValue(ArmorColorParameterName, Colour);

			// Read it back: a colour that cannot land (a material with no parameter by that name) is a
			// fact worth hearing once, not a silent no-op nobody can explain later.
			if (bChosen && !bArmorColorParameterWarned
				&& !MID->K2_GetVectorParameterValue(ArmorColorParameterName).Equals(Colour, 0.01f))
			{
				bArmorColorParameterWarned = true;
				UE_LOG(LogProsperitocracy, Warning,
					TEXT("%s on %s: the body's material does not take a colour through '%s', so a colour cannot "
						 "land on it. The material's own parameter name is what this must be."),
					*GetName(), *GetNameSafe(GetOwner()), *ArmorColorParameterName.ToString());
			}
		}

		Pieces.Add(bChosen
			? FString::Printf(TEXT("trim %s #%06X"), Piece.Name, Hex)
			: FString::Printf(TEXT("trim %s own paint"), Piece.Name));
	}

	// One line with all three pieces, said once — the same shape as every other number this component
	// reports, so what is on the body can be read back out of the log without a screenshot.
	UE_LOG(LogProsperitocracy, Log, TEXT("%s on %s: armor colours — %s"),
		*GetName(), *GetNameSafe(GetOwner()), *FString::Join(Pieces, TEXT(" | ")));
}

void UProsperitocracyPlayerStatsComponent::BindArmorColorListeners()
{
	// Bound fresh each time the armour comes on: the binding is to the ARMOUR's ability system, and a
	// body that wears two weaves in a row must not end up listening twice.
	UnbindArmorColorListeners();

	UAbilitySystemComponent* ArmorASC = ArmorHost ? ArmorHost->GetProsperitocracyAbilitySystemComponent() : nullptr;
	if (!ArmorASC)
	{
		return;
	}

	for (const ProsperitocracyArmor::FPiece& Piece : ProsperitocracyArmor::Pieces)
	{
		const FGameplayAttribute Attribute = UProsperitocracyStatSystemStatics::GetAttributeForStat(Piece.Color);
		if (!Attribute.IsValid())
		{
			continue;
		}

		// The body is told by the NUMBER, not by whoever moved it — the same shape as the movement's
		// listeners. A command today, a customizer later, a re-tune: they all arrive here.
		ArmorColorChangeHandles.Add(
			ArmorASC->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddLambda([this](const FOnAttributeChangeData&) { ApplyArmorColors(); }));
	}
}

void UProsperitocracyPlayerStatsComponent::UnbindArmorColorListeners()
{
	UAbilitySystemComponent* ArmorASC = ArmorHost ? ArmorHost->GetProsperitocracyAbilitySystemComponent() : nullptr;
	if (ArmorASC)
	{
		// Removed BY HANDLE: the bindings are lambdas, so the delegate cannot find them by object.
		for (int32 Index = 0; Index < ProsperitocracyArmor::PieceCount; ++Index)
		{
			const FGameplayAttribute Attribute =
				UProsperitocracyStatSystemStatics::GetAttributeForStat(ProsperitocracyArmor::Pieces[Index].Color);
			if (Attribute.IsValid() && ArmorColorChangeHandles.IsValidIndex(Index))
			{
				ArmorASC->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(ArmorColorChangeHandles[Index]);
			}
		}
	}

	ArmorColorChangeHandles.Reset();
}

float UProsperitocracyPlayerStatsComponent::GetStat(EProsperitocracyStat Stat) const
{
	if (!AbilitySystemComponent)
	{
		return 0.0f;
	}
	return UProsperitocracyStatSystemStatics::GetStatFinal(AbilitySystemComponent, Stat);
}

float UProsperitocracyPlayerStatsComponent::GetWalkSpeed() const
{
	return GetRunSpeed() * WalkSpeedMultiplier;
}

float UProsperitocracyPlayerStatsComponent::GetCarriedWeightLbs() const
{
	const AActor* Owner = GetOwner();
	const UProsperitocracyLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UProsperitocracyLoadoutComponent>() : nullptr;
	return Loadout ? Loadout->GetCarriedWeightLbs() : 0.0f;
}

float UProsperitocracyPlayerStatsComponent::GetWeightSpeedMultiplier() const
{
	// What the body carries is read from its own ROW, through the one evaluator — the same value the
	// readout shows and the same one a weight perk moves. Nothing here adds the kit up itself: the sum
	// has one home (the row), and this is a read of a stat like any other.
	const float Lbs = GetStat(EProsperitocracyStat::CarriedWeight);
	const float Multiplier = 1.0f - (Lbs * WeightPenaltyPercentPerLb * 0.01f);

	// A multiplier below zero would run and jump the body BACKWARDS, which is never wanted — so the
	// penalty bottoms out at a standstill. Where that floor should really sit (a weight past which
	// nothing moves, or one that merely crawls) is a tuning question for heavy builds, and one worth
	// asking rather than inventing: this clamps the arithmetic, nothing more.
	return FMath::Clamp(Multiplier, 0.0f, 1.0f);
}

UCharacterMovementComponent* UProsperitocracyPlayerStatsComponent::GetMovementComponent() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
}

UCharacterMovementComponent* UProsperitocracyPlayerStatsComponent::GetMovingBody(int32& OutSlot) const
{
	UCharacterMovementComponent* Movement = GetMovementComponent();
	OutSlot = (Movement && Movement->IsCrouching()) ? 1 : 0;
	return Movement;
}

void UProsperitocracyPlayerStatsComponent::ApplyToMovement()
{
	PushWalkSpeed();
	ApplyJumpVelocity();

	const UCharacterMovementComponent* Movement = GetMovementComponent();
	UE_LOG(LogProsperitocracy, Log,
		TEXT("%s on %s: movement from stats — walk %.0f | run %.0f | crouch %.0f | jump %.0f | carried %.1f lbs (weight x%.2f)"),
		*GetName(), *GetNameSafe(GetOwner()), Movement ? Movement->MaxWalkSpeed : 0.0f, GetRunSpeed(),
		Movement ? Movement->MaxWalkSpeedCrouched : 0.0f, Movement ? Movement->JumpZVelocity : 0.0f,
		GetCarriedWeightLbs(), GetWeightSpeedMultiplier());
}

void UProsperitocracyPlayerStatsComponent::ApplyJumpVelocity()
{
	UCharacterMovementComponent* Movement = GetMovementComponent();
	if (!Movement)
	{
		return;
	}

	const float JumpVelocity = GetJumpVelocity();
	if (JumpVelocity > 0.0f)
	{
		Movement->JumpZVelocity = JumpVelocity;
	}
}

void UProsperitocracyPlayerStatsComponent::PushWalkSpeed()
{
	UCharacterMovementComponent* Movement = GetMovementComponent();
	if (!Movement)
	{
		return;
	}

	// A stat that is not set must never zero the body out: movement keeps whatever it had until there
	// is a number to give it.
	if (GetStat(EProsperitocracyStat::MoveSpeed) <= 0.0f)
	{
		return;
	}

	// Every speed the body moves at on the ground is this one number: the walk/run number, and the
	// crouch number, which IS the walk number — a crouch is not faster than a walk, so there is no
	// third fraction to keep in step with the stat. (Crouch speed used to be the engine's own default
	// with no stat behind it at all: it is the one speed Move Speed never reached, and now it is.)
	//
	// Floored at a standstill: a shot's push can take the last of the character's speed away, but it
	// can never walk them backwards — the same floor the weight penalty keeps, for the same reason.
	const float WalkSpeed = FMath::Max(0.0f, GetWalkSpeed());
	const float RunSpeed = FMath::Max(0.0f, GetRunSpeed());

	// WHICH of the two the body is moving at is the BODY's business: the walk/run choice belongs to the
	// movement state and its sprint key, not to this component. So the number already on the body
	// decides — a change re-prices the speed the body is IN (walk stays walk, run stays run) and never
	// picks one for it. It is told apart by which of the two numbers this component last published it
	// matched; there are only these two, so there is no third number being guessed at.
	const bool bMovingAtRunSpeed =
		FMath::Abs(Movement->MaxWalkSpeed - LastPublishedRunSpeed) < FMath::Abs(Movement->MaxWalkSpeed - LastPublishedWalkSpeed);

	Movement->MaxWalkSpeed = bMovingAtRunSpeed ? RunSpeed : WalkSpeed;

	// Crouch is not a third speed: a crouch is not faster than a walk, so it IS the walk number.
	Movement->MaxWalkSpeedCrouched = WalkSpeed;

	LastPublishedWalkSpeed = WalkSpeed;
	LastPublishedRunSpeed = RunSpeed;

	// If a shot's push is riding the body right now, the speed it rides ON changed too. The push
	// re-applies its base every frame and hands that base back when its window lapses, so leaving the
	// old base sitting in there would undo this re-price — and leave the body at the old weight's speed
	// for good. The base written is the pure state number, never the pushed one: a push must never be
	// compounded with itself.
	if (bShotPushActive)
	{
		int32 Slot = 0;
		if (GetMovingBody(Slot))
		{
			ShotPushBaseSpeed[Slot] = (Slot == 1) ? WalkSpeed : (bMovingAtRunSpeed ? RunSpeed : WalkSpeed);
		}
	}

	ShotPushLastWritten[0] = Movement->MaxWalkSpeed;
	ShotPushLastWritten[1] = Movement->MaxWalkSpeedCrouched;
}

float UProsperitocracyPlayerStatsComponent::GetAnimationRate() const
{
	// NORMAL, unless a shot is pushing the body. Nothing else slows the animation: not walking, not a
	// heavy load, not crouching. That is deliberate — a body that is permanently a little slower would
	// slow everything it plays, and a reload or a mantle is not something the gun in their hands gets
	// to re-time. The one thing that moves it is a shot's push, which is already moving the body and
	// is already fading on its own.
	if (!bShotPushActive)
	{
		return 1.0f;
	}

	int32 Slot = 0;
	const UCharacterMovementComponent* Movement = GetMovingBody(Slot);
	if (!Movement)
	{
		return 1.0f;
	}

	const float BodySpeed = ShotPushBaseSpeed[Slot];
	if (BodySpeed <= 0.0f)
	{
		return 1.0f;
	}

	// The push's own share, in the same percent form the movement gets: the speed the push is worth
	// this frame over the speed the body would have without it. Moving forward it is negative and the
	// legs slow; backpedalling it is positive and they cycle faster; across the shot's line, or
	// standing still, there is no push to ride and the animation is left at normal.
	const float PushShare = GetShotPushDrag() / BodySpeed;
	return FMath::Clamp(1.0f + PushShare, MinAnimationRate, 2.0f);
}

void UProsperitocracyPlayerStatsComponent::PushAnimationRate(float DeltaSeconds)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	// The body plays its animation at the rate the shot is moving it by: dragged, the legs cycle
	// slower; carried, faster; and back to normal the moment nothing is pushing them. It is read every
	// frame so the push's own decay is what brings it back, not a second timer in here.
	const float Target = GetAnimationRate();
	CurrentAnimationRate = (DeltaSeconds > 0.0f)
		? FMath::FInterpTo(CurrentAnimationRate, Target, DeltaSeconds, AnimationRateEase)
		: Target;

	// Written only when it actually moves, so an unchanged rate costs no churn on the mesh.
	if (!FMath::IsNearlyEqual(Mesh->GlobalAnimRateScale, CurrentAnimationRate, 0.0005f))
	{
		Mesh->GlobalAnimRateScale = CurrentAnimationRate;
	}
}

//~ The shot's push on the body -------------------------------------------------------------------

void UProsperitocracyPlayerStatsComponent::NotifyShotFired(float DragPercent, float CarryPercent, const FVector& ShotDirection)
{
	// A shot fired straight up or straight down has no line on the ground to be pushed along, and
	// guessing one would be inventing the shot's direction. Nothing to do.
	const FVector ShotForward = ShotDirection.GetSafeNormal2D();
	if (ShotForward.IsNearlyZero())
	{
		return;
	}

	const bool bWasIdle = !bShotPushActive;

	// ONE slot for the push. A shot while a push is already running replaces what is in it — the
	// newest gun's two numbers and the newest line — and restarts the window AND the decay. Two shots
	// can never add up into two pushes, which is what "never stacking" means here, and it is why a gun
	// that fires faster than the window (any gun faster than one shot a second) holds one steady push
	// near full strength for as long as the trigger is held.
	ShotPushBackwardAxis = -ShotForward;
	ShotPushDragPercent = DragPercent;
	ShotPushCarryPercent = CarryPercent;
	ShotPushEndTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + ShotPushSeconds;
	bShotPushActive = true;

	// The shot is felt on the frame it happened, not on the next tick.
	ApplyShotPushToWalkSpeed();

	if (bWasIdle)
	{
		UE_LOG(LogProsperitocracy, Log,
			TEXT("%s on %s: shot push — %.0f%% drag / %.0f%% carry of speed back along the shot's line, decaying over %.2fs | walk %.0f | anim x%.2f"),
			*GetName(), *GetNameSafe(GetOwner()), ShotPushDragPercent, ShotPushCarryPercent, ShotPushSeconds, GetWalkSpeed(), GetAnimationRate());
	}
}

//~ The picture's shake ---------------------------------------------------------------------------

void UProsperitocracyPlayerStatsComponent::NotifyViewShake(float ShakeDegrees)
{
	if (ShakeDegrees <= 0.0f)
	{
		return;
	}

	// THE MAN'S OWN SCREEN OR NOBODY'S. A shake is a picture, and a picture belongs to one man at one
	// screen: this finds the LOCAL player behind the body — the one whose controller this is — and hands
	// the number to the shake subsystem that player owns. A dummy, a man driven from another machine and
	// a dedicated server have no local player at all and get nowhere, which is what keeps a cosmetic
	// number off the wire entirely.
	const APawn* Body = Cast<APawn>(GetOwner());
	const APlayerController* OwningController = Body ? Cast<APlayerController>(Body->GetController()) : nullptr;
	ULocalPlayer* Screen = OwningController ? OwningController->GetLocalPlayer() : nullptr;
	if (!Screen)
	{
		return;
	}

	// The subsystem does the rest, and it is the ONLY thing that reaches a screen: a thing hands its own
	// number over and this component decides nothing about what it is worth or where it lands.
	if (UProsperitocracyViewShakeSubsystem* ViewShake = Screen->GetSubsystem<UProsperitocracyViewShakeSubsystem>())
	{
		ViewShake->AddShake(ShakeDegrees);
	}
}

float UProsperitocracyPlayerStatsComponent::GetShotPushDecay() const
{
	if (!bShotPushActive || ShotPushSeconds <= 0.0f)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	// The window runs from (end - ShotPushSeconds) to the end, and the push is worth FULL at the top
	// of it and nothing at the bottom: one straight ramp, the same decay for every gun. Every shot
	// puts it back to the top, so a burst of fire keeps it near full and letting go eases it away.
	const float Elapsed = World->GetTimeSeconds() - (ShotPushEndTime - ShotPushSeconds);
	return FMath::Clamp(1.0f - (Elapsed / ShotPushSeconds), 0.0f, 1.0f);
}

float UProsperitocracyPlayerStatsComponent::GetShotPushDrag() const
{
	if (!bShotPushActive || (ShotPushDragPercent <= 0.0f && ShotPushCarryPercent <= 0.0f))
	{
		return 0.0f;
	}

	int32 Slot = 0;
	UCharacterMovementComponent* Movement = GetMovingBody(Slot);
	if (!Movement)
	{
		return 0.0f;
	}

	FVector MoveDirection = Movement->Velocity;
	MoveDirection.Z = 0.0f;
	const float Speed = MoveDirection.Size();
	if (Speed <= UE_KINDA_SMALL_NUMBER)
	{
		// Standing still: the push is along a line the character is not moving along, so there is
		// nothing to fight and nothing to ride. It is also why firing on the spot never slides
		// anybody anywhere.
		return 0.0f;
	}
	MoveDirection /= Speed;

	// The whole mechanic, in one dot product: moving back along the shot's line rides the push
	// (positive — faster), moving forward fights it (negative — slower), moving across it is neither.
	// There is no branch anywhere for "walking backwards" or "walking forwards": the character's own
	// movement direction picks between the two, which is why it is the same one push either way.
	const float Along = FMath::Clamp(FVector::DotProduct(MoveDirection, ShotPushBackwardAxis), -1.0f, 1.0f);

	// Which of the gun's two numbers this is worth is decided by which way the character is spending
	// it: moving forward it is Drag (you are fighting it), moving back it is Carry (you are riding it,
	// and Carry is Drag at half). No branch on "walking backwards", no second mechanic.
	const float PushPercent = (Along >= 0.0f) ? ShotPushCarryPercent : ShotPushDragPercent;

	// And it is that percent OF THE SPEED THE BODY IS AT — the number the movement state put on it for
	// this frame, standing or crouched, walking, running or sprinting — so the same gun drags the same
	// at any of them. Then the decay: full on the shot, nothing by the end of the window.
	const float PushSpeed = ShotPushBaseSpeed[Slot] * (PushPercent * 0.01f);
	return PushSpeed * Along * GetShotPushDecay();
}

void UProsperitocracyPlayerStatsComponent::AdoptBodySpeedNumbers()
{
	UCharacterMovementComponent* Movement = GetMovementComponent();
	if (!Movement)
	{
		return;
	}

	// The standing number — walk, run, sprint, and whatever the movement state put on the body this
	// frame — is the STATE's, taken as its own. A number that is what this component last wrote is
	// ours (the push), not the state's, and is left out — so adopting can never mistake the push for
	// the state's number and compound it with itself.
	if (!FMath::IsNearlyEqual(Movement->MaxWalkSpeed, ShotPushLastWritten[0]))
	{
		ShotPushBaseSpeed[0] = Movement->MaxWalkSpeed;
	}

	// The crouch number is NOT the state's and is never adopted off the body: it comes from the stat,
	// through the one place that writes it (PushWalkSpeed), and a crouch IS the walk number — so the
	// test above can never tell this component's own crouch write apart from a state's write sitting
	// on the same value. Read that way the push was left holding the zero it starts life with: it
	// rode nothing while crouched, and it handed that zero to the body when the window lapsed, which
	// is what made crouching crawl after the first shot. Ask the same door PushWalkSpeed asks.
	ShotPushBaseSpeed[1] = FMath::Max(0.0f, GetWalkSpeed());
}

void UProsperitocracyPlayerStatsComponent::ApplyShotPushToWalkSpeed()
{
	int32 Slot = 0;
	UCharacterMovementComponent* Movement = GetMovingBody(Slot);
	if (!Movement)
	{
		return;
	}

	// The state's own numbers are taken first — both of them, so the push can move between standing
	// and crouched without ever riding on one of its own values.
	AdoptBodySpeedNumbers();

	// Then the push goes onto whichever number the body is actually moving at. The share is always
	// taken from the state's number, never from an already-pushed one, so it can never compound with
	// itself.
	float& BodySpeed = (Slot == 1) ? Movement->MaxWalkSpeedCrouched : Movement->MaxWalkSpeed;

	// Floored at a standstill, exactly as the stat's own number is: the push can stop the character
	// but it can never walk them backwards.
	BodySpeed = FMath::Max(0.0f, ShotPushBaseSpeed[Slot] + GetShotPushDrag());
	ShotPushLastWritten[Slot] = BodySpeed;
}

void UProsperitocracyPlayerStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The animation rate is read every frame, shot or no shot: the push's own decay moves it while a
	// shot runs, and the ease back to normal keeps going after the push has lapsed.
	PushAnimationRate(DeltaTime);

	if (!bShotPushActive)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() >= ShotPushEndTime)
	{
		// The window lapsed. The body gets its own numbers back — the push never leaves a slower (or
		// faster) body behind it, crouched or standing — and this component goes quiet until the next
		// shot.
		if (UCharacterMovementComponent* Movement = GetMovementComponent())
		{
			Movement->MaxWalkSpeed = ShotPushBaseSpeed[0];
			Movement->MaxWalkSpeedCrouched = ShotPushBaseSpeed[1];
			ShotPushLastWritten[0] = Movement->MaxWalkSpeed;
			ShotPushLastWritten[1] = Movement->MaxWalkSpeedCrouched;
		}

		bShotPushActive = false;
		ShotPushDragPercent = 0.0f;
		ShotPushCarryPercent = 0.0f;
		ShotPushBackwardAxis = FVector::ZeroVector;

		// The tick stays ON: it is what eases the animation back to normal after the push lapses. The
		// push's own work is gated on bShotPushActive, so a body that is not firing pays only for the
		// animation rate.

		const UCharacterMovementComponent* Movement = GetMovementComponent();
		UE_LOG(LogProsperitocracy, Log,
			TEXT("%s on %s: shot push done — walk speed back to %.0f | crouch %.0f"),
			*GetName(), *GetNameSafe(GetOwner()), Movement ? Movement->MaxWalkSpeed : 0.0f,
			Movement ? Movement->MaxWalkSpeedCrouched : 0.0f);
		return;
	}

	// Still alive: the push is re-applied every frame — the decay has moved it down, the character's
	// own movement may have turned under it, and the movement state may have written a fresh number
	// for the frame that this has to ride on top of.
	ApplyShotPushToWalkSpeed();
}
