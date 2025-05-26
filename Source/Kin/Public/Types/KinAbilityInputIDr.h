#pragma once

#include "CoreMinimal.h"

/**
 * Maps EnhancedInput actions (IA_*) to GameplayAbility InputIDs.
 */
UENUM(BlueprintType)
enum class EKinAbilityInputID : uint8
{
    None        UMETA(DisplayName = "None"),
    Throw       UMETA(DisplayName = "Throw"),
    // Add future abilities here, e.g. Dash, Grapple, etc.
};