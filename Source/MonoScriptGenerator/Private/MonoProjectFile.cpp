// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoProjectFile.h"
#include "MonoScriptCodeGeneratorUtils.h"

FMonoProjectFile::FMonoProjectFile(const FString& InSourceDirectory, const FString& InAssemblyName)
	: AssemblyName(InAssemblyName)
	, SourceDirectory(InSourceDirectory)
	, ProjectFilePath(FPaths::Combine(*InSourceDirectory, *FString::Printf(TEXT("%s.csproj"), *InAssemblyName)))
{
	// see if the project already exists, if so re-use the guid to avoid unnecessary changes to the solution
	if (!MonoScriptCodeGeneratorUtils::ParseGuidFromProjectFile(ProjectFileGuid, ProjectFilePath))
	{
		// Did not exist, generate a guid
		FPlatformMisc::CreateGuid(ProjectFileGuid);
		isSdkStyle = true;
	}
	else
	{
		isSdkStyle = false;
	}
}

