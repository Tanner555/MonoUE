// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#define MONO_PINVOKE_EXPORT DLLEXPORT
#if PLATFORM_WINDOWS
#define MONO_PINVOKE_CALLCONV WINAPI
#else
#define MONO_PINVOKE_CALLCONV
#endif

/// Helper macro for defining a pinvoke handler in C++ code that abstracts platform linkage details
/// Usage example: MONO_PINVOKE_FUNCTION(bool) MyFunction(int MyArg)
#define MONO_PINVOKE_FUNCTION(RetType) extern "C" MONO_PINVOKE_EXPORT RetType MONO_PINVOKE_CALLCONV

class FMonoBindings;
class FMonoMainDomain;

class IMonoRuntime : public IModuleInterface
{

public:
	MONORUNTIME_API static const FName ModuleName;

#if WITH_EDITOR
	virtual bool GenerateProjectsAndBuildGameAssemblies(FText& OutFailReason, FFeedbackContext& FeedbackContext) const = 0;
	virtual bool GenerateProjectsAndBuildGameAssemblies(FText& OutFailReason, FFeedbackContext& FeedbackContext, const FString& AppName, const FString& ProjectDir, const FString& ProjectFile, const FString& TargetConfiguration, const FString& TargetType, const FString& TargetPlatform) const = 0;

	/**
	 * Called by the MonoEditor module to start the IDE agent, since it depends on P/Invoking symbols in the
	 * MonoEditor module. 
	 */
	virtual void StartIdeAgent() = 0;

	/**
	 * Called by MonoEditor module to stop the IDE agent. It must be shut down when MonoEditor unloads since it pinvokes into it. 
	 */
	virtual void StopIdeAgent() = 0;

	/**
	* Called by the MonoEditor module to request a hot-reload of the game domain.
	*
	* Returns true if the request was issued, false if not. NOTE: hot reloading is a deferred operation, and success or failure of the hot reload itself
	* is communicated via the OnHotReload event.
	*/
	virtual bool RequestHotReload() = 0;

	DECLARE_EVENT(IMonoRuntime, FStopPIEForHotReloadEvent);
	virtual FStopPIEForHotReloadEvent& GetOnStopPIEForHotReloadEvent() = 0;

	DECLARE_EVENT_OneParam(IMonoRuntime, FHotReloadEvent, bool);
	virtual FHotReloadEvent& GetOnHotReloadEvent() = 0;

	/**
	* Gets the fully qualified Mono class name from a UClass.
	*
	* @return The fully qualified name of the Mono class, or zero-length string if the class is not a Mono class.
	*/
	virtual FString GetMonoQualifiedClassName(const UClass* InClass, bool bExcludeBindings = false) const = 0;

	/**
	* Gets the unqualified Mono class name from a UClass.
	*
	* @return The unqualified name of the Mono class, or zero-length string if the class is not a Mono class.
	*/
	virtual FString GetMonoClassName(const UClass* InClass, bool bExcludeBindings = false) const = 0;

	/**
	* Gets the full namespace of the Mono class from a UClass.
	*
	* @return The full namespace of the Mono class, or zero-length string if the class is not a Mono class.
	*/
	virtual FString GetMonoClassNamespace(const UClass* InClass, bool bExcludeBindings = false) const = 0;

	/**
	* Add mapping from P/Invoke dll name map to specified UE4 module.
	*/
	virtual bool AddDllMapForModule(const char* DllName, const FName AddModuleName) const = 0;

#endif // WITH_EDITOR

	static inline IMonoRuntime& Get()
	{
		return FModuleManager::LoadModuleChecked<IMonoRuntime>(ModuleName);
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}
};

