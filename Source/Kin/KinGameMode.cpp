// Copyright Epic Games, Inc. All Rights Reserved.

#include "KinGameMode.h"
#include "KinCharacter.h"
#include "UObject/ConstructorHelpers.h"

AKinGameMode::AKinGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
