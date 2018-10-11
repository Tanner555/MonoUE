// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoBindingsModule.h"
#include "MonoScriptCodeGeneratorUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

bool IsPluginModule(const FName ModuleName)
{
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
	{
		for (FModuleDescriptor ModuleInfo : Plugin->GetDescriptor().Modules)
		{
			if (ModuleInfo.Name == ModuleName)
			{
				return true;
			}
		}
	}
	return false;
}

FMonoBindingsModule::FMonoBindingsModule(FName InModuleName, const FString& RootMonoSourceDirectory, const FMonoGameModuleInfo* InGameInfo, FName InMappedModuleName)
	: bExportModule(false)
	, bExportExtensions(false)
	, ModuleName(InModuleName)
	, MappedModuleName(InMappedModuleName)
{
	if (nullptr != InGameInfo)
	{
		GameInfo = MakeShareable(new FMonoGameModuleInfo(*InGameInfo));
	}

	bPluginModule = IsPluginModule(ModuleName);

	if (!IsBuiltinEngineModule())
	{
		if (IsGameModule())
		{
			BindingsSourceDirectory = FPaths::Combine(*GameInfo->GameModuleMonoIntermediateDirectory, *InModuleName.ToString());
		}
		else
		{
			BindingsSourceDirectory = FPaths::Combine(*RootMonoSourceDirectory, *InModuleName.ToString());
		}
	}
	else
	{
		BindingsSourceDirectory = FPaths::Combine(*RootMonoSourceDirectory, BUILTIN_MODULES_PROJECT_NAME, *InModuleName.ToString());
	}

	if (IsGameModule())
	{
		Namespace = MappedModuleName.ToString();
	}
	else
	{
		Namespace = FString::Printf(MONO_UE4_NAMESPACE TEXT(".%s"), *MappedModuleName.ToString());
	}
}

bool FMonoBindingsModule::IsBuiltinEngineModule() const
{
	return !bPluginModule && !IsGameModule();
}

bool FMonoBindingsModule::IsGameModule() const
{
	return GameInfo.IsValid();
}

FString FMonoBindingsModule::GetGeneratedProjectDirectory() const
{
	check(!IsBuiltinEngineModule());
	return BindingsSourceDirectory;
}

FString FMonoBindingsModule::GetAssemblyName() const
{
	check(!IsBuiltinEngineModule());
	if (IsGameModule())
	{
		return MappedModuleName.ToString();
	}
	else
	{
		return FString::Printf(MONO_UE4_NAMESPACE TEXT(".%s"), *MappedModuleName.ToString());
	}
}

FString FMonoBindingsModule::GetGameSolutionDirectory() const
{
	check(IsGameModule());
	return GameInfo->GameModuleMonoIntermediateDirectory;
}

FString FMonoBindingsModule::GetGameName() const
{
	check(IsGameModule());
	return GameInfo->GameName;
}

FString FMonoBindingsModule::GetGameModuleManifestDirectory() const
{
	check(IsGameModule());
	return GameInfo->GameModuleManifestDirectory;
}
