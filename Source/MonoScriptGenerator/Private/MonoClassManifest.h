// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"

// Manifest of all native UE4 classes, used to prevent name collisions when building assemblies 
// as UE4 classes are not namespaced
class FMonoClassManifest
{
public:
	void Initialize(const FString& OutputFilePath);

	void AddClass(UClass& InClass);

	void FinishExport();

private:
	FString OutputFilePath;
	TSet<UClass*> AllUnrealClasses;
	TSet<UScriptStruct*> AllUnrealStructs;
	TSet<UEnum*> AllUnrealEnums;
};