// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "MonoGeneratedFileManager.h"
#include "MonoPropertyHandler.h"
#include "MonoBindingsModule.h"
#include "MonoProjectFile.h"
#include "InclusionLists.h"
#include "ScriptGenUtil.h"

class FMonoTextBuilder;

class FCollapsedGetterSetter
{
	
public:
	UFunction* Setter;
	UFunction* Getter;
	UProperty* Property;
	void CreateName(FSupportedPropertyTypes *PropertyHandlers);
	FString SynthesizedName;
};

class FMonoScriptCodeGenerator : IMonoModuleFinder
{
public:
	FMonoScriptCodeGenerator();

	void Initialize(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory);
	void GatherClassForExport(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged);
	void FinishExport(const TSet<FName>& ModulesToExport, const TMap<FName, FMonoGameModuleInfo>& GameModules);

	const FString& GetMonoBuildManifestOutputDirectory() const { return MonoBuildManifestOutputDirectory; }

	inline const MonoScriptNameMapper& GetScriptNameMapper() const { return NameMapper; }

private:
	FString GetQualifiedSuperClassName(const UClass* Class) const;

	void ExportClasses();
	void ExportExtensionMethods();
	void ExportModules(const TSet<FName>& ModulesToExport);

	void ExportClass(const UClass* Class);
	void ExportStruct(const UScriptStruct* Struct);

	bool HasInjectedSource(const UStruct* StructOrClass) const;

	bool CanExportClass(const UClass* Class) const;
	bool IsDerivableClass(const UClass* Class) const;
	bool IsBlueprintVariableClass(const UStruct* Struct) const;

	bool CanExportPropertyShared(const UProperty* Property) const;
	bool CanExportProperty(const UStruct* Struct, UProperty* Property) const;
	bool CanExportParameter(const UProperty* Property) const;
	bool CanExportReturnValue(const UProperty* Property) const;
	bool CanExportFunction(const UStruct* Struct, UFunction* Function) const;

	bool CanExportOverridableParameter(const UProperty* Property) const;
	bool CanExportOverridableReturnValue(const UProperty* Property) const;
	bool CanExportOverridableFunction(const UStruct* Struct, UFunction* Function) const;

	struct ExtensionMethod
	{
		UClass* OverrideClassBeingExtended;
		UFunction* Function;
		UProperty* SelfParameter;
	};

	bool GetExtensionMethodInfo(ExtensionMethod& Info, UFunction& Function) const;

	void ExportStaticConstructor(FMonoTextBuilder& Builder, 
		const UStruct* Struct,
		const TArray<UProperty*>& ExportedProperties,
		const TArray<UFunction*>& ExportedFunction,
		const TArray<UFunction*>& ExportedOverrideableFunctions,
		const TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters) const;

	void ExportMirrorStructMarshalling(FMonoTextBuilder& Builder, const UScriptStruct* Struct, TArray<UProperty*> ExportedProperties);
	void ExportStructMarshaler(FMonoTextBuilder& Builder, const UScriptStruct* Struct);

	void GatherExportedProperties(TArray<UProperty*>& ExportedProperties, const UStruct* Struct) const;
	void CollapseGettersAndSetters(TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters, const UClass* Class, const TArray<UProperty*>& ExportedProperties, const TArray<UFunction*>& ExportedFunctions) const;
	void ExportClassProperties(FMonoTextBuilder& Builder, const UClass* Class, TArray<UProperty*>& ExportedProperties, TSet<FString>& ExportedPropertiesHash) const;
	void ExportPropertiesStaticConstruction(FMonoTextBuilder& Builder, const TArray<UProperty*>& ExportedProperties, const TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters) const;
	void ExportStructProperties(FMonoTextBuilder& Builder, const UStruct* Struct, const TArray<UProperty*>& ExportedProperties, bool bSuppressOffsets) const;

	void ExportClassCollapsedGettersAndSetters(FMonoTextBuilder& Builder, const UClass* Class, TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters, TSet<FString>& ExportedPropertiesHash) const;

	void GatherExportedStructs(TArray<UScriptStruct*>& ExportedStructs, const UClass* Class) const;
	void GatherExportedFunctions(TArray<UFunction*>& ExportedFunctions, const UStruct* Struct) const;
	void GatherExportedOverridableFunctions(TArray<UFunction*>& ExportedFunctions, const UStruct* Struct) const;

	void ExportClassFunctions(FMonoTextBuilder& Builder, const UClass* Class, const TArray<UFunction*>& ExportedFunctions);
	void ExportClassOverridableFunctions(FMonoTextBuilder& Builder, const UClass* Class, const TArray<UFunction*>& ExportedOverridableFunctions) const;

	void ExportClassFunctionsStaticConstruction(FMonoTextBuilder& Builder, const UClass* Class, const TArray<UFunction*>& ExportedFunctions, const TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters) const;
	void ExportClassFunctionStaticConstruction(FMonoTextBuilder& Builder, const UFunction *Function) const;
	void ExportClassOverridableFunctionsStaticConstruction(FMonoTextBuilder& Builder, const UClass* Class, const TArray<UFunction*>& ExportedOverridableFunctions) const;

	FString GetCSharpEnumType(const EPropertyType PropertyType) const;
	void ExportEnums(FMonoTextBuilder& Builder, const TArray<UEnum*>& ExportedEnums) const;

