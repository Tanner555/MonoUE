// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "DefaultTemplateProjectDefs.h"

#include "MonoTemplateProjectDefs.generated.h"
UCLASS()
class UMonoTemplateProjectDefs : public UDefaultTemplateProjectDefs
{
	GENERATED_UCLASS_BODY()

	virtual bool GeneratesCode(const FString& ProjectTemplatePath) const override;
	
	virtual bool IsClassRename(const FString& DestFilename, const FString& SrcFilename, const FString& FileExtension) const override;
	
	virtual void AddConfigValues(TArray<FTemplateConfigValue>& ConfigValuesToSet, const FString& TemplateName, const FString& ProjectName, bool bShouldGenerateCode) const override;

	virtual bool PreGenerateProject(const FString& DestFolder, const FString& SrcFolder, const FString& NewProjectFile, const FString& TemplateFile, bool bShouldGenerateCode, FText& OutFailReason) override;

	virtual bool PostGenerateProject(const FString& DestFolder, const FString& SrcFolder, const FString& NewProjectFile, const FString& TemplateFile, bool bShouldGenerateCode, FText& OutFailReason) override;

};
