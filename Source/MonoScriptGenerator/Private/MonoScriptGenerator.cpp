// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MonoScriptCodeGenerator.h"
#include "MonoScriptGeneratorLog.h"
#include "MonoScriptCodeGeneratorUtils.h"
#include "MonoClassManifest.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY(LogMonoScriptGenerator);

class FMonoScriptGenerator : public IMonoScriptGenerator
{
	/** Specialized script code generator */
	TUniquePtr<FMonoScriptCodeGenerator> CodeGenerator;

	// Unreal native class manifest for engine and engine plugins
	FMonoClassManifest				   EngineNativeClassManifest;

	// Unreal native class manifest for games
	TMap<FString, TSharedPtr<FMonoClassManifest>>	GameNativeClassManifests;

	/** Track modules to export */
	mutable TSet<FName> ModulesToExport;

	/** Game module information */
	mutable TMap<FName, FMonoGameModuleInfo> GameModules;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** IScriptGeneratorPlugin interface */
	virtual FString GetGeneratedCodeModuleName() const override { return TEXT("MonoRuntime"); }
	/** Returns true if this plugin supports exporting scripts for the specified target. This should handle game as well as editor target names */
	virtual bool SupportsTarget(const FString& TargetName) const { return true; }
	/** Returns true if this plugin supports exporting scripts for the specified module */
	virtual bool ShouldExportClassesForModule(const FString& ModuleName, EBuildModuleType::Type ModuleType, const FString& ModuleGeneratedIncludeDirectory) const override;

	virtual void Initialize(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& IncludeBase) override;
	virtual void ExportClass(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged) override;
	virtual void FinishExport() override;
	virtual FString GetGeneratorName() const override;
};

IMPLEMENT_MODULE(FMonoScriptGenerator, MonoScriptGenerator)


void FMonoScriptGenerator::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(TEXT("ScriptGenerator"), this);
	MonoScriptCodeGeneratorUtils::InitializeToolTipLocalization();
	CodeGenerator = MakeUnique<FMonoScriptCodeGenerator>();
}

void FMonoScriptGenerator::ShutdownModule()
{
	CodeGenerator.Reset();
	IModularFeatures::Get().UnregisterModularFeature(TEXT("ScriptGenerator"), this);
}

FString FMonoScriptGenerator::GetGeneratorName() const
{
	return TEXT("Mono Code Generator Plugin");
}

bool FMonoScriptGenerator::ShouldExportClassesForModule(const FString& ModuleName, EBuildModuleType::Type ModuleType, const FString& ModuleGeneratedIncludeDirectory) const
{
	const FName ModuleFName(*ModuleName);

	// only export runtime/game bindings
	if (ModuleType == EBuildModuleType::EngineRuntime || ModuleType == EBuildModuleType::GameRuntime)
	{
		ModulesToExport.Add(ModuleFName);
	}

	if (ModuleType == EBuildModuleType::GameRuntime)
	{
		FString PlatformDirectory = FPaths::Combine(*ModuleGeneratedIncludeDirectory, TEXT(".."), TEXT(".."));
		FPaths::NormalizeDirectoryName(PlatformDirectory);
		FPaths::CollapseRelativeDirectories(PlatformDirectory);
		FString PlatformName = FPaths::GetCleanFilename(PlatformDirectory);

		FMonoGameModuleInfo GameInfo;
		GameInfo.GameModuleMonoIntermediateDirectory = FPaths::Combine(*PlatformDirectory, TEXT("Mono"));
		FPaths::NormalizeDirectoryName(GameInfo.GameModuleMonoIntermediateDirectory);
		FPaths::CollapseRelativeDirectories(GameInfo.GameModuleMonoIntermediateDirectory);

		TArray<FString> FoundProjects;

		// Walk up to the project directory
		TArray<FString> Directories;
		GameInfo.GameModuleMonoIntermediateDirectory.ParseIntoArray(Directories, TEXT("/"));
		int32 IntermediateIndex = Directories.Find(TEXT("Intermediate"));
		if (IntermediateIndex != INDEX_NONE)
		{
			Directories.SetNum(IntermediateIndex);

			FString GameRootDirectory = FString::Join(Directories, TEXT("/"));
			FPaths::NormalizeDirectoryName(GameRootDirectory);

			IFileManager::Get().FindFiles(FoundProjects, *FPaths::Combine(*GameRootDirectory, TEXT("*.uproject")), true, false);
			if (FoundProjects.Num() == 0)
			{
				UE_LOG(LogMonoScriptGenerator, Error, TEXT("Did not find a uproject file in '%s'."), *GameRootDirectory);
			}
			else if (FoundProjects.Num() > 1)
			{
				UE_LOG(LogMonoScriptGenerator, Error, TEXT("Found more than one uproject file in '%s'. Using first one."), *GameRootDirectory);
			}

			GameInfo.GameModuleManifestDirectory = FPaths::Combine(*GameRootDirectory, TEXT("Binaries"), *PlatformName, TEXT("Mono"));
		}

		if (FoundProjects.Num() == 0)
		{
			GameInfo.GameName = TEXT("Unknown");
		}
		else
		{
			GameInfo.GameName = FPaths::GetBaseFilename(FoundProjects[0]);
		}

		GameModules.Add(ModuleFName, GameInfo);
	}

	// note: we need to generate a manifest of *every* uclass, even ones we don't export bindings for, so we can prevent collisions as UE4 classes are not namespaced
	return true;
}

