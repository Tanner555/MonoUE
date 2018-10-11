// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//This is essentially an inlined/modified version of UE_LOG/FMsg::Logf that
//assumes the formatting was handled on the managed side

#include "Misc/MessageDialog.h"
#include "MonoHelpersShared.h"

MONO_PINVOKE_FUNCTION(void) MonoIdeAgent_Log(int verbosityInt, UTF16CHAR* message)
{
#if !NO_LOGGING
	//TODO: allow passing in a custom log category
	FLogCategoryBase* category = &LogMonoEditor;

	ELogVerbosity::Type verbosity = (ELogVerbosity::Type)verbosityInt;

	if (category->IsSuppressed(verbosity))
		return;

	if (verbosity != ELogVerbosity::Fatal)
	{
		FOutputDevice* LogDevice = NULL;
		switch (verbosity)
		{
		case ELogVerbosity::Error:
		case ELogVerbosity::Warning:
		case ELogVerbosity::Display:
		case ELogVerbosity::SetColor:
			if (GWarn)
			{
				LogDevice = GWarn;
				break;
			}
		default:
		{
			LogDevice = GLog;
		}
			break;
		}
		LogDevice->Log(category->GetCategoryName(), verbosity, StringCast<TCHAR>(message).Get());
	}
	else
	{
		GLog->PanicFlushThreadedLogs();

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Fatal error: %s\n"), StringCast<TCHAR>(message).Get());

		if (!FPlatformMisc::IsDebuggerPresent())
		{
			FPlatformMisc::PromptForRemoteDebugging(false);
		}

		UE_DEBUG_BREAK();

		GError->Log(category->GetCategoryName(), verbosity, StringCast<TCHAR>(message).Get());
	}
#endif
}

MONO_PINVOKE_FUNCTION(TCHAR *) MonoIdeAgent_GetVisualStudioCommonToolsPath()
{
	int const VS_2017_VersionKey = (15); // Visual Studio 2017

	FString commonToolsPath;
	if (!FPlatformMisc::GetVSComnTools(VS_2017_VersionKey, commonToolsPath))
	{
		auto const Message = FText::FromString("Visual Studio 2017 could not be found.");
		FMessageDialog::Open(EAppMsgType::Ok, Message);
		return nullptr;
	}

	auto const count = commonToolsPath.Len() * commonToolsPath.GetCharArray().GetTypeSize() + 1;
	TCHAR * const arr = static_cast<TCHAR *>(Mono::CoTaskMemAlloc(count));
	TCHAR const * const src = *commonToolsPath;

	FMemory::Memcpy(arr, src, count);
	return arr;
}

//always start as a standalone process when launching from XS so we can attach the debugger
//based on DebuggerCommands.cpp
MONO_PINVOKE_FUNCTION(bool) MonoIdeAgent_UEditorEngine_RequestPlaySession(bool MobilePreview, const UTF16CHAR* args)
{
	//fail if already running something
	if (GUnrealEd->PlayWorld != NULL)
		return false;

	const bool bAtPlayerStart = GEditor->CheckForPlayerStart()
		&& static_cast<EPlayModeLocations>(GetDefault<ULevelEditorPlaySettings>()->LastExecutedPlayModeLocation) == PlayLocation_DefaultPlayerStart;

	const FVector* StartLoc = NULL;
	const FRotator* StartRot = NULL;

	if (!bAtPlayerStart)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<ILevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();

		if (ActiveLevelViewport.IsValid() && FSlateApplication::Get().FindWidgetWindow(ActiveLevelViewport->AsWidget()).IsValid())
		{
			StartLoc = &ActiveLevelViewport->GetLevelViewportClient().GetViewLocation();
			StartRot = &ActiveLevelViewport->GetLevelViewportClient().GetViewRotation();
		}
	}

	GUnrealEd->RequestPlaySession(StartLoc, StartRot, MobilePreview, false, FString(args));
	return true;
}