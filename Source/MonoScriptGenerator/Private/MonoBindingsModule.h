// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"

struct FMonoGameModuleInfo
{
	FString GameModuleMonoIntermediateDirectory;
	FString GameModuleManifestDirectory;
	FString GameName;
};

struct FMonoBindingsModule
{
	TSet<FString> AdditionalSystemReferences;
	TSet<FName> ModuleReferences;
	TSet<FName> ExportedTypes;
	bool bExportModule;
	bool bExportExtensions;

	FMonoBindingsModule(FName InModuleName, const FString& RootMonoSourceDirectory, const FMonoGameModuleInfo* InGameInfo, FName InMappedModuleName);

	bool IsBuiltinEngineModule() const;
	bool IsGameModule() const;

	const FString& GetGeneratedSourceDirectory() const { return BindingsSourceDirectory; }

	const FString& GetNamespace() const { return Namespace; }

	inline FName GetModuleName() const { return ModuleName; }
	inline FString GetMappedModuleNameString() const { return MappedModuleName.ToString(); }

	// calling these on a built in module will assert
	FString GetGeneratedProjectDirectory() const;
	FString GetAssemblyName() const;

	// calling this on anything but a game module will assert
	FString GetGameSolutionDirectory() const;
	FString GetGameName() const;
	FString GetGameModuleManifestDirectory() const;

private:
	FName ModuleName;
	FName MappedModuleName;
	FString Namespace;
	FString BindingsSourceDirectory;
	bool bPluginModule;
	TSharedPtr<FMonoGameModuleInfo> GameInfo;
};
