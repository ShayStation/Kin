// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"  
#include "AttributeSet.h"  
#include "AbilitySystemComponent.h"  
#include "GameplayEffectExtension.h"  
#include "GameplayEffectTypes.h"
#include "KinCharacterAttributeSet.generated.h"  

/**
*
*/
UCLASS()
class KIN_API UKinCharacterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UKinCharacterAttributeSet();

	/** Current Health */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UKinCharacterAttributeSet, Health)

		/** Max Health */
		UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UKinCharacterAttributeSet, MaxHealth)

		/** Current Stamina */
		UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UKinCharacterAttributeSet, Stamina)

		/** Max Stamina */
		UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UKinCharacterAttributeSet, MaxStamina)

		// Replication  
		virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldStamina);
	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);
};
