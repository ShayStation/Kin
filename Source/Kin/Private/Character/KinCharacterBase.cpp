// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/KinCharacterBase.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"
#include "Logging/LogMacros.h"
#include "Kismet/KismetMathLibrary.h"




AKinCharacterBase::AKinCharacterBase()
{
    PrimaryActorTick.bCanEverTick = true;

    // Auto-possess Player0
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    // Collision & movement
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);

    // Camera boom at head height
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 80.f));

    // Manual camera control: do not inherit controller rotation
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritYaw = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 12.f;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 12.f;
    CameraBoom->bDoCollisionTest = false;

    // Follow camera
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom);
    FollowCamera->bUsePawnControlRotation = false;

    // Initialize zoom
    CurrentZoomIndex = 1;
    CameraBoom->TargetArmLength = ZoomNormal;
    DesiredArmLength = CameraBoom->TargetArmLength;
    DesiredBoomPitch = CameraBoom->GetRelativeRotation().Pitch;

    // Initialize DesiredBoomPitch based on your starting zoom index
    DesiredBoomPitch = (CurrentZoomIndex == 0) ? BoomPitchClose
        : (CurrentZoomIndex == 1) ? BoomPitchNormal
        : BoomPitchFar;

    // Immediately apply that pitch to the spring-arm so it starts at Normal
    {
        FRotator R = CameraBoom->GetRelativeRotation();
        R.Pitch = DesiredBoomPitch;
        CameraBoom->SetRelativeRotation(R);
    }

    // GAS setup
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    AttributeSet = CreateDefaultSubobject<UKinCharacterAttributeSet>(TEXT("AttributeSet"));

    // GAS abilities

}

void AKinCharacterBase::BeginPlay()
{
    Super::BeginPlay();
    // Prepare dynamic materials
    for (USkeletalMeshComponent* MeshComp : TInlineComponentArray<USkeletalMeshComponent*>(this))
    {
        int32 MatCount = MeshComp->GetNumMaterials();
        for (int32 i = 0; i < MatCount; ++i)
        {
            UMaterialInstanceDynamic* Dyn = MeshComp->CreateAndSetMaterialInstanceDynamic(i);
            DynamicMaterials.Add(Dyn);
        }
    }
    for (UStaticMeshComponent* MeshComp : TInlineComponentArray<UStaticMeshComponent*>(this))
    {
        int32 MatCount = MeshComp->GetNumMaterials();
        for (int32 i = 0; i < MatCount; ++i)
        {
            UMaterialInstanceDynamic* Dyn = MeshComp->CreateAndSetMaterialInstanceDynamic(i);
            DynamicMaterials.Add(Dyn);
        }
    }

    if (HasAuthority() && AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        InitializeAbilities();
    }
}

void AKinCharacterBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateCameraFraming();
    HandleCloseFade();


    // — Smooth Zoom Interpolation —
    {
        float CurrentLength = CameraBoom->TargetArmLength;
        float NewLength = FMath::FInterpTo(
            CurrentLength,
            DesiredArmLength,
            DeltaTime,
            ZoomInterpSpeed
        );
        CameraBoom->TargetArmLength = NewLength;
    }

    // — Smoothly interpolate boom pitch —
    {
        // grab entire relative rotation so we preserve yaw/roll
        FRotator R = CameraBoom->GetRelativeRotation();

        // lerp only the Pitch
        R.Pitch = FMath::FInterpTo(
            R.Pitch,
            DesiredBoomPitch,
            DeltaTime,
            ZoomInterpSpeed
        );
        CameraBoom->SetRelativeRotation(R);
    }

    const FVector Vel = GetVelocity();
    if (!Vel.IsNearlyZero())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Tick Loc=%s | Vel=%s | Rot=%s | Mode=%d"),
            *GetActorLocation().ToCompactString(),
            *Vel.ToCompactString(),
            *GetActorRotation().ToCompactString(),
            (int32)GetCharacterMovement()->MovementMode
        );
    }
}

void AKinCharacterBase::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();

    // Add Input Mapping Context
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}
void AKinCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // 1) Add the Enhanced Input Mapping Context first
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Sub =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            Sub->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    // 2) Capture the EnhancedInputComponent
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 3) Bind all your IA_* actions here
        EIC->BindAction(IA_MoveForward, ETriggerEvent::Triggered, this, &AKinCharacterBase::MoveForward);
        EIC->BindAction(IA_MoveRight, ETriggerEvent::Triggered, this, &AKinCharacterBase::MoveRight);
        EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AKinCharacterBase::Look);
        EIC->BindAction(ToggleZoomAction, ETriggerEvent::Started, this, &AKinCharacterBase::ToggleZoom);
        EIC->BindAction(SetOverheadAction, ETriggerEvent::Started, this, &AKinCharacterBase::SetOverheadView);
        EIC->BindAction(RotateCameraAction, ETriggerEvent::Triggered, this, &AKinCharacterBase::RotateCamera);


   
    }
}


