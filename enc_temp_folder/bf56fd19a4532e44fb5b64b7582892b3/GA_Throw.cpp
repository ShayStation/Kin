// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/GA_Throw.h"
#include "Character/KinCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ThrowAimComponent.h"
#include "Types/KinAbilityInputID.h"

UGA_Throw::UGA_Throw()
{
    // One instance per actor, state isolated
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    // Only run on server (spawning + physics)
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

}

void UGA_Throw::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData
)
{

    if (GEngine && ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        FString CharName = ActorInfo->AvatarActor->GetName();
        GEngine->AddOnScreenDebugMessage(
            -1, 2.f, FColor::Emerald,
            FString::Printf(TEXT("GA_Throw::ActivateAbility on %s"), *CharName)
        );
        UE_LOG(LogTemp, Warning, TEXT("GA_Throw activated by %s"), *CharName);
    }

    // 1) Commit cost/tags/etc.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 2) Server-only spawn + impulse
    AKinCharacterBase* Char = Cast<AKinCharacterBase>(ActorInfo->AvatarActor.Get());
    if (Char && ProjectileClass && Char->HasAuthority())
    {
        // 2a) Determine aim point via your ThrowAimComponent
        FVector AimPoint, AimNormal;
        Char->GetThrowAimComponent()->UpdateAimPoint(AimPoint, AimNormal, true);

        // 2b) Choose spawn location (e.g. hand socket)
        const FVector SpawnLoc = Char->GetMesh()->GetSocketLocation(TEXT("ThrowSocket"));
        // 2c) Compute launch rotation toward that point
        const FRotator LaunchRot = (AimPoint - SpawnLoc).Rotation();

        // 2d) Spawn the projectile actor
        FActorSpawnParameters Params;
        Params.Owner = Char;
        Params.Instigator = Char;

        if (AActor* Proj = Char->GetWorld()->SpawnActor<AActor>(
            ProjectileClass, SpawnLoc, LaunchRot, Params
        ))
        {
            // 2e) Apply impulse in its forward vector
            if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(
                Proj->GetComponentByClass(UPrimitiveComponent::StaticClass())))
            {
                Prim->AddImpulse(LaunchRot.Vector() * ThrowStrength, NAME_None, true);
            }
        }
    }

    // 3) Immediately end the ability (no further tick)
    EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
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