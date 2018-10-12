// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoMainDomain.h"
#include "MonoRuntimeCommon.h"
#include "MonoRuntimePrivate.h"
#include "MonoHelpers.h"
#include "CoreMinimal.h"

#include "Containers/StringConv.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/mono-debug.h>

void MonoRegisterDllImportMappings();

FMonoMainDomain::FMonoMainDomain(MonoDomain* InDomain, const FString& InEngineAssemblyDirectory, const FString& InGameAssemblyDirectory)
	: FMonoDomain(InDomain, Mono::InvokeExceptionBehavior::OutputToLog)
	, EngineAssemblyDirectory(InEngineAssemblyDirectory)
	, GameAssemblyDirectory(InGameAssemblyDirectory)
#if MONOUE_STANDALONE
	, Loaded(false)
#endif
{
	const FString MainDomainAssemblyName = ANSI_TO_TCHAR(MONO_UE4_NAMESPACE ".MainDomain");

#if MONOUE_STANDALONE
	if (!MainDomainAssembly.Open(GetDomain(), MainDomainAssemblyName))
	{
		return;
	}
#else
	verify(MainDomainAssembly.Open(GetDomain(), MainDomainAssemblyName));
#endif

	MonoMethod* InitializeMethod = MainDomainAssembly.LookupMethod(MONO_UE4_NAMESPACE ".MainDomain.MainDomain:Initialize");
	check(InitializeMethod);

#if MONO_WITH_HOT_RELOADING
	bool bWithAppDomains = true;
#else
	bool bWithAppDomains = false;
#endif
	Mono::Invoke<void>(*this, InitializeMethod, nullptr,bWithAppDomains);

	AppDomainClass = mono_class_from_name(mono_get_corlib(), "System", "AppDomain");
	check(AppDomainClass);
	AppDomainMonoAppDomainField = mono_class_get_field_from_name(AppDomainClass, "_mono_app_domain");
	check(AppDomainMonoAppDomainField);

#if MONOUE_STANDALONE
	Loaded = true;
#endif
}

FMonoMainDomain::~FMonoMainDomain()
{
	mono_jit_cleanup(GetDomain());
}

struct AssemblySearchPath {
	FString Path;
#if MONO_WITH_HOT_RELOADING
	FString ShadowCopyPath;
	AssemblySearchPath(FString path, FString shadowCopyPath) : Path(path), ShadowCopyPath(shadowCopyPath) {}
	AssemblySearchPath(FString path) : AssemblySearchPath(path, FString()) {}
#else
	AssemblySearchPath(FString path) : Path(path) {}
#endif
};

static TArray<AssemblySearchPath> MonoPreloadSearchPaths;

inline bool SizeAndMtimeEqual(FFileStatData a, FFileStatData b)
{
	return a.ModificationTime == b.ModificationTime && a.FileSize == b.FileSize;
}

