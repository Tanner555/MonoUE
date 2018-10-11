// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoRuntimePrivate.h"
#include "MonoLogBridge.h"
#include "MonoBindings.h"
#include "MonoMainDomain.h"
#include "MonoBuildUtils.h"
#include "MonoUnrealClass.h"

#include "Templates/UniquePtr.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"

#include <mono/jit/jit.h>
#include <mono/utils/mono-logger.h>

#define LOCTEXT_NAMESPACE "MonoRuntime"

const FName NAME_MonoErrors("MonoErrors");
const FName IMonoRuntime::ModuleName("MonoRuntime");

class FMonoRuntime : public IMonoRuntime
{
public:
	FMonoRuntime();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// We don't support dynamic reloading
	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	// Begin IMonoRuntime implementation
#if WITH_EDITOR
	virtual bool GenerateProjectsAndBuildGameAssemblies(FText& OutFailReason, FFeedbackContext& FeedbackContext) const override;
	virtual bool GenerateProjectsAndBuildGameAssemblies(FText& OutFailReason, FFeedbackContext& FeedbackContext, const FString& AppName, const FString& ProjectDir, const FString& ProjectFile, const FString& TargetConfiguration, const FString& TargetType, const FString& TargetPlatform) const override;
	virtual void StartIdeAgent() override;
	virtual void StopIdeAgent() override;
	virtual bool RequestHotReload() override;
	virtual FStopPIEForHotReloadEvent& GetOnStopPIEForHotReloadEvent() override;
	virtual FHotReloadEvent& GetOnHotReloadEvent() override;
	virtual FString GetMonoQualifiedClassName(const UClass* InClass, bool bExcludeBindings = false) const override;
	virtual FString GetMonoClassName(const UClass* InClass, bool bExcludeBindings = false) const override;
	virtual FString GetMonoClassNamespace(const UClass* InClass, bool bExcludeBindings = false) const override;
	virtual bool AddDllMapForModule(const char* DllName, const FName ModuleName) const override;
#endif

	// End IMonoRuntime implementation
private:
	// main app domain created when JIT is initialized
	TUniquePtr<FMonoMainDomain> MonoMainDomain;

	// Bindings in the game app domain, a domain we create so we can tear it down during reloads
	TUniquePtr<FMonoBindings> MonoBindings;

	inline FString GetAssemblyDirectory(const FString &RootDirectory) { return FMonoMainDomain::GetConfigurationSpecificSubdirectory(FPaths::Combine(*RootDirectory, TEXT("Binaries"))); }

#if WITH_EDITOR
	FString PluginDotNETDirectory;
#endif // WITH_EDITOR
};

IMPLEMENT_MODULE( FMonoRuntime, MonoRuntime )

#if !NO_LOGGING
static struct FForceInitAtBootMonoLogBridgeInternalSingleton : public TForceInitAtBoot<FMonoLogBridge>
{} FForceInitAtBootMonoLogBridgeInternalSingleton;
#endif //!NO_LOGGING

// Logging callbacks
extern "C" void MonoPrintf(const char *string, mono_bool is_stdout)
{
#if !NO_LOGGING
	//HACK: Mono uses g_print for this then hard-exits, so we special-case it as a fatal message instead
	if (0 == FCStringAnsi::Strncmp("The assembly mscorlib.dll was not found or could not be loaded", string, 62))
	{
		UE_LOG(LogMono, Fatal, TEXT("%s"), ANSI_TO_TCHAR(string));
	}
	if (UE_LOG_ACTIVE(LogMono, Log))
	{
		FMonoLogBridge::Get().Write(ANSI_TO_TCHAR(string), FCStringAnsi::Strlen(string));
		if (!is_stdout)
		{
			FMonoLogBridge::Get().UserFlush();
		}
	}
#endif
}

extern "C" void MonoLog(const char *log_domain, const char *log_level, const char *message, mono_bool fatal, void *user_data)
{
	// logs are always a single line, so can avoid routing through log bridge
	// note: code is repeated because verbosity suppression is performed at compile-time
	if (fatal || 0 == FCStringAnsi::Strncmp("error", log_level, 5))
	{
		// fatal error
		UE_LOG(LogMono, Fatal, TEXT("%s%s%s"), log_domain != nullptr ? ANSI_TO_TCHAR(log_domain) : TEXT(""), log_domain != nullptr ? TEXT(": ") : TEXT(""), ANSI_TO_TCHAR(message));
	}
#if NO_LOGGING
#else
	else if (0 == FCStringAnsi::Strncmp("warning", log_level, 7))
		{
		UE_LOG(LogMono, Warning, TEXT("%s%s%s"), log_domain != nullptr ? ANSI_TO_TCHAR(log_domain) : TEXT(""), log_domain != nullptr ? TEXT(": ") : TEXT(""), ANSI_TO_TCHAR(message));
			}
	else if (0 == FCStringAnsi::Strncmp("critical", log_level, 8))
				{
		UE_LOG(LogMono, Error, TEXT("%s%s%s"), log_domain != nullptr ? ANSI_TO_TCHAR(log_domain) : TEXT(""), log_domain != nullptr ? TEXT(": ") : TEXT(""), ANSI_TO_TCHAR(message));
			}
	else
				{
		UE_LOG(LogMono, Log, TEXT("%s%s%s"), log_domain != nullptr ? ANSI_TO_TCHAR(log_domain) : TEXT(""), log_domain != nullptr ? TEXT(": ") : TEXT(""), ANSI_TO_TCHAR(message));
			}
#endif
}