UAbilitySystemComponent* AKinCharacterBase::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}


void AKinCharacterBase::InitializeAbilities()
{
    if (!AbilitySystemComponent || !HasAuthority()) return;
    for (auto& AbilityClass : DefaultAbilities)
    {
        if (AbilityClass)
        {
            AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
        }
    }
}

void AKinCharacterBase::Move(const FInputActionValue& Value)
{
    FVector2D Input = Value.Get<FVector2D>();
    UE_LOG(LogTemp, Warning, TEXT("Move Input — X: %f   Y: %f"), Input.X, Input.Y);


    if (!Input.IsNearlyZero())
    {
        const FRotator CameraRot = CameraBoom->GetComponentRotation();
        const FRotator YawRot(0.f, CameraRot.Yaw, 0.f);

        const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
        const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

        // Y drives forward (W/S), X drives right (A/D)
        AddMovementInput(Forward, Input.Y);
        AddMovementInput(Right, Input.X);
    }
}
// If there's any movement input…
//if (!Input.IsNearlyZero())
//{
//    // Movement independent of camera: world forward (+X) and right (+Y)
//    const FVector Forward = FVector::ForwardVector; // (1,0,0)
//    const FVector Right = FVector::RightVector;   // (0,1,0)
//    AddMovementInput(Forward, Input.Y);
//    AddMovementInput(Right, Input.X);
//}

void AKinCharacterBase::MoveForward(const FInputActionValue& Value)
{
    const float Axis = Value.Get<float>();
    if (FMath::IsNearlyZero(Axis)) return;

    // **Use pivot’s relative yaw**, not world yaw
    const float CamYaw = CameraBoom->GetRelativeRotation().Yaw;
    const FRotator YawRot(0.f, CamYaw, 0.f);
    const FVector Dir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);

    AddMovementInput(Dir, Axis);
}

void AKinCharacterBase::MoveRight(const FInputActionValue& Value)
{
    const float Axis = Value.Get<float>();
    if (FMath::IsNearlyZero(Axis)) return;

    const float CamYaw = CameraBoom->GetRelativeRotation().Yaw;
    const FRotator YawRot(0.f, CamYaw, 0.f);
    const FVector Dir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

    AddMovementInput(Dir, Axis);
}



void AKinCharacterBase::Look(const FInputActionValue& Value)
{
    float YawInput = Value.Get<FVector2D>().X;
    if (FMath::IsNearlyZero(YawInput)) return;


    FRotator R = CameraBoom->GetRelativeRotation();

    R.Yaw += YawInput;

    CameraBoom->SetRelativeRotation(R);
}

void AKinCharacterBase::ToggleZoom()
{
    CurrentZoomIndex = (CurrentZoomIndex + 1) % 3;

    // pick lengths…
    DesiredArmLength = (CurrentZoomIndex == 0) ? ZoomClose
        : (CurrentZoomIndex == 1) ? ZoomNormal
        : ZoomFar;

    // …and pick pitches
    DesiredBoomPitch = (CurrentZoomIndex == 0) ? BoomPitchClose
        : (CurrentZoomIndex == 1) ? BoomPitchNormal
        : BoomPitchFar;
}

void AKinCharacterBase::SetOverheadView()
{
    bOverheadMode = !bOverheadMode;
    float Pitch = bOverheadMode ? -89.f : -25.f;
    CameraBoom->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
}

void AKinCharacterBase::RotateCamera(const FInputActionValue& Value)
{
    float YawInput = Value.Get<FVector2D>().X;
    FRotator R = CameraBoom->GetRelativeRotation();
    R.Yaw += YawInput * CameraPanSpeed * GetWorld()->GetDeltaSeconds();
    CameraBoom->SetRelativeRotation(R);
}

void AKinCharacterBase::UpdateCameraFraming()
{
    // TODO: implement group bounding box logic
}


void AKinCharacterBase::HandleCloseFade()
{
    float Dist = FVector::Dist(FollowCamera->GetComponentLocation(), GetActorLocation());
    float Opacity = FMath::Clamp((Dist - 50.f) / (CharacterFadeDistance - 50.f), 0.f, 1.f);
    for (UMaterialInstanceDynamic* Dyn : DynamicMaterials)
    {
        if (Dyn)
        {
            Dyn->SetScalarParameterValue(FadeParamName, Opacity);
        }
    }
}

