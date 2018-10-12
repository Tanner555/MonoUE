// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "IMonoEditorPlugin.h"
#include "MonoEditorCommon.h"
#include "MonoIdeAgent.h"
#include "MonoEditorUtils.h"
#include "SNewMonoClassDialog.h"

#include "MessageLogModule.h"
#include "GameProjectGenerationModule.h"
#include "IHotReload.h"
#include "LevelEditor.h"
#include "UnrealEdGlobals.h"
#include "GameProjectUtils.h"
#include "Misc/FileHelper.h"
#include "Editor/UnrealEdEngine.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Misc/MessageDialog.h"

DEFINE_LOG_CATEGORY(LogMonoEditor);
#define LOCTEXT_NAMESPACE "MonoEditor"

class FMonoEditorPlugin : public IMonoEditorPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnHotReload(bool bSuccess);
	void OnStopPIEForHotReload();

	void AddMainMenuExtension();
	void RemoveMainMenuExtension();
	void ExtendFileMenu(FMenuBuilder& MenuBuilder);
	void OpenVisualStudio();
	/*
	void CreateNewMonoClass();
	*/
	TSharedPtr<FExtender> MainMenuExtender;
};

IMPLEMENT_MODULE(FMonoEditorPlugin, IMonoEditorPlugin)


void FMonoEditorPlugin::StartupModule()
{
#if MONOUE_STANDALONE
	// If in MonoUE.uplugin MonoScriptGenerator is set to Editor/PostConfigInit it seemingly doesn't load.
	// - One option is to change it to PostDefault which will ensure it is loaded. For now manually load it to
	//   reduce the amount of changes outside of "MONOUE_STANDALONE" tags
	FModuleManager::Get().LoadModuleChecked("MonoScriptGenerator");
#endif

	FGameProjectGenerationModule::Get().RegisterTemplateCategory(
		TEXT("CSharp"),
		LOCTEXT("CSharpCategory_Name", "C#"),
		LOCTEXT("CSharpCategory_Description", "Allows you to script your game with C#, a popular, safe and productive language."),
		FEditorStyle::GetBrush("GameProjectDialog.BlueprintIcon"),
		FEditorStyle::GetBrush("GameProjectDialog.BlueprintImage"));

	FGameProjectGenerationModule::Get().RegisterTemplateCategory(
		TEXT("CSharpCpp"),
		LOCTEXT("CSharpCppCategory_Name", "C#/C++"),
		LOCTEXT("CSharpCppCategory_Description", "Allows you to script your game with C#, while also implementing more advanced functionality in C++."),
		FEditorStyle::GetBrush("GameProjectDialog.BlueprintIcon"),
		FEditorStyle::GetBrush("GameProjectDialog.BlueprintImage"));

#if WITH_UNREAL_DEVELOPER_TOOLS
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;
	MessageLogModule.RegisterLogListing("MonoErrors", LOCTEXT("MonoErrorsLabel", "Mono Runtime Errors"), InitOptions);
#endif

#if MONOUE_STANDALONE
	if (!IMonoRuntime::Get().IsLoaded())
	{
		return;
	}
#endif

	IMonoRuntime::Get().AddDllMapForModule("MonoEditor", "MonoEditor");

	IMonoRuntime::Get().GetOnHotReloadEvent().AddRaw(this, &FMonoEditorPlugin::OnHotReload);
	IMonoRuntime::Get().GetOnStopPIEForHotReloadEvent().AddRaw(this, &FMonoEditorPlugin::OnStopPIEForHotReload);

	//only initialize agent and menus if we have a UI
	if (!IsRunningCommandlet())
	{
		if (MonoEditorUtils::HasManagedSolutionFile())
		{
			MonoEditorUtils::EnableIdeIntegration();
		}

		AddMainMenuExtension();
	}
}

void FMonoEditorPlugin::ShutdownModule()
{
#if WITH_UNREAL_DEVELOPER_TOOLS
	// unregister message log
	if (FModuleManager::Get().IsModuleLoaded("MessageLog"))
	{
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.UnregisterLogListing("MonoErrors");
	}
#endif

	if (!IsRunningCommandlet())
	{
		RemoveMainMenuExtension();
	}

	// we currently can't support scenarios where MonoRuntime is unloaded before MonoEditor - putting this check
	// to make sure it does not happen (if it does, we'll need to rethink some stuff)
	check(IMonoRuntime::IsAvailable());

	MonoEditorUtils::DisableIdeIntegration();

	IMonoRuntime::Get().GetOnHotReloadEvent().RemoveAll(this);
	IMonoRuntime::Get().GetOnStopPIEForHotReloadEvent().RemoveAll(this);
}

void FMonoEditorPlugin::OnHotReload(bool bSuccess)
{
	if (bSuccess)
	{
		// forward to editor's hot reload event
		IHotReloadModule::Get().DoHotReloadFromEditor(EHotReloadFlags::None);
	}
}

void FMonoEditorPlugin::OnStopPIEForHotReload()
{
	if (GUnrealEd && GUnrealEd->PlayWorld)
	{
		GUnrealEd->EndPlayMap();
	}
}

void FMonoEditorPlugin::AddMainMenuExtension()
{
	// Add menu option for level editor tutorial
	MainMenuExtender = MakeShareable(new FExtender);
	MainMenuExtender->AddMenuExtension("FileProject", EExtensionHook::After, NULL, FMenuExtensionDelegate::CreateRaw(this, &FMonoEditorPlugin::ExtendFileMenu));
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);
}

void FMonoEditorPlugin::RemoveMainMenuExtension()
{
	if (MainMenuExtender.IsValid() && FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(MainMenuExtender);
	}
}

void FMonoEditorPlugin::ExtendFileMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Mono", LOCTEXT("MonoLabel", "Mono"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("VisualStudioMenuEntryTitle", "Open Visual Studio"),
		LOCTEXT("VisualStudioMenuEntryToolTip", "Opens the game code project in Visual Studio."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.VisualStudio"),
		FUIAction(FExecuteAction::CreateRaw(this, &FMonoEditorPlugin::OpenVisualStudio)));
	/*
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewMonoClassMenuEntryTitle", "Add new Mono class to project..."),
		LOCTEXT("NewMonoClassMenuEntryToolTip", "Adds a new class to Mono project."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.VisualStudio"),
		FUIAction(FExecuteAction::CreateRaw(this, &FMonoEditorPlugin::CreateNewMonoClass)));
		MenuBuilder.EndSection();
	*/
	MenuBuilder.EndSection();
}

void FMonoEditorPlugin::OpenVisualStudio()
{
	if (MonoEditorUtils::HasManagedSolutionFile())
	{
		MonoIdeAgent_SendCommand(true, TEXT("GrabFocus"));
	}
	else
	{
		const FText Message = LOCTEXT("NoManagedSolution", "There's no managed Mono solution file to open. Add a Mono class to the project first");
		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}
}
/*
void FMonoEditorPlugin::CreateNewMonoClass()
{
	TSharedRef<SWindow> AddCodeWindow =
		SNew(SWindow)
		.Title(LOCTEXT("AddCodeWindowHeader", "Add Code"))
		.ClientSize(FVector2D(940, 540))
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMinimize(false).SupportsMaximize(false);

	AddCodeWindow->SetContent(SNew(SNewMonoClassDialog));

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if (MainFrameModule.GetParentWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(AddCodeWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(AddCodeWindow);
	}
}
*/

#undef LOCTEXT_NAMESPACE