// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"

class FMonoGeneratedFileManager
{
public:
	~FMonoGeneratedFileManager();

	/** Saves generated script glue to a temporary file if its contents is different from the existing one. */
	void SaveFileIfChanged(const FString& FilePath, const FString& NewFileContents);
	/** Renames/replaces all existing script glue files with the temporary (new) ones */
	void RenameTempFiles();

private:
	/** List of temporary files crated by SaveFileIfChanged */
	TArray<FString> TempFiles;

};