// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoBuildUtils.h"
#include "MonoRuntimeCommon.h"

#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/WindowsPlatformMisc.h"
#endif


#if WITH_EDITOR
#include "Misc/FeedbackContextMarkup.h"
bool FMonoBuildUtils::RunExternalManagedExecutable(
	const FText& Description,
	const FString& ExePath,
	const FString& Parameters,
	FFeedbackContext* Warn)
{
	if (!FPaths::FileExists(*ExePath))
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("Couldn't find external executable at '%s'"), *ExePath);
		return false;
	}

	Warn->Logf(TEXT("Running %s %s"), *ExePath, *Parameters);

	int32 ExitCode;
    bool bResult;

#if PLATFORM_WINDOWS
	bResult = FFeedbackContextMarkup::PipeProcessOutput(Description, ExePath, Parameters, Warn, &ExitCode);
#elif PLATFORM_MAC
    if (FPaths::GetExtension(ExePath) == TEXT("exe"))
    {
        FString MonoParams(ExePath + TEXT(" ") + Parameters);
        FString MonoExePath(FPaths::EnginePluginsDir() / TEXT("MonoUE/MSBuild/mac-mono.sh"));
        bResult = FFeedbackContextMarkup::PipeProcessOutput(Description, MonoExePath, MonoParams, Warn, &ExitCode);
    }
    else
    {
        bResult = FFeedbackContextMarkup::PipeProcessOutput(Description, ExePath, Parameters, Warn, &ExitCode);
    }
#else
#error Platform not supported
#endif

	return bResult && ExitCode == 0;
}

bool FMonoBuildUtils::BuildManagedCode(const FText& Description, FFeedbackContext* Warn, const FString& AppName, const FString& ProjectDir, const FString& ProjectFile, const FString& TargetConfiguration, const FString& TargetType, const FString& TargetPlatform)
{
	FString BuildToolPath(FPaths::EnginePluginsDir() / TEXT("MonoUE/Binaries/DotNet/MonoUEBuildTool.exe"));

	FString ExternalEngineDir = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::EngineDir());
	FString ExternalPluginDir = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FString(FPaths::EnginePluginsDir() / TEXT("MonoUE")));
	FString ExternalProjectDir = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ProjectDir);
	FString ExternalProjectFile = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ProjectFile);
	//FIXME on Linux we need to append an architecture
	FString PlatformIntermediateDir(TEXT("Intermediate/Build") / TargetPlatform);

	const FString Parameters = FString::Printf(
		TEXT("Build -EngineDir \"%s\" -ProjectDir \"%s\" -TargetName \"%s\" -TargetPlatform \"%s\" -TargetConfiguration \"%s\" -TargetType \"%s\" -ProjectFile \"%s\" -PluginDir \"%s\" -AppName \"%s\" -PlatformIntermediateDir \"%s\""),
		*ExternalEngineDir,
		*ExternalProjectDir,
		*AppName,
		*TargetPlatform,
		*TargetConfiguration,
		*TargetType,
		*ExternalProjectFile,
		*ExternalPluginDir,
		*AppName,
		*PlatformIntermediateDir
	);

	return RunExternalManagedExecutable(Description, BuildToolPath, Parameters, Warn);
}

#endif // WITH_EDITOR
