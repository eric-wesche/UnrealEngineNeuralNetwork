// Copyright Epic Games, Inc. All Rights Reserved.

#include "UENeuralNetworkGameMode.h"
#include "UENeuralNetworkCharacter.h"
#include "UObject/ConstructorHelpers.h"

UMaterialInstanceDynamic* AUENeuralNetworkGameMode::GMDynamicMaterialInstance = nullptr;

AUENeuralNetworkGameMode::AUENeuralNetworkGameMode()
{
	// set default pawn class to our Blueprinted character
	// static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_MyPawn"));

	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