static FString ShadowCopyAssembly(const FString& AsmPath, const FString& AsmName, const FString& AsmCulture, const FString& ShadowCopyRoot)
{
	IFileManager& FileManager = IFileManager::Get();

	FString PdbPath = FPaths::ChangeExtension(AsmPath, TEXT(".pdb"));
	FFileStatData AsmStat = FileManager.GetStatData(*AsmPath);
	FFileStatData PdbStat = FileManager.GetStatData(*PdbPath);

	for (int i = 0; i < 20; i++)
	{
		FString ShadowCopyDirectory = FPaths::Combine(ShadowCopyRoot, FString::FromInt(i));
		if (AsmCulture.Len() != 0)
		{
			ShadowCopyDirectory = FPaths::Combine(ShadowCopyDirectory, *AsmCulture);
		}
		FString ShadowAsmPath = FPaths::Combine(ShadowCopyDirectory, *AsmName);
		FString ShadowPdbPath = FPaths::ChangeExtension(ShadowAsmPath, TEXT(".pdb"));

		FFileStatData ShadowAsmStat = FileManager.GetStatData(*ShadowAsmPath);
		FFileStatData ShadowPdbStat = FileManager.GetStatData(*ShadowPdbPath);
		if (ShadowAsmStat.bIsValid)
		{
			if (SizeAndMtimeEqual (ShadowAsmStat, AsmStat) && (!PdbStat.bIsValid  || SizeAndMtimeEqual (ShadowPdbStat, PdbStat)))
			{
				UE_LOG(LogMono, Log, TEXT("Re-using existing shadow copy '%s'."), *ShadowAsmPath);
				return ShadowAsmPath;
			}

			if (!FileManager.Delete(*ShadowAsmPath, false, false, true) || !FileManager.Delete(*ShadowPdbPath, false, false, true))
			{
				UE_LOG(LogMono, Log, TEXT("Ignoring locked shadow copy '%s'."), *ShadowAsmPath);
				continue;
			}
		}

		uint32 CopyResult = FileManager.Copy(*ShadowAsmPath, *AsmPath);
		if (CopyResult != COPY_OK)
		{
			UE_LOG(LogMono, Error, TEXT("Failed to shadow copy to '%s' (code %u), loading original."), *ShadowAsmPath, CopyResult);
			return AsmPath;
		}

		if (PdbStat.bIsValid)
		{
			uint32 PdbCopyResult = FileManager.Copy(*ShadowPdbPath, *PdbPath);
			if (PdbCopyResult != COPY_OK)
			{
				UE_LOG(LogMono, Error, TEXT("Failed to shadow copy pdb to '%s' (code %u), loading original assembly."), *ShadowPdbPath, PdbCopyResult);
				return AsmPath;
			}
		}

		UE_LOG(LogMono, Log, TEXT("Shadow copied assembly to '%s'."), *ShadowAsmPath);

		return ShadowAsmPath;
	}

	UE_LOG(LogMono, Error, TEXT("Ran out of shadow copy slots for assembly '%s,Culture=%s', loading original."), *AsmName, *AsmCulture);
	return AsmPath;
}

static MonoAssembly*
assembly_preload_hook(MonoAssemblyName *aname, char **assemblies_path, void* user_data)
{
	IFileManager& FileManager = IFileManager::Get();

	const char *name = mono_assembly_name_get_name (aname);
	const char *culture = mono_assembly_name_get_culture(aname);
	auto AsmName = FString(ANSI_TO_TCHAR(name));
	auto AsmCulture = FString(ANSI_TO_TCHAR(culture));

	//NOTE: we don't support .exe in UE extensions
	if (!AsmName.EndsWith(TEXT(".dll"), ESearchCase::IgnoreCase))
	{
		AsmName = AsmName + TEXT(".dll");
	}

	for (auto SearchPath : MonoPreloadSearchPaths)
	{
		auto AsmPath = FPaths::Combine(*SearchPath.Path, *AsmName);
		if (!FPaths::FileExists(AsmPath))
		{
			AsmPath = FPaths::Combine(*SearchPath.Path, *AsmCulture, *AsmName);
			if (!FPaths::FileExists(AsmPath))
			{
				continue;
			}
		}

		//TODO: be picky about versions matching?
		UE_LOG(LogMono, Log, TEXT("Found assembly %s at path '%s'."), *AsmName, *AsmPath);

		//first try to read the file directly from disk
		//since it mmaps the file and is much more memory efficient
		FString AbsoluteAssemblyPath = FileManager.ConvertToAbsolutePathForExternalAppForRead(*AsmPath);

		//when hot reloading is enabled, shadow-copy the assembly first and load the copy
		//so that file locking doesn't prevent rebuilding the assembly
#if MONO_WITH_HOT_RELOADING
		if (SearchPath.ShadowCopyPath.Len() != 0)
		{
			AbsoluteAssemblyPath = ShadowCopyAssembly(AbsoluteAssemblyPath, AsmName, AsmCulture, *SearchPath.ShadowCopyPath);
		}
#endif

		MonoImageOpenStatus status;
		MonoAssembly *loaded_asm = mono_assembly_open(TCHAR_TO_ANSI(*AbsoluteAssemblyPath), &status);
		if (loaded_asm)
		{
			UE_LOG(LogMono, Log, TEXT("Loaded assembly from path '%s'."), *AbsoluteAssemblyPath);
			return loaded_asm;
		}

		//try to read the file from UFS, e.g. pak file
		auto Reader = FileManager.CreateFileReader(*AsmPath);
		if (!Reader)
		{
			UE_LOG(LogMono, Error, TEXT("Failed to read assembly from UFS path '%s'."), *AsmPath);
			continue;
		}

		uint32 Size = Reader->TotalSize();
		void* Data = FMemory::Malloc(Size);
		Reader->Serialize(Data, Size);

		MonoImage *image = mono_image_open_from_data_with_name((char*)Data, Size, true, &status, false, name);

		//since we told Mono to copy the assembly, we can discard the buffer without worrying about lifetime
		//TODO: avoid the copy
		FMemory::Free(Data);

		if (!image)
		{
			UE_LOG(LogMono, Error, TEXT("Failed to load image from UFS path '%s'."), *AsmPath);
			continue;
		}

		loaded_asm = mono_assembly_load_from_full(image, name, &status, 0);
		if (!loaded_asm)
		{
			UE_LOG(LogMono, Error, TEXT("Failed to load image from UFS path '%s'."), *AsmPath);
			continue;
		}

		UE_LOG(LogMono, Log, TEXT("Loaded assembly from UFS path '%s'."), *AsmPath);
		return loaded_asm;
	}

	UE_LOG(LogMono, Error, TEXT("Could not find assembly %s."), *AsmName);
	return NULL;
}

