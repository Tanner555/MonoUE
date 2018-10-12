// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoScriptCodeGenerator.h"
#include "MonoPropertyHandler.h"
#include "MonoScriptCodeGeneratorUtils.h"
#include "MonoScriptGeneratorLog.h"

#include "UObject/Stack.h"
#include "UObject/UObjectIterator.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Templates/Casts.h"

FMonoScriptCodeGenerator::FMonoScriptCodeGenerator()
	: bModuleRegistrationOpen(false), NameMapper(this)
{
}

void FMonoScriptCodeGenerator::Initialize(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory)
{
	UE_LOG(LogMonoScriptGenerator, Log, TEXT("RootLocalPath: %s"), *RootLocalPath);
	UE_LOG(LogMonoScriptGenerator, Log, TEXT("RootBuildPath: %s"), *RootBuildPath);
	UE_LOG(LogMonoScriptGenerator, Log, TEXT("OutputDirectory: %s"), *OutputDirectory);

	PropertyHandlers.Reset(new FSupportedPropertyTypes(NameMapper, Blacklist));

	FString PlatformDirectory = FPaths::Combine(*OutputDirectory, TEXT(".."), TEXT(".."), TEXT(".."));
	FPaths::NormalizeDirectoryName(PlatformDirectory);
	FPaths::CollapseRelativeDirectories(PlatformDirectory);
	PlatformName = FPaths::GetCleanFilename(PlatformDirectory);

	MonoOutputDirectory = FPaths::Combine(*PlatformDirectory, TEXT("Mono"));

	FPaths::CollapseRelativeDirectories(MonoOutputDirectory);
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (!File.CreateDirectoryTree(*MonoOutputDirectory))
	{
		UE_LOG(LogMonoScriptGenerator, Error, TEXT("Could not create directory %s"), *MonoOutputDirectory);
	}

	EngineRoot = RootLocalPath;

	MonoUEPluginDirectory = FPaths::Combine(*RootLocalPath, TEXT("Engine"), TEXT("Plugins"), TEXT("MonoUE"));
	FPaths::CollapseRelativeDirectories(MonoUEPluginDirectory);

	MonoBuildManifestOutputDirectory = FPaths::Combine(*MonoUEPluginDirectory, TEXT("Binaries"), *PlatformName, TEXT("Mono"));
	if (!File.CreateDirectoryTree(*MonoBuildManifestOutputDirectory))
	{
		UE_LOG(LogMonoScriptGenerator, Error, TEXT("Could not create directory %s"), *MonoBuildManifestOutputDirectory);
	}

	const FString ProgramsSourceDirectory = FPaths::Combine(*MonoUEPluginDirectory, TEXT("Source"), TEXT("Programs"));

	const FString ManagedSourceDirectory = FPaths::Combine(*MonoUEPluginDirectory, TEXT("Managed"));

	MonoUEBindingsDirectory = FPaths::Combine(*ManagedSourceDirectory, TEXT("MonoBindings"));

	InjectedSourceDirectory = FPaths::Combine(*MonoUEBindingsDirectory, TEXT("InjectedClasses"));

	// load project template
	const FString ProjectTemplateFile = FPaths::Combine(*MonoUEBindingsDirectory, TEXT("MODULE.csproj.template"));

	verify(FFileHelper::LoadFileToString(ProjectTemplateContents, *ProjectTemplateFile));

	// path to MonoUE bindings project
	MonoUEBindingsProjectPath = FPaths::Combine(*MonoUEBindingsDirectory, MONO_BINDINGS_NAMESPACE TEXT(".csproj"));

	verify(MonoScriptCodeGeneratorUtils::ParseGuidFromProjectFile(MonoUEBindingsGuid, MonoUEBindingsProjectPath));

	// path to MonoUE main domain project
	MonoUEMainDomainProjectPath = FPaths::Combine(*ManagedSourceDirectory, TEXT("MonoMainDomain"), MONO_UE4_NAMESPACE TEXT(".MainDomain.csproj"));

	verify(MonoScriptCodeGeneratorUtils::ParseGuidFromProjectFile(MonoUEMainDomainGuid, MonoUEMainDomainProjectPath));

	// path to mono assembly process project
	MonoAssemblyProcessProjectPath = FPaths::Combine(*ProgramsSourceDirectory, TEXT("MonoAssemblyProcess"), TEXT("MonoAssemblyProcess.csproj"));

	verify(MonoScriptCodeGeneratorUtils::ParseGuidFromProjectFile(MonoAssemblyProcessGuid, MonoAssemblyProcessProjectPath));

	// path to managed extensions project
	MonoManagedExtensionsProjectPath = FPaths::Combine(*ManagedSourceDirectory, TEXT("MonoManagedExtensions"), MONO_UE4_NAMESPACE TEXT(".ManagedExtensions.csproj"));

	verify(MonoScriptCodeGeneratorUtils::ParseGuidFromProjectFile(MonoManagedExtensionsGuid, MonoManagedExtensionsProjectPath));

	// path to build tasks project
	MonoUETasksProjectPath = FPaths::Combine(*ProgramsSourceDirectory, TEXT("MonoUE.Tasks"), TEXT("MonoUE.Tasks.csproj"));

	//this is a new SDK style project, the project itself doesn't have a GUID, but we stil need one for the sln file
	MonoUETasksGuid = FGuid::NewGuid();

	// Initialize whitelists.
	Whitelist.AddProperty(TEXT("Actor"), TEXT("Instigator"));
	Blacklist.AddFunction(TEXT("Actor"), TEXT("GetInstigator")); // Prevents collapsing into a get-only property
	Blacklist.AddProperty(TEXT("Actor"), TEXT("Owner"));
	Blacklist.AddFunction(TEXT("Actor"), TEXT("GetOwner"));	// Called manually as an icall in custom Owner property

	// This enum is manually exported in Runtime assembly so math classes can use it
	Blacklist.AddEnum(TEXT("EAxis"));

	// These are basic math operations that are part of any language, and any we want to keep we either hand roll extension methods
	// or explicitly whitelist
	Blacklist.AddAllFunctions(TEXT("KismetMathLibrary"));
	Whitelist.AddFunction(TEXT("KismetMathLibrary"), TEXT("RandomUnitVector"));
	ManualLibraryFunctionList.AddFunction(TEXT("KismetMathLibrary"), TEXT("RandomUnitVector"));
	Whitelist.AddFunction(TEXT("KismetMathLibrary"), TEXT("RandomUnitVectorInConeInRadians"));
	ManualLibraryFunctionList.AddFunction(TEXT("KismetMathLibrary"), TEXT("RandomUnitVectorInConeInRadians"));

	// These are basic array operations already supported by C# containers
	Blacklist.AddAllFunctions(TEXT("KismetArrayLibrary"));

	// These are basic string operations already supported by C# strings
	Blacklist.AddAllFunctions(TEXT("KismetStringLibrary"));

	// Expose actor spawning for now, but we may want to figure out a better API for this
	Whitelist.AddFunction(TEXT("GameplayStatics"), TEXT("BeginSpawningActorFromClass"));
	Whitelist.AddFunction(TEXT("GameplayStatics"), TEXT("FinishSpawningActor"));
	
	// Handled by Actor.ComponentsBoundingBox
	Blacklist.AddFunction(TEXT("KismetSystemLibrary"), TEXT("GetActorBounds"));

	// Handled by GameModeBase.FindPlayerStart
	Blacklist.AddFunction(TEXT("GameModeBase"), TEXT("K2_FindPlayerStart"));

	// There doesn't seem to be any good reason to hide these setters, and PlayerControllerClass
	// setter is needed for the C# empty template that was ported from C++
	Whitelist.AddProperty(TEXT("GameModeBase"), TEXT("GameStateClass"));
	Whitelist.AddProperty(TEXT("GameModeBase"), TEXT("PlayerControllerClass"));
	Whitelist.AddProperty(TEXT("GameModeBase"), TEXT("PlayerStateClass"));
	Whitelist.AddProperty(TEXT("GameModeBase"), TEXT("SpectatorClass"));
	Whitelist.AddProperty(TEXT("GameModeBase"), TEXT("ReplaySpectatorPlayerControllerClass"));

	Whitelist.AddClass(TEXT("InputComponent"));
	Whitelist.AddStruct(TEXT("InputChord"));
	Whitelist.AddProperty(TEXT("AudioComponent"), TEXT("bAutoDestroy"));

	Whitelist.AddClass(TEXT("EngineTypes"));
	// FHitResult is Blueprint-exposed, but BP treats it as an opaque handle.
	// Whitelist it to force its properties to export for use in script.
	Whitelist.AddStruct(TEXT("HitResult"));

	//need ULevel for World.SpawnActor
	Whitelist.AddClass(TEXT("Level"));

	// Exposure for ShooterGame port
	Whitelist.AddProperty(TEXT("Pawn"), TEXT("LastHitBy"));
	Whitelist.AddProperty(TEXT("Actor"), TEXT("bReplicateMovement"));
	Blacklist.AddFunction(TEXT("Actor"), TEXT("TearOff")); //another property/function conflict. same pattern, field getter w/custom setter. should generate.
	Whitelist.AddProperty(TEXT("Actor"), TEXT("bTearOff"));
	Greylist.AddProperty(TEXT("Actor"), TEXT("bTearOff"));
	Whitelist.AddProperty(TEXT("Actor"), TEXT("RootComponent")); //manually expose so we can add the setter and its checks. blueprint doesn't need the setter, but C# does.
	Greylist.AddProperty(TEXT("Actor"), TEXT("RootComponent"));
	Blacklist.AddFunction(TEXT("Actor"), TEXT("K2_GetRootComponent"));
	Whitelist.AddProperty(TEXT("Actor"), TEXT("bReplicateInstigator"));
	Whitelist.AddFunction(TEXT("Actor"), TEXT("SetRemoteRoleForBackwardsCompat"));
	Whitelist.AddProperty(TEXT("PrimitiveComponent"), TEXT("bReceivesDecals"));
	Whitelist.AddProperty(TEXT("PrimitiveComponent"), TEXT("bCastDynamicShadow"));
	Whitelist.AddProperty(TEXT("SkinnedMeshComponent"), TEXT("bChartDistanceFactor"));
	Whitelist.AddProperty(TEXT("SkeletalMeshComponent"), TEXT("AnimScriptInstance"));
	Whitelist.AddProperty(TEXT("SkeletalMeshComponent"), TEXT("bBlendPhysics"));
	Greylist.AddProperty(TEXT("SceneComponent"), TEXT("RelativeLocation")); // Custom-implement so setter will call SetRelativeLocationAndRotation()
	Greylist.AddProperty(TEXT("SceneComponent"), TEXT("RelativeRotation")); // Custom-implement so setter will call SetRelativeLocationAndRotation()
	Whitelist.AddProperty(TEXT("World"), TEXT("GameState"));
	Whitelist.AddProperty(TEXT("GameState"), TEXT("GameModeClass"));
	Whitelist.AddProperty(TEXT("AnimMontage"), TEXT("BlendOutTime")); // TODO: this could be read only
	Whitelist.AddProperty(TEXT("PlayerState"), TEXT("PlayerName"));
	Whitelist.AddProperty(TEXT("GameMode"), TEXT("bDelayedStart"));
	Whitelist.AddProperty(TEXT("GameMode"), TEXT("GameState"));

	//these are deprecated and conflict with their K2-prefixed replacements
	Blacklist.AddFunction(TEXT("NavigationSystem"), TEXT("GetRandomPointInNavigableRadius"));
	Blacklist.AddFunction(TEXT("NavigationSystem"), TEXT("GetRandomReachablePointInRadius"));
	Blacklist.AddFunction(TEXT("NavigationSystem"), TEXT("ProjectPointToNavigation"));

	//FIXME: property name conflicts with class name, should add mechanism to remap it
	Blacklist.AddProperty(TEXT("ImagePlate"), TEXT("ImagePlate"));
	Blacklist.AddProperty(TEXT("MediaPlane"), TEXT("MediaPlane"));

	//class conflicts with enum
	Blacklist.AddEnum(TEXT("EARSessionStatus"));
	Blacklist.AddProperty(TEXT("ARSessionStatus"), TEXT("Status"));

	// registration isn't open until FinishExport
	bModuleRegistrationOpen = false;
}

void FMonoScriptCodeGenerator::GatherClassForExport(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged)
{
	if (CanExportClass(Class) && !TopLevelExportedClasses.Contains(Class))
	{
		TArray<UProperty*> ExportedProperties;
		TArray<UFunction*> ExportedFunctions;

		const bool bIsDerivableClass = IsDerivableClass(Class);
		GatherExportedProperties(ExportedProperties, Class);
		GatherExportedFunctions(ExportedFunctions, Class);

		const bool bIsWhiteListed = Whitelist.HasClass(Class);
		const bool bIsBlackListed = Blacklist.HasClass(Class);

		if (!bIsBlackListed
			&& (bIsDerivableClass
			|| IsBlueprintVariableClass(Class)
			|| ExportedFunctions.Num() > 0 
			|| ExportedProperties.Num() > 0
			|| bIsWhiteListed))
		{
			TopLevelExportedClasses.Add(Class);
		}
	}
}

void FMonoScriptCodeGenerator::FinishExport(const TSet<FName>& ModulesToExport, const TMap<FName, FMonoGameModuleInfo>& InGameModules)
{
	GameModules = InGameModules;
	bModuleRegistrationOpen = true;

	// Export modules first to ensure that enum prefixes are cached before we need 
	// them to handle default parameter exporting.
	ExportModules(ModulesToExport);

	ExportClasses();

	ExportExtensionMethods();

	// update cs files
	GeneratedFileManager.RenameTempFiles();

	// generate csproj
	GenerateProjectFiles();

	// Commit csprojs
	GeneratedFileManager.RenameTempFiles();

	// Dump a report of unhandled UProperties by type and usage.
	LogUnhandledProperties();
}

FString FMonoScriptCodeGenerator::GetQualifiedSuperClassName(const UClass* Class) const
{
	if (Class->GetName() == TEXT("Object"))
	{
		// special case for UObject, it derives from our bindings object
		return FString::Printf(TEXT("%s.Runtime.UnrealObject"), MONO_UE4_NAMESPACE);
	}
	else
	{
		UClass* SuperClass = Class->GetSuperClass();
		check(SuperClass);
		return NameMapper.GetQualifiedName(*SuperClass);
	}
}

