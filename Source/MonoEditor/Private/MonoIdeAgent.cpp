// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoIdeAgent.h"
#include "MonoEditorCommon.h"

#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "ILevelViewport.h"
#include "Editor/UnrealEdEngine.h"
#include "Misc/FeedbackContext.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Framework/Application/SlateApplication.h"

#include "MonoIdeAgentDerived.inl"

//caller is responsible for keeping the function pointer alive until it's called
MONO_PINVOKE_FUNCTION(void) MonoIdeAgent_DispatchToGameThread(void (*callback)(void*), void* data)
{
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateStatic(callback, data)
		, NULL
		, NULL
		, ENamedThreads::GameThread
		);
}

MONO_PINVOKE_FUNCTION(void) MonoIdeAgent_UEditorEngine_RequestEndPlayMap()
{
	GEditor->RequestEndPlayMap();
}

MONO_PINVOKE_FUNCTION(bool) MonoIdeAgent_HotReload()
{
	return IMonoRuntime::Get().RequestHotReload();
}

static bool(*MonoIdeAgent_CommandCallback)(bool, const UTF16CHAR*);

MONO_PINVOKE_FUNCTION(void) MonoIdeAgent_SetCommandCallback(bool(*callback)(bool, const UTF16CHAR*))
{
	MonoIdeAgent_CommandCallback = callback;
}

bool MonoIdeAgent_SendCommand(bool launch, const TCHAR* command)
{
	return MonoIdeAgent_CommandCallback && MonoIdeAgent_CommandCallback(launch, StringCast<UTF16CHAR>(command).Get());
}

bool MonoIdeAgent_IsConnected()
{
	return MonoIdeAgent_SendCommand(false, TEXT("NoOp"));
}
