// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/GA_Throw.h"
#include "Character/KinCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ThrowAimComponent.h"
#include "Abilities/ThrownProjectile.h"
#include "Types/KinAbilityInputID.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

UGA_Throw::UGA_Throw()
{
    // One instance per actor, state isolated
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    // Only run on server (spawning + physics)
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

    //Auto find our BP_ThrownProjectile so ProjectileClass isn't null
    static ConstructorHelpers::FClassFinder<AActor> ProjBP(
    TEXT("/Game/Blueprints/Abilities/BP_ThrownProjectile")
    );
    if (ProjBP.Succeeded())
        {
        ProjectileClass = ProjBP.Class;
        }
}

void UGA_Throw::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData
)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AKinCharacterBase* Char = Cast<AKinCharacterBase>(ActorInfo->AvatarActor.Get());
    if (!Char || !Char->HasAuthority())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
        return;
    }

    UThrowAimComponent* AimComp = Char->FindComponentByClass<UThrowAimComponent>();
    if (AimComp)
    {
        FVector Start, Velocity;
        if (AimComp->ComputeThrow(Start, Velocity))
        {
            FActorSpawnParameters Params;
            Params.Owner = Char;
            Params.Instigator = Char;

            AThrownProjectile* Proj = Cast<AThrownProjectile>(
                Char->GetWorld()->SpawnActor<AActor>(
                    ProjectileClass,
                    Start,
                    Velocity.Rotation(),
                    Params
                )
            );

            if (Proj)
            {
                Proj->InitTrajectory(
                    Start,
                    Velocity,
                    AimComp->ProjectileGravityScale,
                    AimComp->TimeScale  // use the tunable speed multiplier
                );
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("GA_Throw: failed to spawn projectile"));
            }
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_Throw::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled
)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
