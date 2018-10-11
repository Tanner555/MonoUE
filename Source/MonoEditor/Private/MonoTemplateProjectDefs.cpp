// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoTemplateProjectDefs.h"
#include "MonoEditorCommon.h"
#include "IMonoRuntime.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "GameProjectUtils.h"

#define LOCTEXT_NAMESPACE "MonoEditor"

UMonoTemplateProjectDefs::UMonoTemplateProjectDefs(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UMonoTemplateProjectDefs::GeneratesCode(const FString& ProjectTemplatePath) const
{
	const FString SourceDirectory = FPaths::Combine(*ProjectTemplatePath, TEXT("Source"));

	// search for target files
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(*SourceDirectory, TEXT("*.Target.cs")), true, false);

	return Files.Num() > 0;
}

bool UMonoTemplateProjectDefs::IsClassRename(const FString& DestFilename, const FString& SrcFilename, const FString& FileExtension) const
{

	// TODO: F#
	if (FileExtension == TEXT("cs"))
	{
		// we shouldn't be getting this call if it's a file who's name didn't change
		check(FPaths::GetBaseFilename(SrcFilename) != FPaths::GetBaseFilename(DestFilename));

		FString FileContents;
		if (ensure(FFileHelper::LoadFileToString(FileContents, *DestFilename)))
		{
			// TODO: this is a little fragile - we're looking for ObjectInitializer, which should only be the case for UObject-derived classes
			// Think up a better way (this isn't much worse than what Epic does for headers)
			if (FileContents.Contains(TEXT("ObjectInitializer"), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		return Super::IsClassRename(DestFilename, SrcFilename, FileExtension);
	}
}

void UMonoTemplateProjectDefs::AddConfigValues(TArray<FTemplateConfigValue>& ConfigValuesToSet, const FString& TemplateName, const FString& ProjectName, bool bShouldGenerateCode) const
{
	Super::AddConfigValues(ConfigValuesToSet, TemplateName, ProjectName, bShouldGenerateCode);


	//our managed script package is %TEMPLATENAME%Mono, not %TEMPLATENAME%, so we need extra remaps
	const FString ActiveGameNameRedirectsValue_LongName = FString::Printf(TEXT("(OldGameName=\"/Script/%sMono\",NewGameName=\"/Script/%sMono\")"), *TemplateName, *ProjectName);
	const FString ActiveGameNameRedirectsValue_ShortName = FString::Printf(TEXT("(OldGameName=\"%sMono\",NewGameName=\"/Script/%sMono\")"), *TemplateName, *ProjectName);
	new (ConfigValuesToSet) FTemplateConfigValue(TEXT("DefaultEngine.ini"), TEXT("/Script/Engine.Engine"), TEXT("+ActiveGameNameRedirects"), *ActiveGameNameRedirectsValue_LongName, /*InShouldReplaceExistingValue=*/false);
	new (ConfigValuesToSet) FTemplateConfigValue(TEXT("DefaultEngine.ini"), TEXT("/Script/Engine.Engine"), TEXT("+ActiveGameNameRedirects"), *ActiveGameNameRedirectsValue_ShortName, /*InShouldReplaceExistingValue=*/false);
}

bool UMonoTemplateProjectDefs::PreGenerateProject(const FString& DestFolder, const FString& SrcFolder, const FString& NewProjectFile, const FString& TemplateFile, bool bShouldGenerateCode, FText& OutFailReason)
{
	//Add Project GUIDS
	TArray<FString> ProjectFiles;
	IFileManager::Get().FindFilesRecursive(ProjectFiles, *DestFolder, TEXT("*.csproj"), true, false);
	for (const FString &CSProjectFile : ProjectFiles)
	{
		FString FileContents;
		if (ensure(FFileHelper::LoadFileToString(FileContents, *CSProjectFile)))
		{
			FGuid ProjectGUID = FGuid::NewGuid();
			FileContents = FileContents.Replace(TEXT("%PROJECT_GUID%"), *ProjectGUID.ToString(EGuidFormats::DigitsWithHyphens), ESearchCase::CaseSensitive);
			FText FailReason;
			if (!GameProjectUtils::WriteOutputFile(CSProjectFile, FileContents, FailReason))
			{
				OutFailReason = FText::FromString(FString::Printf(TEXT("Couldn't write project GUID to %s"), *CSProjectFile));
				return false;
			}
		}
	}

	return true;
}

bool UMonoTemplateProjectDefs::PostGenerateProject(const FString& DestFolder, const FString& SrcFolder, const FString& NewProjectFile, const FString& TemplateFile, bool bShouldGenerateCode, FText& OutFailReason)
{
	// native projects generate their projects as part of creation via UBT, and the managed projects will be generated at same time
	if (!bShouldGenerateCode)
	{
		//hopefully this is a safe assumption
		FString UBTTarget = FString(TEXT("Editor"));
		FString UBTConfig = FString(FModuleManager::GetUBTConfiguration());
		FString UBTPlatform = FPlatformProcess::GetBinariesSubdirectory();
		FString AppName = FPaths::GetBaseFilename(DestFolder);
		
		IMonoRuntime::Get().GenerateProjectsAndBuildGameAssemblies(OutFailReason, *GWarn, *AppName, *DestFolder, *NewProjectFile, *UBTConfig, *UBTTarget, *UBTPlatform);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE