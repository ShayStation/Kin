// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Types/KinAbilityInputID.h"
#include "GA_Throw.generated.h"

/**
 * 
 */
UCLASS()
class KIN_API UGA_Throw : public UGameplayAbility
{
	GENERATED_BODY()

public:
    UGA_Throw();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled
    ) override;

protected:
    // Class to spawn as the projectile
    UPROPERTY(EditDefaultsOnly, Category = "Throw")
    TSubclassOf<AActor> ProjectileClass;

    // Initial launch speed (used by SuggestProjectileVelocity)
    UPROPERTY(EditDefaultsOnly, Category = "Throw")
    float LaunchSpeed = 1500.0f;

    // Gravity scale on the projectile movement component
    UPROPERTY(EditDefaultsOnly, Category = "Throw")
    float ProjectileGravityScale = 0.4f;

    // If >0, clamps the arc apex to this height above spawn
    UPROPERTY(EditDefaultsOnly, Category = "Throw")
    float MaxArcHeight = 500.0f;

};
