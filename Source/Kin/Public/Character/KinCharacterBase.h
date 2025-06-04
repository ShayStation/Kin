// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "AbilitySystemInterface.h"                         // /Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AbilitySystemInterface.h
#include "AbilitySystemComponent.h"                         // /Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AbilitySystemComponent.h
#include "GameplayAbilitySpec.h"                            // /Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/GameplayAbilitySpec.h
#include "GameplayEffectTypes.h"                            // /Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/GameplayEffectTypes.h
#include "KinCharacterAttributeSet.h"
#include "Components/ThrowAimComponent.h"

#include "InputAction.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "KinCharacterBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS(config = Game)
class KIN_API AKinCharacterBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AKinCharacterBase();
    virtual void Tick(float DeltaTime) override;

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;


protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void NotifyControllerChanged() override;


    // Movement & Look
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void MoveForward(const FInputActionValue& Value);
    void MoveRight(const FInputActionValue& Value);

    //Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throw")
    UThrowAimComponent * ThrowAimComponent;

    // Grant default abilities
    void InitializeAbilities();

    // Camera components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    UCameraComponent* FollowCamera;

    // Zoom levels
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
    float ZoomClose = 400.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
    float ZoomNormal = 600.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
    float ZoomFar = 800.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
    float BoomPitchClose = -10.0f;

    /** Pitch the boom to when ZoomIndex==1 (normal) */
    UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
    float BoomPitchNormal = -30.0f;

    /** Pitch the boom to when ZoomIndex==2 (far) */
    UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
    float BoomPitchFar = -40.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
    float OcclusionInterpSpeed = 10.f; // smoothing speed for occlusion adjustments

    float CameraPanSpeed = 100.f; // Speed for camera rotation input



    // Fade-on-close settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
    float CharacterFadeDistance = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
    FName FadeParamName = TEXT("Opacity");

    // Silhouette-on-occlude
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
    UMaterialInterface* SilhouetteMaterial;

    /** Input mapping context */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputMappingContext* DefaultMappingContext;


    // Input actions
    //UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    //UInputAction* JumpAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* IA_Move;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* IA_Look;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* IA_MoveForward;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* IA_MoveRight;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* LookAction;

    // Ability input actions
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* IA_Throw;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* IA_ManualLockOn;

    // Camera input actions
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* ToggleZoomAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* SetOverheadAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
    UInputAction* RotateCameraAction;

    // GAS Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GAS, meta = (AllowPrivateAccess = "true"))
    UAbilitySystemComponent* AbilitySystemComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GAS, meta = (AllowPrivateAccess = "true"))
    UKinCharacterAttributeSet* AttributeSet;
    UPROPERTY(EditDefaultsOnly, Category = GAS, meta = (AllowPrivateAccess = "true"))
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;


    // Occlusion settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
    float OcclusionProbeRadius = 50.f;



    // Input callbacks for camera
    void ToggleZoom();
    void SetOverheadView();
    void RotateCamera(const FInputActionValue& Value);
    void PerformManualLock();
    UFUNCTION()
    void ToggleManualLock(const FInputActionValue& Value);

    // Camera helper methods
    void UpdateCameraFraming();

    // New fade & silhouette handlers
    void HandleCloseFade();


private:
    int32 CurrentZoomIndex = 1;
    bool bOverheadMode = false;
    // Store dynamic materials for fade
    TArray<UMaterialInstanceDynamic*> DynamicMaterials;
    // Original materials for silhouette
    TArray<UMaterialInterface*> OriginalMaterials;

    // The desired springarm length we’re interpolating toward
    float DesiredArmLength;

    /** Desired boom pitch we’re interpolating toward */
    float DesiredBoomPitch;

    /** How quickly to interpolate zoom (higher = snappier) */
    UPROPERTY(EditAnywhere, Category = "Camera")
    float ZoomInterpSpeed = 5.f;

public:
    FORCEINLINE UThrowAimComponent* GetThrowAimComponent() const { return ThrowAimComponent; }
};
