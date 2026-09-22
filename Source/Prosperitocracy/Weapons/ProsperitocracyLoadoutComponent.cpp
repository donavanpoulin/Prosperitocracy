// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyLoadoutComponent.h"

#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "AbilitySystem/ProsperitocracyStatHostActor.h"
#include "Character/ProsperitocracyPlayerStatsComponent.h"
#include "Classes/ProsperitocracyClass.h"
#include "Components/ChildActorComponent.h"
#include "GameFramework/Pawn.h"
#include "ProsperitocracyLogChannels.h"
#include "ProsperitocracyGameplayTags.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "UObject/UObjectGlobals.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyLoadoutComponent)

UProsperitocracyLoadoutComponent::UProsperitocracyLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UProsperitocracyLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	// This character plays on its OWN COPY of every loadout it can play, never on the asset a class
	// ships with — and there is one place those copies are made, because a class chosen IN PLAY makes
	// the same ones (MakePlayingCopies).
	MakePlayingCopies();

	// And the rig is told what those loadouts carry, so it builds the weapons this body actually plays
	// with rather than the two the template assumes (see ApplyRigBodies).
	ApplyRigBodies();
}

void UProsperitocracyLoadoutComponent::ApplyRigBodies()
{
	AActor* Owner = GetOwner();
	UProsperitocracyLoadout* Playing = GetLoadout();
	if (!Owner || !Playing)
	{
		return;
	}

	// The rig's two channels, and the slot each one carries. A channel is found by the name the template
	// gave its component: this is the rig's own vocabulary, and there are exactly two of them.
	struct FRigChannel
	{
		const TCHAR* ComponentName;
		FGameplayTag Slot;
		const FProsperitocracyWeaponSlot* Entry;
	};
	const FRigChannel Channels[] =
	{
		{ TEXT("RifleChild"),  ProsperitocracyGameplayTags::Weapon_Slot_Primary,   &Playing->Primary   },
		{ TEXT("PistolChild"), ProsperitocracyGameplayTags::Weapon_Slot_Secondary, &Playing->Secondary },
	};

	TArray<UChildActorComponent*> RigBodies;
	Owner->GetComponents(RigBodies);

	for (const FRigChannel& Channel : Channels)
	{
		UChildActorComponent* RigBody = nullptr;
		for (UChildActorComponent* Candidate : RigBodies)
		{
			if (Candidate && Candidate->GetFName() == FName(Channel.ComponentName))
			{
				RigBody = Candidate;
				break;
			}
		}

		if (!RigBody)
		{
			continue;
		}

		// What this channel spawns: the slot's body, or NOTHING when the slot names nothing — a loadout
		// that carries no secondary has no second weapon, and an empty slot is a real state, not a gap.
		UClass* Body = Channel.Entry ? Channel.Entry->BodyClass.LoadSynchronous() : nullptr;
		if (RigBody->GetChildActorClass() != Body)
		{
			RigBody->SetChildActorClass(Body);

			UE_LOG(LogProsperitocracy, Log,
				TEXT("[Loadout] rig channel %s now carries %s for the %s slot"),
				Channel.ComponentName, *GetNameSafe(Body), *Channel.Slot.ToString());
		}

		// A slot's store belongs to the WEAPON in it — the block, not the channel: it is given up only
		// when the weapon itself changes, never because a channel was re-set to the same thing. Keyed on
		// the block for exactly that reason: the rig is re-dressed whenever anything about the body
		// changes, and a store that went with every re-dress would empty a gun that never changed hands.
		UProsperitocracyStatTable* Block = Channel.Entry ? Channel.Entry->StatBlock.LoadSynchronous() : nullptr;
		const TWeakObjectPtr<UProsperitocracyStatTable>* Filled = AmmoFilledFromBlock.Find(Channel.Slot);
		if (!Filled || Filled->Get() != Block)
		{
			AmmoBySlot.Remove(Channel.Slot);
			AmmoFilledFromBlock.Add(Channel.Slot, Block);

			UE_LOG(LogProsperitocracy, Log,
				TEXT("[Loadout] %s slot now holds %s — its ammo starts fresh from that weapon's own numbers"),
				*Channel.Slot.ToString(), *GetNameSafe(Block));
		}
	}
}