void FMonoScriptCodeGenerator::ExportClasses()
{
	TSet<UClass*> ExportedClasses;
	TSet<UScriptStruct*> ExportedStructs;
	TArray<UStruct*> ExportStack;

	for (auto Class : TopLevelExportedClasses)
	{
		ExportStack.Push(Class);
	}

	while (ExportStack.Num() != 0)
	{
		UStruct* Struct = ExportStack.Pop();
		UClass* Class = Cast<UClass>(Struct);
		UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Struct);

		if ((Class && !ExportedClasses.Contains(Class)) || (ScriptStruct && !ExportedStructs.Contains(ScriptStruct)))
		{
			if (Class)
			{
				ExportedClasses.Add(Class);
			}
			else
			{
				check(ScriptStruct);
				ExportedStructs.Add(ScriptStruct);
			}

			TArray<UProperty*> ExportedProperties;
			TArray<UFunction*> ExportedFunctions;
			TArray<UFunction*> ExportedOverridableFunctions;

			GatherExportedProperties(ExportedProperties, Struct);
			GatherExportedFunctions(ExportedFunctions, Struct);
			GatherExportedOverridableFunctions(ExportedOverridableFunctions, Struct);

			TSet<UStruct*> References;

			// gather super classes for export
			if (Class)
			{
				UClass* SuperClass = Class->GetSuperClass();
				while (nullptr != SuperClass)
				{
					References.Add(SuperClass);
					SuperClass = SuperClass->GetSuperClass();
				}

				// Make sure we're including any blueprint-visible structs declared in this class's header.
				TArray<UScriptStruct*> StructsInClassHeader;
				GatherExportedStructs(StructsInClassHeader, Class);
				for (auto StructInHeader : StructsInClassHeader)
				{
					if (!ExportedStructs.Contains(StructInHeader))
					{
						ExportStack.Push(StructInHeader);
					}
				}
			}

			// register classes referred by object properties
			for (auto Property : ExportedProperties)
			{
				PropertyHandlers->Find(Property).AddReferences(Property, References);
			}

			// register classes used as function parameters
			for (auto Function : ExportedFunctions)
			{
				for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
				{
					UProperty* Property = *ParamIt;
					PropertyHandlers->Find(Property).AddReferences(Property, References);
				}
			}

			// register classes used as overridable function parameters
			for (auto Function : ExportedOverridableFunctions)
			{
				for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
				{
					UProperty* Property = *ParamIt;
					PropertyHandlers->Find(Property).AddReferences(Property, References);
				}
			}

			// remove blacklisted references
			for (TSet<UStruct*>::TIterator It(References); It; ++It)
			{
				UClass* ReferencedClass = Cast<UClass>(*It);
				if ((nullptr != ReferencedClass && Blacklist.HasClass(ReferencedClass))
					|| (nullptr == ReferencedClass && Blacklist.HasStruct(*It)))
				{
					It.RemoveCurrent();
				}
			}

			RegisterClassModule(Struct, References);

			for (auto Reference : References)
			{
				UClass* ReferencedClass = Cast<UClass>(Reference);
				UScriptStruct* ReferencedScriptStruct = Cast<UScriptStruct>(Reference);
				if ((ReferencedClass && !ExportedClasses.Contains(ReferencedClass)) || (ReferencedScriptStruct && !ExportedStructs.Contains(ReferencedScriptStruct)))
				{
					ExportStack.Push(Reference);
				}
			}
		}
	}

	// export full classes
	for (const UClass* Class : ExportedClasses)
	{
		UE_LOG(LogMonoScriptGenerator, Log, TEXT("Exporting class %s.%s"), *MonoScriptCodeGeneratorUtils::GetModuleName(*Class), *GetScriptNameMapper().MapClassName (Class));
		ExportClass(Class);
	}

	for (const UScriptStruct* Struct : ExportedStructs)
	{
		UE_LOG(LogMonoScriptGenerator, Log, TEXT("Exporting struct %s.%s"), *MonoScriptCodeGeneratorUtils::GetModuleName(*Struct), *GetScriptNameMapper().MapStructName(Struct));
		ExportStruct(Struct);
	}
}

void FMonoScriptCodeGenerator::ExportExtensionMethods()
{
	for (auto&& ModuleExtensionMethods : ExtensionMethods)
	{
		const FName BindingsModuleName = ModuleExtensionMethods.Key;
		const TArray<ExtensionMethod>& Methods = ModuleExtensionMethods.Value;

		if (Methods.Num() > 0)
		{
			FMonoBindingsModule& BindingsModule = FindOrRegisterModule(BindingsModuleName);
			BindingsModule.bExportExtensions = true;

			FMonoTextBuilder Builder(FMonoTextBuilder::IndentType::Spaces);

			Builder.AppendLine(TEXT("using System;"));
			Builder.AppendLine(TEXT("using System.Runtime.InteropServices;"));
			Builder.AppendLine(FString::Printf(TEXT("using %s;"), MONO_BINDINGS_NAMESPACE));
			Builder.AppendLine();
			Builder.AppendLine();
			Builder.AppendLine(FString::Printf(TEXT("namespace %s"), *BindingsModule.GetNamespace()));
			Builder.OpenBrace();

			Builder.AppendLine(FString::Printf(TEXT("public static partial class %sExtensions"), *BindingsModule.GetMappedModuleNameString()));
			Builder.OpenBrace();

			for(const auto& Method : Methods)
			{
				PropertyHandlers->Find(Method.Function).ExportExtensionMethod(Builder,*Method.Function, Method.SelfParameter, Method.OverrideClassBeingExtended);
			}
			Builder.CloseBrace();
			Builder.CloseBrace();

			SaveExtensionsGlue(BindingsModule, Builder.ToText().ToString());
		}
	}
}

void FCollapsedGetterSetter::CreateName(FSupportedPropertyTypes *PropertyHandlers)
{
	if (Getter != nullptr && Getter->GetReturnProperty()->IsA(UBoolProperty::StaticClass()))
	{
		FString FuncName = PropertyHandlers->GetScriptNameMapper().MapFunctionName(Getter);
		if ((FuncName.StartsWith(TEXT("Is"))) ||
			(FuncName.StartsWith(TEXT("Has"))) ||
			(FuncName.StartsWith(TEXT("Can"))) ||
			(FuncName.StartsWith(TEXT("Should"))))
		{
			SynthesizedName = FuncName;
			return;
		}
	}
	
	if (Property != nullptr)
	{
		SynthesizedName = PropertyHandlers->GetScriptNameMapper().MapPropertyName(Property);
		return;
	}

	if (Getter != nullptr)
	{
		SynthesizedName = PropertyHandlers->GetScriptNameMapper().MapFunctionName(Getter).RightChop(3);
		return;
	}

	if (Setter != nullptr)
	{
		SynthesizedName = PropertyHandlers->GetScriptNameMapper().MapFunctionName(Setter).RightChop(3);
		return;
	}
}

void FMonoScriptCodeGenerator::CollapseGettersAndSetters(TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters, const UClass* Class, const TArray<UProperty*>& ExportedProperties, const TArray<UFunction*>& ExportedFunctions) const
{
	TMap<FName, UFunction*> Getters;
	TMap<FName, UFunction*> Setters;

	// find getters and setters
	for (UFunction* Function : ExportedFunctions)
	{
		if (Function->GetReturnProperty() != nullptr 
			&& Function->NumParms == 1) // return value counts as a parm
		{
			FString FuncName = NameMapper.MapScriptMethodName(Function);
		
			if (FuncName.StartsWith(TEXT("Get")))
			{
				FuncName = FuncName.RightChop(3);

				Getters.Add(FName(*FuncName), Function);
			}
			else if (Function->GetReturnProperty()->IsA(UBoolProperty::StaticClass()))
			{
				if (FuncName.StartsWith(TEXT("Is")))
				{
					FuncName = FuncName.RightChop(2);
					Getters.Add(FName(*FuncName), Function);
				}
				else if (FuncName.StartsWith(TEXT("Has")))
				{
					FuncName = FuncName.RightChop(3);
					Getters.Add(FName(*FuncName), Function);
				}
				else if (FuncName.StartsWith(TEXT("Can")))
				{
					FuncName = FuncName.RightChop(3);
					Getters.Add(FName(*FuncName), Function);
				}
				else if (FuncName.StartsWith(TEXT("Should")))
				{
					FuncName = FuncName.RightChop(6);
					Getters.Add(FName(*FuncName), Function);
				}
			}
		}
		else if (Function->GetReturnProperty() == nullptr
			&& Function->NumParms == 1)
		{
			FString FuncName = NameMapper.MapFunctionName(Function);

			if (FuncName.StartsWith(TEXT("Set")))
			{
				FuncName = FuncName.RightChop(3);
				if (MonoScriptCodeGeneratorUtils::GetFirstParam(Function)->IsA(UBoolProperty::StaticClass()))
				{
					if (FuncName.StartsWith(TEXT("Is")))
					{
						FuncName = FuncName.RightChop(2);
					}
					else if (FuncName.StartsWith(TEXT("Has")))
					{
						FuncName = FuncName.RightChop(3);
					}
					else if (FuncName.StartsWith(TEXT("Can")))
					{
						FuncName = FuncName.RightChop(3);
					}
					else if (FuncName.StartsWith(TEXT("Should")))
					{
						FuncName = FuncName.RightChop(6);
					}
				}
				Setters.Add(FName(*FuncName), Function);
			}
		}
	}

	// find paired setters/props
	for (auto Property : ExportedProperties)
	{
		// strip stuff like b prefix for booleans
		FString StrippedPropertyName = NameMapper.MapPropertyName(Property);

		UFunction* Setter = Setters.FindRef(*StrippedPropertyName);
		if (Setter)
		{
			UProperty* Param = MonoScriptCodeGeneratorUtils::GetFirstParam(Setter);
			check(Param);

			if (Param->SameType(Property))
			{
				FCollapsedGetterSetter Collapsed;
				Collapsed.Setter = Setter;
				Collapsed.Property = Property;
				Collapsed.Getter = nullptr;
				Collapsed.CreateName(PropertyHandlers.Get());
				CollapsedGettersAndSetters.Add(Collapsed);
				UE_LOG(LogMonoScriptGenerator, Log, TEXT("Paired property '%s' with setter '%s' on class '%s'"), *Property->GetName(), *Setter->GetName(), *NameMapper.MapClassName(Class));
			}
		}
	}

	// find paired getters/setters or standalone getters that should be properties in C#
	for (auto&& Getter : Getters)
	{
		UFunction* Setter = Setters.FindRef(Getter.Key);
		if (Setter)
		{
			UProperty* Param = MonoScriptCodeGeneratorUtils::GetFirstParam(Setter);
			check(Param);

			UProperty* ReturnParam = Getter.Value->GetReturnProperty();
			check(ReturnParam);

			if (Param->SameType(ReturnParam))
			{
				UProperty* Property = nullptr;
				bool bAlreadyCollapsed = false;
				// see if this has already been collapsed (this can happen because sometimes exposes both a read only prop and a getter)
				for (auto&& Collapsed : CollapsedGettersAndSetters)
				{
					if (Collapsed.Setter == Setter)
					{
						Collapsed.Getter = Getter.Value;
						check(Collapsed.Property);
						Property = Collapsed.Property;
						bAlreadyCollapsed = true;
						break;
					}
				}

				if (!bAlreadyCollapsed)
				{
					// see if a non-read only property has same name
					const FString Name = Getter.Key.ToString();
					Property = nullptr;
					for (int32 i = 0; nullptr == Property && i < ExportedProperties.Num(); ++i)
					{
						const FString StrippedPropertyName = NameMapper.MapPropertyName(ExportedProperties[i]);
						if (StrippedPropertyName == Name)
						{
							Property = ExportedProperties[i];
						}
					}


					FCollapsedGetterSetter Collapsed;
					Collapsed.Getter = Getter.Value;
					Collapsed.Setter = Setter;
					Collapsed.Property = Property;
					Collapsed.CreateName(PropertyHandlers.Get());
					CollapsedGettersAndSetters.Add(Collapsed);
				}
				UE_LOG(LogMonoScriptGenerator, Log, TEXT("Paired getter '%s' with setter '%s' %son class '%s'"), 
					*Getter.Value->GetName(), *Setter->GetName(), Property != nullptr ? *FString::Printf(TEXT("and property '%s' "), *Property->GetName()) : TEXT(""), *NameMapper.MapClassName(Class));
			}
		}
		else
		{
			// see if we should transform this into a get only property
			bool bAlreadyCollapsed = false;
			// see if this has already been collapsed (this can happen because sometimes exposes both a read only prop and a getter)
			for (auto&& Collapsed : CollapsedGettersAndSetters)
			{
				if (Collapsed.Property != nullptr)
				{
					const FString StrippedPropertyName = NameMapper.MapPropertyName(Collapsed.Property);

					if (StrippedPropertyName == Getter.Key.ToString())
					{
						Collapsed.Getter = Getter.Value;
						bAlreadyCollapsed = true;
						UE_LOG(LogMonoScriptGenerator, Log, TEXT("Paired getter '%s' with exiting collapsed property '%s'  on class '%s'"), *Getter.Key.ToString(), *Collapsed.Property->GetName(), *NameMapper.MapClassName(Class));

						break;
					}
				}
			}

			if (!bAlreadyCollapsed)
			{
				UProperty* MatchingProperty = nullptr;
				// see if ANY property, exported or not, matches this getter
				for (TFieldIterator<UProperty> It(Class); It; ++It)
				{
					const FString StrippedPropertyName = NameMapper.MapPropertyName(*It);
					if (FName(*StrippedPropertyName) == Getter.Key)
					{
						MatchingProperty = *It;
						break;
					}
				}

				FString FuncName = NameMapper.MapFunctionName(Getter.Value);
				if (nullptr != MatchingProperty || 
				   (Getter.Value->GetReturnProperty()->IsA(UBoolProperty::StaticClass()) &&
				    ((FuncName.StartsWith(TEXT("Is")))  ||
				     (FuncName.StartsWith(TEXT("Has"))) ||
				     (FuncName.StartsWith(TEXT("Can"))) ||
				     (FuncName.StartsWith(TEXT("Should"))))))
				{
					FCollapsedGetterSetter Collapsed;
					Collapsed.Getter = Getter.Value;
					Collapsed.Setter = nullptr;
					Collapsed.Property = MatchingProperty;
					Collapsed.CreateName(PropertyHandlers.Get());
					CollapsedGettersAndSetters.Add(Collapsed);
					UE_LOG(LogMonoScriptGenerator, Log, TEXT("Converting getter '%s' into property on class '%s'"), *Getter.Value->GetName(), *NameMapper.MapClassName(Class));
				}
			}
		}
	}
}

