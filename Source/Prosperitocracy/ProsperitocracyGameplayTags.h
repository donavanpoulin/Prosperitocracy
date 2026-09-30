// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

namespace ProsperitocracyGameplayTags
{
	PROSPERITOCRACY_API	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString = false);

	// Declare all of the custom native tags that Prosperitocracy will use
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_IsDead);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cooldown);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cost);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_TagsBlocked);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_TagsMissing);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Networking);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_ActivationGroup);

	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Behavior_SurvivesDeath);

	// THE BAR: an ability carrying this tag is fired from the ability bar, so the player's 1–4 key for
	// the slot the loadout granted it in activates it. Empty means no number ever does — one absence
	// answering both an ability that is PASSIVE and one that lives on another input (the tag below,
	// ServesSecondPressOfSlot, names which input that is when it is that kind).
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Behavior_FiresFromBar);

	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Mouse);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Stick);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Crouch);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_AutoRun);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_QuickSlot_CycleForward);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_QuickSlot_CycleBackward);

	// The bash (a gun's melee). The tag is how the body's melee action addresses the bash ABILITY:
	// abilities are granted with an input tag and activated by it (UProsperitocracyAbilitySet), so
	// the character's melee needs no handle and no cast — it asks for the tag.
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Bash);

	// THE FOUR BAR NUMBERS (1–4). A granted spec carries the tag of the slot whose number it answers,
	// stamped by the loadout at grant time — and only on an ability that carries
	// Ability.Behavior.FiresFromBar. So the key addresses a SLOT and the input path finds whatever the
	// loadout put there; nothing anywhere looks an ability up by what it does, and the same ability
	// answers a different number just by being granted into a different slot.
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Slot1);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Slot2);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Slot3);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Slot4);

	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_Spawned);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataAvailable);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataInitialized);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_GameplayReady);

	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Death);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Reset);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_RequestReset);

	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Heal);

	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cheat_GodMode);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cheat_UnlimitedHealth);

	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Crouching);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_AutoRunning);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dying);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dead);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Stun);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Burn);

	// Damage types — the ONLY two. Fire/burn, stun, lasers, bullets, explosives, push, grenades,
	// vehicle damage are all either Impact or Piercing (or an effect/status), never separate damage types.
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Impact);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Piercing);

	// THE EXPLOSION MARK (Design/explosions.md). A tag on a thing's STAT BLOCK, and that block plus
	// this tag is the ENTIRE hook-up: carrying it is what enrols the thing in the one explosion pass —
	// the ball, the damage and its falloff, the push, the shake and the sound — so hooking up a new
	// explosive is putting one tag on its block and nothing else anywhere. Presence is scope, as
	// everywhere else in this project: no tag, no bang.
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Explosive);

	// Fire modes — tags on weapons, NOT stats (Design/weapons.md): player-only, no perks, no evaluator.
	// Every ranged weapon carries exactly one; anything without a fire-mode tag isn't a gun.
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_FireMode_FullAuto);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_FireMode_SemiAuto);

	// Slots — the tag IS the slot (Design/weapons.md): a weapon carries exactly one, and it goes in
	// that slot, period. Not a stat: no number, and the evaluator never touches it.
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Slot_Primary);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Slot_Secondary);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Slot_Special);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Slot_Grenade);

	// The steady-aim (ADS) camera mode tag — the blend weight of this camera IS how far into
	// ADS the player is (weapon feel: posture multipliers + reticle circle opacity read it).
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_SteadyAimingCamera);

	// Enemy tiers (Space Marine 2 style) — perks reference tiers ("kill T3+ enemies...").
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Tier_T1);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Tier_T2);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Tier_T3);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Tier_T4);

	// These are mappings from MovementMode enums to GameplayTags associated with those enums (below)
	PROSPERITOCRACY_API	extern const TMap<uint8, FGameplayTag> MovementModeTagMap;
	PROSPERITOCRACY_API	extern const TMap<uint8, FGameplayTag> CustomMovementModeTagMap;

	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Walking);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_NavWalking);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Falling);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Swimming);
	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Flying);

	PROSPERITOCRACY_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Custom);
};
