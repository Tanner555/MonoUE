// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "IMonoScriptGenerator.h"
#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMonoScriptGenerator, Log, All);

// generator runs as warnaserror by default, so tone it down
#define GenerationWarning Log

#define MONOUE_GENERATOR_ISSUE(Verbosity, Format, ...) \
	UE_LOG(LogMonoScriptGenerator, Verbosity, TEXT(Format), ##__VA_ARGS__); \
	if (ELogVerbosity::Verbosity <= ELogVerbosity::Error && FPlatformMisc::IsDebuggerPresent()) UE_DEBUG_BREAK();
