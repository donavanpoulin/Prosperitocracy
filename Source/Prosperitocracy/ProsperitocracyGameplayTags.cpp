// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProsperitocracyGameplayTags.h"

#include "Engine/EngineTypes.h"
#include "GameplayTagsManager.h"
#include "ProsperitocracyLogChannels.h"

namespace ProsperitocracyGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_IsDead, "Ability.ActivateFail.IsDead", "Ability failed to activate because its owner is dead.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cooldown, "Ability.ActivateFail.Cooldown", "Ability failed to activate because it is on cool down.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cost, "Ability.ActivateFail.Cost", "Ability failed to activate because it did not pass the cost checks.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_TagsBlocked, "Ability.ActivateFail.TagsBlocked", "Ability failed to activate because tags are blocking it.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_TagsMissing, "Ability.ActivateFail.TagsMissing", "Ability failed to activate because tags are missing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Networking, "Ability.ActivateFail.Networking", "Ability failed to activate because it did not pass the network checks.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_ActivationGroup, "Ability.ActivateFail.ActivationGroup", "Ability failed to activate because of its activation group.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Behavior_SurvivesDeath, "Ability.Behavior.SurvivesDeath", "An ability with this type tag should not be canceled due to death.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "InputTag.Move", "Move input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Mouse, "InputTag.Look.Mouse", "Look (mouse) input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Stick, "InputTag.Look.Stick", "Look (stick) input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Crouch, "InputTag.Crouch", "Crouch input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_AutoRun, "InputTag.AutoRun", "Auto-run input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickSlot_CycleForward, "InputTag.Ability.Quickslot.CycleForward", "Quickbar cycle forward (mouse wheel up).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickSlot_CycleBackward, "InputTag.Ability.Quickslot.CycleBackward", "Quickbar cycle backward (mouse wheel down).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_Spawned, "InitState.Spawned", "1: Actor/component has initially spawned and can be extended");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_DataAvailable, "InitState.DataAvailable", "2: All required data has been loaded/replicated and is ready for initialization");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_DataInitialized, "InitState.DataInitialized", "3: The available data has been initialized for this actor/component, but it is not ready for full gameplay");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_GameplayReady, "InitState.GameplayReady", "4: The actor/component is fully ready for active gameplay");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Death, "GameplayEvent.Death", "Event that fires on death. This event only fires on the server.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Reset, "GameplayEvent.Reset", "Event that fires once a player reset is executed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_RequestReset, "GameplayEvent.RequestReset", "Event to request a player's pawn to be instantly replaced with a new one at a valid spawn location.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "SetByCaller tag used by damage gameplay effects.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Heal, "SetByCaller.Heal", "SetByCaller tag used by healing gameplay effects.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cheat_GodMode, "Cheat.GodMode", "GodMode cheat is active on the owner.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cheat_UnlimitedHealth, "Cheat.UnlimitedHealth", "UnlimitedHealth cheat is active on the owner.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Crouching, "Status.Crouching", "Target is crouching.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_AutoRunning, "Status.AutoRunning", "Target is auto-running.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death, "Status.Death", "Target has the death status.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dying, "Status.Death.Dying", "Target has begun the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dead, "Status.Death.Dead", "Target has finished the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Stun, "Status.Stun", "Stun status. Duration only (no rate, no piercing damage stat).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Burn, "Status.Burn", "Burn status (fire). Stats: Rate, Duration, Piercing Damage. Deals Piercing.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Impact, "Damage.Type.Impact", "Impact damage type. One of the ONLY two damage types (blunt melee, explosions, grenades, push, mech stomp, vehicle ram, thrown objects).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Piercing, "Damage.Type.Piercing", "Piercing damage type. One of the ONLY two damage types (sword, bullets, lasers, fire, turrets, drones, burn).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_FireMode_FullAuto, "Prosperitocracy.Weapon.FireMode.FullAuto", "Ranged weapon fire mode: fires continuously while the trigger is held (a tag on the weapon, not a stat).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_FireMode_SemiAuto, "Prosperitocracy.Weapon.FireMode.SemiAuto", "Ranged weapon fire mode: one shot per trigger press (a tag on the weapon, not a stat).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Slot_Primary, "Prosperitocracy.Weapon.Slot.Primary", "Primary weapon slot: the two rifles (Design/weapons.md). The tag IS the slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Slot_Secondary, "Prosperitocracy.Weapon.Slot.Secondary", "Secondary weapon slot: pistol and SMG (Design/weapons.md). The tag IS the slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Slot_Special, "Prosperitocracy.Weapon.Slot.Special", "Special weapon slot (Design/weapons.md). The tag IS the slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Slot_Grenade, "Prosperitocracy.Weapon.Slot.Grenade", "Grenade slot: one grenade at a time (Design/weapons.md). The tag IS the slot.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_SteadyAimingCamera, "Prosperitocracy.Weapon.SteadyAimingCamera", "The steady-aim (ADS) camera mode: its blend weight is how far into ADS the player is.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Tier_T1, "Enemy.Tier.T1", "Enemy tier 1 (e.g. Zombies).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Tier_T2, "Enemy.Tier.T2", "Enemy tier 2 (e.g. Robots).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Tier_T3, "Enemy.Tier.T3", "Enemy tier 3 (e.g. Cybertank).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Tier_T4, "Enemy.Tier.T4", "Enemy tier 4 (T4 boss; pending design).");
						  
	// These are mapped to the movement modes inside GetMovementModeTagMap()
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_Walking, "Movement.Mode.Walking", "Default Character movement tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_NavWalking, "Movement.Mode.NavWalking", "Default Character movement tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_Falling, "Movement.Mode.Falling", "Default Character movement tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_Swimming, "Movement.Mode.Swimming", "Default Character movement tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_Flying, "Movement.Mode.Flying", "Default Character movement tag");

	// When extending Prosperitocracy, you can create your own movement modes but you need to update GetCustomMovementModeTagMap()
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_Custom, "Movement.Mode.Custom", "This is invalid and should be replaced with custom tags.  See ProsperitocracyGameplayTags::CustomMovementModeTagMap.");

	// Unreal Movement Modes
	const TMap<uint8, FGameplayTag> MovementModeTagMap =
	{
		{ MOVE_Walking, Movement_Mode_Walking },
		{ MOVE_NavWalking, Movement_Mode_NavWalking },
		{ MOVE_Falling, Movement_Mode_Falling },
		{ MOVE_Swimming, Movement_Mode_Swimming },
		{ MOVE_Flying, Movement_Mode_Flying },
		{ MOVE_Custom, Movement_Mode_Custom }
	};

	// Custom Movement Modes
	const TMap<uint8, FGameplayTag> CustomMovementModeTagMap =
	{
		// Fill these in with your custom modes
	};

	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString)
	{
		const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagString), false);

		if (!Tag.IsValid() && bMatchPartialString)
		{
			FGameplayTagContainer AllTags;
			Manager.RequestAllGameplayTags(AllTags, true);

			for (const FGameplayTag& TestTag : AllTags)
			{
				if (TestTag.ToString().Contains(TagString))
				{
					UE_LOG(LogProsperitocracy, Display, TEXT("Could not find exact match for tag [%s] but found partial match on tag [%s]."), *TagString, *TestTag.ToString());
					Tag = TestTag;
					break;
				}
			}
		}

		return Tag;
	}
}