void UProsperitocracyLoadoutComponent::MakePlayingCopies()
{
	// A class's three loadouts ARE the defaults the game ships (Design/loadout.md), so this character's
	// copy is the only thing anything in play is allowed to change: the shipped default is never
	// written, a switch comes back to the copy with your changes still in it, and a save will hold the
	// copy when there is one.
	//
	// One copy per loadout, under the index it is played at. Index 0 is also where a class-less
	// character's single loadout lands: a character has a class or it does not, so the two never meet.
	//
	// What is in the map already belongs to the class being left — a copy of another class's loadout is
	// not a thing this character plays — so this REPLACES it rather than adding to it.
	PlayingCopies.Reset();

	UProsperitocracyLoadout* Sources[3] = { nullptr, nullptr, nullptr };
	if (Class)
	{
		Sources[0] = Class->Loadout1;
		Sources[1] = Class->Loadout2;
		Sources[2] = Class->Loadout3;
	}
	else
	{
		Sources[0] = Loadout;
	}

	for (int32 Index = 0; Index < 3; ++Index)
	{
		// A loadout this character does not have there is a real state — a class can carry fewer than
		// three — and there is nothing to copy for it.
		if (!Sources[Index])
		{
			continue;
		}

		// Owned by this component, so the copy lives exactly as long as the body does. Transient by
		// construction: it is runtime state, and there is nothing about it to author.
		if (UProsperitocracyLoadout* Copy = DuplicateObject<UProsperitocracyLoadout>(Sources[Index], this))
		{
			Copy->SetFlags(RF_Transient);
			PlayingCopies.Add(Index, Copy);
		}
	}
}

UProsperitocracyLoadout* UProsperitocracyLoadoutComponent::ResolveLoadoutSource(int32 Index) const
{
	// A character with no class plays the one loadout it was authored with, and that IS the whole of what
	// it has: index 0, and nothing at any other index. That is not a second path — it is the same
	// question ("what do I carry?") answered for a character with no class's three to choose between.
	if (!Class)
	{
		return (Index == 0) ? Loadout : nullptr;
	}

	switch (Index)
	{
	case 0:
		return Class->Loadout1;
	case 1:
		return Class->Loadout2;
	case 2:
		return Class->Loadout3;
	default:
		// An index that is not one of the three carries nothing, rather than quietly answering with a
		// loadout nobody selected. A loadout with nothing on it is a real state (Design/loadout.md).
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("%s on %s: loadout %d is not one of a class's three, so nothing is carried."),
			*GetName(), *GetNameSafe(GetOwner()), Index);
		return nullptr;
	}
}

UProsperitocracyLoadout* UProsperitocracyLoadoutComponent::GetLoadoutAt(int32 Index) const
{
	// The copy this character holds there, or — before it has copied anything (an editor-side read, or a
	// loadout it does not have) — the asset that copy would be made from, so a read never comes back
	// wrong just because of WHEN it was asked.
	if (const TObjectPtr<UProsperitocracyLoadout>* Copy = PlayingCopies.Find(Index))
	{
		return *Copy;
	}

	return ResolveLoadoutSource(Index);
}

UProsperitocracyLoadout* UProsperitocracyLoadoutComponent::GetLoadout() const
{
	// The one being played. Everything that has to know what this character carries asks HERE, so a
	// change made in play and a loadout switch are answered by the same object.
	return GetLoadoutAt(SelectedLoadout);
}