static void install_preload_hook(const FString& MonoRuntimeDirectory, const FString& InEngineAssemblyDirectory, const FString& InGameAssemblyDirectory)
{
	if (MonoPreloadSearchPaths.Num() == 0)
	{
		mono_install_assembly_preload_hook(assembly_preload_hook, NULL);
	}
	else
	{
		MonoPreloadSearchPaths.Empty();
	}

	//game and engine assemblies are shadow copied as they are likely to change, runtime assemblies are not
#if MONO_WITH_HOT_RELOADING
	IFileManager& FileManager = IFileManager::Get();
	FString GameShadowCopyAssemblyRelative = FMonoMainDomain::GetConfigurationSpecificSubdirectory(FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("MonoShadowCopy")));
	FString GameShadowCopyDirectory = FileManager.ConvertToAbsolutePathForExternalAppForWrite(*GameShadowCopyAssemblyRelative);

	FString EngineShadowCopyAssemblyRelative = FMonoMainDomain::GetConfigurationSpecificSubdirectory(FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("MonoShadowCopy")));
	FString EngineShadowCopyDirectory = FileManager.ConvertToAbsolutePathForExternalAppForWrite(*EngineShadowCopyAssemblyRelative);

	if (!FileManager.MakeDirectory(*GameShadowCopyDirectory, true))
	{
		UE_LOG(LogMono, Error, TEXT("Could not create game shadow copy directory assembly %s. Disabling shadow copying for game assemblies."), *GameShadowCopyDirectory);
		GameShadowCopyDirectory = FString();
	}

	if (!FileManager.MakeDirectory(*EngineShadowCopyDirectory, true))
	{
		UE_LOG(LogMono, Error, TEXT("Could not create engine shadow copy directory assembly %s. Disabling shadow copying for engine assemblies."), *EngineShadowCopyDirectory);
		EngineShadowCopyDirectory = FString();
	}

	MonoPreloadSearchPaths.Add(AssemblySearchPath(*InGameAssemblyDirectory, GameShadowCopyDirectory));
	MonoPreloadSearchPaths.Add(AssemblySearchPath(*InEngineAssemblyDirectory, EngineShadowCopyDirectory));
#else
	MonoPreloadSearchPaths.Add(AssemblySearchPath(*InGameAssemblyDirectory));
	MonoPreloadSearchPaths.Add(AssemblySearchPath(*InEngineAssemblyDirectory));
