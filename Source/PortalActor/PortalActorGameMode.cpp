// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PortalActor.h"
#include "PortalActorGameMode.h"
#include "PortalActorHUD.h"
#include "PortalActorCharacter.h"

APortalActorGameMode::APortalActorGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = APortalActorHUD::StaticClass();
}
