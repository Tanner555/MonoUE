// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
class FMonoBuildUtils
{
public:

	static bool RunExternalManagedExecutable(const FText& Description,
											const FString& ExePath, 
											const FString& Parameters,
											FFeedbackContext* Warn);

	static bool BuildManagedCode(const FText& Description, FFeedbackContext* Warn, const FString& AppName, const FString& ProjectDir, const FString& ProjectFile, const FString& TargetConfiguration, const FString& TargetType, const FString& TargetPlatform);
};
#endif // WITH_EDITOR
