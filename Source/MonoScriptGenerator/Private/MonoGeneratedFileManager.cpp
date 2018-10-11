// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoGeneratedFileManager.h"
#include "MonoScriptGeneratorLog.h"
#include "Misc/FileHelper.h"

FMonoGeneratedFileManager::~FMonoGeneratedFileManager()
{
	RenameTempFiles();
}

void FMonoGeneratedFileManager::SaveFileIfChanged(const FString& FilePath, const FString& NewFileContents)
{
	FString OriginalFileLocal;
	FFileHelper::LoadFileToString(OriginalFileLocal, *FilePath);

	const bool bHasChanged = OriginalFileLocal.Len() == 0 || FCString::Strcmp(*OriginalFileLocal, *NewFileContents);
	if (bHasChanged)
	{
		// save the updated version to a tmp file so that the user can see what will be changing
		const FString TempFileName = FilePath + TEXT(".tmp");

		// delete any existing temp file
		IFileManager::Get().Delete(*TempFileName, false, true);
		if (!FFileHelper::SaveStringToFile(NewFileContents, *TempFileName))
		{
			UE_LOG(LogMonoScriptGenerator, Warning, TEXT("Failed to save glue export: '%s'"), *TempFileName);
		}
		else
		{
			TempFiles.Add(TempFileName);
		}
	}
}


/** Renames/replaces all existing script glue files with the temporary (new) ones */
void FMonoGeneratedFileManager::RenameTempFiles()
{
	// Rename temp files
	for (auto& TempFilename : TempFiles)
	{
		FString Filename = TempFilename.Replace(TEXT(".tmp"), TEXT(""));
		if (!IFileManager::Get().Move(*Filename, *TempFilename, true, true))
		{
			UE_LOG(LogMonoScriptGenerator, Error, TEXT("%s"), *FString::Printf(TEXT("Couldn't write file '%s'"), *Filename));
		}
		else
		{
			UE_LOG(LogMonoScriptGenerator, Log, TEXT("Exported updated script glue: %s"), *Filename);
		}
	}

	TempFiles.Empty();
}