#endif

#if !UE_BUILD_SHIPPING
	//the framework directory should only be used at dev time - for staged builds, we copy all the
	//framework assemblies to the EngineAssemblyDirectory

	const FString MonoFacadesDirectory = FPaths::Combine(*MonoRuntimeDirectory, TEXT("Facades"));

	MonoPreloadSearchPaths.Add(AssemblySearchPath(*MonoRuntimeDirectory));
	MonoPreloadSearchPaths.Add(AssemblySearchPath(*MonoFacadesDirectory));
#endif
}

FMonoMainDomain* FMonoMainDomain::CreateMonoJIT(const FString& MonoRuntimeDirectory, const FString& InEngineAssemblyDirectory, const FString& InGameAssemblyDirectory)
{
	install_preload_hook(MonoRuntimeDirectory, InEngineAssemblyDirectory, InGameAssemblyDirectory);

#if !UE_BUILD_SHIPPING

	FString MonoArgs;
	if (FParse::Value(FCommandLine::Get(), TEXT("MONOARGS="), MonoArgs))
	{
		TArray<ANSICHAR*> Options;
		FString Token;
		const TCHAR* MonoArgsPtr = *MonoArgs;
		while (FParse::Token(MonoArgsPtr, Token, false))
		{
			auto s = new ANSICHAR[Token.Len() + 1];
			FCStringAnsi::Strcpy(s, Token.Len() + 1, TCHAR_TO_ANSI(*Token));
			Options.Add(s);
		}
		mono_jit_parse_options(Options.Num(), Options.GetData());

		for (auto s : Options)
		{
			delete[] s;
		}
	}

	mono_debug_init(MONO_DEBUG_FORMAT_MONO);

#endif

	MonoDomain* MainDomain = mono_jit_init_version(TCHAR_TO_ANSI(FApp::GetProjectName()), "mobile");
	check(MainDomain);

	char* mono_version = mono_get_runtime_build_info();
	auto MonoVersion = FString(ANSI_TO_TCHAR(mono_version));
	mono_free(mono_version);

	UE_LOG(LogMono, Log, TEXT("Loaded Mono runtime %s"), *MonoVersion);

	MonoRegisterDllImportMappings();

	return new FMonoMainDomain(MainDomain, InEngineAssemblyDirectory, InGameAssemblyDirectory);
}

MonoDomain* FMonoMainDomain::CreateGameDomain()
{
	MonoDomain* GameDomain = mono_domain_create_appdomain((char*)"foo", nullptr);
	return GameDomain;
}

//MUST BE IN SYNC : MonoUE.Core.props, MonoRuntime.Plugin.cs, MonoMainDomain.cpp, MonoRuntimeStagingRules.cs, MonoScriptCodeGenerator.cpp, and IDE extensions
FString FMonoMainDomain::GetConfigurationSpecificSubdirectory(const FString &ParentDirectory)
{
	const TCHAR* ConfigSuffix = NULL;
	switch (FApp::GetBuildConfiguration())
	{
	case EBuildConfigurations::Debug:
		ConfigSuffix = TEXT("-Debug");
		break;
	case EBuildConfigurations::DebugGame:
		ConfigSuffix = TEXT("-DebugGame");
		break;
	case EBuildConfigurations::Development:
		ConfigSuffix = NULL;
		break;
	case EBuildConfigurations::Test:
		ConfigSuffix = TEXT("-Test");
		break;
	case EBuildConfigurations::Shipping:
		ConfigSuffix = TEXT("-Shipping");
		break;
	}

#if WITH_EDITOR
	FString Name = TEXT("MonoEditor");
#elif UE_SERVER
	FString Name = TEXT("MonoServer");
#elif !WITH_SERVER_CODE
	FString Name = TEXT("MonoClient");
#else
	FString Name = TEXT("Mono");
#endif

	Name += FString(ConfigSuffix);

	return FPaths::Combine(*ParentDirectory, FPlatformProcess::GetBinariesSubdirectory(), *Name);
}
