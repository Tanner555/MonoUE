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
#if MONOUE_STANDALONE
private:
	IConsoleObject* GenerateCodeCmd;

	void GenerateCode(const TArray<FString>& Args);

public:
	FMonoScriptGenerator()
		: GenerateCodeCmd(nullptr)
	{	
	}

private:
#endif

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

#if MONOUE_STANDALONE
void FMonoScriptGenerator::GenerateCode(const TArray<FString>& Args)
{
	// Stripped down example of a uhtmanifest which is used to supply IScriptGeneratorPluginInterface with various paths
	// MonoTest\Intermediate\Build\Win64\MonoTestEditor\Development\MonoTestEditor.uhtmanifest
	//{
	//    "IsGameTarget": true,
	//    "RootLocalPath": "C:\\Program Files\\Epic Games\\UE_4.20",
	//    "RootBuildPath": "C:\\Program Files\\Epic Games\\UE_4.20\\",
	//    "TargetName": "MonoTestEditor",
	//    "ExternalDependenciesFile": "C:\\Projects\\MonoTest\\Intermediate\\Build\\Win64\\MonoTestEditor\\Development\\MonoTestEditor.deps",
	//    "Modules": [{
	//        "Name": "CoreUObject",
	//        "ModuleType": "EngineRuntime",
	//        "BaseDirectory": "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Source\\Runtime\\CoreUObject",
	//        "IncludeBase": "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Source\\Runtime",
	//        "OutputDirectory": "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Intermediate\\Build\\Win64\\UE4Editor\\Inc\\CoreUObject",
	//        "ClassesHeaders": [],
	//        "PublicHeaders": ["C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Source\\Runtime\\CoreUObject\\Public\\UObject\\CoreNetTypes.h", "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Source\\Runtime\\CoreUObject\\Public\\UObject\\CoreOnline.h", "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Source\\Runtime\\CoreUObject\\Public\\UObject\\NoExportTypes.h"],
	//        "PrivateHeaders": [],
	//        "PCH": "",
	//        "GeneratedCPPFilenameBase": "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Intermediate\\Build\\Win64\\UE4Editor\\Inc\\CoreUObject\\CoreUObject.gen",
	//        "SaveExportedHeaders": false,
	//        "UHTGeneratedCodeVersion": "None"
	//    }, {
	//        "Name": "InputCore",
	//        "ModuleType": "EngineRuntime",
	//        "BaseDirectory": "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Source\\Runtime\\InputCore",
	//        "IncludeBase": "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Source\\Runtime",
	//        "OutputDirectory": "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Intermediate\\Build\\Win64\\UE4Editor\\Inc\\InputCore",
	//        "ClassesHeaders": ["C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Source\\Runtime\\InputCore\\Classes\\InputCoreTypes.h"],
	//        "PublicHeaders": [],
	//        "PrivateHeaders": [],
	//        "PCH": "",
	//        "GeneratedCPPFilenameBase": "C:\\Program Files\\Epic Games\\UE_4.20\\Engine\\Intermediate\\Build\\Win64\\UE4Editor\\Inc\\InputCore\\InputCore.gen",
	//        "SaveExportedHeaders": false,
	//        "UHTGeneratedCodeVersion": "None"
	//    }]
	//}

	// These paths are from .uhtmanifest
	FString RootLocalPath = FPaths::Combine(*FPaths::EngineDir(), TEXT(".."));
	FPaths::CollapseRelativeDirectories(RootLocalPath);
	
	FString RootBuildPath = RootLocalPath;// This is just the same as RootLocalPath?	

	// OutputDirectory / IncludeBase are obtained from the module entry in .uhtmanifest (defined by GetGeneratedCodeModuleName which is "MonoRuntime")
	// - The MonoUE generator doesn't currently use IncludeBase but does use OutputDirectory for the output code
	FString PluginBaseDir = FPaths::GetPath(FModuleManager::Get().GetModuleFilename("MonoScriptGenerator"));
	PluginBaseDir = FPaths::Combine(*PluginBaseDir, TEXT("../../"));
	FPaths::CollapseRelativeDirectories(PluginBaseDir);

	// I think this is where it normally goes?
	FString OutDir = FPaths::Combine(*PluginBaseDir, TEXT("Intermediate/Build/Win64/UE4Editor/Inc/MonoRuntime"));
	FPaths::CollapseRelativeDirectories(OutDir);

	FString OutputDirectory = OutDir;
	FString IncludeBase = OutDir;

	Initialize(RootLocalPath, RootBuildPath, OutputDirectory, IncludeBase);	

	// TODO: ModuleType info for modules doesn't appear to be available anywhere in the engine. Therefore we would need to search for all
	//       .uplugin files and match them up to the loaded modules. Then use the "Type" to get the EBuildModuleType::Type from the json.
	//       - I think UBT does this but UBT is written in C# so we would have to emulate this ourselves.

	TArray<FName> ModuleNames;
	FModuleManager::Get().FindModules(TEXT("*"), ModuleNames);

	// Gather all UClass

	TMap<UPackage*, TArray<UClass*>> ClassesByPackage;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		UPackage* Package = Class->GetOutermost();

		TArray<UClass*>* Classes = ClassesByPackage.Find(Package);
		if (Classes == nullptr)
		{
			Classes = &ClassesByPackage.Add(Package, TArray<UClass*>());
		}

		Classes->Add(Class);
	}

	for (FName ModuleName : ModuleNames)
	{
		// Force everything to be a EBuildModuleType::EngineRuntime for now
		// Last arg should be the module OutputDirectory from the json, this is currently only use for EBuildModuleType::GameRuntime
		if (ShouldExportClassesForModule(ModuleName.ToString(), EBuildModuleType::EngineRuntime, TEXT("")))
		{
			FString PackageName = FString(TEXT("/Script/")) + ModuleName.ToString();
			UPackage* Package = FindPackage(nullptr, *PackageName);
			if (Package != nullptr)
			{
				TArray<UClass*>* Classes = ClassesByPackage.Find(Package);
				if (Classes != nullptr)
				{
					for (UClass* Class : *Classes)
					{
						// Source / header file paths aren't used so just pass empty strings
						ExportClass(Class, TEXT(""), TEXT(""), false);
					}
				}
			}
		}
	}

	FinishExport();
}
#endif

void FMonoScriptGenerator::StartupModule()
{
#if MONOUE_STANDALONE
	GenerateCodeCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("MonoGen"),
			TEXT("MonoUE generate C# code"),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FMonoScriptGenerator::GenerateCode),
			ECVF_Default);
#endif

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
