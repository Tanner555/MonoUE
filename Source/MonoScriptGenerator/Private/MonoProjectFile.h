// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "MonoBindingsModule.h"

struct FMonoProjectFile
{
	FString AssemblyName;
	FString SourceDirectory;
	FString ProjectFilePath;
	FGuid   ProjectFileGuid;
	bool   isSdkStyle;
	TArray<FMonoBindingsModule> BindingsModules;

	FMonoProjectFile(const FString& InSourceDirectory, const FString& InAssemblyName);
};
