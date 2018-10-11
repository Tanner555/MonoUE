// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "Logging/LogMacros.h"
#include "Internationalization/Internationalization.h"

MONORUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogMono, Log, All);

// define this to 1 to support hot reloading
#define MONO_WITH_HOT_RELOADING WITH_EDITOR

#define MONO_UE4_NAMESPACE "UnrealEngine"
#define MONO_BINDINGS_NAMESPACE MONO_UE4_NAMESPACE ".Runtime"
#define MONO_RUNTIME_NAMESPACE MONO_UE4_NAMESPACE ".MonoRuntime"
#define MONO_COREUOBJECT_NAMESPACE MONO_UE4_NAMESPACE ".Core"
#define MONO_ENGINE_NAMESPACE MONO_UE4_NAMESPACE ".Engine"

#if WITH_EDITOR
#define MONO_PROJECT_COOKIE_FILE_NAME TEXT(".monoue4")
#endif // WITH_EDITOR

extern const FName NAME_MonoErrors;