FMonoRuntime::FMonoRuntime()
{
}

void FMonoRuntime::StartupModule()
{
#if MONO_IS_DYNAMIC_LIB
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	Mono::LoadMonoDLL();
#endif

	//let native crash handlers work
	mono_set_signal_chaining(1);

#if WITH_EDITOR
	FModuleStatus status;
	verify(FModuleManager::Get().QueryModule("MonoRuntime", status));

	PluginDotNETDirectory = FPaths::Combine(*FPaths::GetPath(status.FilePath), TEXT(".."), TEXT("DotNET"));
	FPaths::CollapseRelativeDirectories(PluginDotNETDirectory);
#endif // WITH_EDITOR

	// Set up directories
	const FString MonoDirectory = FPaths::EnginePluginsDir() / TEXT("MonoUE/ThirdParty/mono/fx/MonoUE/v1.0");

	//set up log hooks
	mono_trace_set_log_handler(MonoLog, nullptr);
	mono_trace_set_print_handler(MonoPrintf);
	mono_trace_set_printerr_handler(MonoPrintf);

	// set up our engine and game assemblies directory
	const FString EngineAssemblyDirectory = GetAssemblyDirectory (FPaths::EnginePluginsDir() / "MonoUE");
	const FString GameAssemblyDirectory = GetAssemblyDirectory(FPaths::ProjectDir());

	// Initialize JIT, and create main domain
	MonoMainDomain.Reset(FMonoMainDomain::CreateMonoJIT(MonoDirectory, EngineAssemblyDirectory, GameAssemblyDirectory));

	// Initialize game bindings/domain
	MonoBindings.Reset(FMonoBindings::CreateMonoBindings(*MonoMainDomain, EngineAssemblyDirectory, GameAssemblyDirectory));

	// Initialization of Mono UObject classes is deferred so that MonoBindings is valid when managed ctors are called.
	// Otherwise, class default objects wouldn't be able to create subobjects.
	MonoBindings->InitializeMonoClasses();
}


void FMonoRuntime::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	MonoBindings.Reset();
	MonoMainDomain.Reset();

	Mono::UnloadMonoDLL();
}

#if WITH_EDITOR
bool FMonoRuntime::GenerateProjectsAndBuildGameAssemblies(FText& OutFailReason, FFeedbackContext& FeedbackContext) const
{
	check(FPaths::IsProjectFilePathSet());

	FString UBTConfig = FString(FModuleManager::GetUBTConfiguration());
	FString UBTTarget(TEXT("Editor"));
	FString UBTPlatform = FPlatformProcess::GetBinariesSubdirectory();
	return GenerateProjectsAndBuildGameAssemblies(OutFailReason, FeedbackContext, FApp::GetProjectName(), FPaths::ProjectDir(), FPaths::GetProjectFilePath(), UBTConfig, UBTTarget, UBTPlatform);
}

bool FMonoRuntime::GenerateProjectsAndBuildGameAssemblies(FText& OutFailReason, FFeedbackContext& FeedbackContext, const FString& AppName, const FString& ProjectDir, const FString& ProjectFile, const FString& TargetConfiguration, const FString& TargetType, const FString& TargetPlatform) const
{
	const FString ExternalProjectDir = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ProjectDir);
	const FString ExternalProjectFile = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ProjectFile);

	const FString MonoUEBuildToolPath = FPaths::Combine(*PluginDotNETDirectory, TEXT("MonoUEBuildTool.exe"));
	const FString Parameters = FString::Printf(TEXT("GenerateProjects -SolutionDir \"%s\" -SolutionName \"%s\" \"%s\""), *ExternalProjectDir, *AppName, *ExternalProjectFile);

	if (!FMonoBuildUtils::RunExternalManagedExecutable(LOCTEXT("GeneratingManagedProjectFiles", "Generating managed project files..."), MonoUEBuildToolPath, Parameters, &FeedbackContext))
	{
		OutFailReason = FText::Format(LOCTEXT("CouldNotGenerateManagedProjectFiles", "Could not generate managed project files - failure to launch command line {0} {1}"), FText::FromString(MonoUEBuildToolPath), FText::FromString(Parameters));
		return false;
	}

	// Build the generated solution so we have assemblies to run on first load
	if (!FMonoBuildUtils::BuildManagedCode(LOCTEXT("BuildingManagedAssemblies", "Building managed assemblies..."), &FeedbackContext, *AppName, *ProjectDir, *ProjectFile, *TargetConfiguration,  *TargetType, *TargetPlatform))
	{
		OutFailReason = FText::Format(LOCTEXT("CouldNotBuildManagedAssemblies", "Failed building managed assemblies for project '{0}'"), FText::FromString(ProjectFile));
		return false;
	}
	return true;
}

