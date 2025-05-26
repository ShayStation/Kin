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

    // Called when the ability is activated
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

    // Called when the ability ends
    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled
    ) override;

protected:
    /** Projectile class to spawn */
    UPROPERTY(EditDefaultsOnly, Category = "Throw")
    TSubclassOf<AActor> ProjectileClass;

    /** Force applied to the projectile */
    UPROPERTY(EditDefaultsOnly, Category = "Throw")
    float ThrowStrength = 1200.f;
};