	//IMonoModuleFinder
	virtual const FMonoBindingsModule& FindModule(const UObject& Object) const override;
	virtual const FMonoBindingsModule& FindModule(FName ModuleFName) const override;

	FMonoBindingsModule& FindOrRegisterModule(FName ModuleFName);

	void RegisterClassModule(UStruct* Struct, const TSet<UStruct*>& References);

	void SaveGlue(const FMonoBindingsModule& Bindings, const FString& Filename, const FString& GeneratedGlue);
	void SaveTypeGlue(const UStruct* Struct, const FString& GeneratedGlue);
	void SaveModuleGlue(UPackage* Package, const FString& GeneratedGlue);
	void SaveExtensionsGlue(const FMonoBindingsModule& Bindings, const FString& GeneratedGlue);

	void GenerateProjectFiles();
	void GenerateMSBuildPropsFile(const FString& PropsFilePath, const TArray<const FMonoProjectFile*>& Projects, const FString& AssemblyLocationVariable, bool bIncludeRuntime);
	void GenerateSolutionFile(const FString& SolutionFilePath, const FMonoProjectFile& BuiltinProjectFile, const TArray<TSharedPtr<FMonoProjectFile>>& PluginProjectFiles, const TArray<TSharedPtr<FMonoProjectFile>>* GameProjectFiles);
	void GenerateProjectFile(const FMonoProjectFile& ProjectFile, const TMap<FName, FMonoProjectFile*>* ModuleToProjectFileMap, bool bIsGameModule);
	FString GetProjectReferenceText(const FString& ReferencerProjectDirectory, const FString& ReferenceeAssemblyName, const FString& ReferenceeProjectPath, const FGuid& ReferenceeProjectGuid) const;

	void LogUnhandledProperties() const;

	// Generated file manager
	FMonoGeneratedFileManager GeneratedFileManager;

	// Game Modules - available in FinishExport
	TMap<FName, FMonoGameModuleInfo>	GameModules;

	// Is module registration open? (we can't do it until FinishExport)
	bool		bModuleRegistrationOpen;

	// Modules we are exporting mono bindings for
	TMap<FName, FMonoBindingsModule> MonoBindingsModules;

	TSet<UClass*> TopLevelExportedClasses;

	// output dir of mono bindings generated cs files
	FString		MonoOutputDirectory;
	// output dir for build manifest
	FString		MonoBuildManifestOutputDirectory;
	// MonoUE plugin directory
	FString		MonoUEPluginDirectory;
	//root engine directory
	FString EngineRoot;

	FString PlatformName;
	
	// Path to MonoUE bindings directory
	FString		MonoUEBindingsDirectory;
	// Path to injected source directory (source injected into bindings DLLs)
	FString		InjectedSourceDirectory;
	// MonoUE  bindings csproj path
	FString		MonoUEBindingsProjectPath;
	// MonoUE bindings csproj guid
	FGuid		MonoUEBindingsGuid;
	// MonoUE main domain csproj path
	FString		MonoUEMainDomainProjectPath;
	// MonoUE main domain csproj guid
	FGuid		MonoUEMainDomainGuid;
	// MonoAssemblyProcess csproj path
	FString		MonoAssemblyProcessProjectPath;
	// MonoAssemblyProcess csproj guid;
	FGuid		MonoAssemblyProcessGuid;
	// MonoManagedExtensions csproj path
	FString		MonoManagedExtensionsProjectPath;
	// MonoManagedExtensions csproj guid;
	FGuid		MonoManagedExtensionsGuid;
	// MonoUETasks csproj path
	FString		MonoUETasksProjectPath;
	// MonoUETasks csproj guid
	FGuid		MonoUETasksGuid;

	// Contents of our csproj template
	FString		ProjectTemplateContents;

	MonoScriptNameMapper NameMapper;

	// Forced bindings generation.
	// Whitelisted items are still subject to limitations of the currently supported property types.
	FInclusionLists Whitelist;

	// Suppressed bindings generation.
	FInclusionLists Blacklist;

	// Greylisted properties still export their offsets and static construction, but not a getter or setter.
	// The expectation is that a custom implementation will be provided in an _Injected.cs file.
	// 
	// Greylisted structs, functions, and overridable functions are not currently supported.
	FInclusionLists Greylist;

	// Library functions in BlueprintFunctionLibraries which should be forced to be marked internal, but not have an extension method exposed
	FInclusionLists ManualLibraryFunctionList;

	// property handlers
	TUniquePtr<FSupportedPropertyTypes> PropertyHandlers;

	// extension method tracking
	TMap<FName, TArray<ExtensionMethod>> ExtensionMethods;

	// Maps property names to a count of how many properties of that type were rejected by the null handler.
	typedef TMap<FName, int32> UnhandledPropertyCounts;

	mutable UnhandledPropertyCounts UnhandledProperties;
	mutable UnhandledPropertyCounts UnhandledParameters;
	mutable UnhandledPropertyCounts UnhandledReturnValues;
	mutable UnhandledPropertyCounts UnhandledOverridableParameters;
	mutable UnhandledPropertyCounts UnhandledOverridableReturnValues;

	FString GetAssemblyDirectory(const FString &RootDirectory, const EBuildConfigurations::Type Configuration, const FString &PlatformName, const FString &TargetName);
};