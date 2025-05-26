// Fill out your copyright notice in the Description page of Project Settings.


#include "GameModes/KinGameModeBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Character/KinCharacterBase.h"


AKinGameModeBase::AKinGameModeBase()
{
    DefaultPawnClass = AKinCharacterBase::StaticClass();

    // If using Blueprint subclass instead, uncomment and use path:
    //static ConstructorHelpers::FClassFinder<APawn> PawnBPClass(TEXT("/Game/Blueprints/BP_KinCharacter.BP_KinCharacter_C"));
    //if (PawnBPClass.Class) DefaultPawnClass = PawnBPClass.Class;


}