void FMonoScriptGenerator::Initialize(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& IncludeBase)
{
	CodeGenerator->Initialize(RootLocalPath, RootBuildPath, OutputDirectory);
	EngineNativeClassManifest.Initialize(FPaths::Combine(*CodeGenerator->GetMonoBuildManifestOutputDirectory(), TEXT("AllNativeClasses.manifest")));
}

void FMonoScriptGenerator::ExportClass(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged)
{
	check(Class);
	FName ClassModule = MonoScriptCodeGeneratorUtils::GetModuleFName(*Class);

	if (ModulesToExport.Contains(ClassModule))
	{
		CodeGenerator->GatherClassForExport(Class, SourceHeaderFilename, GeneratedHeaderFilename, bHasChanged);
	}

	const FMonoGameModuleInfo* GameModuleInfo = GameModules.Find(ClassModule);

	if (nullptr != GameModuleInfo)
	{
		// see if a native class manifest exists for this game's output path
		TSharedPtr<FMonoClassManifest> ClassManifest = GameNativeClassManifests.FindRef(GameModuleInfo->GameModuleManifestDirectory);
		if (!ClassManifest.IsValid())
		{
			ClassManifest = MakeShareable(new FMonoClassManifest);
			ClassManifest->Initialize(FPaths::Combine(*GameModuleInfo->GameModuleManifestDirectory, TEXT("AllNativeClasses.manifest")));
			GameNativeClassManifests.Add(GameModuleInfo->GameModuleManifestDirectory, ClassManifest);
		}
		check(ClassManifest.IsValid());
		ClassManifest->AddClass(*Class);
	}
	else
	{
		EngineNativeClassManifest.AddClass(*Class);
	}
}

void FMonoScriptGenerator::FinishExport()
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	// create all game intermediate and output directories
	for (auto&& GameModule : GameModules)
	{
		if (!PlatformFile.CreateDirectoryTree(*GameModule.Value.GameModuleMonoIntermediateDirectory))
		{
			UE_LOG(LogMonoScriptGenerator, Error, TEXT("Error creating directory %s"), *GameModule.Value.GameModuleMonoIntermediateDirectory);
		}

		if (!PlatformFile.CreateDirectoryTree(*GameModule.Value.GameModuleManifestDirectory))
		{
			UE_LOG(LogMonoScriptGenerator, Error, TEXT("Error creating directory %s"), *GameModule.Value.GameModuleManifestDirectory);
		}
	}
	CodeGenerator->FinishExport(ModulesToExport, GameModules);
	EngineNativeClassManifest.FinishExport();
	for (auto&& ClassManifest : GameNativeClassManifests)
	{
		ClassManifest.Value->FinishExport();
	}
	GameModules.Empty();
	GameNativeClassManifests.Empty();
}