void FMonoScriptCodeGenerator::ExportClass(const UClass* Class)
{
	check(Class);
	check(CanExportClass(Class));

	FString MappedClassName = NameMapper.MapClassName(Class);

	const FMonoBindingsModule& BindingsModule = FindModule(*Class);

	TArray<UProperty*> ExportedProperties;
	TArray<UFunction*> ExportedFunctions;
	TArray<UFunction*> ExportedOverridableFunctions;
	TArray<FCollapsedGetterSetter> CollapsedGettersAndSetters;

	GatherExportedProperties(ExportedProperties, Class);
	GatherExportedFunctions(ExportedFunctions, Class);
	
	CollapseGettersAndSetters(CollapsedGettersAndSetters, Class, ExportedProperties, ExportedFunctions);

	// remove any properties which are collapsed, they will be handled as a special case
	if (CollapsedGettersAndSetters.Num() > 0)
	{
		ExportedProperties.RemoveAll(
			[&CollapsedGettersAndSetters](UProperty* Property)
		{
			return CollapsedGettersAndSetters.ContainsByPredicate([Property](const FCollapsedGetterSetter& Collapsed) { return Collapsed.Property == Property || Collapsed.SynthesizedName == Property->GetName(); });
		});

		ExportedFunctions.RemoveAll(
			[&CollapsedGettersAndSetters](UFunction* Function)
		{
			return CollapsedGettersAndSetters.ContainsByPredicate([Function](const FCollapsedGetterSetter& Collapsed) { return Collapsed.Getter == Function || Collapsed.Setter == Function;  });
		});
	}

	bool bClassIsDerivable = IsDerivableClass(Class);

	if (bClassIsDerivable)
	{
		GatherExportedOverridableFunctions(ExportedOverridableFunctions, Class);
	}

	bool bClassIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);

	FMonoTextBuilder Builder(FMonoTextBuilder::IndentType::Spaces);

	Builder.AppendLine(TEXT("using System;"));
	Builder.AppendLine(TEXT("using System.Runtime.InteropServices;"));
	Builder.AppendLine(FString::Printf(TEXT("using %s;"), MONO_BINDINGS_NAMESPACE));
	Builder.AppendLine();
	Builder.AppendLine();
	Builder.AppendLine(FString::Printf(TEXT("namespace %s"), *BindingsModule.GetNamespace()));
	Builder.OpenBrace();

	const FString QualifiedSuperClassName = GetQualifiedSuperClassName(Class);

	Builder.AppendDocCommentFromMetadata(*Class);

	{
		FMonoCSharpPropertyBuilder PropBuilder;

		PropBuilder.AddAttribute(TEXT("UClass(UserClassFlags.NativeBindingsClass)"));
		PropBuilder.AddMetaData(*Class);
		PropBuilder.Finish();

		Builder.AppendLine(PropBuilder.ToString());
	}

	const TCHAR* PartialSpecifier = TEXT("partial ");
	const TCHAR* AbstractSpecifier = bClassIsAbstract ? TEXT("abstract ") : TEXT("");

	Builder.AppendLine(FString::Printf(TEXT("%spublic %sclass %s : %s"), AbstractSpecifier, PartialSpecifier, *MappedClassName, *QualifiedSuperClassName));
	Builder.OpenBrace();

	TSet<FString> ExportedPropertiesHash;

	if (ExportedProperties.Num() > 0)
	{
		ExportClassProperties(Builder, Class, ExportedProperties, ExportedPropertiesHash);
	}

	// Export special cased "world" property
	// Only export this for non-UObject, non-BlueprintFunctionLibrary basemost blueprintable types
	if (Class != UObject::StaticClass()
		&& Class->GetName() != TEXT("BlueprintFunctionLibrary"))
	{
		if (Class->HasMetaData(MD_IsBlueprintBase))
		{
			bool bClassIsBlueprintBase = Class->GetBoolMetaData(MD_IsBlueprintBase);

			if (bClassIsBlueprintBase)
			{
				UClass* SuperClass = Class->GetSuperClass();

				if (!MonoScriptCodeGeneratorUtils::GetBoolMetaDataHeirarchical(SuperClass, MD_IsBlueprintBase, MonoScriptCodeGeneratorUtils::BoolHierarchicalMetaDataMode::SearchStopAtTrueValue))
				{
					// this is the basemost blueprintable class
					// export a special case "World" property
					Builder.AppendLine();
					Builder.AppendLine(TEXT("// World access"));
					Builder.AppendLine(TEXT("public UnrealEngine.Engine.World World"));
					Builder.OpenBrace();
					Builder.AppendLine(TEXT("get"));
					Builder.OpenBrace();
					Builder.AppendLine(TEXT("CheckDestroyedByUnrealGC();"));
					Builder.AppendLine(TEXT("return GetWorldFromContextObjectNative(NativeObject);"));
					Builder.CloseBrace();
					Builder.CloseBrace();

					UE_LOG(LogMonoScriptGenerator, Log, TEXT("Exported 'World' property on blueprintable class '%s'"), *MappedClassName);
				}
			}
		}
	}


	if (CollapsedGettersAndSetters.Num() > 0)
	{
		ExportClassCollapsedGettersAndSetters(Builder, Class, CollapsedGettersAndSetters, ExportedPropertiesHash);
	}

	// Generate static constructor
	Builder.AppendLine();
	ExportStaticConstructor(Builder, Class, ExportedProperties, ExportedFunctions, ExportedOverridableFunctions, CollapsedGettersAndSetters);

	// Generate native constructor
	Builder.AppendLine();
	Builder.AppendLine(FString::Printf(TEXT("protected %s(IntPtr InNativeObject)"), *MappedClassName));
	Builder.AppendLine(TEXT("  : base(InNativeObject)"));
	Builder.OpenBrace();
	Builder.CloseBrace();

	// generate inheriting constructor
	Builder.AppendLine();
	Builder.AppendLine(FString::Printf(TEXT("protected %s(ObjectInitializer initializer)"), *MappedClassName));
	Builder.AppendLine(TEXT("  : base(initializer)"));
	Builder.OpenBrace();
	Builder.CloseBrace();

	if (ExportedFunctions.Num() > 0)
	{
		ExportClassFunctions(Builder, Class, ExportedFunctions);
	}

	if (ExportedOverridableFunctions.Num() > 0)
	{
		ExportClassOverridableFunctions(Builder, Class, ExportedOverridableFunctions);
	}

	Builder.AppendLine();

	Builder.CloseBrace(); // close class
	Builder.AppendLine();

	if (bClassIsAbstract)
	{
		// for abstract classes, create a sealed wrapper only version of the class
		// This is so we can expose objects we may not have generated bindings for, we can expose them as the
		// most derived super class we have bindings for
		Builder.AppendLine(FString::Printf(TEXT("sealed class %s_WrapperOnly : %s"), *MappedClassName, *MappedClassName));
		Builder.OpenBrace();

		// Generate native constructor
		Builder.AppendLine();
		Builder.AppendLine(FString::Printf(TEXT("%s_WrapperOnly(IntPtr InNativeObject)"), *MappedClassName));
		Builder.AppendLine(TEXT("  : base(InNativeObject)"));
		Builder.OpenBrace();
		Builder.CloseBrace();

		Builder.CloseBrace(); // close namespace

		Builder.AppendLine();
	}
	Builder.CloseBrace(); // close namespace

	SaveTypeGlue(Class, Builder.ToText().ToString());
}

void FMonoScriptCodeGenerator::ExportStruct(const UScriptStruct* Struct)
{
	check(Struct);
	const FMonoBindingsModule& BindingsModule = FindModule(*Struct);

	TArray<UProperty*> ExportedProperties;
	GatherExportedProperties(ExportedProperties, Struct);

	FMonoTextBuilder Builder(FMonoTextBuilder::IndentType::Spaces);

	Builder.AppendLine(TEXT("using System;"));
	Builder.AppendLine(TEXT("using System.Runtime.InteropServices;"));
	Builder.AppendLine(FString::Printf(TEXT("using %s;"), MONO_BINDINGS_NAMESPACE));
	Builder.AppendLine();
	Builder.AppendLine();
	Builder.AppendLine(FString::Printf(TEXT("namespace %s"), *BindingsModule.GetNamespace()));
	Builder.OpenBrace();

	const bool bIsBlittable = PropertyHandlers->IsStructBlittable(*Struct);

	Builder.AppendDocCommentFromMetadata(*Struct);

	// emit UStruct property
	{
		UClass* ClassOwner = Cast<UClass>(Struct->GetOuter());

		FMonoCSharpPropertyBuilder PropBuilder;

		PropBuilder.AddAttribute(TEXT("UStruct"));
		if (bIsBlittable)
		{
			PropBuilder.AddArgument(TEXT("NativeBlittable=true"));
		}
		if (ClassOwner != nullptr)
		{
			PropBuilder.AddArgument(FString::Printf(TEXT("NativeClassOwner=\"%s\""), *ClassOwner->GetName()));
		}

		PropBuilder.AddMetaData(*Struct);
		PropBuilder.Finish();

		Builder.AppendLine(PropBuilder.ToString());
	}

	const TCHAR* PartialSpecifier = HasInjectedSource(Struct) ? TEXT("partial ") : TEXT("");

	Builder.AppendLine(FString::Printf(TEXT("public %sstruct %s"), PartialSpecifier, *NameMapper.MapStructName(Struct)));
	Builder.OpenBrace();

	ExportStructProperties(Builder, Struct, ExportedProperties, bIsBlittable);

	if (!bIsBlittable)
	{
		// Generate static constructor
		Builder.AppendLine();
		TArray<UFunction*> EmptyFunctionArray;
		TArray<FCollapsedGetterSetter> EmptyCollapsedGettersAndSettersArray;
		ExportStaticConstructor(Builder, Struct, ExportedProperties, EmptyFunctionArray, EmptyFunctionArray, EmptyCollapsedGettersAndSettersArray);

		// Generate native constructor
		Builder.AppendLine();
		ExportMirrorStructMarshalling(Builder, Struct, ExportedProperties);
	}
	
	Builder.CloseBrace(); // struct

	if (!bIsBlittable)
	{
		// Generate custom marshaler for arrays of this struct
		ExportStructMarshaler(Builder, Struct);
	}

	Builder.CloseBrace(); // namespace


	SaveTypeGlue(Struct, Builder.ToText().ToString());
}

void FMonoScriptCodeGenerator::ExportMirrorStructMarshalling(FMonoTextBuilder& Builder, const UScriptStruct* Struct, TArray<UProperty*> ExportedProperties)
{
	Builder.AppendLine();
	Builder.AppendLine(TEXT("// Construct by marshalling from a native buffer."));
	Builder.AppendLine(FString::Printf(TEXT("public %s(IntPtr InNativeStruct)"), *NameMapper.MapStructName(Struct)));
	Builder.OpenBrace();

	for (auto Property : ExportedProperties)
	{
		const FMonoPropertyHandler& PropertyHandler = PropertyHandlers->Find(Property);
		FString NativePropertyName = Property->GetName();
		FString CSharpPropertyName = NameMapper.MapPropertyName(Property);
		PropertyHandler.ExportMarshalFromNativeBuffer(
			Builder, 
			Property, 
			TEXT("null"),
			NativePropertyName,
			FString::Printf(TEXT("%s ="), *CSharpPropertyName),
			TEXT("InNativeStruct"), 
			FString::Printf(TEXT("%s_Offset"),*NativePropertyName),
			false,
			false);
	}

	Builder.CloseBrace(); // ctor


	Builder.AppendLine();
	Builder.AppendLine(TEXT("// Marshal into a preallocated native buffer."));
	Builder.AppendLine(TEXT("public void ToNative(IntPtr Buffer)"));
	Builder.OpenBrace();

	for (auto Property : ExportedProperties)
	{
		const FMonoPropertyHandler& PropertyHandler = PropertyHandlers->Find(Property);
		FString NativePropertyName = Property->GetName();
		FString CSharpPropertyName = NameMapper.MapPropertyName(Property);
		PropertyHandler.ExportMarshalToNativeBuffer(
			Builder, 
			Property, 
			TEXT("null"),
			NativePropertyName,
			TEXT("Buffer"), 
			FString::Printf(TEXT("%s_Offset"), *NativePropertyName),
			CSharpPropertyName);
	}

	Builder.CloseBrace(); // ToNative
}

void FMonoScriptCodeGenerator::ExportStructMarshaler(FMonoTextBuilder& Builder, const UScriptStruct* Struct)
{
	FString StructName = NameMapper.MapStructName(Struct);

	Builder.AppendLine();
	Builder.AppendLine(FString::Printf(TEXT("public static class %sMarshaler"), *StructName));
	Builder.OpenBrace();

	Builder.AppendLine(FString::Printf(TEXT("public static %s FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)"), *StructName));
	Builder.OpenBrace();
	Builder.AppendLine(FString::Printf(TEXT("return new %s(nativeBuffer + arrayIndex * GetNativeDataSize());"), *StructName));
	Builder.CloseBrace(); // MarshalNativeToManaged

	Builder.AppendLine();
	Builder.AppendLine(FString::Printf(TEXT("public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, %s obj)"), *StructName));
	Builder.OpenBrace();
	Builder.AppendLine(TEXT("obj.ToNative(nativeBuffer + arrayIndex * GetNativeDataSize());"));
	Builder.CloseBrace(); // MarshalManagedToNative

	Builder.AppendLine();
	Builder.AppendLine(TEXT("public static int GetNativeDataSize()"));
	Builder.OpenBrace();
	Builder.AppendLine(FString::Printf(TEXT("return %s.NativeDataSize;"), *StructName));
	Builder.CloseBrace(); // GetNativeDataSize

	Builder.CloseBrace(); // Marshaler
}

