#include "Components/ThrowAimComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
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

    // Compute spawn start + velocity (spawn still from socket)
    FVector SpawnStart, Velocity;
    if (!ComputeThrow(SpawnStart, Velocity))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // 1) Determine capsule-center start for tracing
    UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
    FVector TraceStart = Capsule
        ? Capsule->GetComponentLocation()
        : SpawnStart;

    FVector Forward = Owner->GetActorForwardVector();
    FVector TraceEnd = TraceStart + Forward * MaxTraceDistance;

    // 2) Line trace to find aim point
    FHitResult Hit;
    FCollisionQueryParams Params(TEXT("ThrowTrace"), false, Owner);
    bool bHit = World->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params
    );
    FVector AimPoint = bHit ? Hit.Location : TraceEnd;

    // 3) Debug: blue line for capsule trace + red sphere at impact
    DrawDebugLine(World, TraceStart, TraceEnd, FColor::Blue, false, 0.0f, 0, 2.0f);
    DrawDebugSphere(World, AimPoint, 8.0f, 12, FColor::Red, false, 0.0f);

    // 4) Snap-up debug
    if (bHit && SnapUpHeight > 0.0f)
    {
        FVector UpStart = AimPoint + FVector(0, 0, SnapUpHeight);
        FHitResult UpHit;
        if (World->LineTraceSingleByChannel(
            UpHit, UpStart, AimPoint, ECC_WorldStatic, Params))
        {
            AimPoint = UpHit.Location;
            DrawDebugSphere(World, AimPoint, 8.0f, 12, FColor::Green, false, 0.0f);
        }
    }

    // 5) Debug: yellow arrow showing initial velocity from socket
    DrawDebugLine(
        World,
        SpawnStart,
        SpawnStart + Velocity.GetSafeNormal() * 100.0f,
        FColor::Yellow,
        false,
        0.0f,
        0,
        2.0f
    );

    // 6) Debug: apex label
    {
        float WorldG = -World->GetGravityZ();
        float EffG = WorldG * ProjectileGravityScale;
        float Apex = Velocity.Z * Velocity.Z / (2.0f * EffG);
        DrawDebugString(
            World,
            SpawnStart + FVector(0, 0, Apex),
            FString::Printf(TEXT("Apex: %.1f"), Apex),
            nullptr,
            FColor::White,
            0.0f
        );
    }

    // 7) Debug: cyan parabola from socket to AimPoint
    {
        const int32 Segs = 30;
        float ApexHeight = MaxArcHeight * ArcParam;
        FVector Prev = SpawnStart;
        for (int32 i = 1; i <= Segs; ++i)
        {
            float t = float(i) / float(Segs);
            FVector Base = FMath::Lerp(SpawnStart, AimPoint, t);
            float OffsetZ = ApexHeight * 4.0f * t * (1.0f - t);
            FVector Pt = Base + FVector(0, 0, OffsetZ);
            DrawDebugLine(World, Prev, Pt, FColor::Cyan, false, 0.0f, 0, 1.0f);
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
    if (!Owner)
    {
        return false;
    }
    UWorld* World = Owner->GetWorld();
    if (!World)
    {
        return false;
    }

    // 1) Spawn location remains at the hand socket
    USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh)
    {
        return false;
    }
    FVector SpawnLocation = Mesh->GetSocketLocation(TEXT("ThrowSocket"));
    OutStart = SpawnLocation;

    // 2) But aim-trace should start from capsule center
    UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
    FVector TraceStart = Capsule
        ? Capsule->GetComponentLocation()
        : OutStart;

    // 3) Horizontal trace
    FVector Forward = Owner->GetActorForwardVector();
    FVector TraceEnd = TraceStart + Forward * MaxTraceDistance;

    FHitResult Hit;
    FCollisionQueryParams Params(TEXT("ThrowTrace"), false, Owner);
    bool bHit = World->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params
    );
    FVector AimPoint = bHit ? Hit.Location : TraceEnd;

    // 4) Snap-up if needed
    if (bHit && SnapUpHeight > 0.0f)
    {
        FVector UpStart = AimPoint + FVector(0, 0, SnapUpHeight);
        FHitResult UpHit;
        if (World->LineTraceSingleByChannel(
            UpHit, UpStart, AimPoint, ECC_WorldStatic, Params))
        {
            AimPoint = UpHit.Location;
        }
    }

    // 5) Solve symmetric arc from SpawnLocation to AimPoint
    FVector ToTarget = AimPoint - OutStart;
    FVector Dir2D(ToTarget.X, ToTarget.Y, 0.0f);
    float Dist2D = Dir2D.Size();
    if (Dist2D < KINDA_SMALL_NUMBER)
    {
        return false;
    }
    Dir2D.Normalize();

    float H = MaxArcHeight * ArcParam;
    float g = -World->GetGravityZ() * ProjectileGravityScale;
    if (g <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    float Vz = FMath::Sqrt(2.0f * g * H);
    float T = 2.0f * Vz / g;
    float Vx = Dist2D / T;
    OutVelocity = Dir2D * Vx + FVector(0, 0, Vz);

    return true;
}