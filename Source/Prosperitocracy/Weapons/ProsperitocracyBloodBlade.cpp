// Copyright Prosperitocracy. All Rights Reserved.

#include "Weapons/ProsperitocracyBloodBlade.h"

#include "Character/ProsperitocracyCharacter.h"
#include "ProsperitocracyLogChannels.h"
#include "Stats/ProsperitocracyStat.h"
#include "Weapons/ProsperitocracyLoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyBloodBlade)

int32 AProsperitocracyBloodBlade::GetBloodPoolSize() const
{
	// The pool is the blade's own MagSize, read FINAL through the blade's own GAS home — so a perk on
	// the pool is already in this number, and the swing's cost and the drain (both derived from it)
	// move with it. One number, three readers.
	return FMath::Max(0, FMath::RoundToInt(GetWeaponStat(EProsperitocracyStat::MagSize)));
}

int32 AProsperitocracyBloodBlade::GetSwingCost() const
{
	// A share of the pool, with a floor so a pool too small to share out still costs a swing something.
	// Both numbers are universal constants; the pool is the only authored one, and it is a row.
	return FMath::Max(ProsperitocracyBloodHandling::SwingCostFloor,
		FMath::RoundToInt(GetBloodPoolSize() * ProsperitocracyBloodHandling::SwingCostShareOfPool));
}

float AProsperitocracyBloodBlade::GetDrainPerSecond() const
{
	if (ProsperitocracyBloodHandling::DrainWindowSeconds <= 0.0f)
	{
		return 0.0f;
	}

	// The pool over its window — the same shape as contested health's fade, so the number the player
	// reads (the pool) is the only thing there is to move.
	return static_cast<float>(GetBloodPoolSize()) / ProsperitocracyBloodHandling::DrainWindowSeconds;
}

FProsperitocracyWeaponAmmo* AProsperitocracyBloodBlade::GetBloodStore()
{
	// The blood lives where a gun's magazine lives: the slot's store on the carrier, filled from this
	// thing's own numbers the first time it is asked for. A blade has NO Capacity row, and an absent
	// Capacity is worth no spare magazines — so the pool is the whole of the blood, which is exactly
	// "the ammo, minus the spare".
	if (!EnsureInitialized() || !OwnerLoadout || !Slot.IsValid())
	{
		return nullptr;
	}

	const int32 Capacity = FMath::Max(0, FMath::RoundToInt(GetWeaponStat(EProsperitocracyStat::Capacity)));
	return &OwnerLoadout->GetOrCreateAmmoForSlot(Slot, GetBloodPoolSize(), Capacity);
}

int32 AProsperitocracyBloodBlade::GetBloodLeft() const
{
	// Read through the weapon's own public answer for its magazine, so the pool is read the same way a
	// gun's rounds are — it IS the same number in the same store.
	return GetMagazineAmmo();
}

bool AProsperitocracyBloodBlade::IsInHand() const
{
	// The rig decides which weapon is out — this body does not get to say it is the one being held. A
	// blade that is carried but not out costs nothing.
	const AProsperitocracyCharacter* Character = Cast<AProsperitocracyCharacter>(OwningPawn.Get());
	return Character && Character->GetGunInHand() == this;
}

bool AProsperitocracyBloodBlade::SpendSwingCost()
{
	FProsperitocracyWeaponAmmo* Blood = GetBloodStore();
	const int32 Cost = GetSwingCost();

	// No pool to take from, or not enough in it: the swing is refused rather than going into debt. A
	// swing is not a shot — there is no cadence here, because the combo's own parts are its cadence
	// and the animation is what decides when the next one may come.
	if (!Blood || Blood->Magazine < Cost)
	{
		return false;
	}

	Blood->Magazine -= Cost;
	return true;
}

void AProsperitocracyBloodBlade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Only the blade that is OUT drains, and only what the pool can cover: the tick never goes into
	// debt, so killing to refuel stays the only way back up once the blood runs low.
	if (!IsInHand())
	{
		DrainRemainder = 0.0f;
		return;
	}

	FProsperitocracyWeaponAmmo* Blood = GetBloodStore();
	if (!Blood || Blood->Magazine <= 0)
	{
		return;
	}

	// The blood is spent in whole points and a drain is a fraction of one a second, so the fraction is
	// carried between frames — otherwise every frame's share would round away and nothing would ever
	// leave the pool.
	DrainRemainder += GetDrainPerSecond() * DeltaSeconds;

	const int32 Whole = FMath::FloorToInt(DrainRemainder);
	if (Whole > 0)
	{
		Blood->Magazine = FMath::Max(0, Blood->Magazine - Whole);
		DrainRemainder -= Whole;
	}
}
