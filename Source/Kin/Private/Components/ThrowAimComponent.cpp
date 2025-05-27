#include "Components/ThrowAimComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Abilities/ThrownProjectile.h"

UThrowAimComponent::UThrowAimComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UThrowAimComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bDebugDraw)
    {
        return;
    }

    // Compute start + velocity
    FVector Start, Velocity;
    if (!ComputeThrow(Start, Velocity))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 1) Trace forward from Start along actor's forward
    AActor* Owner = GetOwner();
    FVector Forward = Owner->GetActorForwardVector();
    FVector TraceEnd = Start + Forward * MaxTraceDistance;

    FHitResult Hit;
    FCollisionQueryParams Params(TEXT("ThrowTrace"), false, Owner);
    bool bHit = World->LineTraceSingleByChannel(
        Hit, Start, TraceEnd, ECC_WorldStatic, Params
    );
    FVector AimPoint = bHit ? Hit.Location : TraceEnd;

    // 2) Debug: blue line & red sphere
    DrawDebugLine(World, Start, TraceEnd, FColor::Blue, false, 0.0f, 0, 2.0f);
    DrawDebugSphere(World, AimPoint, 8.0f, 12, FColor::Red, false, 0.0f);

    // 3) Snap-up debug
    if (bHit && SnapUpHeight > 0.0f)
    {
        FVector UpStart = AimPoint + FVector(0, 0, SnapUpHeight);
        FHitResult UpHit;
        if (World->LineTraceSingleByChannel(
            UpHit, UpStart, AimPoint,
            ECC_WorldStatic, Params))
        {
            AimPoint = UpHit.Location;
            DrawDebugSphere(World, AimPoint, 8.0f, 12,
                FColor::Green, false, 0.0f);
        }
    }

    // 4) Debug: yellow arrow for initial direction
    DrawDebugLine(World, Start,
        Start + Velocity.GetSafeNormal() * 100.0f,
        FColor::Yellow, false, 0.0f, 0, 2.0f);

    // 5) Debug: apex label
    {
        float WorldG = -World->GetGravityZ();
        float EffG = WorldG * ProjectileGravityScale;
        float Apex = Velocity.Z * Velocity.Z / (2.0f * EffG);
        DrawDebugString(World,
            Start + FVector(0, 0, Apex),
            FString::Printf(TEXT("Apex: %.1f"), Apex),
            nullptr, FColor::White, 0.0f
        );
    }

    // 6) Debug: symmetric cyan parabola to AimPoint
    {
        FVector FinalPoint = AimPoint;
        const int32 Segs = 30;
        float ApexHeight = MaxArcHeight * ArcParam;
        FVector Prev = Start;

        for (int32 i = 1; i <= Segs; ++i)
        {
            float t = float(i) / float(Segs);
            FVector Base = FMath::Lerp(Start, FinalPoint, t);
            float OffsetZ = ApexHeight * 4.0f * t * (1.0f - t);
            FVector Pt = Base + FVector(0, 0, OffsetZ);

            DrawDebugLine(World, Prev, Pt, FColor::Cyan,
                false, 0.0f, 0, 1.0f);
            Prev = Pt;
        }
    }
}

bool UThrowAimComponent::ComputeThrow(
    FVector& OutStart,
    FVector& OutVelocity
)
{
    AActor* Owner = GetOwner();
    if (!Owner) return false;
    UWorld* World = Owner->GetWorld();
    if (!World) return false;

    // 1) Socket spawn point
    USkeletalMeshComponent* Mesh =
        Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh) return false;
    OutStart = Mesh->GetSocketLocation(TEXT("ThrowSocket"));

    // 2) Horizontal trace along actor forward
    FVector Forward = Owner->GetActorForwardVector();
    FVector TraceEnd = OutStart + Forward * MaxTraceDistance;

    FHitResult Hit;
    FCollisionQueryParams Params(TEXT("ThrowTrace"), false, Owner);
    bool bHit = World->LineTraceSingleByChannel(
        Hit, OutStart, TraceEnd, ECC_WorldStatic, Params
    );
    FVector AimPoint = bHit ? Hit.Location : TraceEnd;

    // 3) Snap up if needed
    if (bHit && SnapUpHeight > 0.0f)
    {
        FVector UpStart = AimPoint + FVector(0, 0, SnapUpHeight);
        FHitResult UpHit;
        if (World->LineTraceSingleByChannel(
            UpHit, UpStart, AimPoint,
            ECC_WorldStatic, Params))
        {
            AimPoint = UpHit.Location;
        }
    }

    // 4) Symmetric parabola solve from OutStart to AimPoint
    FVector ToTarget = AimPoint - OutStart;
    FVector Dir2D(ToTarget.X, ToTarget.Y, 0.0f);
    float Dist2D = Dir2D.Size();
    if (Dist2D < KINDA_SMALL_NUMBER) return false;
    Dir2D.Normalize();

    float H = MaxArcHeight * ArcParam;
    float g = -World->GetGravityZ() * ProjectileGravityScale;
    if (g <= KINDA_SMALL_NUMBER) return false;

    float Vz = FMath::Sqrt(2.0f * g * H);
    float T = 2.0f * Vz / g;
    float Vx = Dist2D / T;
    OutVelocity = Dir2D * Vx + FVector(0, 0, Vz);

    return true;
}