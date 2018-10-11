// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once
#include "MonoSourceCodeNavigation.h"

class MonoEditorUtils
{
public:
	static bool HasManagedSolutionFile();
	static FString GetManagedSolutionFilepath();
	/*
	static bool AddCodeToProject(const GameProjectUtils::FModuleContextInfo &ModuleInfo, const FString& NewClassName, const FString& NewClassPath, const GameProjectUtils::FNewClassInfo ParentClassInfo, FString& OutClassFilePath, FText& OutFailReason);
	*/
	static void EnableIdeIntegration();
	static void DisableIdeIntegration();
private:
	static bool StartedIdeIntegration;
	static FMonoSourceCodeNavigationHandler NavigationHandler;

	/*
	static bool ReadTemplateFile(const FString& TemplateFileName, FString& OutFileContents, FText& OutFailReason);
	static bool GenerateClassFile(const FString& NewClassFileName, const FString UnPrefixedClassName, const GameProjectUtils::FNewClassInfo ClassInfo, const TArray<FString>& PropertyOverrides, const FString& AdditionalMemberDefinitions, const GameProjectUtils::FModuleContextInfo& ModuleInfo, FText& OutFailReason);
	static bool GenerateEmptyProject(const GameProjectUtils::FModuleContextInfo &ModuleInfo, const FString &ProjectFolder, FText& OutFailReason);
	*/
};