bool FMonoScriptCodeGenerator::HasInjectedSource(const UStruct* StructOrClass) const
{
	check(StructOrClass);
	const FString ModuleName = MonoScriptCodeGeneratorUtils::GetModuleName(*StructOrClass);

	const FString InjectedFile = FPaths::Combine(*InjectedSourceDirectory, *ModuleName, *FString::Printf(TEXT("%s_Injected.cs"), *StructOrClass->GetName()));

	return FPaths::FileExists(InjectedFile);
}

bool FMonoScriptCodeGenerator::CanExportClass(const UClass* Class) const
{
	bool bCanExport =
		ScriptGenUtil::ShouldExportClass(Class)
		&& !Class->HasAnyClassFlags(CLASS_Deprecated); // Don't export deprecated classes
		
	return bCanExport;
}

bool FMonoScriptCodeGenerator::IsDerivableClass(const UClass* Class) const
{
	const bool bCanCreate =
		!Class->HasAnyClassFlags(CLASS_Deprecated)
		&& !Class->HasAnyClassFlags(CLASS_NewerVersionExists)
		&& !Class->ClassGeneratedBy;

	const bool bIsBlueprintBase = MonoScriptCodeGeneratorUtils::GetBoolMetaDataHeirarchical(Class, MD_IsBlueprintBase, MonoScriptCodeGeneratorUtils::BoolHierarchicalMetaDataMode::SearchStopAtAnyValue);

	const bool bIsValidClass =bIsBlueprintBase
		|| (Class == UObject::StaticClass())
		|| (Class->GetFName() == MD_BlueprintFunctionLibrary);

	return bCanCreate && bIsValidClass;
}

bool FMonoScriptCodeGenerator::IsBlueprintVariableClass(const UStruct* Struct) const
{
	// UObject is an exception, and is always a blueprint-able type
	if (Struct == UObject::StaticClass())
	{
		return true;
	}

	const UStruct* ParentStruct = Struct;
	while (ParentStruct)
	{
		// Climb up the class hierarchy and look for "BlueprintType" and "NotBlueprintType" to see if this class is allowed.
		if (ParentStruct->GetBoolMetaData(MD_AllowableBlueprintVariableType)
			|| ParentStruct->HasMetaData(MD_BlueprintSpawnableComponent))
		{
			return true;
		}
		else if (ParentStruct->GetBoolMetaData(MD_NotAllowableBlueprintVariableType))
		{
			return false;
		}
		ParentStruct = ParentStruct->GetSuperStruct();
	}

	return false;
}

bool FMonoScriptCodeGenerator::CanExportPropertyShared(const UProperty* Property) const
{
	const FMonoPropertyHandler& Handler = PropertyHandlers->Find(Property);

	// must be blueprint visible, should not be deprecated, arraydim == 1
	//if it's CPF_BlueprintVisible, we know it's RF_Public, CPF_Protected or MD_AllowPrivateAccess
	bool bCanExport = ScriptGenUtil::ShouldExportProperty(Property)
		&& !Property->HasAnyPropertyFlags(CPF_Deprecated)
		&& (Property->ArrayDim == 1 || (Handler.IsSupportedInStaticArray() && Property->GetOuter()->IsA(UClass::StaticClass())));

	return bCanExport;
}

bool FMonoScriptCodeGenerator::CanExportProperty(const UStruct* Struct, UProperty* Property) const
{
	bool bCanExport = !Blacklist.HasProperty(Struct, Property)
						&& (CanExportPropertyShared(Property)
							|| Whitelist.HasProperty(Struct, Property)
							// Always include UProperties for whitelisted structs.
							// If their properties where blueprint-exposed, we wouldn't have had to whitelist them!
							|| Whitelist.HasStruct(Struct));

	if (bCanExport)
	{
		bool bIsClassProperty = Struct->IsA(UClass::StaticClass());
		check(bIsClassProperty || Struct->IsA(UScriptStruct::StaticClass()));

		const FMonoPropertyHandler& Handler = PropertyHandlers->Find(Property);
		if ((bIsClassProperty && !Handler.IsSupportedAsProperty()) 
			|| (!bIsClassProperty && !Handler.IsSupportedAsStructProperty()) 
			|| !Handler.CanHandleProperty(Property))
		{
			++UnhandledProperties.FindOrAdd(Property->GetClass()->GetFName());
			bCanExport = false;
		}
	}

	return bCanExport;
}

bool FMonoScriptCodeGenerator::CanExportParameter(const UProperty* Property) const
{
	// don't handle static array params yet
	bool bCanExport = Property->ArrayDim == 1;

	if (bCanExport)
	{
		const FMonoPropertyHandler& Handler = PropertyHandlers->Find(Property);
		if (!Handler.IsSupportedAsParameter() || !Handler.CanHandleProperty(Property))
		{
			bCanExport = false;
		}
	}

	if (!bCanExport)
	{
		++UnhandledParameters.FindOrAdd(Property->GetClass()->GetFName());
	}

	return bCanExport;
}

bool FMonoScriptCodeGenerator::CanExportReturnValue(const UProperty* Property) const
{
	bool bCanExport = Property->ArrayDim == 1;

	if (bCanExport)
	{
		const FMonoPropertyHandler& Handler = PropertyHandlers->Find(Property);
		if (!Handler.IsSupportedAsReturnValue() || !Handler.CanHandleProperty(Property))
		{
			++UnhandledReturnValues.FindOrAdd(Property->GetClass()->GetFName());
			bCanExport = false;
		}
	}

	return bCanExport;
}


bool FMonoScriptCodeGenerator::CanExportFunction(const UStruct* Struct, UFunction* Function) const
{
	bool bBlacklisted = Blacklist.HasFunction(Struct, Function);
	bool bWhitelisted = Whitelist.HasFunction(Struct, Function);

	// must be blueprint callable and public or protected
	// allow whitelist to override black list
	// explicitly filter out BlueprintEvent functions as they are handled by CanExportOverridableFunction
	bool bCanExport = (bWhitelisted || (!bBlacklisted && ScriptGenUtil::ShouldExportFunction(Function)) && !Function->HasAnyFunctionFlags(FUNC_BlueprintEvent));

	//we don't support latent actions yet
	if (Function->HasMetaData(MD_Latent))
	{
		MONOUE_GENERATOR_ISSUE(GenerationWarning, "Skipping unsupported latent action '%s.%s'", *Struct->GetName(), *Function->GetName());
		return false;
	}

	if (bCanExport)
	{
		UProperty* ReturnProperty = Function->GetReturnProperty();
		bCanExport = ReturnProperty == NULL || CanExportReturnValue(ReturnProperty);
		if (bCanExport && Function->NumParms > 0)
		{
			for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
			{
				UProperty* ParamProperty = *ParamIt;
				if (!ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					if (!CanExportParameter(ParamProperty))
					{
						bCanExport = false;
					}
				}
			}
		}
	}

	return bCanExport;
}

bool FMonoScriptCodeGenerator::CanExportOverridableParameter(const UProperty* Property) const
{
	bool bCanExport = Property->ArrayDim == 1;

	if (bCanExport)
	{
		const FMonoPropertyHandler& Handler = PropertyHandlers->Find(Property);
		bCanExport = Handler.IsSupportedAsOverridableFunctionParameter() && Handler.CanHandleProperty(Property);
	}

	if (!bCanExport)
	{
		++UnhandledOverridableParameters.FindOrAdd(Property->GetClass()->GetFName());
	}

	return bCanExport;
}

bool FMonoScriptCodeGenerator::CanExportOverridableReturnValue(const UProperty* Property) const
{
	bool bCanExport = Property->ArrayDim == 1;

	if (bCanExport)
	{
		const FMonoPropertyHandler& Handler = PropertyHandlers->Find(Property);
		if (!Handler.IsSupportedAsOverridableFunctionReturnValue() || !Handler.CanHandleProperty(Property))
		{
			bCanExport = false;
		}
	}

	if (!bCanExport)
	{
		++UnhandledOverridableReturnValues.FindOrAdd(Property->GetClass()->GetFName());
	}

	return bCanExport;
}

bool FMonoScriptCodeGenerator::CanExportOverridableFunction(const UStruct* Struct, UFunction* Function) const
{
	bool bBlacklisted = Blacklist.HasOverridableFunction(Struct, Function);
	bool bWhitelisted = Whitelist.HasOverridableFunction(Struct, Function);
	bool bCanExport = (bWhitelisted || (!bBlacklisted && ScriptGenUtil::ShouldExportFunction(Function)) && Function->HasAnyFunctionFlags(FUNC_BlueprintEvent));

	if (bCanExport)
	{
		UProperty* ReturnProperty = Function->GetReturnProperty();
		if (ReturnProperty != NULL && !CanExportOverridableReturnValue(ReturnProperty))
		{
			bCanExport = false;
		}
		if (bCanExport && Function->NumParms > 0)
		{
			for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
			{
				UProperty* ParamProperty = *ParamIt;
				if (!ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					if (!CanExportOverridableParameter(ParamProperty))
					{
						bCanExport = false;
					}
				}
			}
		}
	}

	return bCanExport;

}

UProperty* FindParameter(const  UFunction* Function, const FString& Name)
{
	for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		UProperty* ParamProperty = *ParamIt;
		if (ParamProperty->GetName() == Name)
		{
			return ParamProperty;
		}
	}
	return NULL;
}

bool FMonoScriptCodeGenerator::GetExtensionMethodInfo(ExtensionMethod& Info, UFunction& Function) const
{
	static const FName MD_WorldContext(TEXT("WorldContext"));

	UProperty* SelfParameter = NULL;
	bool isWorldContext = false;

	// ScriptMethod is the canonical metadata for extension methods
	if (Function.HasMetaData(ScriptGenUtil::ScriptMethodMetaDataKey))
	{
		SelfParameter = CastChecked<UProperty>(Function.Children);
	}

	// however, we can also convert DefaultToSelf parameters to extension methods
	if (!SelfParameter && Function.HasMetaData(MD_DefaultToSelf))
	{
		SelfParameter = FindParameter(&Function, Function.GetMetaData(MD_DefaultToSelf));
	}

	// if a world context is specified, we can use that to determine whether the parameter is a world context
	// we can also convert WorldContext methods into extension methods, if we didn't match on some other parameter already
	if (Function.HasMetaData(MD_WorldContext))
	{
		FString WorldContextName = Function.GetMetaData(MD_WorldContext);
		if (SelfParameter)
		{
			if (SelfParameter->GetName() == WorldContextName)
			{
				isWorldContext = true;
			}
		}
		else
		{
			SelfParameter = FindParameter(&Function, WorldContextName);
			isWorldContext = true;
		}
	}

	if (!SelfParameter)
	{
		return false;
	}

	// some world context parameters might not be annotated, so check the name
	if (!isWorldContext)
	{
		FString ParamName = SelfParameter->GetName();
		isWorldContext |= ParamName == TEXT("WorldContextObject") || ParamName == TEXT("WorldContext");
	}

	Info.Function = &Function;
	Info.SelfParameter = SelfParameter;
	Info.OverrideClassBeingExtended = nullptr;

	// if it's a world context, type it more strongly
	if (isWorldContext)
	{
		UClass* WorldClass = FindObject<UClass>(ANY_PACKAGE, TEXT("World"));
		check(WorldClass);
		Info.OverrideClassBeingExtended = WorldClass;
	}

	return true;
}

void FMonoScriptCodeGenerator::ExportStaticConstructor(
	FMonoTextBuilder& Builder, 
	const UStruct* Struct,
	const TArray<UProperty*>& ExportedProperties, 
	const TArray<UFunction*>& ExportedFunctions,
	const TArray<UFunction*>& ExportedOverrideableFunctions,
	const TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters) const
{
	const UClass* Class = Cast<UClass>(Struct);
	const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Struct);
	if (nullptr != ScriptStruct || ExportedProperties.Num() > 0 || ExportedFunctions.Num() > 0 || ExportedOverrideableFunctions.Num() > 0 || CollapsedGettersAndSetters.Num() > 0)
	{
		bool bHasStaticFunctions = false;
		for (UFunction* Func : ExportedFunctions)
		{
			if (Func->HasAnyFunctionFlags(FUNC_Static))
			{
				bHasStaticFunctions = true;
				break;
			}
		}

		for (FCollapsedGetterSetter const & GetterSetter : CollapsedGettersAndSetters)
		{
			if ((GetterSetter.Getter && GetterSetter.Getter->HasAnyFunctionFlags(FUNC_Static)) || (GetterSetter.Setter && GetterSetter.Setter->HasAnyFunctionFlags(FUNC_Static)))
			{
				bHasStaticFunctions = true;
				break;
			}
		}

		if (bHasStaticFunctions)
		{
			// Keep the class pointer so we can use the CDO to invoke static functions.
			Builder.AppendLine(TEXT("static readonly IntPtr NativeClassPtr;"));
		}

		if (nullptr != ScriptStruct)
		{
			Builder.AppendLine(TEXT("public static readonly int NativeDataSize;"));
		}

		FString Name = ScriptStruct? NameMapper.MapStructName(ScriptStruct) : NameMapper.MapClassName(Class);

		// static constructor to initialize property offset struct
		Builder.AppendLine(FString::Printf(TEXT("static %s()"), *Name));
		Builder.OpenBrace();

		Builder.AppendLine(FString::Printf(TEXT("%sNativeClassPtr = UnrealInterop.GetNative%sFromName(\"%s\");"), 
			bHasStaticFunctions ? TEXT("") : TEXT("IntPtr "), 
			Class ? TEXT("Class") : TEXT("Struct"), 
			*Name));

		Builder.AppendLine();

		ExportPropertiesStaticConstruction(Builder, ExportedProperties, CollapsedGettersAndSetters);

		if (nullptr != Class)
		{
			Builder.AppendLine();
			ExportClassFunctionsStaticConstruction(Builder, Class, ExportedFunctions, CollapsedGettersAndSetters);

			Builder.AppendLine();
			ExportClassOverridableFunctionsStaticConstruction(Builder, Class, ExportedOverrideableFunctions);

			Builder.AppendLine();
		}
		else
		{
			check(nullptr != ScriptStruct);
			Builder.AppendLine();
			Builder.AppendLine(TEXT("NativeDataSize = UnrealInterop.GetNativeStructSize(NativeClassPtr);"));
		}

		Builder.CloseBrace(); // static ctor
	}
}