void UProsperitocracyLoadoutComponent::Redress()
{
	// The RIG first: what each of its channels carries has to be right before anything is dressed into
	// them — the dress below is what gives a gun its numbers, and the gun it must dress is the one this
	// loadout actually carries, not whatever the template left in the rig.
	ApplyRigBodies();

	// The armour, through the one door a body is dressed through: the loadout decides what the body is
	// wearing — a weave it names is worn, and a loadout that names none is a bare body.
	if (AActor* Owner = GetOwner())
	{
		if (UProsperitocracyPlayerStatsComponent* Stats = Owner->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
		{
			Stats->DressFromLoadout(GetLoadout());
		}
	}

	// And the GUNS, each through DressGun — the one act that gives a gun its numbers, whether that is
	// at spawn, after the rig brings a gun back, or here. They are collected first, because re-dressing
	// a gun records it in that map and a map is not something to walk while it is written.
	//
	// A slot this loadout carries nothing in answers NoEntry and its gun is left as it is: which gun
	// actors exist in the rig belongs to the rig, and a loadout with an empty slot is a real loadout.
	TArray<AProsperitocracyWeapon*> LiveGuns;
	LiveGuns.Reserve(DressedGunsBySlot.Num());
	for (const TPair<FGameplayTag, TWeakObjectPtr<AProsperitocracyWeapon>>& Dressed : DressedGunsBySlot)
	{
		if (AProsperitocracyWeapon* Gun = Dressed.Value.Get())
		{
			LiveGuns.Add(Gun);
		}
	}
	for (AProsperitocracyWeapon* Gun : LiveGuns)
	{
		DressGun(Gun);
	}
}

bool UProsperitocracyLoadoutComponent::SelectLoadout(int32 Index)
{
	if (!Class)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("%s on %s: no class is chosen, so there are no loadouts to pick from."),
			*GetName(), *GetNameSafe(GetOwner()));
		return false;
	}

	if (Index < 0 || Index > 2)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("%s on %s: %d is not one of a class's three loadouts (0, 1, 2)."),
			*GetName(), *GetNameSafe(GetOwner()), Index);
		return false;
	}

	SelectedLoadout = Index;

	// The loadout being played is now that index's COPY — the one made when the character came up, with
	// whatever has been changed in it since — and what it carries comes with it, through the same dress
	// a change made in play goes through.
	Redress();

	return true;
}

bool UProsperitocracyLoadoutComponent::SelectClass(UProsperitocracyClass* InClass)
{
	// A character plays AS a class, so a null is a caller's bug: nothing changes and it says so.
	if (!InClass)
	{
		UE_LOG(LogProsperitocracy, Warning,
			TEXT("%s on %s: no class was given, so nothing changed — a character plays as a class."),
			*GetName(), *GetNameSafe(GetOwner()));
		return false;
	}

	// Already playing it: nothing to re-copy, and asking again must never throw away the changes made
	// in play — the copy IS the thing those changes live in.
	if (InClass == Class)
	{
		return true;
	}

	Class = InClass;

	// The new class brings its OWN three loadouts, copied fresh — the copies of the class just left are
	// dropped, because another class's loadout is not a thing this character plays. The class ASSET is
	// still never written: what is played is this character's copy, made the same way as at spawn.
	MakePlayingCopies();

	// A class arrives playing its FIRST loadout, exactly as a character coming up does, and what that
	// loadout carries comes with it through the same dress every other change goes through.
	SelectedLoadout = 0;
	Redress();

	return true;
}

const FProsperitocracyWeaponSlot* UProsperitocracyLoadoutComponent::GetEntryForSlot(const FGameplayTag& Slot) const
{
	// What this character carries in that slot, asked of the loadout being PLAYED — a class's, when a
	// class is chosen, and the one it was authored with when none is. The authored field is never read
	// for an answer: it is only one of the two things the playing loadout can be.
	UProsperitocracyLoadout* Playing = GetLoadout();
	return Playing ? Playing->FindEntryForSlot(Slot) : nullptr;
}

TSubclassOf<UGameplayEffect> UProsperitocracyLoadoutComponent::GetGunDamageEffectClass() const
{
	UProsperitocracyLoadout* Playing = GetLoadout();
	return Playing ? Playing->GunDamageEffectClass : nullptr;
}

