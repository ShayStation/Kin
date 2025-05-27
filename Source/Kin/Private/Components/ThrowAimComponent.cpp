#include "Components/ThrowAimComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Abilities/ThrownProjectile.h"


// --------------------------------------------------------------------------------
// Constructor
// --------------------------------------------------------------------------------
UThrowAimComponent::UThrowAimComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;

    // initialize the smoothed range
    CurrentEffectiveRange = MaxTraceDistance;
}

// --------------------------------------------------------------------------------
// TickComponent: draws debug and drives ComputeThrow each frame
// --------------------------------------------------------------------------------
void UThrowAimComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bDebugDraw)
        return;

    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner)
        return;

    //
    // 1) Wall-distance trace to compute TargetRange ? smoothly lerp to CurrentEffectiveRange
    //
    UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
    FVector WallStart = Capsule
        ? Capsule->GetComponentLocation()
        : Owner->GetActorLocation();

    FCollisionQueryParams WallParams(TEXT("WallClamp"), false, Owner);
    FHitResult WallHit;
    bool bHitWall = World->LineTraceSingleByChannel(
        WallHit,
        WallStart,
        WallStart + Owner->GetActorForwardVector() * MaxTraceDistance,
        ECC_WorldStatic,
        WallParams
    );

    float DistanceToWall = bHitWall
        ? FVector(WallHit.Location - WallStart).Size2D()
        : MaxTraceDistance;

    float TargetRange = FMath::Clamp(DistanceToWall + ClearanceBuffer, 0.0f, MaxTraceDistance);
    CurrentEffectiveRange = FMath::FInterpTo(
        CurrentEffectiveRange,
        TargetRange,
        DeltaTime,
        RangeInterpSpeed
    );

    //
    // 2) ComputeThrow ? gives us spawn start, launch velocity, AND final AimPoint
    //
    FVector SpawnStart, LaunchVelocity, AimPoint;
    if (!ComputeThrow(SpawnStart, LaunchVelocity, AimPoint))
        return;

    //
    // 3) Debug-draw: blue line of CurrentEffectiveRange, green landing sphere, yellow velocity arrow, white apex text
    //
    {
        // blue: clamped range line
        FVector TraceStart = WallStart;
        FVector TraceEnd = TraceStart + Owner->GetActorForwardVector() * CurrentEffectiveRange;
        DrawDebugLine(World, TraceStart, TraceEnd, FColor::Blue, false, 0.0f, 0, 2.0f);

        // green: landing point
        DrawDebugSphere(World, AimPoint, 8.0f, 12, FColor::Green, false, 0.0f);

        // yellow: direction arrow
        DrawDebugLine(
            World,
            SpawnStart,
            SpawnStart + LaunchVelocity.GetSafeNormal() * 100.0f,
            FColor::Yellow,
            false,
            0.0f,
            0,
            2.0f
        );

        // white: apex height text
        float WorldG = -World->GetGravityZ();
        float EffG = WorldG * ProjectileGravityScale;
        float Apex = LaunchVelocity.Z * LaunchVelocity.Z / (2.0f * EffG);
        DrawDebugString(
            World,
            SpawnStart + FVector(0, 0, Apex),
            FString::Printf(TEXT("Apex: %.1f"), Apex),
            nullptr,
            FColor::White,
            0.0f
        );
    }

    //
    // 4) Debug-draw: physics-sampled trajectory in cyan
    //
    {
        const int32 Segs = 30;

        // re-solve total flight time: descending root of quadratic
        float WorldG = -World->GetGravityZ();
        float g = WorldG * ProjectileGravityScale;
        float DeltaZ = AimPoint.Z - SpawnStart.Z;
        float Vz = LaunchVelocity.Z;
        float Discr = Vz * Vz - 2.0f * g * DeltaZ;
        if (Discr <= 0.0f)
            return;  // can't reach target

        // descending solution: (Vz + sqrt(Vz^2 - 2g?Z)) / g
        float TotalT = (Vz + FMath::Sqrt(Discr)) / g;

        FVector PrevPt = SpawnStart;
        for (int32 i = 1; i <= Segs; ++i)
        {
            // sample time (include TimeScale to match your projectile)
            float tSim = (float(i) / Segs) * TotalT * TimeScale;

            // projectile kinematics: P = P0 + V0*t + 0.5*a*t^2
            FVector GravityAccel = FVector(0, 0, World->GetGravityZ() * ProjectileGravityScale);
            FVector Pt = SpawnStart
                + LaunchVelocity * tSim
                + 0.5f * GravityAccel * tSim * tSim;

            DrawDebugLine(World, PrevPt, Pt, FColor::Cyan, false, 0.0f, 0, 1.0f);
            PrevPt = Pt;
        }
    }
}

