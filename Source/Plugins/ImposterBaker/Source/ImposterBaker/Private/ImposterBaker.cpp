// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImposterBaker.h"
#include "ImposterBakerStyle.h"
#include "ImposterBakerCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

static const FName ImposterBakerTabName("ImposterBaker");

#define LOCTEXT_NAMESPACE "FImposterBakerModule"

void FImposterBakerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FImposterBakerStyle::Initialize();
	FImposterBakerStyle::ReloadTextures();

	FImposterBakerCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FImposterBakerCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FImposterBakerModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FImposterBakerModule::RegisterMenus));
}

void FImposterBakerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FImposterBakerStyle::Shutdown();

	FImposterBakerCommands::Unregister();
}

void FImposterBakerModule::PluginButtonClicked()
{
	// Put your "OnButtonClicked" stuff here
	FText DialogText = FText::Format(
							LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
							FText::FromString(TEXT("FImposterBakerModule::PluginButtonClicked()")),
							FText::FromString(TEXT("ImposterBaker.cpp"))
					   );
	FMessageDialog::Open(EAppMsgType::Ok, DialogText);
}

void FImposterBakerModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FImposterBakerCommands::Get().PluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FImposterBakerCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FImposterBakerModule, ImposterBaker)