// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

class UObject;

PROSPERITOCRACY_API DECLARE_LOG_CATEGORY_EXTERN(LogProsperitocracy, Log, All);
PROSPERITOCRACY_API DECLARE_LOG_CATEGORY_EXTERN(LogProsperitocracyAbilitySystem, Log, All);

PROSPERITOCRACY_API FString GetClientServerContextString(UObject* ContextObject = nullptr);
