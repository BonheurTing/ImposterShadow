// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImposterBakerCommands.h"

#define LOCTEXT_NAMESPACE "FImposterBakerModule"

void FImposterBakerCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "ImposterBaker", "Execute ImposterBaker action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
