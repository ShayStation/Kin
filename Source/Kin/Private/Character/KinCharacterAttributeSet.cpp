// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/KinCharacterAttributeSet.h"
#include "Net/UnrealNetwork.h"

UKinCharacterAttributeSet::UKinCharacterAttributeSet()
    : Health(100.f)
    , MaxHealth(100.f)
    , Stamina(100.f)
    , MaxStamina(100.f)
{
}

void UKinCharacterAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UKinCharacterAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UKinCharacterAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UKinCharacterAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UKinCharacterAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
}

void UKinCharacterAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UKinCharacterAttributeSet, Health, OldHealth);
}

void UKinCharacterAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UKinCharacterAttributeSet, MaxHealth, OldMaxHealth);
}

void UKinCharacterAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UKinCharacterAttributeSet, Stamina, OldStamina);
}

void UKinCharacterAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UKinCharacterAttributeSet, MaxStamina, OldMaxStamina);
}

