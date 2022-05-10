// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ImposterBakerStyle.h"

class FImposterBakerCommands : public TCommands<FImposterBakerCommands>
{
public:

	FImposterBakerCommands()
		: TCommands<FImposterBakerCommands>(TEXT("ImposterBaker"), NSLOCTEXT("Contexts", "ImposterBaker", "ImposterBaker Plugin"), NAME_None, FImposterBakerStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