void FMonoRuntime::StartIdeAgent()
{
	//MonoMainDomain
	MonoMethod* agentStartMethod = MonoMainDomain->GetMainAssembly().LookupMethod("MonoUE.IdeAgent.UnrealAgentServer:Start");
	check(agentStartMethod);
	Mono::Invoke<void>(*MonoMainDomain, agentStartMethod, nullptr, FPaths::EngineDir(), FPaths::ProjectDir());
}

void FMonoRuntime::StopIdeAgent()
{
	MonoMethod* agentStopMethod = MonoMainDomain->GetMainAssembly().LookupMethod("MonoUE.IdeAgent.UnrealAgentServer:Stop");
	check(agentStopMethod);
	Mono::Invoke<void>(*MonoMainDomain, agentStopMethod, nullptr);
}

bool FMonoRuntime::RequestHotReload()
{
#if MONO_WITH_HOT_RELOADING
	GEngine->DeferredCommands.Add(TEXT("MonoRuntime.HotReload"));
	return true;
#else
	return false;
#endif
}

IMonoRuntime::FStopPIEForHotReloadEvent& FMonoRuntime::GetOnStopPIEForHotReloadEvent()
{
#if MONO_WITH_HOT_RELOADING
	return MonoBindings->GetOnStopPIEForHotReloadEvent();
#else
	static FStopPIEForHotReloadEvent Dummy;
	return Dummy;
#endif
}
IMonoRuntime::FHotReloadEvent& FMonoRuntime::GetOnHotReloadEvent()
{
#if MONO_WITH_HOT_RELOADING
	return MonoBindings->GetOnHotReloadEvent();
#else
	static FHotReloadEvent Dummy;
	return Dummy;
#endif
}


FString FMonoRuntime::GetMonoQualifiedClassName(const UClass* InClass, bool bExcludeBindings) const
{
	if (bExcludeBindings)
	{
		const UMonoUnrealClass* Class = MonoBindings->GetMonoUnrealClass(InClass);
		if (Class)
		{
			return Class->GetQualifiedName();
		}
	}
	else
	{
		MonoClass * Class = MonoBindings->GetMonoClassFromUnrealClass(*InClass);
		if (Class)
		{
			const FString ClassName(mono_class_get_name(Class));
			const FString Namespace(mono_class_get_namespace(Class));

			if (Namespace.Len() > 0)
			{
				return FString::Printf(TEXT("%s.%s"), *Namespace, *ClassName);
			}
			else
			{
				return ClassName;
			}
		}
	}
	return FString();
}

FString FMonoRuntime::GetMonoClassName(const UClass* InClass, bool bExcludeBindings) const
{
	if (bExcludeBindings)
	{
		const UMonoUnrealClass* Class = MonoBindings->GetMonoUnrealClass(InClass);
		if (Class)
		{
			return Class->GetName();
		}
	}
	else
	{
		MonoClass * Class = MonoBindings->GetMonoClassFromUnrealClass(const_cast<UClass &> (*InClass));
		if (Class)
		{
			return mono_class_get_name(Class);
		}
	}
	return FString();
}
FString FMonoRuntime::GetMonoClassNamespace(const UClass* InClass, bool bExcludeBindings) const
{
	if (bExcludeBindings)
	{
		const UMonoUnrealClass* Class = MonoBindings->GetMonoUnrealClass(InClass);
		if (Class)
		{
			return Class->GetNamespace();
		}
	}
	else
	{
		MonoClass * Class = MonoBindings->GetMonoClassFromUnrealClass(const_cast<UClass &> (*InClass));
		if (Class)
		{
			return mono_class_get_namespace(Class);
		}
	}
	return FString();
}

bool FMonoRuntime::AddDllMapForModule(const char* DllName, const FName AddModuleName) const
{
	FModuleStatus ModuleStatus;
	if (!FModuleManager::Get().QueryModule(AddModuleName, ModuleStatus))
		return false;

	mono_dllmap_insert(NULL, DllName, NULL, TCHAR_TO_ANSI(*ModuleStatus.FilePath) , NULL);
	return true;
}

#endif

#undef LOCTEXT_NAMESPACE