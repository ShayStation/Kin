


#include "Abilities/ThrownProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

AThrownProjectile::AThrownProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;

    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCollisionObjectType(ECC_WorldDynamic);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Mesh->SetNotifyRigidBodyCollision(false);
}

void AThrownProjectile::InitTrajectory(
    const FVector& StartLoc,
    const FVector& InitialVel,
    float InGravityScale,
    float InTimeScale
)
{
    InitialLocation = StartLoc;
    LaunchVelocity = InitialVel;
    GravityScale = InGravityScale;
    TimeScale = InTimeScale;

    if (UWorld* W = GetWorld())
    {
        SpawnTime = W->GetTimeSeconds();
    }

    SetActorLocation(StartLoc);

    if (AActor* Inst = GetOwner())
    {
        Mesh->IgnoreActorWhenMoving(Inst, true);
    }
}

void AThrownProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UWorld* World = GetWorld();
    if (!World) return;

    // Simulated time
    float tReal = World->GetTimeSeconds() - SpawnTime;
    float tSim = tReal * TimeScale;

    FVector Gravity = FVector(0, 0, World->GetGravityZ() * GravityScale);
    FVector NewLoc = InitialLocation
        + LaunchVelocity * tSim
        + 0.5f * Gravity * tSim * tSim;

    // Sweep so we block the floor/walls and then stop
    FHitResult HitRes;
    SetActorLocation(NewLoc, true, &HitRes);
    if (HitRes.IsValidBlockingHit())
    {
        // Land and stay
        SetActorLocation(HitRes.Location);
        PrimaryActorTick.bCanEverTick = false;
    }
}