// --------------------------------------------------------------------------------
// ComputeThrow: solves for OutStart, OutVelocity, and final OutAimPoint on top of obstacles
// --------------------------------------------------------------------------------
bool UThrowAimComponent::ComputeThrow(
    FVector& OutStart,
    FVector& OutVelocity,
    FVector& OutAimPoint
)
{
    AActor* Owner = GetOwner();
    if (!Owner) return false;
    UWorld* World = Owner->GetWorld();
    if (!World) return false;

    // 1) Spawn location at socket
    USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh) return false;
    OutStart = Mesh->GetSocketLocation(TEXT("ThrowSocket"));

    // 2) Horizontal “wall” trace using CurrentEffectiveRange
    UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
    FVector TraceStart = Capsule
        ? Capsule->GetComponentLocation()
        : OutStart;
    FVector TraceEnd = TraceStart + Owner->GetActorForwardVector() * CurrentEffectiveRange;

    FCollisionQueryParams Params(TEXT("ThrowTrace"), false, Owner);
    FHitResult Hit;
    bool bHit = World->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        ECC_WorldStatic,
        Params
    );

    // compute horizontal landing XY and base Z
    FVector LandXY = TraceStart + Owner->GetActorForwardVector() * CurrentEffectiveRange;
    float BaseZ = bHit
        ? Hit.Location.Z
        : TraceStart.Z;

    // 3) Snap-up: shoot down from apex height above LandXY to get true top surface Z
    float H = MaxArcHeight * ArcParam;                    // desired apex above OutStart
    float g = -World->GetGravityZ() * ProjectileGravityScale;
    float Vz = FMath::Sqrt(2.0f * g * H);                  // initial vertical speed
    float UpZ0 = BaseZ + H;                                  // start Z for downward trace

    FHitResult UpHit;
    if (World->LineTraceSingleByChannel(
        UpHit,
        FVector(LandXY.X, LandXY.Y, UpZ0),
        FVector(LandXY.X, LandXY.Y, BaseZ),
        ECC_WorldStatic,
        Params
    ))
    {
        BaseZ = UpHit.Location.Z;
    }

    // final AimPoint (XY + snapped Z)
    OutAimPoint = FVector(LandXY.X, LandXY.Y, BaseZ);

    // 4) Solve for flight time to OutAimPoint (descending root)
    float DeltaZ = OutAimPoint.Z - OutStart.Z;
    float Discr = Vz * Vz - 2.0f * g * DeltaZ;
    if (Discr < 0.0f) return false;

    float Time = (Vz + FMath::Sqrt(Discr)) / g;

    // horizontal speed
    FVector Dir2D = FVector(OutAimPoint.X - OutStart.X, OutAimPoint.Y - OutStart.Y, 0.0f);
    float Dist2D = Dir2D.Size();
    if (Dist2D < KINDA_SMALL_NUMBER) return false;
    Dir2D.Normalize();

    float Vx = Dist2D / Time;
    OutVelocity = Dir2D * Vx + FVector(0, 0, Vz);

    return true;
}