EProsperitocracyWeaponDressResult UProsperitocracyLoadoutComponent::DressGun(AProsperitocracyWeapon* Gun)
{
	if (!Gun)
	{
		return EProsperitocracyWeaponDressResult::Undressed;
	}

	// The loadout BEING PLAYED, asked once and used for every answer below — the body lookup, the block
	// and the damage effect all come from it. Reading the authored field here instead would dress a gun
	// from the loadout the character is not playing, which is not a wrong number but a wrong gun.
	UProsperitocracyLoadout* Playing = GetLoadout();
	if (!Playing)
	{
		return EProsperitocracyWeaponDressResult::NoLoadout;
	}

	// Which slot is this gun? The rig spawned it and said nothing, so the loadout answers by body.
	FGameplayTag Slot;
	int32 Matches = 0;
	const FProsperitocracyWeaponSlot* Entry = Playing->FindEntryForBodyClass(Gun->GetClass(), Slot, Matches);
	if (!Entry)
	{
		return (Matches > 1) ? EProsperitocracyWeaponDressResult::AmbiguousEntry
		                     : EProsperitocracyWeaponDressResult::NoEntry;
	}

	UProsperitocracyStatTable* StatBlock = Entry->StatBlock.LoadSynchronous();

	if (!StatBlock)
	{
		return EProsperitocracyWeaponDressResult::NoStatBlock;
	}

	// The weapon owns its slot: the tag is on its stat block. When the block says it belongs in a
	// different slot than the one carrying it, one of the two is wrong — and a gun dressed on a guess
	// is exactly the kind of quiet wrong answer this whole shape exists to prevent.
	if (StatBlock->GetSlot() != Slot)
	{
		return EProsperitocracyWeaponDressResult::SlotMismatch;
	}

	// This component goes with the numbers, because it is where the ammo for that slot lives.
	const bool bDressed = Gun->ApplyLoadoutEntry(Slot, StatBlock, Playing->GunDamageEffectClass, this, Cast<APawn>(GetOwner()));
	if (!bDressed)
	{
		return EProsperitocracyWeaponDressResult::Undressed;
	}

	// Remember the live gun for this slot: its weight is read from its own GAS home from now on, so a
	// gun whose weight was modified by something is counted for what it now is.
	DressedGunsBySlot.Add(Slot, Gun);

	// What this character carries just changed, so the body's Carried Weight row is written again — the
	// ONE number everything else reads, including the movement. The movement numbers follow that row by
	// themselves (the stats component listens to it), so nothing here has to know what to re-apply and
	// nothing can forget to: this only tells the truth about the kit, and the body gets the new weight.
	if (AActor* Owner = GetOwner())
	{
		if (UProsperitocracyPlayerStatsComponent* Stats = Owner->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
		{
			Stats->SyncCarriedWeight();
		}
	}

	return EProsperitocracyWeaponDressResult::Dressed;
}

float UProsperitocracyLoadoutComponent::GetCarriedWeightLbs() const
{
	float TotalLbs = 0.0f;

	// Everything in a slot, off the loadout being played — the same answer a gun and the armour get.
	if (UProsperitocracyLoadout* Playing = GetLoadout())
	{
		TArray<TPair<FGameplayTag, UProsperitocracyStatTable*>> Carried;
		Playing->CollectCarriedBlocks(Carried);

		for (const TPair<FGameplayTag, UProsperitocracyStatTable*>& Entry : Carried)
		{
			// A live gun first: its own GAS home is the truth about its weight once it exists.
			if (const TWeakObjectPtr<AProsperitocracyWeapon>* LiveGun = DressedGunsBySlot.Find(Entry.Key))
			{
				if (const AProsperitocracyWeapon* Gun = LiveGun->Get())
				{
					TotalLbs += Gun->GetWeaponStat(EProsperitocracyStat::Weight);
					continue;
				}
			}

			// Otherwise the block it will be built from — the same number the host would be handed.
			TotalLbs += Entry.Value->GetBaseValue(EProsperitocracyStat::Weight);
		}
	}

	// And what the character is WEARING, asked by that same rule: a thing that exists answers through
	// its own GAS home (so an armor whose weight something modified is counted for what it now is),
	// and a thing that is not up yet answers from the block it will be built from. One total, one
	// place, both kinds of carried thing in it.
	if (const AActor* Owner = GetOwner())
	{
		if (const UProsperitocracyPlayerStatsComponent* Stats = Owner->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
		{
			if (const AProsperitocracyStatHostActor* ArmorHost = Stats->GetArmorHost())
			{
				TotalLbs += UProsperitocracyStatSystemStatics::GetStatFinal(
					ArmorHost->GetProsperitocracyAbilitySystemComponent(), EProsperitocracyStat::Weight);
			}
			else if (const UProsperitocracyStatTable* ArmorWeave = Stats->GetWornWeave())
			{
				TotalLbs += ArmorWeave->GetBaseValue(EProsperitocracyStat::Weight);
			}
		}
	}

	return TotalLbs;
}

FProsperitocracyWeaponAmmo& UProsperitocracyLoadoutComponent::GetOrCreateAmmoForSlot(const FGameplayTag& Slot, int32 MagazineSize, int32 MagazineCapacity)
{
	// One store per slot. The first ask fills the magazine; every later ask — a gun the rig
	// re-created, a reload, the readout — is answered by this same store, which is what makes a free
	// magazine impossible rather than merely unlikely.
	if (FProsperitocracyWeaponAmmo* Existing = AmmoBySlot.Find(Slot))
	{
		return *Existing;
	}

	const int32 Rounds = FMath::Max(0, MagazineSize);
	const int32 Magazines = FMath::Max(0, MagazineCapacity);

	FProsperitocracyWeaponAmmo& Ammo = AmmoBySlot.Add(Slot);
	Ammo.Magazine = Rounds;
	Ammo.Spare = FMath::Max(0, Magazines * Rounds - Rounds);
	return Ammo;
}

const FProsperitocracyWeaponAmmo* UProsperitocracyLoadoutComponent::FindAmmoForSlot(const FGameplayTag& Slot) const
{
	return AmmoBySlot.Find(Slot);
}

bool UProsperitocracyLoadoutComponent::SetWeaponInSlot(FGameplayTag Slot, FProsperitocracyWeaponSlot Entry, FString& OutMessage)
{
	UProsperitocracyLoadout* Playing = GetLoadout();
	if (!Playing)
	{
		OutMessage = TEXT("this character has no loadout to change");
		return false;
	}

	if (!Slot.IsValid())
	{
		OutMessage = TEXT("that is not one of the weapon slots");
		return false;
	}

	// The entry's block, loaded once: every rule below is about it, and so is the answer.
	UProsperitocracyStatTable* NewBlock = Entry.StatBlock.LoadSynchronous();

	// The block says which slot it belongs in, and a block that disagrees with the slot it is being put
	// in is refused — the same check the dress path makes on a gun the rig built.
	if (NewBlock && NewBlock->GetSlot() != Slot)
	{
		OutMessage = FString::Printf(TEXT("%s belongs in %s, not %s"),
			*NewBlock->GetName(), *NewBlock->GetSlot().ToString(), *Slot.ToString());
		return false;
	}

	// The class's access rules, checked at the door a weapon goes into a slot through — the same door the
	// picker will use, so what is allowed here and what is allowed there cannot drift apart.
	if (Class)
	{
		const FString ClassName = Class->DisplayName.ToString();

		if (!Class->UsableSlots.HasTag(Slot))
		{
			OutMessage = FString::Printf(TEXT("the %s carries no %s weapon"), *ClassName, *Slot.ToString());
			return false;
		}

		// A class whose Primary IS its melee (the Reclaimer's blade) cannot have a gun there. Melee is a
		// weapon with no fire-mode tag (Design/weapons.md), so the block's own tag is the whole test.
		if (Slot == ProsperitocracyGameplayTags::Weapon_Slot_Primary && Class->bPrimaryMustBeMelee
			&& NewBlock && NewBlock->GetFireMode().IsValid())
		{
			OutMessage = FString::Printf(TEXT("the %s's Primary is its melee, so a gun cannot go there"), *ClassName);
			return false;
		}
	}

	FProsperitocracyWeaponSlot* Destination = Playing->FindMutableEntryForSlot(Slot);
	if (!Destination)
	{
		OutMessage = FString::Printf(TEXT("'%s' is not one of the loadout's slots"), *Slot.ToString());
		return false;
	}

	// The whole entry — the body AND the block, because those two together are what a gun is. One write,
	// into the loadout being played, and nowhere else.
	*Destination = Entry;

	// A different gun means different numbers, so this slot's ammo goes and the next ask fills it from
	// the new block: the same first ask that fills a gun at spawn — magazine at MagSize, and a spare pool
	// of Capacity x MagSize with the loaded magazine counted as one of them.
	AmmoBySlot.Remove(Slot);

	// And the change reaches the game the way every change does: the body is dressed from the loadout.
	Redress();

	OutMessage = NewBlock
		? FString::Printf(TEXT("%s into %s"), *NewBlock->GetName(), *Slot.ToString())
		: FString::Printf(TEXT("%s emptied"), *Slot.ToString());
	return true;
}

bool UProsperitocracyLoadoutComponent::SetWeave(UProsperitocracyStatTable* Weave, FString& OutMessage)
{
	UProsperitocracyLoadout* Playing = GetLoadout();
	if (!Playing)
	{
		OutMessage = TEXT("this character has no loadout to change");
		return false;
	}

	// Armour is not class-locked (Design/armor.md) — a weave is a weave for either class — so there is no
	// access rule to check here. Null is a real choice: a loadout that names no weave is a bare body.
	Playing->Weave = Weave;

	// The armour reaches the body the way it always does: dressed from the loadout, which wears the weave
	// and then paints the three regions the loadout names.
	Redress();

	OutMessage = Weave
		? FString::Printf(TEXT("wearing %s"), *Weave->GetName())
		: FString(TEXT("bare - no armour"));
	return true;
}

bool UProsperitocracyLoadoutComponent::SetTrim(EProsperitocracyStat Trim, int32 Hex, FString& OutMessage)
{
	UProsperitocracyLoadout* Playing = GetLoadout();
	if (!Playing)
	{
		OutMessage = TEXT("this character has no loadout to change");
		return false;
	}

	// The region, found in the ONE list of them (ProsperitocracyArmor::Pieces): the numbering is written
	// down there and nowhere else, so a region cannot be spoken of two ways.
	int32 Region = INDEX_NONE;
	for (int32 Index = 0; Index < ProsperitocracyArmor::PieceCount; ++Index)
	{
		if (ProsperitocracyArmor::Pieces[Index].Color == Trim)
		{
			Region = Index;
			break;
		}
	}

	if (Region == INDEX_NONE)
	{
		OutMessage = TEXT("that is not one of the armour's colour regions");
		return false;
	}

	// The colour is written into the loadout being played — one number, one holder — and then the body is
	// painted from it. A colour is not part of the weave and never needs one (Design/armor.md): it is the
	// body's own row, so a colour is one number set and nothing about the armour or the guns is re-read.
	switch (Region)
	{
	case 0: Playing->Trim1 = Hex; break;
	case 1: Playing->Trim2 = Hex; break;
	case 2: Playing->Trim3 = Hex; break;
	default: break;
	}

	if (AActor* Owner = GetOwner())
	{
		if (UProsperitocracyPlayerStatsComponent* Stats = Owner->FindComponentByClass<UProsperitocracyPlayerStatsComponent>())
		{
			Stats->SetArmorColor(Trim, Hex);
		}
	}

	OutMessage = (Hex == ProsperitocracyArmor::NoColor)
		? FString::Printf(TEXT("region %d back to its own paint"), Region + 1)
		: FString::Printf(TEXT("region %d #%06X"), Region + 1, Hex);
	return true;
}