void FMonoScriptCodeGenerator::GatherExportedProperties(TArray<UProperty*>& ExportedProperties, const UStruct* Struct) const
{
	for (TFieldIterator<UProperty> PropertyIt(Struct, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		if (CanExportProperty(Struct, Property))
		{
			ExportedProperties.Add(Property);
		}
	}
}

void FMonoScriptCodeGenerator::ExportClassProperties(FMonoTextBuilder& Builder, const UClass* Class, TArray<UProperty*>& ExportedProperties, TSet<FString>& ExportedPropertiesHash) const
{
	Builder.AppendLine(TEXT("// Unreal properties"));

	// Export properties
	for (int i = 0; i < ExportedProperties.Num(); i++)
	{
		const UProperty* Property = ExportedProperties[i];
		FString ManagedName = NameMapper.MapPropertyName(Property);
		if (ExportedPropertiesHash.Contains(ManagedName))
		{
			MONOUE_GENERATOR_ISSUE(GenerationWarning, "Skipping conflicting property '%s.%s'", *NameMapper.MapClassName(Class), *ManagedName);
			ExportedProperties.RemoveAt(i--);
			continue;
		}
		ExportedPropertiesHash.Add(ManagedName);
		PropertyHandlers->Find(Property).ExportWrapperProperty(Builder, Property, Greylist.HasProperty(Class, Property), Whitelist.HasProperty(Class, Property));
	}
}

void FMonoScriptCodeGenerator::ExportStructProperties(FMonoTextBuilder& Builder, const UStruct* Struct, const TArray<UProperty*>& ExportedProperties, bool bSuppressOffsets) const
{
	Builder.AppendLine(TEXT("// Unreal properties"));

	for (UProperty* Property : ExportedProperties)
	{
		PropertyHandlers->Find(Property).ExportMirrorProperty(Builder, Property, Greylist.HasProperty(Struct, Property), bSuppressOffsets);
	}
}

void FMonoScriptCodeGenerator::ExportClassCollapsedGettersAndSetters(FMonoTextBuilder& Builder, const UClass* Class, TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters, TSet<FString>& ExportedPropertiesHash) const
{
	Builder.AppendLine(TEXT("// Collapsed getters and setters"));

	for (int i = 0; i < CollapsedGettersAndSetters.Num(); i++)
	{
		const FCollapsedGetterSetter& Collapsed = CollapsedGettersAndSetters[i];
		if (ExportedPropertiesHash.Contains(Collapsed.SynthesizedName))
		{
			MONOUE_GENERATOR_ISSUE(GenerationWarning, "Skipping conflicting synthetic property '%s.%s'", *NameMapper.MapClassName(Class), *Collapsed.SynthesizedName);
			CollapsedGettersAndSetters.RemoveAt(i--);
			continue;
		}
		ExportedPropertiesHash.Add(Collapsed.SynthesizedName);

		if (Collapsed.Getter == nullptr)
		{
			UProperty* Property = Collapsed.Property;
			check(Property);
			check(Collapsed.Setter);
			// read only property + setter case
			const FMonoPropertyHandler& Handler = PropertyHandlers->Find(Property);
			
			// export as greylisted to set up any required variables for the getter
			Handler.ExportWrapperProperty(Builder, Property, true, false);

			FMonoPropertyHandler::FunctionExporter Exporter(PropertyHandlers->Find(Collapsed.Setter), *Collapsed.Setter, FMonoPropertyHandler::ProtectionMode::UseUFunctionProtection);

			// export function variables
			Exporter.ExportFunctionVariables(Builder);

			// export a normal getter
			const TCHAR* Protection = FMonoPropertyHandler::GetPropertyProtection(Property);

			const FString NativePropertyName = Property->GetName();
			Handler.BeginWrapperPropertyAccessorBlock(Builder, Property, Collapsed.SynthesizedName, Property);

			// export getter
			Builder.AppendLine(TEXT("get"));
			Builder.OpenBrace();

			Handler.ExportPropertyGetter(Builder, Property, NativePropertyName);

			Builder.CloseBrace();

			// export setter which calls function
			Exporter.ExportSetter(Builder);

			Handler.EndWrapperPropertyAccessorBlock(Builder, Property);
		}
		else
		{
			if (Collapsed.Setter == nullptr)
			{
				// get only case
				check(Collapsed.Getter);
				
				UProperty* Property = Collapsed.Getter->GetReturnProperty();
				FMonoPropertyHandler::FunctionExporter GetterExporter(PropertyHandlers->Find(Collapsed.Getter), *Collapsed.Getter, FMonoPropertyHandler::ProtectionMode::UseUFunctionProtection, FMonoPropertyHandler::OverloadMode::SuppressOverloads);

				GetterExporter.ExportFunctionVariables(Builder);

				const FMonoPropertyHandler& Handler = PropertyHandlers->Find(Property);

				check(nullptr != Collapsed.Property || Collapsed.SynthesizedName.Len() > 0);


				Handler.BeginWrapperPropertyAccessorBlock(Builder, Property, Collapsed.SynthesizedName, Collapsed.Property);

				GetterExporter.ExportGetter(Builder);

				Handler.EndWrapperPropertyAccessorBlock(Builder, Property);
			}
			else
			{
				// getter + setter case
				check(Collapsed.Getter);
				check(Collapsed.Setter);
				check(Collapsed.SynthesizedName.Len() > 0);

				UProperty* Property = Collapsed.Getter->GetReturnProperty();

				FMonoPropertyHandler::FunctionExporter SetterExporter(PropertyHandlers->Find(Collapsed.Setter), *Collapsed.Setter, FMonoPropertyHandler::ProtectionMode::UseUFunctionProtection, FMonoPropertyHandler::OverloadMode::SuppressOverloads);
				FMonoPropertyHandler::FunctionExporter GetterExporter(PropertyHandlers->Find(Collapsed.Getter), *Collapsed.Getter, FMonoPropertyHandler::ProtectionMode::UseUFunctionProtection, FMonoPropertyHandler::OverloadMode::SuppressOverloads);

				GetterExporter.ExportFunctionVariables(Builder);
				SetterExporter.ExportFunctionVariables(Builder);

				const FMonoPropertyHandler& Handler = PropertyHandlers->Find(Property);

				Handler.BeginWrapperPropertyAccessorBlock(Builder, Property, Collapsed.SynthesizedName, Collapsed.Property);

				GetterExporter.ExportGetter(Builder);
				SetterExporter.ExportSetter(Builder);

				Handler.EndWrapperPropertyAccessorBlock(Builder, Property);
			}
		}

		Builder.AppendLine();
	}
}

void FMonoScriptCodeGenerator::ExportPropertiesStaticConstruction(FMonoTextBuilder& Builder, const TArray<UProperty*>& ExportedProperties, const TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters) const
{
	//we already warn on conflicts when exporting the properties themselves, so here we can just silently skip them
	TSet<FString> ExportedPropertiesHash;

	for (UProperty* Property : ExportedProperties)
	{
		FString ManagedName = NameMapper.MapPropertyName(Property);
		if (ExportedPropertiesHash.Contains(ManagedName))
		{
			continue;
		}
		ExportedPropertiesHash.Add(ManagedName);

		PropertyHandlers->Find(Property).ExportPropertyStaticConstruction(Builder, Property, Property->GetName());
	}

	for (auto&& Collapsed : CollapsedGettersAndSetters)
	{
		if (ExportedPropertiesHash.Contains(Collapsed.SynthesizedName))
		{
			continue;
		}
		ExportedPropertiesHash.Add(Collapsed.SynthesizedName);

		if (Collapsed.Property != nullptr && Collapsed.Getter == nullptr)
		{
			PropertyHandlers->Find(Collapsed.Property).ExportPropertyStaticConstruction(Builder, Collapsed.Property, Collapsed.Property->GetName());
		}
	}
}

void FMonoScriptCodeGenerator::GatherExportedStructs(TArray<UScriptStruct*>& ExportedStructs, const UClass* Class) const
{
	for (TFieldIterator<UScriptStruct> StructIt(Class, EFieldIteratorFlags::ExcludeSuper); StructIt; ++StructIt)
	{
		UScriptStruct* Struct = *StructIt;

		if (Whitelist.HasStruct(Struct) || (!Blacklist.HasStruct(Struct)
			//FIXME: we need a way to force export enums used from Blueprint-exported functions
			// && ScriptGenUtil::ShouldExportStruct(Struct)
			))
		{
			ExportedStructs.Add(Struct);
		}
	}
}

void FMonoScriptCodeGenerator::GatherExportedFunctions(TArray<UFunction*>& ExportedFunctions, const UStruct* Struct) const
{
	for (TFieldIterator<UFunction> FunctionIt(Struct, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
	{
		check(Struct->IsA(UClass::StaticClass()));

		UFunction* Function = *FunctionIt;
		if (CanExportFunction(Struct, Function))
		{
			ExportedFunctions.Add(Function);
		}
	}
}

void FMonoScriptCodeGenerator::GatherExportedOverridableFunctions(TArray<UFunction*>& ExportedFunctions, const UStruct* Struct) const
{
	for (TFieldIterator<UFunction> FunctionIt(Struct, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
	{
		check(Struct->IsA(UClass::StaticClass()));

		UFunction* Function = *FunctionIt;
		if (CanExportOverridableFunction(Struct, Function))
		{
			ExportedFunctions.Add(Function);
		}
	}
}

void FMonoScriptCodeGenerator::ExportClassFunctions(FMonoTextBuilder& Builder, const UClass* Class, const TArray<UFunction*>& ExportedFunctions)
{
	Builder.AppendLine();
	Builder.AppendLine(TEXT("// UFunctions"));
	for (UFunction* Function : ExportedFunctions)
	{
		FMonoPropertyHandler::FunctionType FuncType = FMonoPropertyHandler::FunctionType::Normal;
		if (Function->HasAnyFunctionFlags(FUNC_Static)
			&& MonoScriptCodeGeneratorUtils::IsBlueprintFunctionLibrary(Class))
		{
			ExtensionMethod Method;
			if (GetExtensionMethodInfo(Method, *Function))
			{
				FuncType = FMonoPropertyHandler::FunctionType::ExtensionOnAnotherClass;

				const FMonoBindingsModule& BindingsModule = FindModule(*Class);
				TArray<ExtensionMethod>& ModuleExtensionMethods = ExtensionMethods.FindOrAdd(BindingsModule.GetModuleName());
				ModuleExtensionMethods.Add(Method);
			}
			else if (ManualLibraryFunctionList.HasFunction(Class, Function))
			{
				// export as an library function wrapped with an extension method, but the extension method is manually implemented
				FuncType = FMonoPropertyHandler::FunctionType::ExtensionOnAnotherClass;
			}
		}

		PropertyHandlers->Find(Function).ExportFunction(Builder, Function, FuncType);
	}
}

void FMonoScriptCodeGenerator::ExportClassOverridableFunctions(FMonoTextBuilder& Builder, const UClass* Class, const TArray<UFunction*>& ExportedOverridableFunctions) const
{
	Builder.AppendLine(TEXT("// Overridable functions"));

	for (UFunction* Function : ExportedOverridableFunctions)
	{
		PropertyHandlers->Find(Function).ExportOverridableFunction(Builder, Function);
	}
}

void FMonoScriptCodeGenerator::ExportClassFunctionsStaticConstruction(FMonoTextBuilder& Builder, const UClass* Class, const TArray<UFunction*>& ExportedFunctions, const TArray<FCollapsedGetterSetter>& CollapsedGettersAndSetters) const
{
	for (UFunction* Function : ExportedFunctions)
	{
		ExportClassFunctionStaticConstruction(Builder, Function);
	}

	for (auto&& Collapsed : CollapsedGettersAndSetters)
	{
		if (Collapsed.Getter != nullptr)
		{
			ExportClassFunctionStaticConstruction(Builder, Collapsed.Getter);
		}
		if (Collapsed.Setter != nullptr)
		{
			ExportClassFunctionStaticConstruction(Builder, Collapsed.Setter);
		}
	}
}

void FMonoScriptCodeGenerator::ExportClassFunctionStaticConstruction(FMonoTextBuilder& Builder, const UFunction *Function) const
{
	FString NativeMethodName = Function->GetName ();
	Builder.AppendLine(FString::Printf(TEXT("%s_NativeFunction = GetNativeFunctionFromClassAndName(NativeClassPtr, \"%s\");"), *NativeMethodName, *Function->GetName()));
	if (Function->NumParms > 0)
	{
		Builder.AppendLine(FString::Printf(TEXT("%s_ParamsSize = GetNativeFunctionParamsSize(%s_NativeFunction);"), *NativeMethodName, *NativeMethodName));
	}
	for (TFieldIterator<UProperty> It(Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UProperty* Property = *It;
		const FMonoPropertyHandler& ParamHandler = PropertyHandlers->Find(Property);
		ParamHandler.ExportParameterStaticConstruction(Builder, NativeMethodName, Property);
	}
}

void FMonoScriptCodeGenerator::ExportClassOverridableFunctionsStaticConstruction(FMonoTextBuilder& Builder, const UClass* Class, const TArray<UFunction*>& ExportedOverridableFunctions) const
{
	for (UFunction* Function : ExportedOverridableFunctions)
	{
		if (Function->NumParms)
		{
			FString NativeMethodName = Function->GetName();
			Builder.AppendLine(FString::Printf(TEXT("IntPtr %s_NativeFunction = GetNativeFunctionFromClassAndName(NativeClassPtr, \"%s\");"), *NativeMethodName, *NativeMethodName));
			Builder.AppendLine(FString::Printf(TEXT("%s_ParamsSize = GetNativeFunctionParamsSize(%s_NativeFunction);"), *NativeMethodName, *NativeMethodName));
			for (TFieldIterator<UProperty> It(Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				UProperty* Property = *It;
				const FMonoPropertyHandler& ParamHandler = PropertyHandlers->Find(Property);
				ParamHandler.ExportParameterStaticConstruction(Builder, NativeMethodName, Property);
			}

			Builder.AppendLine();
		}
	}

}

FString FMonoScriptCodeGenerator::GetCSharpEnumType(const EPropertyType PropertyType) const
{
	switch (PropertyType)
	{
#if MONOUE_STANDALONE
	case EPropertyType::CPT_Int8:   return TEXT("sbyte");   break;
	case EPropertyType::CPT_Int16:  return TEXT("short");  break;
	case EPropertyType::CPT_Int:    return TEXT("int");    break;
	case EPropertyType::CPT_Int64:  return TEXT("long");  break;
	case EPropertyType::CPT_Byte:   return TEXT("byte");   break;
	case EPropertyType::CPT_UInt16: return TEXT("ushort"); break;
	case EPropertyType::CPT_UInt32: return TEXT("uint"); break;
	case EPropertyType::CPT_UInt64: return TEXT("ulong"); break;
#else
	case EPropertyType::Int8:   return TEXT("sbyte");   break;
	case EPropertyType::Int16:  return TEXT("short");  break;
	case EPropertyType::Int:    return TEXT("int");    break;
	case EPropertyType::Int64:  return TEXT("long");  break;
	case EPropertyType::Byte:   return TEXT("byte");   break;
	case EPropertyType::UInt16: return TEXT("ushort"); break;
	case EPropertyType::UInt32: return TEXT("uint"); break;
	case EPropertyType::UInt64: return TEXT("ulong"); break;
#endif
	default:
		return TEXT("");
	}
}

bool IsEnumValueValidWithoutPrefix(FString& RawName, FString& Prefix)
{
	if (RawName.Len() <= Prefix.Len())
	{
		return false;
	}
	auto ch = RawName[Prefix.Len()];
	return FChar::IsAlpha(ch) || ch == '_';
}

void FMonoScriptCodeGenerator::ExportEnums(FMonoTextBuilder& Builder, const TArray<UEnum*>& ExportedEnums) const
{
	for (UEnum* Enum : ExportedEnums)
	{
		check(Enum);

		if (Whitelist.HasEnum(Enum) || (!Blacklist.HasEnum(Enum)
			//FIXME: we need a way to force export enums used from Blueprint-exported functions
			//&& ScriptGenUtil::ShouldExportEnum(Enum)
			))
		{
			Builder.AppendDocCommentFromMetadata(*Enum);

			FString EnumAttribute = FString::Printf(TEXT("[UEnum(NativeEnumName=\"%s\""), *Enum->GetName());
			if (Enum->GetOuter()->IsA(UClass::StaticClass()))
			{
				EnumAttribute += FString::Printf(TEXT(", NativeClassOwner=\"%s\""), *Enum->GetOuter()->GetName());
			}
			EnumAttribute += TEXT(")]");
			Builder.AppendLine(EnumAttribute);

			FString EnumName = NameMapper.MapEnumName (Enum);

#if MONOUE_STANDALONE
			Builder.AppendLine(FString::Printf(TEXT("public enum %s"), *EnumName));
#else
			FString CSharpEnumType = GetCSharpEnumType(Enum->UnderlyingType);
			if (CSharpEnumType.Len() == 0)
			{
				//old untyped enum, assume u8
				CSharpEnumType = TEXT("byte");
			}

			Builder.AppendLine(FString::Printf(TEXT("public enum %s : %s"), *EnumName, *CSharpEnumType));
#endif
			Builder.OpenBrace();
			const int32 ValueCount = Enum->NumEnums();

			// Try to identify a common prefix of the form PRE_, so we can strip it from all values.
			// We'll only strip it if it's present on all values not explicitly skipped.
			FString CommonPrefix;
			int32 CommonPrefixCount = 0;
			int32 SkippedValueCount = 0;

			TArray<FString> EnumValues;
			TArray<FString> EnumDocCommentSummaries;

			EnumValues.Reserve(ValueCount);
			EnumDocCommentSummaries.Reserve(ValueCount);
			for (int32 i = 0; i < ValueCount; ++i)
			{
				FString& RawName = *(new(EnumValues) FString());

				if (!ScriptGenUtil::ShouldExportEnumEntry(Enum, i))
				{
					RawName = FString();
					EnumDocCommentSummaries.Add(FString());
					++SkippedValueCount;
					continue;
				}

				FName ValueName = Enum->GetNameByIndex(i);

				FString QualifiedValueName = ValueName.ToString();

				const int32 ColonPos = QualifiedValueName.Find(TEXT("::"));


				if (INDEX_NONE != ColonPos)
				{
					RawName = QualifiedValueName.Mid(ColonPos + 2);
				}
				else
				{
					RawName = QualifiedValueName;
				}

				if (i == (ValueCount - 1)
					&& RawName.EndsWith(TEXT("MAX")))
				{
					// skip the MAX enum added so C++ can get the number of enum values
					++SkippedValueCount;
					EnumValues.Pop(false);
					continue;
				}

				EnumDocCommentSummaries.Add(MonoScriptCodeGeneratorUtils::GetEnumValueToolTip(*Enum, i));

				// We can skip all of the common prefix checks for enums that are already namespaced in C++.
				// In the cases where a namespaced enum does have a common prefix for its values, it doesn't
				// match the PRE_* pattern, and it's generally necessary for syntactic reasons, 
				// i.e. Touch1, Touch2, and so on in ETouchIndex.
				if (Enum->GetCppForm() == UEnum::ECppForm::Regular)
				{
					// A handful of enums have bad values named this way in C++.
					if (RawName.StartsWith(TEXT("TEMP_BROKEN")))
					{
						++SkippedValueCount;
					}
					// UHT inserts spacers for sparse enums.  Since we're omitting the _MAX value, we'll
					// still export these to ensure that C# reflection gives an accurate value count, but
					// don't hold them against the common prefix count.
					else if (RawName.StartsWith(TEXT("UnusedSpacer_")))
					{
						++SkippedValueCount;
					}
					// Infer the prefix from the first unskipped value.
					else if (!CommonPrefix.Len())
					{
						int32 UnderscorePos = RawName.Find(TEXT("_"));
						if (UnderscorePos != INDEX_NONE)
						{
							CommonPrefix = RawName.Left(UnderscorePos + 1);
							if (IsEnumValueValidWithoutPrefix(RawName, CommonPrefix))
							{
								++CommonPrefixCount;
							}
						}
					}
					else if (RawName.StartsWith(CommonPrefix) && IsEnumValueValidWithoutPrefix(RawName, CommonPrefix))
					{
						++CommonPrefixCount;
					}
				}
			}

			if (ValueCount != (CommonPrefixCount + SkippedValueCount))
			{
				if (CommonPrefix.Len())
				{
					UE_LOG(LogMonoScriptGenerator, Log, TEXT("Rejecting common prefix %s for %s (%d).  ValueCount=%d, CommonPrefixCount=%d, SkippedValueCount=%d"),
						*CommonPrefix,
						*Enum->GetName(),
						Enum->GetFName().GetDisplayIndex(),
						ValueCount,
						CommonPrefixCount,
						SkippedValueCount);
				}

				CommonPrefix.Empty();
			}

			if (CommonPrefix.Len())
			{
				FEnumPropertyHandler::AddStrippedPrefix(Enum, CommonPrefix);
			}

			check(EnumDocCommentSummaries.Num() == EnumValues.Num());

			for (int32 i = 0; i < EnumValues.Num(); ++i)
			{
				FString& EnumValue = EnumValues[i];
				if (EnumValue.Len() == 0)
				{
					continue;
				}

				EnumValue.RemoveFromStart(CommonPrefix);
				EnumValue = NameMapper.ScriptifyName (EnumValue, EScriptNameKind::EnumValue);
				Builder.AppendDocCommentSummary(EnumDocCommentSummaries[i]);
				Builder.AppendLine(FString::Printf(TEXT("%s=%d,"), *EnumValue, i));
			}

			Builder.CloseBrace();
			Builder.AppendLine();
		}
	}
}

const FMonoBindingsModule& FMonoScriptCodeGenerator::FindModule(const UObject& Object) const
{
	return FindModule(MonoScriptCodeGeneratorUtils::GetModuleFName(Object));
}

const FMonoBindingsModule& FMonoScriptCodeGenerator::FindModule(FName ModuleFName) const
{
	const FMonoBindingsModule* BindingsModule = MonoBindingsModules.Find(ModuleFName);
	check(BindingsModule);
	return *BindingsModule;
}

FMonoBindingsModule& FMonoScriptCodeGenerator::FindOrRegisterModule(FName ModuleFName)
{
	// module registration is not open until FinishExport
	check(bModuleRegistrationOpen);

	FMonoBindingsModule* BindingsModule = MonoBindingsModules.Find(ModuleFName);
	if (nullptr == BindingsModule)
	{
		FMonoBindingsModule NewBindingsModule(ModuleFName, MonoOutputDirectory, GameModules.Find(ModuleFName), NameMapper.MapModuleName(ModuleFName));
		BindingsModule = &MonoBindingsModules.Add(ModuleFName, NewBindingsModule);
	}

	check(BindingsModule);
	return *BindingsModule;
}

void FMonoScriptCodeGenerator::RegisterClassModule(UStruct* Struct, const TSet<UStruct*>& References)
{
	check(Struct);
	FName ModuleFName = MonoScriptCodeGeneratorUtils::GetModuleFName(*Struct);

	FMonoBindingsModule& BindingsModule = FindOrRegisterModule(ModuleFName);

	check(!BindingsModule.ExportedTypes.Contains(Struct->GetFName()));
	BindingsModule.ExportedTypes.Add(Struct->GetFName());

	for (auto ReferencedClass : References)
	{
		check(ReferencedClass);
		FName ReferencedModuleFName = MonoScriptCodeGeneratorUtils::GetModuleFName(*ReferencedClass);

		if (ReferencedModuleFName != BindingsModule.GetModuleName())
		{
			BindingsModule.ModuleReferences.Add(ReferencedModuleFName);
		}
	}
}

void FMonoScriptCodeGenerator::ExportModules(const TSet<FName>& ModulesToExport)
{
	TMultiMap<UPackage*, UEnum*> EnumsByPackage;
	for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
	{
		UEnum* Enum = *EnumIt;
		if (UPackage* Package = Cast<UPackage>(Enum->GetOuter()))
		{
			EnumsByPackage.Add(Package, Enum);
		}
		else
		{
			UClass* Class = CastChecked<UClass>(Enum->GetOuter());
			EnumsByPackage.Add(Class->GetOutermost(), Enum);
		}
	}

	TArray<UPackage*> PackagesToExport;
	EnumsByPackage.GetKeys(PackagesToExport);

	for (UPackage* Package : PackagesToExport)
	{
		check(Package);
		FName ModuleFName = MonoScriptCodeGeneratorUtils::GetModuleFName(*Package);
		if (ModulesToExport.Contains(ModuleFName))
		{
			FMonoBindingsModule& Bindings = FindOrRegisterModule(ModuleFName);
			Bindings.bExportModule = true;

			FMonoTextBuilder Builder(FMonoTextBuilder::IndentType::Spaces);

			Builder.AppendLine(TEXT("using System;"));
			Builder.AppendLine(TEXT("using System.Runtime.InteropServices;"));
			Builder.AppendLine(FString::Printf(TEXT("using %s;"), MONO_BINDINGS_NAMESPACE));
			Builder.AppendLine();
			Builder.AppendLine();
			Builder.AppendLine(FString::Printf(TEXT("namespace %s"), *Bindings.GetNamespace()));
			Builder.OpenBrace();

			TArray<UEnum*> PackageEnums;
			EnumsByPackage.MultiFind(Package, PackageEnums);
			ExportEnums(Builder, PackageEnums);

			Builder.CloseBrace(); // close namespace

			SaveModuleGlue(Package, Builder.ToText().ToString());
		}
	}
}

static FString GetClassExportFilename(FName ClassFName)
{
	return ClassFName.ToString() + TEXT(".cs");
}

static FString GetModuleExportFilename(FName ModuleFName)
{
	return ModuleFName.ToString() + TEXT("Module.cs");
}

static FString GetModuleExtensionsFilename(FName ModuleFName)
{
	return ModuleFName.ToString() + TEXT("Extensions.cs");
}

void FMonoScriptCodeGenerator::SaveGlue(const FMonoBindingsModule& Bindings, const FString& Filename, const FString& GeneratedGlue)
{
	const FString& BindingsSourceDirectory = Bindings.GetGeneratedSourceDirectory();

	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (!File.CreateDirectoryTree(*BindingsSourceDirectory))
	{
		UE_LOG(LogMonoScriptGenerator, Error, TEXT("Could not create directory %s"), *BindingsSourceDirectory);
		return;
	}

	const FString GlueOutputPath = FPaths::Combine(*BindingsSourceDirectory, *Filename);

	GeneratedFileManager.SaveFileIfChanged(GlueOutputPath, GeneratedGlue);
}

void FMonoScriptCodeGenerator::SaveTypeGlue(const UStruct* Struct, const FString& GeneratedGlue)
{
	check(Struct);
	FName ModuleFName = MonoScriptCodeGeneratorUtils::GetModuleFName(*Struct);
	const FMonoBindingsModule* BindingsPtr = MonoBindingsModules.Find(ModuleFName);
	check(BindingsPtr);

	SaveGlue(*BindingsPtr, GetClassExportFilename(Struct->GetFName()), GeneratedGlue);
}

void FMonoScriptCodeGenerator::SaveModuleGlue(UPackage* Package, const FString& GeneratedGlue)
{
	check(Package);
	FName ModuleFName = MonoScriptCodeGeneratorUtils::GetModuleFName(*Package);
	const FMonoBindingsModule* BindingsPtr = MonoBindingsModules.Find(ModuleFName);
	check(BindingsPtr);

	FString Filename = GetModuleExportFilename(ModuleFName);
	check(!FindObject<UClass>(Package, *Filename));

	SaveGlue(*BindingsPtr, Filename, GeneratedGlue);
}

void FMonoScriptCodeGenerator::SaveExtensionsGlue(const FMonoBindingsModule& Bindings, const FString& GeneratedGlue)
{
	SaveGlue(Bindings, GetModuleExtensionsFilename(Bindings.GetModuleName()), GeneratedGlue);
}

struct GameSolutionInfo
{
	TArray<TSharedPtr<FMonoProjectFile>> Projects;
	TMap<FName, FMonoProjectFile*> GameModuleToProjectFileMap;
	FString GameName;
	FString ManifestOutputDirectory;
};

static const FString ENGINE_ASSEMBLY_VARIABLE(TEXT("$(UE4EngineAssembliesPath)"));
static const FString GAME_ASSEMBLY_VARIABLE(TEXT("$(UE4GameAssembliesPath)"));

void FMonoScriptCodeGenerator::GenerateProjectFiles()
{
	FMonoProjectFile BuiltinModules(FPaths::Combine(*MonoOutputDirectory, BUILTIN_MODULES_PROJECT_NAME), MONO_UE4_NAMESPACE TEXT(".BuiltinModules"));
	TArray<TSharedPtr<FMonoProjectFile>> PluginProjects;
	TMap<FString, GameSolutionInfo> GameSolutionDirectoryToProjectMap;

	TMap<FName, FMonoProjectFile*> ModuleToProjectFileMap;

	for (const auto& Pair : MonoBindingsModules)
	{
		const FMonoBindingsModule& BindingsModule = Pair.Value;
		if (!BindingsModule.IsBuiltinEngineModule())
		{
			TSharedPtr<FMonoProjectFile> ProjectFile = MakeShareable(new FMonoProjectFile(BindingsModule.GetGeneratedProjectDirectory(), BindingsModule.GetAssemblyName()));
			ProjectFile->BindingsModules.Add(BindingsModule);
			if (Pair.Value.IsGameModule())
			{
				FString GameSolutionDirectory = BindingsModule.GetGameSolutionDirectory();
				
				GameSolutionInfo* GameInfo = GameSolutionDirectoryToProjectMap.Find(GameSolutionDirectory);
				if (nullptr == GameInfo)
				{
					GameInfo = &GameSolutionDirectoryToProjectMap.Add(GameSolutionDirectory, GameSolutionInfo());
					GameInfo->GameName = BindingsModule.GetGameName();
					GameInfo->ManifestOutputDirectory = BindingsModule.GetGameModuleManifestDirectory();
				}
				else
				{
					check(GameInfo->GameName == BindingsModule.GetGameName());
					check(GameInfo->ManifestOutputDirectory == BindingsModule.GetGameModuleManifestDirectory());
				}
				check(GameInfo);
				
				GameInfo->GameModuleToProjectFileMap.Add(Pair.Key, ProjectFile.Get());
				GameInfo->Projects.Add(MoveTemp(ProjectFile));
			}
			else
			{
				ModuleToProjectFileMap.Add(Pair.Key, ProjectFile.Get());
				PluginProjects.Add(MoveTemp(ProjectFile));
			}
		}
		else
		{
			BuiltinModules.BindingsModules.Add(Pair.Value);
			// safe to add this since pointer to BuiltinModules will not change
			ModuleToProjectFileMap.Add(Pair.Key, &BuiltinModules);
		}
	}

	GenerateProjectFile(BuiltinModules, nullptr, false);
	for (const auto& PluginProject : PluginProjects)
	{
		GenerateProjectFile(*PluginProject, &ModuleToProjectFileMap, false);
	}

	// Generate game projects
	for (const auto& SolutionGameProjectsPair : GameSolutionDirectoryToProjectMap)
	{
		const GameSolutionInfo& GameInfo = SolutionGameProjectsPair.Value;
		// create a unique module to project file map so games don't cross-reference each other if they happen to have the same module names
		TMap<FName, FMonoProjectFile*> GameModuleToProjectFileMap = ModuleToProjectFileMap;

		GameModuleToProjectFileMap.Append(GameInfo.GameModuleToProjectFileMap);

		for (const auto& GameProject : GameInfo.Projects)
		{
			GenerateProjectFile(*GameProject, &GameModuleToProjectFileMap, true);
		}
	}

	// Generate engine bindings solution
	GenerateSolutionFile(FPaths::Combine(*MonoOutputDirectory, TEXT("UE4_Bindings.sln")), BuiltinModules, PluginProjects, nullptr);

	for (const auto& SolutionGameProjectsPair : GameSolutionDirectoryToProjectMap)
	{
		const FString& SolutionDir = SolutionGameProjectsPair.Key;
		const GameSolutionInfo& GameInfo = SolutionGameProjectsPair.Value;
		GenerateSolutionFile(FPaths::Combine(*SolutionDir, *(GameInfo.GameName + TEXT("_Bindings.sln"))), BuiltinModules, PluginProjects, &GameInfo.Projects);
	}

	// generate engine props file
	{
		const FString EnginePropsFilePath = FPaths::Combine(*MonoBuildManifestOutputDirectory, MONO_BINDINGS_NAMESPACE TEXT(".props"));

		TArray<const FMonoProjectFile*> Projects;
		Projects.Add(&BuiltinModules);

		for (const auto& Project : PluginProjects)
		{
			Projects.Add(Project.Get());
		}

		GenerateMSBuildPropsFile(EnginePropsFilePath, Projects, ENGINE_ASSEMBLY_VARIABLE, true);
	}

	// generate game props file
	for (const auto& SolutionGameProjectsPair : GameSolutionDirectoryToProjectMap)
	{
		const GameSolutionInfo& GameInfo = SolutionGameProjectsPair.Value;
		const FString GamePropsFilePath = FPaths::Combine(*GameInfo.ManifestOutputDirectory, MONO_BINDINGS_NAMESPACE TEXT(".props"));

		TArray<const FMonoProjectFile*> Projects;

		for (const auto& Project : GameInfo.Projects)
		{
			Projects.Add(Project.Get());
		}

		GenerateMSBuildPropsFile(GamePropsFilePath, Projects, GAME_ASSEMBLY_VARIABLE, false);
	}
}

static void AddAssemblyReference(FMonoTextBuilder& PropsFileText, const FString& AssemblyLocationVariable, const FString& AssemblyName)
{
	PropsFileText.AppendLine(FString::Printf(TEXT("<Reference Include=\"%s\">"), *AssemblyName));
	PropsFileText.Indent();
	PropsFileText.AppendLine(FString::Printf(TEXT("<HintPath>%s\\%s.dll</HintPath>"), *AssemblyLocationVariable, *AssemblyName));
	PropsFileText.AppendLine(TEXT("<Private>False</Private>"));
	PropsFileText.Unindent();
	PropsFileText.AppendLine(TEXT("</Reference>"));
}

void FMonoScriptCodeGenerator::GenerateMSBuildPropsFile(const FString& PropsFilePath, const TArray<const FMonoProjectFile*>& Projects, const FString& AssemblyLocationVariable, bool bIncludeRuntime)
{
	FMonoTextBuilder PropsFileText(FMonoTextBuilder::IndentType::Tabs);

	// create an msbuild props file which contains references for our bindings assemblies
	PropsFileText.AppendLine(TEXT("<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">"));
	PropsFileText.Indent();
	
	if (Projects.Num() || bIncludeRuntime)
	{
		PropsFileText.AppendLine(TEXT("<ItemGroup>"));
		PropsFileText.Indent();

		if (bIncludeRuntime)
		{
			AddAssemblyReference(PropsFileText, ENGINE_ASSEMBLY_VARIABLE, MONO_BINDINGS_NAMESPACE);
		}

		for (auto ProjectFile : Projects)
		{
			AddAssemblyReference(PropsFileText, AssemblyLocationVariable, ProjectFile->AssemblyName);
		}

		PropsFileText.Unindent();
		PropsFileText.AppendLine(TEXT("</ItemGroup>"));
	}

	PropsFileText.Unindent();
	PropsFileText.AppendLine(TEXT("</Project>"));

	GeneratedFileManager.SaveFileIfChanged(*PropsFilePath, PropsFileText.ToText().ToString());

}

static void AddProjectToSolution(FMonoTextBuilder& SolutionText,
	const FString& SolutionDirectory,
	const FString& ProjectName,
	const FString& ProjectPath,
	const FGuid& ProjectGuid,
	const bool isSdkStyle,
	const TArray<FGuid>* Dependencies=nullptr)
{
	FString RelativeProjectPath = ProjectPath;

	FPaths::MakePathRelativeTo(RelativeProjectPath, *(SolutionDirectory + TEXT("/")));
	FPaths::MakePlatformFilename(RelativeProjectPath);

	FString FlavorGuid = isSdkStyle? "{9A19103F-16F7-4668-BE54-9A1E7A4F7556}" : "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}";

	SolutionText.AppendLine(FString::Printf(TEXT("Project(\"%s\") = \"%s\", \"%s\", \"%s\""),
		*FlavorGuid,
		*ProjectName,
		*RelativeProjectPath,
		*ProjectGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces)));

	if (nullptr != Dependencies && Dependencies->Num() > 0)
	{
		SolutionText.Indent();
		SolutionText.AppendLine(TEXT("ProjectSection(ProjectDependencies) = postProject"));
		SolutionText.Indent();

		for (const auto& guid : *Dependencies)
		{
			SolutionText.AppendLine(FString::Printf(TEXT("%s = %s"), *guid.ToString(EGuidFormats::DigitsWithHyphensInBraces), *guid.ToString(EGuidFormats::DigitsWithHyphensInBraces)));
		}
		SolutionText.Unindent();
		SolutionText.AppendLine(TEXT("EndProjectSection"));
		SolutionText.Unindent();
	}
	SolutionText.AppendLine(TEXT("EndProject"));
}

static void AddProjectToSolution(FMonoTextBuilder& SolutionText,
	const FString& SolutionDirectory,
	const FMonoProjectFile& ProjectFile)
{
	AddProjectToSolution(SolutionText, SolutionDirectory, ProjectFile.AssemblyName, ProjectFile.ProjectFilePath, ProjectFile.ProjectFileGuid, ProjectFile.isSdkStyle); 
}

static void AddProjectConfigurationPlatforms(FMonoTextBuilder& SolutionText, const FGuid& ProjectGuid, const FString& SlnConfig, const FString& SlnPlatform, const FString& ProjConfig, const FString& ProjPlatform)
{
	SolutionText.AppendLine(FString::Printf(TEXT("%s.%s|%s.ActiveCfg = %s|%s"), *ProjectGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces), *SlnConfig, *SlnPlatform, *ProjConfig, *ProjPlatform));
	SolutionText.AppendLine(FString::Printf(TEXT("%s.%s|%s.Build.0 = %s|%s"), *ProjectGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces), *SlnConfig, *SlnPlatform, *ProjConfig, *ProjPlatform));
}

void FMonoScriptCodeGenerator::GenerateSolutionFile(const FString& SolutionFilePath, const FMonoProjectFile& BuiltinProjectFile, const TArray<TSharedPtr<FMonoProjectFile>>& PluginProjectFiles, const TArray<TSharedPtr<FMonoProjectFile>>* GameProjectFiles)
{
	const FString SolutionDirectory = FPaths::GetPath(SolutionFilePath);

	FMonoTextBuilder SolutionText(FMonoTextBuilder::IndentType::Tabs);

	SolutionText.AppendLine();
	SolutionText.AppendLine(TEXT("Microsoft Visual Studio Solution File, Format Version 12.00"));
	SolutionText.AppendLine(TEXT("# Visual Studio 2013"));
	SolutionText.AppendLine(TEXT("VisualStudioVersion = 12.0.30501.0"));
	SolutionText.AppendLine(TEXT("MinimumVisualStudioVersion = 10.0.40219.1"));

	// add build tasks project
	const FString BaseMonoUETasksName = FPaths::GetBaseFilename(MonoUETasksProjectPath);
	AddProjectToSolution(SolutionText, SolutionDirectory, BaseMonoUETasksName, MonoUETasksProjectPath, MonoUETasksGuid, false);

	// add mono assembly process project
	const FString BaseMonoAssemblyProcessName = FPaths::GetBaseFilename(MonoAssemblyProcessProjectPath);
	AddProjectToSolution(SolutionText, SolutionDirectory, BaseMonoAssemblyProcessName, MonoAssemblyProcessProjectPath, MonoAssemblyProcessGuid, false);

	//build tools need to be built before bindings projects, add them as explicit dependencies
	TArray<FGuid> BuildToolsDependencies;
	BuildToolsDependencies.Add(MonoUETasksGuid);
	BuildToolsDependencies.Add(MonoAssemblyProcessGuid);

	// add base bindings project
	const FString BaseBindingsName = FPaths::GetBaseFilename(MonoUEBindingsProjectPath);
	AddProjectToSolution(SolutionText, SolutionDirectory, BaseBindingsName, MonoUEBindingsProjectPath, MonoUEBindingsGuid, false, &BuildToolsDependencies);
	
	// Add main domain project
	const FString BaseMainDomainName = FPaths::GetBaseFilename(MonoUEMainDomainProjectPath);
	AddProjectToSolution(SolutionText, SolutionDirectory, BaseMainDomainName, MonoUEMainDomainProjectPath, MonoUEMainDomainGuid, false);

	// Add managed extensions project
	// It must set explicit dependencies so everything else builds before it
	TArray<FGuid> ExtensionsDependencies;
	ExtensionsDependencies.Add(MonoUEBindingsGuid);
	ExtensionsDependencies.Add(BuiltinProjectFile.ProjectFileGuid);
	for (const auto& Proj : PluginProjectFiles)
	{
		ExtensionsDependencies.Add(Proj->ProjectFileGuid);
	}
	if (nullptr != GameProjectFiles)
	{
		for (const auto& Proj : *GameProjectFiles)
		{
			ExtensionsDependencies.Add(Proj->ProjectFileGuid);
		}
	}
	ExtensionsDependencies.Add(MonoAssemblyProcessGuid);

	// add managed extensions
	const FString BaseExtensionsName = FPaths::GetBaseFilename(MonoManagedExtensionsProjectPath);
	AddProjectToSolution(SolutionText, SolutionDirectory, BaseExtensionsName, MonoManagedExtensionsProjectPath, MonoManagedExtensionsGuid, false, &ExtensionsDependencies);

	// add module projects
	AddProjectToSolution(SolutionText, SolutionDirectory, BuiltinProjectFile);
	for (const auto& PluginProjectFile : PluginProjectFiles)
	{
		AddProjectToSolution(SolutionText, SolutionDirectory, *PluginProjectFile);
	}

	if (nullptr != GameProjectFiles)
	{
		for (const auto& GameProjectFile : *GameProjectFiles)
		{
			AddProjectToSolution(SolutionText, SolutionDirectory, *GameProjectFile);
		}
	}

	SolutionText.AppendLine(TEXT("Global"));
	SolutionText.Indent();
	
	SolutionText.AppendLine(TEXT("GlobalSection(SolutionConfigurationPlatforms) = preSolution"));
	SolutionText.Indent();

	TArray<FString> ConfigNames;

	ConfigNames.Add(TEXT("Debug Client"));
	ConfigNames.Add(TEXT("Debug Editor"));
	ConfigNames.Add(TEXT("Debug Server"));
	ConfigNames.Add(TEXT("Debug"));

	ConfigNames.Add(TEXT("DebugGame Client"));
	ConfigNames.Add(TEXT("DebugGame Editor"));
	ConfigNames.Add(TEXT("DebugGame Server"));
	ConfigNames.Add(TEXT("DebugGame"));

	ConfigNames.Add(TEXT("Development Client"));
	ConfigNames.Add(TEXT("Development Editor"));
	ConfigNames.Add(TEXT("Development Server"));
	ConfigNames.Add(TEXT("Development"));

	ConfigNames.Add(TEXT("Shipping Client"));
	ConfigNames.Add(TEXT("Shipping Server"));
	ConfigNames.Add(TEXT("Shipping"));

	ConfigNames.Add(TEXT("Test Client"));
	ConfigNames.Add(TEXT("Test Server"));
	ConfigNames.Add(TEXT("Test"));

	const TCHAR* ConfigDebug = TEXT("Debug");
	const TCHAR* ConfigRelease = TEXT("Release");
	const TCHAR* PlatformAnyCPU = TEXT("Any CPU");


	for (const auto& ConfigName : ConfigNames)
	{
		SolutionText.AppendLine(FString::Printf(TEXT("%s|%s = %s|%s"), *ConfigName, *PlatformName, *ConfigName, *PlatformName));
	}
	
	SolutionText.Unindent();
	SolutionText.AppendLine(TEXT("EndGlobalSection"));

	// project config platforms
	SolutionText.AppendLine(TEXT("GlobalSection(ProjectConfigurationPlatforms) = postSolution"));
	SolutionText.Indent();

	//visual studio groups these by project, so we do too, or VS will re-sort them

	for (const auto& ConfigName : ConfigNames)
	{
		AddProjectConfigurationPlatforms(SolutionText, MonoUETasksGuid, ConfigName, PlatformName, ConfigRelease, PlatformAnyCPU);
	}

	for (const auto& ConfigName : ConfigNames)
	{
		AddProjectConfigurationPlatforms(SolutionText, MonoAssemblyProcessGuid, ConfigName, PlatformName, ConfigRelease, PlatformAnyCPU);
	}

	for (const auto& ConfigName : ConfigNames)
	{
		AddProjectConfigurationPlatforms(SolutionText, MonoUEBindingsGuid, ConfigName, PlatformName, ConfigName, PlatformName);
	}

	for (const auto& ConfigName : ConfigNames)
	{
		AddProjectConfigurationPlatforms(SolutionText, MonoUEMainDomainGuid, ConfigName, PlatformName, ConfigName, PlatformName);
	}

	for (const auto& ConfigName : ConfigNames)
	{
		AddProjectConfigurationPlatforms(SolutionText, MonoManagedExtensionsGuid, ConfigName, PlatformName, ConfigName, PlatformName);
	}

	for (const auto& ConfigName : ConfigNames)
	{
		AddProjectConfigurationPlatforms(SolutionText, BuiltinProjectFile.ProjectFileGuid, ConfigName, PlatformName, ConfigName, PlatformName);
	}

	for (const auto& PluginProjectFile : PluginProjectFiles)
	{
		for (const auto& ConfigName : ConfigNames)
		{
			AddProjectConfigurationPlatforms(SolutionText, PluginProjectFile->ProjectFileGuid, ConfigName, PlatformName, ConfigName, PlatformName);
		}
	}

	if (nullptr != GameProjectFiles)
	{
		for (const auto& GameProjectFile : *GameProjectFiles)
		{
			for (const auto& ConfigName : ConfigNames)
			{
				AddProjectConfigurationPlatforms(SolutionText, GameProjectFile->ProjectFileGuid, ConfigName, PlatformName, ConfigName, PlatformName);
			}
		}
	}

	SolutionText.Unindent();
	SolutionText.AppendLine(TEXT("EndGlobalSection"));

	SolutionText.AppendLine(TEXT("GlobalSection(SolutionProperties) = preSolution"));
	SolutionText.Indent();
	SolutionText.AppendLine(TEXT("HideSolutionNode = FALSE"));

	SolutionText.Unindent();
	SolutionText.AppendLine(TEXT("EndGlobalSection"));

	SolutionText.Unindent();
	SolutionText.AppendLine(TEXT("EndGlobal"));

	GeneratedFileManager.SaveFileIfChanged(SolutionFilePath, SolutionText.ToText().ToString());
}

void FMonoScriptCodeGenerator::GenerateProjectFile(const FMonoProjectFile& ProjectFile, const TMap<FName, FMonoProjectFile*>* ModuleToProjectFileMap, const bool bIsGameModule)
{
	FString ModuleProjectContents = ProjectTemplateContents;

	ModuleProjectContents = ModuleProjectContents.Replace(TEXT("%PROJECTGUID%"), *ProjectFile.ProjectFileGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
	ModuleProjectContents = ModuleProjectContents.Replace(TEXT("%ASSEMBLYNAME%"), *ProjectFile.AssemblyName);

	FString MonoUEProps = FPaths::Combine(*MonoUEPluginDirectory, TEXT("MSBuild"), bIsGameModule ? TEXT("MonoUE.GameBinding.props") : TEXT("MonoUE.EngineBinding.props"));
	FPaths::MakePlatformFilename(MonoUEProps);

	ModuleProjectContents = ModuleProjectContents.Replace(TEXT("%MONOUEPROPS%"), *MonoUEProps);

	// TODO: Add additional system references
	ModuleProjectContents = ModuleProjectContents.Replace(TEXT("%SYSTEMREFERENCES%"), TEXT(""));

	// Add input files
	{
		FString InputFileText;

		for (const auto& BindingsModule : ProjectFile.BindingsModules)
		{
			const FName ModuleFName = BindingsModule.GetModuleName();
			const FString Pattern = FPaths::Combine(*BindingsModule.GetGeneratedSourceDirectory(), TEXT("*.cs"));

			TSet<FString> ExpectedInputFiles;

			// get our set of expected input files
			for (auto ExportedClass : BindingsModule.ExportedTypes)
			{
				FString ExpectedFilename = GetClassExportFilename(ExportedClass);
				ExpectedInputFiles.Add(ExpectedFilename);
			}

			if (BindingsModule.bExportModule)
			{
				ExpectedInputFiles.Add(GetModuleExportFilename(ModuleFName));
			}

			if (BindingsModule.bExportExtensions)
			{
				ExpectedInputFiles.Add(GetModuleExtensionsFilename(ModuleFName));
			}

			TArray<FString> InputFiles;
			IFileManager& FileManager = IFileManager::Get();
			FileManager.FindFiles(InputFiles, *Pattern, true, false);

			for (auto InputFile : InputFiles)
			{
				if (ExpectedInputFiles.Contains(InputFile))
				{
					// expected input file found, add to project
					FString RelativeFile = (BindingsModule.GetGeneratedSourceDirectory() / InputFile);
					FPaths::MakePathRelativeTo(RelativeFile, *(ProjectFile.SourceDirectory + TEXT("/")));
					FPaths::MakePlatformFilename(RelativeFile);
					InputFileText += FString::Printf(TEXT("<Compile Include=\"%s\" />\r\n"), *RelativeFile);
				}
				else
				{
					// unexpected file found, delete
					UE_LOG(LogMonoScriptGenerator, Log, TEXT("Deleting stale bindings file %s in module %s"), *InputFile, *ModuleFName.ToString());
					FileManager.Delete(*FPaths::Combine(*BindingsModule.GetGeneratedSourceDirectory(), *InputFile));
				}
			}

			// error about files we expected to find but didn't
			for (auto ExpectedFile : ExpectedInputFiles)
			{
				if (!InputFiles.Contains(ExpectedFile))
				{
					UE_LOG(LogMonoScriptGenerator, Error, TEXT("Expected to find bindings file %s in module %s, did not!"), *ExpectedFile, *ModuleFName.ToString());
				}
			}

			// hand-written files injected into generated assemblies
			TArray<FString> InjectedFiles;

			const FString InjectedDirectory = FPaths::Combine(*InjectedSourceDirectory, *ModuleFName.ToString());
			const FString InjectedDirectoryPattern = FPaths::Combine(*InjectedDirectory, TEXT("*.cs"));

			FileManager.FindFiles(InjectedFiles, *InjectedDirectoryPattern, true, false);

			for (auto InjectedFile : InjectedFiles)
			{
				FString RelativeFile = (InjectedDirectory / InjectedFile);
				FPaths::MakePathRelativeTo(RelativeFile, *(ProjectFile.SourceDirectory + TEXT("/")));
				FPaths::MakePlatformFilename(RelativeFile);

				FString RelativeLink = BindingsModule.GetGeneratedSourceDirectory() / FPaths::GetCleanFilename(*RelativeFile);
				FPaths::MakePathRelativeTo(RelativeLink, *(ProjectFile.SourceDirectory + TEXT("/")));
				FPaths::MakePlatformFilename(RelativeLink);

				InputFileText += FString::Printf(TEXT("<Compile Include=\"%s\" ><Link>%s</Link></Compile>\r\n"), *RelativeFile, *RelativeLink);
			}
		}

		ModuleProjectContents = ModuleProjectContents.Replace(TEXT("%COMPILE%"), *InputFileText);
	}

	// Add project references
	{
		FString ProjectReferencesText;

		// add base bindings reference
		ProjectReferencesText += GetProjectReferenceText(ProjectFile.SourceDirectory, FPaths::GetBaseFilename(MonoUEBindingsProjectPath), MonoUEBindingsProjectPath, MonoUEBindingsGuid) + TEXT("\r\n");

		if (nullptr != ModuleToProjectFileMap)
		{
			TSet<FMonoProjectFile*> UniqueReferences;

			for (const auto& BindingsModule : ProjectFile.BindingsModules)
			{
				for (auto ReferenceModule : BindingsModule.ModuleReferences)
				{
					FMonoProjectFile* RefProjectFile = ModuleToProjectFileMap->FindRef(ReferenceModule);
					check(RefProjectFile);
					UniqueReferences.Add(RefProjectFile);
				}
			}

			for (auto RefProjectFile : UniqueReferences)
			{
				ProjectReferencesText += GetProjectReferenceText(ProjectFile.SourceDirectory, RefProjectFile->AssemblyName, RefProjectFile->ProjectFilePath, RefProjectFile->ProjectFileGuid);
			}
		}

		ModuleProjectContents = ModuleProjectContents.Replace(TEXT("%PROJECTREFERENCES%"), *ProjectReferencesText);
	}

	GeneratedFileManager.SaveFileIfChanged(ProjectFile.ProjectFilePath, ModuleProjectContents);
}

FString FMonoScriptCodeGenerator::GetProjectReferenceText(const FString& ReferencerProjectDirectory, const FString& ReferenceeAssemblyName, const FString& ReferenceeProjectPath, const FGuid& ReferenceeProjectGuid) const
{
	FString PlatformReferenceeProjectPath = ReferenceeProjectPath;
	FPaths::MakePathRelativeTo(PlatformReferenceeProjectPath, *(ReferencerProjectDirectory +TEXT("/")));
	FPaths::MakePlatformFilename(PlatformReferenceeProjectPath);

	return FString::Printf(TEXT("<ProjectReference Include=\"%s\"><Project>%s</Project><Name>%s</Name><Private>False</Private></ProjectReference>"), *PlatformReferenceeProjectPath, *ReferenceeProjectGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces), *ReferenceeAssemblyName);
}
 
void FMonoScriptCodeGenerator::LogUnhandledProperties() const
{
	if (UnhandledProperties.Num() || UnhandledParameters.Num() || UnhandledReturnValues.Num())
	{
		TSet<FName> UnhandledPropertyTypes;
		TArray<FName> Keys;
		UnhandledProperties.GenerateKeyArray(Keys);
		UnhandledPropertyTypes.Append(Keys);
		UnhandledParameters.GenerateKeyArray(Keys);
		UnhandledPropertyTypes.Append(Keys);
		UnhandledReturnValues.GenerateKeyArray(Keys);
		UnhandledPropertyTypes.Append(Keys);
		UnhandledOverridableParameters.GenerateKeyArray(Keys);
		UnhandledPropertyTypes.Append(Keys);
		UnhandledOverridableReturnValues.GenerateKeyArray(Keys);
		UnhandledPropertyTypes.Append(Keys);

		UE_LOG(LogMonoScriptGenerator, Log, TEXT("========== Unhandled UProperty Counts =========="));
		for (FName PropertyClassName : UnhandledPropertyTypes)
		{
			UE_LOG(LogMonoScriptGenerator, Log, TEXT("%s: %d props, %d params, %d returns, %d overridable params, %d overridable returns"),
				*PropertyClassName.ToString(), 
				UnhandledProperties.FindRef(PropertyClassName), 
				UnhandledParameters.FindRef(PropertyClassName), 
				UnhandledReturnValues.FindRef(PropertyClassName),
				UnhandledOverridableParameters.FindRef(PropertyClassName),
				UnhandledOverridableReturnValues.FindRef(PropertyClassName));
		}
	}
}

//MUST BE IN SYNC : MonoUE.Core.props, MonoRuntime.Plugin.cs, MonoMainDomain.cpp, MonoRuntimeStagingRules.cs, MonoScriptCodeGenerator.cpp, and IDE extensions
FString FMonoScriptCodeGenerator::GetAssemblyDirectory(const FString &RootDirectory, const EBuildConfigurations::Type Configuration, const FString &InPlatformName, const FString &TargetName)
{
	const TCHAR* ConfigSuffix = NULL;
	switch (Configuration)
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

	FString Name;
	if (TargetName == TEXT("Editor"))
	{
		Name = TEXT("MonoEditor") + FString(ConfigSuffix);
	}
	else if(TargetName == TEXT("Server"))
	{
		Name = TEXT("MonoServer") + FString(ConfigSuffix);
	}
	else if(TargetName == TEXT("Client"))
	{
		Name = TEXT("MonoClient") + FString(ConfigSuffix);
	}
	else
	{
		Name = TEXT("Mono") + FString(ConfigSuffix);
	}

	return FPaths::Combine(*RootDirectory, TEXT("Binaries"), *InPlatformName, *Name);
}
