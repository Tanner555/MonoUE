// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoBindings.h"

#include "MonoRuntimeCommon.h"
#include "MonoHelpers.h"
#include "MonoAssemblyMetadata.h"
#include "MonoCompiledClassAsset.h"
#include "MonoUnrealClass.h"
#include "MonoMainDomain.h"
#include "MonoPropertyFactory.h"

#include "Logging/MessageLog.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#endif // WITH_EDITOR

#include "MonoBindingsDerived.inl"
#include "../../MonoScriptGenerator/Private/MapModuleName.inl"

#define LOCTEXT_NAMESPACE "MonoRuntime"

struct FObjectInitializerWrapper
{
	const UObject* NativeObject;
	const FObjectInitializer* NativePointer;
};

static FString GetBuiltinModuleBindingsAssemblyName()
{
	return FString(MONO_UE4_NAMESPACE ".BuiltinModules");
}

static void GatherAlreadyLoadedScriptPackages(TSet<FName>& AlreadyLoadedScriptPackages)
{
	// we only want actual UClasses, not any blueprint ones
	for (TObjectIterator<UClass> ClassIt(RF_ClassDefaultObject, false); ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		UPackage* Package = Class->GetTypedOuter<UPackage>();
		check(Package);

		AlreadyLoadedScriptPackages.Add(Package->GetFName());
	}
}

namespace Mono
{
	// TODO: could make an easier way to marshal structs, but this'll do for now
	template<>
	struct Marshal<FObjectInitializerWrapper, void>
	{
		static inline void* Parameter(const FMonoBindings& Bindings, const FObjectInitializerWrapper& ObjectInitializerW)
		{
			return const_cast<FObjectInitializerWrapper*>(&ObjectInitializerW);
		}

		static inline bool IsValidParameterType(MonoType* typ)
		{
			return 0 == FCStringAnsi::Strcmp(mono_type_get_name(typ), MONO_BINDINGS_NAMESPACE ".ObjectInitializer");
		}
	};
}

extern void AddUnrealObjectInternalCalls();

//////////////////////////////////////////////////////////////////////////
/// CachedUnrealClass
FMonoBindings::CachedUnrealClass::CachedUnrealClass()
	: Class(nullptr)
	, WrapperClass(nullptr)
	, NativeWrapperConstructor(nullptr)
{

}

bool FMonoBindings::CachedUnrealClass::Resolve(const FCachedAssembly& CachedAssembly, MonoClass* ManagedClass, MonoClass* ManagedWrapperClass, UClass& UnrealClass)
{
	const FString ClassName = UnrealClass.GetName();

	Class = ManagedClass;
	WrapperClass = ManagedWrapperClass;
	if (nullptr == WrapperClass)
	{
		if (UnrealClass.HasAnyClassFlags(CLASS_Abstract))
		{
			// abstract classes should have a wrapper, this is an error
			FFormatNamedArguments Args;
			Args.Add(TEXT("ManagedClassName"), FText::FromString(FString::Printf(TEXT("%s.%s"), ANSI_TO_TCHAR(mono_class_get_namespace(ManagedClass)), ANSI_TO_TCHAR(mono_class_get_name(ManagedClass)))));
			Args.Add(TEXT("ClassName"), FText::FromString(ClassName));
			Args.Add(TEXT("WrapperClassName"), FText::FromString(FString::Printf(TEXT("%s.%s"), ANSI_TO_TCHAR(mono_class_get_namespace(ManagedClass)), *(ClassName + TEXT("_WrapperOnly")))));
			FMessageLog(NAME_MonoErrors).Warning(FText::Format(LOCTEXT("CouldNotFindWrapperClass", "Found managed class '{ManagedClassName}' for unreal class '{ClassName}', but it is abstract and did not have a wrapper class named '{WrapperClassName}'"), Args));
			return false;
		}
		WrapperClass = Class;
	}

	NativeWrapperConstructor = Mono::LookupMethodOnClass(WrapperClass, ":.ctor(intptr)");
	if (nullptr == NativeWrapperConstructor)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ManagedClassName"), FText::FromString(FString::Printf(TEXT("%s.%s"), ANSI_TO_TCHAR(mono_class_get_namespace(WrapperClass)), ANSI_TO_TCHAR(mono_class_get_name(WrapperClass)))));
		Args.Add(TEXT("ClassName"), FText::FromString(ClassName));
		FMessageLog(NAME_MonoErrors).Warning(FText::Format(LOCTEXT("CouldNotFindWrapperClassConstructor", "Found managed wrapper class 'ManagedClassName' for unreal class '{ClassName}', but it did not have a constructor of the form '{ClassName}(IntPtr)'"), Args));
		return false;
	}
	return true;

}

MonoObject* FMonoBindings::CachedUnrealClass::ConstructUnrealObjectWrapper(const FMonoBindings& InBindings, UObject& InObject) const
{
	check(WrapperClass);
	check(NativeWrapperConstructor);
	return Mono::ConstructObject(InBindings, WrapperClass, NativeWrapperConstructor, (PTRINT)&InObject);
}

//////////////////////////////////////////////////////////////////////////
/// DeferredUnrealClassCreationInfo
FMonoBindings::DeferredUnrealTypeCreationInfo::DeferredUnrealTypeCreationInfo(const TSharedPtr<struct FMonoAssemblyMetadata>& InAssemblyMetadata, 
																 const FMonoTypeReferenceMetadata& InTypeReferenceMetadata, 
																 const TSharedPtr<FCachedAssembly>& InCachedAssembly,
																 const FString& InCleanMetaDataFileName)
	: AssemblyMetadata(InAssemblyMetadata)
	, TypeReferenceMetadata(&InTypeReferenceMetadata)
	, ScriptPackage(nullptr)
	, CachedAssembly(InCachedAssembly)
	, CleanMetadataFileName(InCleanMetaDataFileName)
	, CreatedType(nullptr)
	, ResolveCount(0)
{

}

FMonoBindings::DeferredUnrealTypeCreationInfo::~DeferredUnrealTypeCreationInfo()
{
}

void FMonoBindings::DeferredUnrealTypeCreationInfo::AssociateWithScriptPackage(UPackage& InScriptPackage)
{
	check(nullptr == ScriptPackage);
	ScriptPackage = &InScriptPackage;
}

UField* FMonoBindings::DeferredUnrealTypeCreationInfo::Resolve(FMonoBindings& InBindings)
{
	++ResolveCount;

	if (nullptr == CreatedType)
	{
		check(ResolveCount == 1);

		CreateType(InBindings);
		check(CreatedType);
	}

	--ResolveCount;
	return CreatedType;
}

void FMonoBindings::DeferredUnrealClassCreationInfo::CreateType(FMonoBindings& InBindings)
{
	InBindings.CreateGameClass(*this);
}

void FMonoBindings::DeferredUnrealStructCreationInfo::CreateType(FMonoBindings& InBindings)
{
	InBindings.CreateGameStruct(*this);
}

void FMonoBindings::DeferredUnrealEnumCreationInfo::CreateType(FMonoBindings& InBindings)
{
	InBindings.CreateGameEnum(*this);
}


//////////////////////////////////////////////////////////////////////////
/// UnrealTypeReference
FMonoBindings::UnrealTypeReference::UnrealTypeReference()
	: UnrealType(nullptr)
{

}

FMonoBindings::UnrealTypeReference::UnrealTypeReference(UField& InUnrealType)
	: UnrealType(&InUnrealType)
{
}

FMonoBindings::UnrealTypeReference::UnrealTypeReference(const TSharedPtr<DeferredUnrealTypeCreationInfo>& DeferredCreation)
	: UnrealType(nullptr)
	, DeferredCreationInfo(DeferredCreation)
{
}

UField* FMonoBindings::UnrealTypeReference::Resolve(FMonoBindings& InBindings)
{
	if (nullptr == UnrealType && DeferredCreationInfo.IsValid())
	{
		UnrealType = DeferredCreationInfo->Resolve(InBindings);
		if (!DeferredCreationInfo->IsResolving())
		{
			DeferredCreationInfo.Reset();
		}
	}
	return UnrealType;
}

FMonoBindings::MonoRuntimeState::MonoRuntimeState()
	: BindingsGCHandle(0)
	, NameClass(nullptr)
	, LifetimeReplicatedPropertyClass(nullptr)
	, LoadAssemblyMethod(nullptr)
	, FindUnrealClassesInAssemblyMethod(nullptr)
	, GetLifetimeReplicationListMethod(nullptr)
	, GetCustomReplicationListMethod(nullptr)
	, ExceptionCount(0)
{

}

FMonoBindings::MonoRuntimeState::MonoRuntimeState(MonoRuntimeState&& Other)
	: BindingsGCHandle(0)
	, NameClass(nullptr)
	, LifetimeReplicatedPropertyClass(nullptr)
	, LoadAssemblyMethod(nullptr)
	, FindUnrealClassesInAssemblyMethod(nullptr)
	, GetLifetimeReplicationListMethod(nullptr)
	, GetCustomReplicationListMethod(nullptr)
	, ExceptionCount(0)
{
	*this = MoveTemp(Other);
}

FMonoBindings::MonoRuntimeState::~MonoRuntimeState()
{
	mono_gchandle_free(BindingsGCHandle);
}

FMonoBindings::MonoRuntimeState& FMonoBindings::MonoRuntimeState::operator=(MonoRuntimeState&& Other)
{
	if (&Other != this)
	{
		MonoBindingsAssembly = Other.MonoBindingsAssembly;
		Other.MonoBindingsAssembly.Reset();
		MonoRuntimeAssembly = Other.MonoRuntimeAssembly;
		Other.MonoRuntimeAssembly.Reset();
		Exchange(AllAssemblies, Other.AllAssemblies);
		Other.AllAssemblies.Empty();
		Exchange(ScriptPackageToBindingsAssemblyMap, Other.ScriptPackageToBindingsAssemblyMap);
		Other.ScriptPackageToBindingsAssemblyMap.Empty();
		Exchange(NativeWrapperMap, Other.NativeWrapperMap);
		Other.NativeWrapperMap.Empty();
		Exchange(MonoTypeToUnrealTypeMap, Other.MonoTypeToUnrealTypeMap);
		Other.MonoTypeToUnrealTypeMap.Empty();
		BindingsGCHandle = Other.BindingsGCHandle;
		Other.BindingsGCHandle = 0;
		NameClass = Other.NameClass;
		Other.NameClass = nullptr;
		LifetimeReplicatedPropertyClass = Other.LifetimeReplicatedPropertyClass;
		Other.LifetimeReplicatedPropertyClass = nullptr;
		LoadAssemblyMethod = Other.LoadAssemblyMethod;
		Other.LoadAssemblyMethod = nullptr;
		FindUnrealClassesInAssemblyMethod = Other.FindUnrealClassesInAssemblyMethod;
		Other.FindUnrealClassesInAssemblyMethod = nullptr;
		GetLifetimeReplicationListMethod = Other.GetLifetimeReplicationListMethod;
		Other.GetLifetimeReplicationListMethod = nullptr;
		GetCustomReplicationListMethod = Other.GetCustomReplicationListMethod;
		Other.GetCustomReplicationListMethod = nullptr;
		MonoObjectTable = MoveTemp(Other.MonoObjectTable);
		Exchange(MonoClasses, Other.MonoClasses);
		Other.MonoClasses.Empty();
#if MONO_WITH_HOT_RELOADING
		Exchange(MonoStructs, Other.MonoStructs);
		Other.MonoStructs.Empty();
		Exchange(MonoEnums, Other.MonoEnums);
		Other.MonoEnums.Empty();
#endif
		ExceptionCount = Other.ExceptionCount;
		Other.ExceptionCount = 0;
	}
	return *this;

}

#if MONO_WITH_HOT_RELOADING

FMonoBindings::ReloadEnum::ReloadEnum(UEnum* InOldEnum)
	: TReloadType<UEnum>(InOldEnum)
{
}

void FMonoBindings::ReloadEnum::MoveToTransientPackage()
{
	TReloadType<UEnum>::MoveToTransientPackage();
	FixEnumNames();
}

void FMonoBindings::ReloadEnum::CancelReload()
{
	TReloadType<UEnum>::CancelReload();
	FixEnumNames();
}

void FMonoBindings::ReloadEnum::FixEnumNames()
{
	TArray<TPair<FName, int64>> Names;
	for (int i = 0; i < GetOldType()->NumEnums() - 1; ++i)
	{
		FString OldName = GetOldType()->GetNameStringByIndex(i);
		FName Name = *FString::Printf(TEXT("%s::%s"), *GetOldType()->GetName(), *OldName);
		Names.Add(TPairInitializer<FName, int64>(Name, (int64)i));
	}
	GetOldType()->SetEnums(Names, UEnum::ECppForm::Namespaced);
}

void FMonoBindings::ReloadEnum::FinishReload()
{
	GetOldType()->RemoveFromRoot();
}

FMonoBindings::ReloadClass::ReloadClass(UMonoUnrealClass* InOldClass)
: TReloadType<UMonoUnrealClass>(InOldClass)
, PreviousCDOFlags(RF_NoFlags)
{
	TArray<UClass*> ChildrenOfClass;
	GetDerivedClasses(GetOldType(), ChildrenOfClass);
	ChildCount = ChildrenOfClass.Num();

	// reset the deleted flag in case class is re-added
	GetOldType()->SetDeletedDuringHotReload(false);
}

void FMonoBindings::ReloadClass::InternalMoveToTransientPackage(UMonoUnrealClass& InType, const TCHAR* Prefix)
{
	UObject* DefaultObject = InType.GetDefaultObject();
	DefaultObject->ClearFlags(GARBAGE_COLLECTION_KEEPFLAGS | RF_Public);
	DefaultObject->RemoveFromRoot ();
	TArray<UObject*> ChildObjects;
	//Mono classes and all heir sub-properties get put in the RootSet by
	//Obj.cpp's MarkObjectsToDisregardForGC(), this causes the "Old" classes to not get GCed.
	//Additionally, all CPP UProperties are constructed with RF_Native. This is a GARBAGE_COLLECTION_KEEP_FLAG
	//And also prevents duplicate classes from being GCed. Here we remove these flags.
	FReferenceFinder CDOReferences(ChildObjects, InType.GetDefaultObject(), false, false, true);
	CDOReferences.FindReferences(InType.GetDefaultObject());
	for (UObject* Obj : ChildObjects)
	{
		Obj->ClearFlags(GARBAGE_COLLECTION_KEEPFLAGS);
		Obj->RemoveFromRoot();
	}
	TReloadType<UMonoUnrealClass>::InternalMoveToTransientPackage(InType, Prefix);
}
void FMonoBindings::ReloadClass::MoveToTransientPackage()
{
	PreviousCDOFlags = (GetOldType()->GetDefaultObject()->GetFlags()) | (RF_Standalone | RF_Public);

	TReloadType<UMonoUnrealClass>::MoveToTransientPackage();
	
}
int32 FMonoBindings::ReloadClass::GetChildCount() const
{
	return ChildCount;
}

void FMonoBindings::ReloadClass::CancelReload()
{
	// Cache this value, since the base class version will clear it.
	bool bIsReinstancedType = IsReinstancedType();
	if (bIsReinstancedType)
	{
		UObject* DefaultObject = GetOldType()->GetDefaultObject();
		DefaultObject->ClearFlags(RF_Standalone | RF_Public);
		DefaultObject->RemoveFromRoot();
	}

	TReloadType<UMonoUnrealClass>::CancelReload();

	if (bIsReinstancedType)
	{
		GetOldType()->GetDefaultObject()->SetFlags(PreviousCDOFlags);
	}
}
void FMonoBindings::ReloadClass::FinishReload(TArray<UObject*>& ExistingManagedObjects)
{
	if (IsReinstancedType())
	{
		GetOldType()->RemoveFromRoot();

		// remove any re-instanced classes from managed object tracking, they will be added 
		UMonoUnrealClass* TempOldClass = GetOldType();
		ExistingManagedObjects.RemoveAllSwap([TempOldClass](UObject* Object) { return Object->IsA(TempOldClass); });

		TSet<UObject*> WasAlreadyInRootSet;

		WasAlreadyInRootSet.Empty(ExistingManagedObjects.Num());

		// reinstancing calls GC, so protect existing managed objects from GC
		for (auto Object : ExistingManagedObjects)
		{
			if (Object->IsRooted ())
			{
				WasAlreadyInRootSet.Add(Object);
			}
			else
			{
				Object->AddToRoot();
			}
		}

		FCoreUObjectDelegates::RegisterClassForHotReloadReinstancingDelegate.Broadcast(GetOldType(), GetNewType());

		// reset root set state
		for (auto Object : ExistingManagedObjects)
		{
			if (!WasAlreadyInRootSet.Contains(Object))
			{
				Object->RemoveFromRoot();
			}
		}

	}	
}

#endif // MONO_WITH_HOT_RELOADING

// Track our one and only instance of FMonoBindings
FMonoBindings* FMonoBindings::GInstance=nullptr;

FMonoBindings::FMonoBindings(FMonoMainDomain& InMainDomain, const FString& InEngineAssemblyDirectory, const FString& InGameAssemblyDirectory)
	: FMonoDomain(Mono::InvokeExceptionBehavior::OutputToMessageLog)
	, MainDomain(InMainDomain)
	, EngineAssemblyDirectory(InEngineAssemblyDirectory)
	, GameAssemblyDirectory(InGameAssemblyDirectory)
#if MONO_WITH_HOT_RELOADING
	, CurrentReloadContext(nullptr)
	, HotReloadCommand(TEXT("MonoRuntime.HotReload"),
						TEXT("Reload all game assemblies on the fly."),
						FConsoleCommandDelegate::CreateRaw(this, &FMonoBindings::ReloadDomainCommand))
#endif // MONO_WITH_HOT_RELOADING
{
	check(nullptr == GInstance);
	GInstance = this;

#if WITH_EDITOR
	BuildMissingAssemblies();
#endif  
	FMessageLog(NAME_MonoErrors).NewPage(LOCTEXT("MonoErrorsLabel", "Mono Runtime Errors"));

	TArray<FMonoLoadedAssemblyMetadata> EngineAssemblies;
	TArray<FMonoLoadedAssemblyMetadata> GameAssemblies;

	if (FMonoLoadedAssemblyMetadata::LoadAssemblyMetadataInDirectory(EngineAssemblies, EngineAssemblyDirectory) && (InEngineAssemblyDirectory != InGameAssemblyDirectory))
	{
		FMonoLoadedAssemblyMetadata::LoadAssemblyMetadataInDirectory(GameAssemblies, GameAssemblyDirectory);
	}
	InitializeDomain(EngineAssemblies, GameAssemblies);
}

FMonoBindings::~FMonoBindings()
{
	check(this == GInstance);
	GInstance = nullptr;
}

FMonoBindings& FMonoBindings::Get()
{
	check(GInstance);
	return *GInstance;
}

void FMonoBindings::ShowAnyErrorsOrWarnings()
{
	check(IsInGameThread());
	FMessageLog(NAME_MonoErrors).Open(EMessageSeverity::Warning);
}

void FMonoBindings::OnExceptionSentToMessageLog()
{
	check(IsInGameThread());
	if (RuntimeState.ExceptionCount == 0)
	{
		ShowAnyErrorsOrWarnings();
	}
	RuntimeState.ExceptionCount++;
}


#if MONO_WITH_HOT_RELOADING
bool FMonoBindings::ReloadDomain()
{
	// Force a GC to remove any pending kill objects from our object map
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	check(GetDomain() != nullptr);
	check(CurrentReloadContext == nullptr);

	FMessageLog(NAME_MonoErrors).NewPage(LOCTEXT("MonoErrorsLabel", "Mono Runtime Errors"));

	TArray<FMonoLoadedAssemblyMetadata> EngineAssemblies;
	TArray<FMonoLoadedAssemblyMetadata> GameAssemblies;

	if (!FMonoLoadedAssemblyMetadata::LoadAssemblyMetadataInDirectory(EngineAssemblies, EngineAssemblyDirectory))
	{
		ShowAnyErrorsOrWarnings();
		return false;
	}

	if (!FMonoLoadedAssemblyMetadata::LoadAssemblyMetadataInDirectory(GameAssemblies, GameAssemblyDirectory))
	{
		ShowAnyErrorsOrWarnings();
		return false;
	}

	const bool bReinstancing = HotReloadRequiresReinstancing(EngineAssemblies) || HotReloadRequiresReinstancing(GameAssemblies);

	// see if any classes require re instancing before beginning the hot reload, so we can shut down simulation/PIE if it is running
	if (bReinstancing)
	{
		StopPIEForHotReloadEvent.Broadcast();
	}

	bool bHotReloadSuccess = true;

	{
		ReloadContext Context;
		CurrentReloadContext = &Context;
		BeginReload(Context, bReinstancing);

		bHotReloadSuccess = InitializeDomain(EngineAssemblies, GameAssemblies);

		if (bHotReloadSuccess)
		{
			bHotReloadSuccess = InitializeMonoClasses();
		}

		if (bHotReloadSuccess)
		{
			EndReload(Context);
			// TODO: unload the previous domain here
		}
		else
		{
			// hot reload failed, restore cached domain
			CancelReload(Context);
		}

		CurrentReloadContext = nullptr;
	}

	// always fire the hot reload event
	HotReloadEvent.Broadcast(bHotReloadSuccess);

	// clean up
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	
	
	for (TObjectIterator<UClass> ClassIter; ClassIter; ++ClassIter)
	{
		UClass *Class = *ClassIter;
		if (Class->GetName().StartsWith("MONOHOTRELOAD_") || Class->GetName().StartsWith("REINST_"))
		{
			UE_LOG(LogMono, Log, TEXT("Residual Class: %s"), *Class->GetName());
			// This is still firing in ShooterGame, re-enable once fixed.
//			checkNoEntry();
		}
	}

	for (TObjectIterator<UScriptStruct> StructIter; StructIter; ++StructIter)
	{
		UScriptStruct *Struct = *StructIter;
		if (Struct->GetName().StartsWith("MONOHOTRELOAD_") || Struct->GetName().StartsWith("REINST_"))
		{
			UE_LOG(LogMono, Log, TEXT("Residual Struct: %s"), *Struct->GetName());
			// This is still firing in ShooterGame, re-enable once fixed.
			//			checkNoEntry();
		}
	}

	for (TObjectIterator<UEnum> EnumIter; EnumIter; ++EnumIter)
	{
		UEnum *Enum = *EnumIter;
		if (Enum->GetName().StartsWith("MONOHOTRELOAD_") || Enum->GetName().StartsWith("REINST_"))
		{
			UE_LOG(LogMono, Log, TEXT("Residual Enum: %s"), *Enum->GetName());
			// This is still firing in ShooterGame, re-enable once fixed.
			//			checkNoEntry();
		}
	}


	if (!bHotReloadSuccess)
	{
		ShowAnyErrorsOrWarnings();
	}
	return bHotReloadSuccess;
}

// Blueprint reinstancing hack
// When blueprint recompiles it will duplicate CDOs using *old* classes, which are implemented in the old domain.
// This is a UE4 bug, since even with native hot reloading this will lead to executing *old* class ObjectInitializer constructors.
//
// It's not a trivial fix (might require a large rewrite of the Blueprint compile path), and until Epic addresses it, we
// hack around it here by allowing the old domain to be swapped back
void FMonoBindings::HACK_SetOldDomainAsCurrent()
{
	check(CurrentReloadContext != nullptr);
	if (CurrentReloadContext->HACK_DomainInMonoBindings != HACK_CurrentActiveDomain::OldDomain)
	{
		Exchange(CurrentReloadContext->CachedRuntimeState, RuntimeState);
		MonoDomain* OldDomain = CurrentReloadContext->CachedPreviousDomain;
		CurrentReloadContext->CachedPreviousDomain = GetDomain();
		SetDomain(OldDomain);

		CurrentReloadContext->HACK_DomainInMonoBindings = HACK_CurrentActiveDomain::OldDomain; \
	}
}

void FMonoBindings::HACK_SetNewDomainAsCurrent()
{
	check(CurrentReloadContext != nullptr);
	if (CurrentReloadContext->HACK_DomainInMonoBindings != HACK_CurrentActiveDomain::NewDomain)
	{
		Exchange(CurrentReloadContext->CachedRuntimeState, RuntimeState);
		MonoDomain* OldDomain = CurrentReloadContext->CachedPreviousDomain;
		CurrentReloadContext->CachedPreviousDomain = GetDomain();
		SetDomain(OldDomain);

		CurrentReloadContext->HACK_DomainInMonoBindings = HACK_CurrentActiveDomain::NewDomain;
	}
}

void FMonoBindings::BeginReload(ReloadContext& Context, bool bReinstancing)
{
	check(IsInGameThread());
	RuntimeState.MonoObjectTable.ResetForReload();

	// cache off runtime state
	Context.CachedRuntimeState = MoveTemp(RuntimeState);

	// Track managed objects so we can recreate their companions
	Context.CachedRuntimeState.MonoObjectTable.GetObjectsWithCompanions(Context.ManagedObjects);

	for (auto Pair : Context.CachedRuntimeState.MonoStructs)
	{
		UScriptStruct* MonoUnrealStruct = Pair.Key;
		Context.ReloadStructs.Add(ReloadStruct(MonoUnrealStruct));
	}

	for (auto Pair : Context.CachedRuntimeState.MonoEnums)
	{
		UEnum *MonoUnrealEnum = Pair.Key;
		Context.ReloadEnums.Add(ReloadEnum(MonoUnrealEnum));
	}

	for (auto MonoUnrealClass : Context.CachedRuntimeState.MonoClasses)
	{
		// if the class was deleted for a hot reload, search for any instances of it and consider them managed objects
		// since it might be re-added
		if (MonoUnrealClass->WasDeletedDuringHotReload())
		{
			TArray<UObject*> ObjectsOfClass;
			GetObjectsOfClass(MonoUnrealClass, ObjectsOfClass, false, RF_NoFlags);

			for (auto Object : ObjectsOfClass)
			{
				Context.ManagedObjects.AddUnique(Object);
			}
		}
		Context.ReloadClasses.Add(ReloadClass(MonoUnrealClass));
	}

#if !MONOUE_STANDALONE
	// if we're not reinstancing (i.e. game may be running), track any actors with input enabled
	if (!bReinstancing)
	{
		for (auto Object : Context.ManagedObjects)
		{
			if (!Object->IsTemplate())
			{
				AActor* Actor = Cast<AActor>(Object);
				if (nullptr != Actor && nullptr != Actor->InputComponent)
				{
					UMonoUnrealClass& UnrealClass = UMonoUnrealClass::GetMonoUnrealClassFromClass(Actor->GetClass());
					if (UnrealClass.HasInputDelegateBindings(Object))
					{
						Context.BoundInputActors.Add(Actor);
					}
				}
			}
		}
	}
#endif

	Context.CachedPreviousDomain = GetDomain();
	SetDomain(nullptr);
	Context.HACK_DomainInMonoBindings = HACK_CurrentActiveDomain::NewDomain;
}

void FMonoBindings::EndReload(ReloadContext& Context)
{
	// we know the hot reload succeeded, unregister all of the old object delegates. Input will be rebound below
	// This will cause their bound UE4 delegates to be null (since the UE4 delegates hold on to weak pointers), so they won't be called anymore
	// We could go through all input bindings and compact their delegate arrays but that seems like overkill
	Context.CachedRuntimeState.MonoObjectTable.UnregisterAllObjectDelegates();

	check(Context.HACK_DomainInMonoBindings == HACK_CurrentActiveDomain::NewDomain);

	TSet<UMonoUnrealClass*> DeletedClasses;

	//By this point all Mono UClasses have been duplicated and appropriately wired up to super-structs. However descendant blueprint
	//classes have not. The engine's blueprint reinstancer doesn't know how to handle multiple levels of inheritance. It only knows
	//how to connect a single level of inheritance's UClasses to their new parent classes. The BlueprintReinstancer handles duplicating
	//UClasses, connecting them to parents, and reinstancing all in the same function call.
	//
	//The blueprint reinstancer reinstances all recursively descendant blueprint objects. Not just the ones immediately descendant of
	//the class being reinstanced. So if we reinstance a Mono superclass first. It's descendant blueprint UClasses haven't been reconnected
	//to their new descendant Mono classes. To avoid this, we sort classes into a child first order. By reinstancing children first we 
	//make sure that all descendant Mono classes's descendant blueprints have been properly duplicated and connected to the new Mono UClasses.
	//
	//We sort by the recursively counted number of descendant UClasses to ensure that children are reinstanced first.
	Context.ReloadClasses.Sort([](const ReloadClass &a, const ReloadClass &b) { return a.GetChildCount() < b.GetChildCount(); });

	for (auto&& PreviousClass : Context.ReloadClasses)
	{
		// note: reinstanced classes are removed from the ManagedObjects array, they were re-added to the object table when reinstanced
		PreviousClass.FinishReload(Context.ManagedObjects);

		// see if this class was deleted (only if it wasn't reinstanced)
		if (!PreviousClass.IsReinstancedType()
			&& !RuntimeState.MonoClasses.Contains(PreviousClass.GetOldType()))
		{
			UMonoUnrealClass* MonoUnrealClass = PreviousClass.GetOldType();
			// this was a deleted class, mark it deleted, re-add it
			// right now we re-add it even if the only managed object was its CDO
			// This avoids searching for any kind of references (class references, derived classes)
			// We could search for those refs and if the only one is the CDO, move the class to the transient package/from root
			// But that doesn't seem worth the complexity
			UE_LOG(LogMono, Log, TEXT("Class %s was deleted during hot reload."), *MonoUnrealClass->GetPathName());
			MonoUnrealClass->SetDeletedDuringHotReload(true);
			RuntimeState.MonoClasses.Add(MonoUnrealClass);
			DeletedClasses.Add(MonoUnrealClass);
		}

	}

	for (auto&& PreviousEnum : Context.ReloadEnums)
	{
		PreviousEnum.FinishReload();
	}

	FCoreUObjectDelegates::ReinstanceHotReloadedClassesDelegate.Broadcast();

	check(Context.HACK_DomainInMonoBindings == HACK_CurrentActiveDomain::NewDomain);

	for (auto&& Object : Context.ManagedObjects)
	{
		UMonoUnrealClass& UnrealClass = UMonoUnrealClass::GetMonoUnrealClassFromClass(Object->GetClass());

		if (!DeletedClasses.Contains(&UnrealClass))
		{
			const FMonoCompiledClassAsset& ClassAsset = UnrealClass.GetCompiledClassAsset();

			MonoClass* AssetClass = ClassAsset.GetAssetClass();
			check(AssetClass);

			MonoMethod* AssetNativeConstructor = ClassAsset.GetAssetNativeConstructor();

			MonoObject* CompanionObject = Mono::ConstructObject(*this, AssetClass, AssetNativeConstructor, (PTRINT)Object);

			RuntimeState.MonoObjectTable.AddCompanionObject(*Object, CompanionObject);

			AActor* Actor = Cast<AActor>(Object);

#if !MONOUE_STANDALONE
			if (nullptr != Actor && nullptr != Actor->InputComponent && Context.BoundInputActors.Contains(Actor))
			{
				// rebind input
				UnrealClass.BindInputDelegates(Object);
			}
#endif
		}
		else
		{
			// don't add objects of a deleted class to managed object table, it's no longer a managed object
		}
	}

}

void FMonoBindings::CancelReload(ReloadContext& Context)
{
	check(Context.HACK_DomainInMonoBindings == HACK_CurrentActiveDomain::NewDomain);

	// TODO: unload newly created domain here
	RuntimeState = MoveTemp(Context.CachedRuntimeState);
	SetDomain(Context.CachedPreviousDomain);

	for (auto&& PreviousStruct : Context.ReloadStructs)
	{
		PreviousStruct.CancelReload();
	}

	for (auto&& PreviousClass : Context.ReloadClasses)
	{
		PreviousClass.CancelReload();
	}
}

void FMonoBindings::RenamePreviousStruct(UScriptStruct& OldStruct)
{
	check(IsReloading());

	// A struct has been changed enough to require reinstancing
	bool bFoundOne = false;
	for (auto&& PreviousStruct : CurrentReloadContext->ReloadStructs)
	{
		if (PreviousStruct.GetOldType() == &OldStruct)
		{
			PreviousStruct.MoveToTransientPackage();
			bFoundOne = true;
			break;
		}
	}

	checkf(bFoundOne, TEXT("Failed to find reloaded struct %s"), *OldStruct.GetPathName())
}

void FMonoBindings::DeferStructReinstance(UScriptStruct& OldStruct, UScriptStruct& NewStruct)
{
	check(IsReloading());

	bool bFoundOne = false;
	for (auto&& PreviousStruct : CurrentReloadContext->ReloadStructs)
	{
		if (PreviousStruct.GetOldType() == &OldStruct)
		{
			PreviousStruct.SetNewType(NewStruct);
			bFoundOne = true;
			break;
		}
	}

	checkf(bFoundOne, TEXT("Failed to find reloaded struct %s"), *OldStruct.GetPathName())

}

void FMonoBindings::RenamePreviousClass(UMonoUnrealClass& OldClass)
{
	check(IsReloading());

	// A class has been changed enough to require reinstancing
	bool bFoundOne = false;
	for (auto&& PreviousClass : CurrentReloadContext->ReloadClasses)
	{
		if (PreviousClass.GetOldType() == &OldClass)
		{
			PreviousClass.MoveToTransientPackage();
			bFoundOne = true;
			break;
		}
	}

	checkf(bFoundOne, TEXT("Failed to find reloaded class %s"), *OldClass.GetPathName())
}

void FMonoBindings::DeferClassReinstance(UMonoUnrealClass& OldClass, UMonoUnrealClass& NewClass)
{
	check(IsReloading());

	bool bFoundOne = false;
	for (auto&& PreviousClass : CurrentReloadContext->ReloadClasses)
	{
		if (PreviousClass.GetOldType() == &OldClass)
		{
			PreviousClass.SetNewType(NewClass);
			bFoundOne = true;
			break;
		}
	}

	checkf(bFoundOne, TEXT("Failed to find reloaded class %s"), *OldClass.GetPathName())

}


void FMonoBindings::RenamePreviousEnum(UEnum& OldEnum)
{
	check(IsReloading());

	// A Enum has been changed enough to require reinstancing
	bool bFoundOne = false;
	for (auto&& PreviousEnum : CurrentReloadContext->ReloadEnums)
	{
		if (PreviousEnum.GetOldType() == &OldEnum)
		{
			PreviousEnum.MoveToTransientPackage();
			bFoundOne = true;
			break;
		}
	}

	checkf(bFoundOne, TEXT("Failed to find reloaded Enum %s"), *OldEnum.GetPathName())
}

void FMonoBindings::DeferEnumReinstance(UEnum& OldEnum, UEnum& NewEnum)
{
	check(IsReloading());

	bool bFoundOne = false;
	for (auto&& PreviousEnum : CurrentReloadContext->ReloadEnums)
	{
		if (PreviousEnum.GetOldType() == &OldEnum)
		{
			PreviousEnum.SetNewType(NewEnum);
			bFoundOne = true;
			break;
		}
	}

	checkf(bFoundOne, TEXT("Failed to find reloaded Enum %s"), *OldEnum.GetPathName())

}

bool FMonoBindings::HotReloadRequiresReinstancing(const TArray<FMonoLoadedAssemblyMetadata>& NewMetadata)
{
	bool bAnyRequireReinstancing = false;

	for (auto&& LoadedMetadata : NewMetadata)
	{
		check(LoadedMetadata.AssemblyMetadata.IsValid());
		for (auto&& ClassMetadata : LoadedMetadata.AssemblyMetadata->Classes)
		{
			UClass* UnrealClass = GetUnrealClassFromTypeReference(ClassMetadata);

			if (nullptr != UnrealClass)
			{
				UMonoUnrealClass* MonoUnrealClass = GetMonoUnrealClass(UnrealClass);
				if (nullptr != MonoUnrealClass)
				{
					if (MonoUnrealClass->GetClassHash() != ClassMetadata.ClassHash)
					{
						// see if any non-template instances of this class actually exist
						TArray<UObject*> Objects;
						GetObjectsOfClass(MonoUnrealClass, Objects);

						if (Objects.Num() > 0)
						{
							bAnyRequireReinstancing = true;
							break;
						}
					}
				}
			}
		}
	}

	return bAnyRequireReinstancing;
}


void FMonoBindings::ReloadDomainCommand()
{
	if (!ReloadDomain())
	{
		UE_LOG(LogMono, Error, TEXT("Hot reload failed."));
	}
}

#endif // MONO_WITH_HOT_RELOADING

#if WITH_EDITOR
void FMonoBindings::BuildMissingAssemblies()
{
	if (FPaths::IsProjectFilePathSet())
	{
		// if this is a mono project
		if (FPaths::FileExists(FPaths::Combine(*FPaths::ProjectDir(), MONO_PROJECT_COOKIE_FILE_NAME)))
		{
			// TODO: This could be more robust. Right now we just build them if they don't exist.
			// Ideally we'd have some sort of cheap dependency check (i.e. version based like native modules, doing a full dependency check of source would be too expensive)
			TArray<FString> GameAssemblies;

			IFileManager::Get().FindFiles(GameAssemblies, *FPaths::Combine(*GameAssemblyDirectory, TEXT("*.json")), true, false);

			if (GameAssemblies.Num() == 0)
			{
				int32 Result = FPlatformMisc::MessageBoxExt(EAppMsgType::YesNoCancel, TEXT("Game assemblies are missing or out of date. Would you like to recompile them?"), TEXT("Question"));
				if (Result == EAppReturnType::Yes)
				{
					FFeedbackContext *Context = FDesktopPlatformModule::Get()->GetNativeFeedbackContext();

					Context->BeginSlowTask(LOCTEXT("GameAssembliesOutOfDate", "Game assemblies are out of date, recompiling..."), true, true);
					FText FailureReason;
					bool bBuildSuccess = IMonoRuntime::Get().GenerateProjectsAndBuildGameAssemblies(FailureReason, *Context);
					Context->EndSlowTask();

					if (!bBuildSuccess)
					{
						if (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *FString::Printf(TEXT("%s. Continue trying to start anyway?"), *FailureReason.ToString()), TEXT("Error")) == EAppReturnType::No)
						{
							FPlatformMisc::RequestExit(false);
						}
					}
				}
				else if (Result == EAppReturnType::Cancel)
				{
					FPlatformMisc::RequestExit(false);
				}
			}
		}
	}
}

#endif // WITH_EDITOR

bool FMonoBindings::InitializeDomain(const TArray<FMonoLoadedAssemblyMetadata>& EngineAssemblyMetadata, const TArray<FMonoLoadedAssemblyMetadata>& GameAssemblyMetadata)
{
	check(IsInGameThread());
	check(GetDomain() == nullptr);
#if MONO_WITH_HOT_RELOADING
	MonoDomain* GameDomain = MainDomain.CreateGameDomain();
#else
	// we shouldn't have a domain!
	MonoDomain* GameDomain = MainDomain.GetDomain();
#endif
	SetDomain(GameDomain);

	const FString MonoAssemblyName = ANSI_TO_TCHAR(MONO_BINDINGS_NAMESPACE);

	RuntimeState.MonoBindingsAssembly = MakeShareable(new FCachedAssembly());
#if MONOUE_STANDALONE
	if (!RuntimeState.MonoBindingsAssembly->Open(GetDomain(), MonoAssemblyName))
	{
		return false;
	}
#else
	verify(RuntimeState.MonoBindingsAssembly->Open(GetDomain(), MonoAssemblyName));
#endif
	RuntimeState.AllAssemblies.Add(MONO_BINDINGS_NAMESPACE, RuntimeState.MonoBindingsAssembly);

	// Ensure that type lookups work correctly for object properties of type UnrealObject.
	MonoClass* UnrealObjectClass = RuntimeState.MonoBindingsAssembly->GetClass(MONO_BINDINGS_NAMESPACE, "UnrealObject");
	check(UnrealObjectClass);
	RuntimeState.MonoTypeToUnrealTypeMap.Add(mono_class_get_type(UnrealObjectClass), UnrealTypeReference(*UObject::StaticClass()));

	RuntimeState.NameClass = RuntimeState.MonoBindingsAssembly->GetClass(MONO_BINDINGS_NAMESPACE, "Name");
	check(RuntimeState.NameClass);

	RuntimeState.LifetimeReplicatedPropertyClass = RuntimeState.MonoBindingsAssembly->GetClass(MONO_BINDINGS_NAMESPACE, "LifetimeReplicatedProperty");
	check(RuntimeState.LifetimeReplicatedPropertyClass);

	RuntimeState.LoadAssemblyMethod = RuntimeState.MonoBindingsAssembly->LookupMethod(MONO_BINDINGS_NAMESPACE ".Bindings:LoadAssembly");
	check(RuntimeState.LoadAssemblyMethod);

	RuntimeState.FindUnrealClassesInAssemblyMethod = RuntimeState.MonoBindingsAssembly->LookupMethod(MONO_BINDINGS_NAMESPACE ".Bindings:FindUnrealClassesInAssembly");
	check(RuntimeState.FindUnrealClassesInAssemblyMethod);

	RuntimeState.GetLifetimeReplicationListMethod = RuntimeState.MonoBindingsAssembly->LookupMethod(MONO_BINDINGS_NAMESPACE ".UnrealObject:GetLifetimeReplicationList");
	check(RuntimeState.GetLifetimeReplicationListMethod);

	RuntimeState.GetCustomReplicationListMethod = RuntimeState.MonoBindingsAssembly->LookupMethod(MONO_BINDINGS_NAMESPACE ".UnrealObject:GetCustomReplicationList");
	check(RuntimeState.GetCustomReplicationListMethod);

	MonoMethod* ClearNativePointerMethod = RuntimeState.MonoBindingsAssembly->LookupMethod(MONO_BINDINGS_NAMESPACE ".UnrealObject:ClearNativePointer");
	check(ClearNativePointerMethod);

	RuntimeState.MonoObjectTable.Initialize(*this, ClearNativePointerMethod);

	// Call our one time setup C# side
	MonoMethod* InitializeMethod = RuntimeState.MonoBindingsAssembly->LookupMethod(MONO_BINDINGS_NAMESPACE ".Bindings:Initialize");
	check(InitializeMethod);

	MonoObject* BindingsMonoObject = Mono::Invoke<MonoObject*>(*this,
		InitializeMethod,
		nullptr,
		IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*EngineAssemblyDirectory),
		IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*GameAssemblyDirectory));
	check(BindingsMonoObject);
	RuntimeState.BindingsGCHandle = mono_gchandle_new(BindingsMonoObject, false);

	// load the assembly which contains all built-in modules
	FString ErrorMessage;
#if MONOUE_STANDALONE
	if (!LoadAssembly(ErrorMessage, GetBuiltinModuleBindingsAssemblyName()).IsValid())
	{
		UE_LOG(LogMono, Error, TEXT("Failed to load bindings assembly %s: %s"), *GetBuiltinModuleBindingsAssemblyName(), *ErrorMessage);
		return false;
	}
#else
	verifyf(LoadAssembly(ErrorMessage, GetBuiltinModuleBindingsAssemblyName()).IsValid(), TEXT("Failed to load bindings assembly %s: %s"), *GetBuiltinModuleBindingsAssemblyName(), *ErrorMessage);
#endif

	// TODO: handle on the fly modules
	TSet<FName> AlreadyLoadedScriptPackages;
	GatherAlreadyLoadedScriptPackages(AlreadyLoadedScriptPackages);

	LoadBindingsForScriptPackages(AlreadyLoadedScriptPackages);

	bool bRet = LoadGameAssemblies(EngineAssemblyMetadata);

	if (bRet)
	{
		bRet = LoadGameAssemblies(GameAssemblyMetadata);
	}

	ShowAnyErrorsOrWarnings();

	return bRet;
}

FMonoBindings* FMonoBindings::CreateMonoBindings(FMonoMainDomain& InMainDomain, const FString& InEngineAssemblyDirectory, const FString& InGameAssemblyDirectory)
{
	// add UnrealObject internal calls
	AddUnrealObjectInternalCalls();

	return new FMonoBindings(InMainDomain, InEngineAssemblyDirectory, InGameAssemblyDirectory);
}

void FMonoBindings::ThrowUnrealObjectDestroyedException(const FString& Message)
{
	MonoException* Exception = RuntimeState.MonoBindingsAssembly->CreateExceptionByName(MONO_UE4_NAMESPACE MONO_BINDINGS_NAMESPACE, "UnrealObjectDestroyedException", Message);
	check(Exception);
	mono_raise_exception(Exception);
}

MonoObject* FMonoBindings::GetUnrealObjectWrapper(UObject* InObject) const
{
	checkSlow(IsInGameThread());

	if (nullptr == InObject)
	{
		return nullptr;
	}

	if (InObject->IsPendingKill())
	{
		// if we're pending kill, return null
		// remove from object table if it is in it
		RuntimeState.MonoObjectTable.RemoveObject(*InObject);
		return nullptr;
	}

	MonoObject* WrapperObject = RuntimeState.MonoObjectTable.GetManagedObject(*InObject);

	if (nullptr == WrapperObject)
	{
		WrapperObject = ConstructUnrealObjectWrapper(*InObject);
		RuntimeState.MonoObjectTable.AddWrapperObject(*InObject, WrapperObject);
	}

	check(WrapperObject)
	return WrapperObject;
}

bool FMonoBindings::InitializeMonoClasses()
{
	bool bAnyFailed = false;

	// register classes
	for (auto&& Pair : RuntimeState.MonoTypeToUnrealTypeMap)
	{
		auto& UnrealTypeRef = Pair.Value;

		if (nullptr == UnrealTypeRef.Resolve(*this))
		{
			bAnyFailed = true;
		}
	}

	// create CDOs

	for (auto MonoUnrealClass : RuntimeState.MonoClasses)
	{
		// Re-link to ensure all property sizes and offsets are valid.
		// Struct properties may have received an invalid element size on the first pass due
		// to circular references between user UStructs and user UClasses.
		MonoUnrealClass->StaticLink(true);

		// Force CDO generation.
		MonoUnrealClass->GetDefaultObject();
	}
	
	if (bAnyFailed)
	{
		ShowAnyErrorsOrWarnings();
	}

	return !bAnyFailed;
}

MonoClass* FMonoBindings::GetMonoClassFromUnrealClass(const UClass& InClass) const
{
	const UClass* CurrentClass = &InClass;

	while (nullptr != CurrentClass)
	{
		// see if it's a wrapped native class
		const CachedUnrealClass* CachedClass = RuntimeState.NativeWrapperMap.Find(CurrentClass);
		if (nullptr == CachedClass)
		{
			// see if it's a UMonoUnrealClass
			if (RuntimeState.MonoClasses.Contains(static_cast<UMonoUnrealClass*>(const_cast<UClass *> (CurrentClass))))
			{
				const UMonoUnrealClass* MonoUnrealClass = static_cast<const UMonoUnrealClass*>(CurrentClass);
				return MonoUnrealClass->GetMonoClass();
			}
			else
			{
				CurrentClass = CurrentClass->GetSuperClass();
			}
		}
		else
		{
			return CachedClass->GetClass();
		}
	}

	return nullptr;
}

UField* FMonoBindings::GetUnrealTypeFromMonoType(MonoType* InMonoType)
{
	return nullptr;
}


UPackage* FMonoBindings::GetPackageFromNamespaceAndAssembly(bool& bIsBindingsAssembly, const FString& InNamespace, const FString& InAssemblyName)
{
	const FString NamespacePrefix(MONO_UE4_NAMESPACE);
	const FString BindingsNamespace(MONO_BINDINGS_NAMESPACE);

	if (InNamespace == BindingsNamespace)
	{
		bIsBindingsAssembly = true;
		return nullptr;
	}

	UPackage* Package = nullptr;

	// Is this likely a generated namespace?
	if (InNamespace.StartsWith(NamespacePrefix) && InNamespace != NamespacePrefix)
	{
		const FString PackageName = InNamespace.Mid(NamespacePrefix.Len() + 1);

		// package name should be last entry in namespace 
		if (!PackageName.Contains(TEXT(".")))
		{
			FString MappedModuleName = ScriptGenUtil::MapScriptModuleNameToModuleName(FName(*PackageName)).ToString();
			Package = FindPackage(nullptr, *FString::Printf(TEXT("/Script/%s"), *MappedModuleName));
		}
	}

	bIsBindingsAssembly = nullptr != Package;

	// see if its a user created class
	if (nullptr == Package && InAssemblyName.Len() > 0)
	{
		const FString AssemblyName = SanitizeScriptPackageName(InAssemblyName);
		const FString PackageNameString = *FString::Printf(TEXT("/Script/%s"), *AssemblyName);
		Package = FindObject<UPackage>(nullptr, *PackageNameString, true);

	}
	return Package;
}

UClass* FMonoBindings::GetUnrealClassFromTypeReference(const FMonoTypeReferenceMetadata& ClassReference)
{
	MonoType* ResolvedType = ResolveTypeReference(ClassReference);
	if (nullptr == ResolvedType)
	{
		return nullptr;
	}
	return GetUnrealClassFromType(ResolvedType);
}

UClass* FMonoBindings::GetUnrealClassFromType(MonoType* InMonoType)
{
	UnrealTypeReference* TypeReference = RuntimeState.MonoTypeToUnrealTypeMap.Find(InMonoType);

	if (nullptr == TypeReference)
	{
		return nullptr;
	}

	return CastChecked<UClass>(TypeReference->Resolve(*this), ECastCheckedType::NullAllowed);
}

UScriptStruct* FMonoBindings::GetUnrealStructFromTypeReference(const FMonoTypeReferenceMetadata& StructReference)
{
	MonoType* ResolvedType = ResolveTypeReference(StructReference);
	if (nullptr == ResolvedType)
	{
		return nullptr;
	}
	return GetUnrealStructFromType(ResolvedType);
}

UScriptStruct* FMonoBindings::GetUnrealStructFromType(MonoType* InMonoType)
{
	UnrealTypeReference* TypeReference = RuntimeState.MonoTypeToUnrealTypeMap.Find(InMonoType);

	if (nullptr == TypeReference)
	{
		return nullptr;
	}

	return CastChecked<UScriptStruct>(TypeReference->Resolve(*this), ECastCheckedType::NullAllowed);
}

void FMonoBindings::CreateCompanionObject(UObject* InObject, MonoClass* Class, MonoMethod* ConstructorMethod, const FObjectInitializer& ObjectInitializer)
{
	check(InObject);
	check(Class);
	check(ConstructorMethod);

	FObjectInitializerWrapper Wrapper;
	Wrapper.NativeObject = InObject;
	Wrapper.NativePointer = &ObjectInitializer;

	MonoObject* CompanionObject = Mono::ConstructObject(*this, Class, ConstructorMethod, Wrapper);

	RuntimeState.MonoObjectTable.AddCompanionObject(*InObject, CompanionObject);
}

TSharedRef<FMonoDelegateHandle> FMonoBindings::CreateObjectDelegate(UObject& InOwner, MonoObject* Delegate, UObject* OptionalTargetObject)
{
	TSharedRef<FMonoDelegateHandle> DelegateHandle = MakeShareable(new FMonoDelegateHandle(FMonoBindings::Get(), Delegate, OptionalTargetObject));
	RuntimeState.MonoObjectTable.RegisterObjectDelegate(InOwner, *DelegateHandle);

	return DelegateHandle;
}

// HACK - This is a mirror of System.ModuleHandle, which has an internal IntPtr field "value"
// i.e. we're depending on Mono internals here and if they change we're boned
// What we really need is an API to get a MonoImage* from a MonoReflectionAssembly* (or a MonoAssembly* from a MonoReflectionAssembly*)
struct SystemModuleHandle
{
	MonoImage* Image;
};

struct LoadReturnStruct
{
	MonoReflectionAssembly* ReflectionAssembly;
	SystemModuleHandle ModuleHandle;
	MonoObject* ErrorString;
};

namespace Mono
{
	template <>
	struct Marshal<LoadReturnStruct, void>
	{
		static LoadReturnStruct ReturnValue(const FMonoBindings& Bindings, MonoObject* Object)
		{
			LoadReturnStruct* ReturnStruct = (LoadReturnStruct*) mono_object_unbox(Object);
			return *ReturnStruct;
		}

		static inline bool IsValidReturnType(MonoType* typ)
		{
			return mono_type_is_struct(typ) 
				&& 0 == FCStringAnsi::Strcmp(mono_type_get_name(typ), MONO_BINDINGS_NAMESPACE ".LoadAssemblyReturnStruct");
		}
	};
}

MonoType* FMonoBindings::ResolveTypeReference(const FMonoTypeReferenceMetadata& TypeReference) const
{
	TSharedPtr<FCachedAssembly> CachedAssembly = RuntimeState.AllAssemblies.FindRef(TypeReference.AssemblyName);
	if (!CachedAssembly.IsValid())
	{
		return nullptr;
	}
	return CachedAssembly->ResolveType(TypeReference);
}

MonoObject* FMonoBindings::ConstructUnrealObjectWrapper(UObject& InObject) const
{
	UClass* Class = InObject.GetClass();
	check(Class);
	check(!Class->HasAnyClassFlags(CLASS_Abstract)); // shouldn't ever get an abstract class here (how'd we get an instance?)

	// look up our mono wrapper
	const CachedUnrealClass* CachedClass = nullptr;

	// work our way down the super class chain until we find a class we've generated bindings for
	while (nullptr == (CachedClass = RuntimeState.NativeWrapperMap.Find(Class)))
	{
		Class = Class->GetSuperClass();
		// if we've hit null, something is horribly wrong because we should have at least found Object_WrapperOnly (the wrapper for UObject)
		check(Class);
	}
	check(CachedClass);
	return CachedClass->ConstructUnrealObjectWrapper(*this, InObject);
}

void FMonoBindings::LoadBindingsForScriptPackages(const TSet<FName>& ScriptPackages)
{
	TMap<FName, FString> UnloadedScriptPackageBindings = GetUnloadedScriptPackageBindings(ScriptPackages);

	for (const auto& Module : UnloadedScriptPackageBindings)
	{
		FName ScriptPackageName = Module.Key;
		FName ModuleName = FPackageName::GetShortFName(ScriptPackageName);
		const FString AssemblyName = FPaths::GetBaseFilename(Module.Value);
		FString ErrorMessage;
		TSharedPtr<FCachedAssembly> CachedAssembly = LoadAssembly(ErrorMessage, AssemblyName);
		checkf(CachedAssembly.IsValid(), TEXT("Failed to load bindings assembly %s: %s"), *AssemblyName, *ErrorMessage);

		RuntimeState.ScriptPackageToBindingsAssemblyMap.Add(ScriptPackageName, CachedAssembly);
		CacheUnrealClassesForAssembly(ScriptPackageName, *CachedAssembly);

		if (ModuleName == IMonoRuntime::ModuleName)
		{
			RuntimeState.MonoRuntimeAssembly = CachedAssembly;
		}
	}
}

TSharedPtr<FCachedAssembly> FMonoBindings::LoadAssembly(FString& ErrorString, const FString& AssemblyName)
{
	TSharedPtr<FCachedAssembly> CachedAssembly = RuntimeState.AllAssemblies.FindRef(AssemblyName);

	if (CachedAssembly.IsValid())
	{
		return CachedAssembly;
	}

	LoadReturnStruct ReturnedValue = Mono::Invoke<LoadReturnStruct>(*this, RuntimeState.LoadAssemblyMethod, nullptr, AssemblyName);

	if (ReturnedValue.ErrorString != nullptr)
	{
		ErrorString = Mono::Marshal<FString>::ReturnValue(*this, ReturnedValue.ErrorString);
		return CachedAssembly;
	}

	CachedAssembly = MakeShareable(new FCachedAssembly(ReturnedValue.ReflectionAssembly, ReturnedValue.ModuleHandle.Image));
	RuntimeState.AllAssemblies.Add(AssemblyName, CachedAssembly);
	return CachedAssembly;
}

bool IsPluginModule(const FName ModuleName)
{
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
	{
		for (const FModuleDescriptor ModuleInfo : Plugin->GetDescriptor().Modules)
		{
			if (ModuleInfo.Name == ModuleName)
			{
				return true;
			}
		}
	}
	return false;
}

TMap<FName, FString> FMonoBindings::GetUnloadedScriptPackageBindings(const TSet<FName>& ScriptPackageSet) const
{
	TMap<FName, FString> BoundScriptPackages;

	BoundScriptPackages.Empty(ScriptPackageSet.Num());

	// filter out any modules we don't have a binding assembly for
	for (auto ScriptPackageName : ScriptPackageSet)
	{
		// skip already loaded ones
		if (nullptr == RuntimeState.ScriptPackageToBindingsAssemblyMap.Find(ScriptPackageName))
		{
			FName ModuleName = FPackageName::GetShortFName(ScriptPackageName);

			bool bIsPlugin = IsPluginModule(ModuleName);
			bool bGameModuleStatusDetermined = true;

			FString AssemblyName;

			if (bIsPlugin)
			{
				// plugins always have the long form
				AssemblyName = FString::Printf(TEXT("%s.%s"), ANSI_TO_TCHAR(MONO_UE4_NAMESPACE), *ModuleName.ToString());
			}
			else
			{
				// we can't know for sure since module manager might not know about a loaded module yet
				bGameModuleStatusDetermined = false;
				// game modules do not have a prefix on their assembly, so try that first
				AssemblyName = ModuleName.ToString();

				// module manager might not know about this module yet because it was loaded as a dependency by something else
				FModuleStatus ModuleStatus;
				if (FModuleManager::Get().QueryModule(ModuleName, ModuleStatus))
				{
					// we know for sure if its a game module or not
					bGameModuleStatusDetermined = true;
					if (!ModuleStatus.bIsGameModule)
					{
						AssemblyName = GetBuiltinModuleBindingsAssemblyName();
					}
				}
			}


			FString BindingsAssemblyDLL = AssemblyName + TEXT(".dll");

			FString PotentialPaths[4];

			// support bindings assemblies living in either engine (engine modules and plugins) or game (game modules and plugins).
			PotentialPaths[0] = FPaths::Combine(*EngineAssemblyDirectory, *BindingsAssemblyDLL);
			PotentialPaths[1] = FPaths::Combine(*GameAssemblyDirectory, *BindingsAssemblyDLL);
			if (!bGameModuleStatusDetermined)
			{
				// if it might be a game module, we try the game module resolution first
				// Otherwise it must be a builtin
				check(!bIsPlugin);
				AssemblyName = GetBuiltinModuleBindingsAssemblyName();
				BindingsAssemblyDLL = AssemblyName + TEXT(".dll");
				PotentialPaths[2] = FPaths::Combine(*EngineAssemblyDirectory, *BindingsAssemblyDLL);
				PotentialPaths[3] = FPaths::Combine(*GameAssemblyDirectory, *BindingsAssemblyDLL);
			}

			for (int i = 0; i < ARRAY_COUNT(PotentialPaths); ++i)
			{
				if (PotentialPaths[i].IsEmpty())
				{
					continue;
				}

				// ignore assemblies with side-by-side json files, those aren't bindings assemblies
				const FString PotentialMetadataFile = FPaths::GetBaseFilename(PotentialPaths[i], false) + TEXT(".json");
				if (FPaths::FileExists(PotentialPaths[i]) && !FPaths::FileExists(PotentialMetadataFile))
				{
					BoundScriptPackages.Add(ScriptPackageName, PotentialPaths[i]);
					break;
				}
			}
		}
	}

	return BoundScriptPackages;
}

// Mirror of Bindings.FindUnrealClassesReturnStruct
struct FindUnrealClassesReturnStruct
{
	MonoReflectionType* ReflectionType;
	MonoReflectionType* ReflectionWrapperType;
	int32 UnrealClassIndex;
};

namespace Mono
{
	template <>
	struct Marshal < TArray<FindUnrealClassesReturnStruct>, void >
	{
		static TArray<FindUnrealClassesReturnStruct> ReturnValue(const FMonoBindings& Bindings, MonoObject* Object)
		{
			TArray<FindUnrealClassesReturnStruct> Ret;
			MonoValueArrayToTArray(Ret, Object);
			return Ret;
		}

		static inline bool IsValidReturnType(MonoType* typ)
		{
			return  IsValidArrayType(typ, MONO_BINDINGS_NAMESPACE ".FindUnrealClassesReturnStruct", false);
		}
	};
}

void FMonoBindings::CacheUnrealClassesForAssembly(FName ScriptPackageName, const FCachedAssembly& CachedAssembly)
{
	FString ModuleName = ScriptGenUtil::MapModuleNameToScriptModuleName (FPackageName::GetShortFName(ScriptPackageName)).ToString();
	TSet<UClass*> UnrealClassesInPackage;

	// we only want actual UClasses, not any blueprint ones
	for (TObjectIterator<UClass> ClassIt(RF_ClassDefaultObject, false); ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		UPackage* Package = Class->GetTypedOuter<UPackage>();
		check(Package);


		if (Package->GetFName() == ScriptPackageName)
		{
			UnrealClassesInPackage.Add(Class);
		}
	}

	TMap<UClass*, CachedUnrealClass> CachedClasses;

	{
		TArray<UClass*> UnrealClasses;
		TArray<FString> UnrealClassNames;

		UnrealClasses.Empty(UnrealClassesInPackage.Num());
		UnrealClassNames.Empty(UnrealClassesInPackage.Num());

		const FName ScriptNameMetaDataKey = TEXT("ScriptName");

		for (auto Class : UnrealClassesInPackage)
		{
			UnrealClasses.Add(Class);
			if (Class->HasMetaData(ScriptNameMetaDataKey))
			{
				FString ScriptName = Class->GetMetaData(ScriptNameMetaDataKey);
				UnrealClassNames.Add(ScriptName);
			}
			else
			{
				UnrealClassNames.Add(Class->GetName());
			}
		}

		check(CachedAssembly.ReflectionAssembly);
		TArray<FindUnrealClassesReturnStruct> FoundClasses = Mono::Invoke<TArray<FindUnrealClassesReturnStruct>>(*this, RuntimeState.FindUnrealClassesInAssemblyMethod, nullptr, CachedAssembly.ReflectionAssembly, UnrealClassNames, ModuleName);

		UE_CLOG(FoundClasses.Num() >0, LogMono, Log, TEXT("Found %d managed class bindings for script package '%s'"), FoundClasses.Num(), *ScriptPackageName.ToString());

		for (const auto& FoundClass : FoundClasses)
		{
			UClass* UnrealClass = UnrealClasses[FoundClass.UnrealClassIndex];
			check(FoundClass.ReflectionType);
			MonoClass* ManagedClass = Mono::GetClassFromReflectionType(FoundClass.ReflectionType);
			// wrapper type is optional
			MonoClass* ManagedWrapperClass = FoundClass.ReflectionWrapperType != nullptr ? Mono::GetClassFromReflectionType(FoundClass.ReflectionWrapperType) : nullptr;

			CachedUnrealClass CachedClass;
			
			if (CachedClass.Resolve(CachedAssembly, ManagedClass, ManagedWrapperClass, *UnrealClass))
			{
				CachedClasses.Add(UnrealClass, CachedClass);
			}
		}
	}

	for (const auto& CachedClassPair : CachedClasses)
	{
		UClass* Class = CachedClassPair.Key;
		const CachedUnrealClass& CachedClass = CachedClassPair.Value;

		// we should not be registered already
		check(nullptr == RuntimeState.NativeWrapperMap.Find(Class));
		// map from MonoType to UnrealTypeReference
		RuntimeState.MonoTypeToUnrealTypeMap.Add(mono_class_get_type(CachedClass.GetClass()), UnrealTypeReference(*Class));
		if (CachedClass.GetClass() != CachedClass.GetWrapperClass())
		{
			// TODO: Our type system breaks down here since we don't have managed types equivalent to all unreal object types, only ones exposed to blueprint
			// This will cause problems with using managed types in the API since they can't reflect all possible unreal types
			// One possible solution would be to add *all* unreal types to the managed assemblies, but ones which are not exposed can't be created or derived from
			// For now I create a mapping from the wrapper class to the unreal class
			RuntimeState.MonoTypeToUnrealTypeMap.Add(mono_class_get_type(CachedClass.GetWrapperClass()), UnrealTypeReference(*Class));
		}
		RuntimeState.NativeWrapperMap.Add(Class, CachedClass);
	}
}

bool FMonoBindings::LoadGameAssemblies(const TArray<FMonoLoadedAssemblyMetadata>& DirectoryMetadata)
{
	bool bAnyFailed = false;
	for (auto&& LoadedMetadata : DirectoryMetadata)
	{
		if (!LoadGameAssembly(LoadedMetadata))
		{
			bAnyFailed = true;
		}
	}
	return !bAnyFailed;
}

#if WITH_METADATA
static bool IsDerivable(UClass* Class)
{
	static const FName MD_IsBlueprintBase(TEXT("IsBlueprintBase"));

	const bool bCanCreate =
		!Class->HasAnyClassFlags(CLASS_Deprecated)
		&& !Class->HasAnyClassFlags(CLASS_NewerVersionExists)
		&& !Class->ClassGeneratedBy;

	const bool bIsValidClass = Class->GetBoolMetaDataHierarchical(MD_IsBlueprintBase)
		|| (Class == UObject::StaticClass())
		|| (Class == UBlueprintFunctionLibrary::StaticClass());

	return bCanCreate && bIsValidClass;
}
#endif // WITH_METADATA

bool FMonoBindings::LoadGameAssembly(const FMonoLoadedAssemblyMetadata& LoadedMetadata)
{
	FString CleanMetadataFile(FPaths::GetCleanFilename(LoadedMetadata.MetadataFile));
	TSharedPtr<FMonoAssemblyMetadata> Metadata = LoadedMetadata.AssemblyMetadata;
	const FString& AssemblyFile = LoadedMetadata.AssemblyFile;

	// Load the assembly
	FString ErrorMessage;
	const FString AssemblyName = FPaths::GetBaseFilename(AssemblyFile);
	TSharedPtr<FCachedAssembly> CachedAssembly = LoadAssembly(ErrorMessage, AssemblyName);
	if (!CachedAssembly.IsValid())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("AssemblyFile"), FText::FromString(AssemblyFile));
		Args.Add(TEXT("ErrorMessage"), FText::FromString(ErrorMessage));

		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotLoadAssembly", "'{AssemblyFile}': {ErrorMessage}"), Args));

		return false;
	}

	TArray<TSharedPtr<DeferredUnrealTypeCreationInfo>> UnrealTypes;

	bool bAnyFailed = false;

	for (const auto& Struct : Metadata->Structs)
	{
		MonoType* StructType = ResolveTypeReference(Struct);

		check(!RuntimeState.MonoTypeToUnrealTypeMap.Contains(StructType));

		TSharedPtr<DeferredUnrealTypeCreationInfo> DeferredCreate = MakeShareable(new DeferredUnrealStructCreationInfo(Metadata, Struct, CachedAssembly, CleanMetadataFile));
		RuntimeState.MonoTypeToUnrealTypeMap.Add(StructType, UnrealTypeReference(DeferredCreate));

		UnrealTypes.Add(DeferredCreate);
	}

	for (const auto& Enum : Metadata->Enums)
	{
		MonoType* EnumType = ResolveTypeReference(Enum);

		check(!RuntimeState.MonoTypeToUnrealTypeMap.Contains(EnumType));

		TSharedPtr<DeferredUnrealTypeCreationInfo> DeferredCreate = MakeShareable(new DeferredUnrealEnumCreationInfo(Metadata, Enum, CachedAssembly, CleanMetadataFile));
		RuntimeState.MonoTypeToUnrealTypeMap.Add(EnumType, UnrealTypeReference(DeferredCreate));

		UnrealTypes.Add(DeferredCreate);
	}

	// filter out non-derivable classes
	for (const auto& Class : Metadata->Classes)
	{
		UClass* BaseNativeClass = GetUnrealClassFromTypeReference(Class.BaseUnrealNativeClass);
		if (nullptr == BaseNativeClass)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("CleanMetadataFile"), FText::FromString(CleanMetadataFile));
			Args.Add(TEXT("ClassQualifiedName"), FText::FromString(Class.GetQualifiedName()));
			Args.Add(TEXT("UnrealClassName"), FText::FromString(Class.BaseUnrealNativeClass.GetQualifiedName()));
			FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotFindUnrealClass", "'{CleanMetadataFile}': Could not load class '{ClassQualifiedName}': could not find unreal class '{UnrealClassName}'"), Args));
			bAnyFailed = true;
		}
		else
		{
#if WITH_METADATA
			// This is an assert because we now verify a class is actually derivable in MonoAssemblyProcess, and metadata is not available outside the editor
			checkf(IsDerivable(BaseNativeClass), TEXT("%s: Could not import class %s: parent unreal class %s is not a valid base class for Mono types."), *CleanMetadataFile, *Class.GetQualifiedName(), *BaseNativeClass->GetName());
#endif // WITH_METADATA
			MonoType* ClassType = ResolveTypeReference(Class);

			check(!RuntimeState.MonoTypeToUnrealTypeMap.Contains(ClassType)); 

			TSharedPtr<DeferredUnrealTypeCreationInfo> DeferredCreate = MakeShareable(new DeferredUnrealClassCreationInfo(Metadata, Class, *BaseNativeClass, CachedAssembly, CleanMetadataFile));
			RuntimeState.MonoTypeToUnrealTypeMap.Add(ClassType, UnrealTypeReference(DeferredCreate));

			UnrealTypes.Add(DeferredCreate);
		}
	}

	if (bAnyFailed)
	{
		return false;
	}

	if (UnrealTypes.Num() > 0)
	{
		// create package for this assembly
		FName PackageName(*FString::Printf(TEXT("/Script/%s"), *FPaths::GetBaseFilename(CleanMetadataFile, true)));
		const FString PackageNameString = SanitizeScriptPackageName(PackageName.ToString());

		UPackage* Package = FindObject<UPackage>(nullptr, *PackageNameString, true);

		if (nullptr != Package
#if MONO_WITH_HOT_RELOADING
			&& !IsReloading()
#endif // MONO_WITH_HOT_RELOADING
			)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("CleanMetadataFile"), FText::FromString(CleanMetadataFile));
			Args.Add(TEXT("PackageNameString"), FText::FromString(PackageNameString));

			FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("ScriptPackageExists", "'{CleanMetadataFile}': Script package named '{PackageNameString}' already exists."), Args));
			return false;
		}

#if MONO_WITH_HOT_RELOADING
		if (nullptr == Package)
#endif // MONO_WITH_HOT_RELOADING
		{
			Package = CreatePackage(nullptr, *PackageNameString);
			check(Package);

			Package->SetPackageFlags(PKG_CompiledIn);

			Package->SetGuid(LoadedMetadata.ScriptPackageGuid);
		}

		// now that our script package is created, associate
		for (auto&& DeferredClass : UnrealTypes)
		{
			DeferredClass->AssociateWithScriptPackage(*Package);
		}
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(CleanMetadataFile));
		Args.Add(TEXT("AssemblyFile"), FText::FromString(AssemblyFile));
		FMessageLog(NAME_MonoErrors).Warning(FText::Format(LOCTEXT("NoUnrealClassesFound", "'{CleanMetadataFile}': No unreal classes found in assembly '{AssemblyFile}'"), Args));
	}
	return true;
}

bool FMonoBindings::CreateGameStruct(DeferredUnrealStructCreationInfo& StructInfo)
{
	check(StructInfo.TypeReferenceMetadata);

	// This has to remain valid and unchanged for the duration of this function, Epic's class registration system assumes the package name is a constant literal
	check(StructInfo.ScriptPackage);
	UPackage& ScriptPackage = *StructInfo.ScriptPackage;
	const FString PackageNameString = ScriptPackage.GetName();

	const FMonoStructMetadata& Metadata = *StructInfo.GetMetadata();
	UScriptStruct* NewStruct = FindObject<UScriptStruct>(&ScriptPackage, *Metadata.Name);

	StructInfo.CreatedType = nullptr;

	MonoClass* ManagedClass = StructInfo.CachedAssembly->GetClass(*Metadata.Namespace, *Metadata.Name);
	if (nullptr == ManagedClass)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(StructInfo.CleanMetadataFileName));
		Args.Add(TEXT("StructName"), FText::FromString(Metadata.GetQualifiedName()));
		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotFindManagedStruct", "'{CleanMetadataFile}': Could't find managed struct named '{StructName}'"), Args));
		return false;
	}

	if (nullptr != NewStruct
#if MONO_WITH_HOT_RELOADING
		&& (!IsReloading() || (IsReloading() && !CurrentReloadContext->CachedRuntimeState.MonoStructs.Contains(NewStruct)))
#endif // MONO_WITH_HOT_RELOADING
		)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(StructInfo.CleanMetadataFileName));
		Args.Add(TEXT("StructName"), FText::FromString(Metadata.Name));
		Args.Add(TEXT("PackageName"), FText::FromString(ScriptPackage.GetName()));

		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("ExistingStructError", "'{CleanMetadataFile}': Existing struct named '{StructName}' in package '{PackageName}'"), Args));
		return false;
	}

	const FMonoPropertyFactory& PropertyFactory = FMonoPropertyFactory::Get();

#if MONO_WITH_HOT_RELOADING
	UScriptStruct* OldStruct = nullptr;
	if (IsReloading() && nullptr != NewStruct)
	{
		const FString& StructHash = *CurrentReloadContext->CachedRuntimeState.MonoStructs.Find(NewStruct);
		if (StructHash != Metadata.StructHash)
		{
			OldStruct = NewStruct;
			NewStruct = nullptr;
			RenamePreviousStruct(*OldStruct);
		}
		else
		{
			// There's no compiled class asset or functions to hot reload, so just wire up the existing 
			// UScriptStruct and call it a day.
			StructInfo.CreatedType = NewStruct;

#if MONO_WITH_HOT_RELOADING
			RuntimeState.MonoStructs.Add(NewStruct, Metadata.StructHash);
#endif // MONO_WITH_HOT_RELOADING
		}
	}
	if (nullptr != NewStruct)
	{
		check(IsReloading());
		check(OldStruct == nullptr);
	}
	else
#endif // MONO_WITH_HOT_RELOADING
	{
		NewStruct = new(EC_InternalUseOnlyConstructor, &ScriptPackage, *Metadata.Name, RF_Public | RF_Transient | RF_MarkAsNative) UScriptStruct(FObjectInitializer(), NULL, NULL, static_cast<EStructFlags>(Metadata.StructFlags));

		// Create in reverse order for ease of matching against property metadata on hot reload.
		// StaticLink() will insert properties at the head of the PropertyLink list, in creation order.
		for (int i = Metadata.Properties.Num() - 1; i >= 0; --i)
		{
			const FMonoPropertyMetadata& PropMetadata = Metadata.Properties[i];
			UProperty* Property = PropertyFactory.Create(*NewStruct, *this, PropMetadata);
			check(Property);
		}

		StructInfo.CreatedType = NewStruct;

		NewStruct->StaticLink(true);
		check(NewStruct->PropertiesSize > 0);

#if MONO_WITH_HOT_RELOADING
		if (IsReloading() && OldStruct != nullptr)
		{
			DeferStructReinstance(*OldStruct, *NewStruct);
		}

		RuntimeState.MonoStructs.Add(NewStruct, Metadata.StructHash);
#endif // MONO_WITH_HOT_RELOADING
	}

	return true;
}

bool FMonoBindings::CreateGameClass(DeferredUnrealClassCreationInfo& ClassInfo)
{
	check(ClassInfo.TypeReferenceMetadata);

	// This has to remain valid and unchanged for the duration of this function, Epic's class registration system assumes the package name is a constant literal
	check(ClassInfo.ScriptPackage);
	UPackage& ScriptPackage = *ClassInfo.ScriptPackage;
	const FString PackageNameString = ScriptPackage.GetName();

	// can't use Cast<> macro because UMonoUnrealClass is not in UE4's reflection system
	check(ClassInfo.TypeReferenceMetadata);
	const FMonoClassMetadata& Metadata = *ClassInfo.GetMetadata();
	UMonoUnrealClass* NewClass = (UMonoUnrealClass*)FindObject<UClass>(&ScriptPackage, *Metadata.Name);


	ClassInfo.CreatedType = nullptr;

	if (nullptr != NewClass
#if MONO_WITH_HOT_RELOADING
		&& (!IsReloading() || (IsReloading() && !CurrentReloadContext->CachedRuntimeState.MonoClasses.Contains(NewClass)))
#endif // MONO_WITH_HOT_RELOADING
		)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(ClassInfo.CleanMetadataFileName));
		Args.Add(TEXT("ClassName"), FText::FromString(Metadata.Name));
		Args.Add(TEXT("PackageName"), FText::FromString(ScriptPackage.GetName()));

		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("ExistingClassError","'{CleanMetadataFile}': Existing class named '{ClassName}' in package '{PackageName}'"), Args)); 
		return false;
	}

	MonoClass* ManagedClass = ClassInfo.CachedAssembly->GetClass(*Metadata.Namespace, *Metadata.Name);
	if (nullptr == ManagedClass)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(ClassInfo.CleanMetadataFileName));
		Args.Add(TEXT("ClassName"), FText::FromString(Metadata.GetQualifiedName()));
		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotFindManagedClass", "'{CleanMetadataFile}': Could't find managed class named '{ClassName}'"), Args));
		return false;
	}

	FString ErrorMessage;
	TUniquePtr<FMonoCompiledClassAsset> CompiledClassAsset = FMonoCompiledClassAsset::CreateCompiledClassAsset(ErrorMessage, *this, ManagedClass);

	if (!CompiledClassAsset)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(ClassInfo.CleanMetadataFileName));
		Args.Add(TEXT("ClassName"), FText::FromString(Metadata.GetQualifiedName()));
		Args.Add(TEXT("ErrorMessage"), FText::FromString(ErrorMessage));
		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotCreateManagedClass", "'{CleanMetadataFile}': Could not create managed class '{ClassName}': {ErrorMessage}"), Args));
		return false;
	}

	// get the super class. If the super class is managed, it will be created/hot reloaded here
	UClass* SuperClass = GetUnrealClassFromTypeReference(Metadata.BaseClass); 
	if (nullptr == SuperClass)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(ClassInfo.CleanMetadataFileName));
		Args.Add(TEXT("ClassName"), FText::FromString(Metadata.GetQualifiedName()));
		Args.Add(TEXT("BaseClassName"), FText::FromString(Metadata.BaseClass.GetQualifiedName()));
		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotFindBaseClass", "'{CleanMetadataFile}': Could not create managed class '{ClassName}': Failed to find or create base class '{BaseClassName}'"), Args));
		return false;
	}

#if MONO_WITH_HOT_RELOADING
	UMonoUnrealClass* OldClass = nullptr;
	if (IsReloading() && NewClass != nullptr)
	{
		// see if the class has changed 
		if (NewClass->GetClassHash() != Metadata.ClassHash)
		{
			OldClass = NewClass;
			NewClass = nullptr;
			RenamePreviousClass(*OldClass);
		}
		else
		{
			// set this whether or not the hot reload is successful - it will always attempt to preserve an existing class in a 'good state'
			ClassInfo.CreatedType = NewClass;

			NewClass->HotReload(SuperClass, ClassInfo.NativeParentClass, MoveTemp(CompiledClassAsset), Metadata);

			RuntimeState.MonoClasses.Add(NewClass);
		}
	}
	if (nullptr != NewClass)
	{
		check(IsReloading());
		check(OldClass == nullptr);
	}
	else
#endif
	{
		check(nullptr == NewClass);
		// TODO: figure out if this should go in permanent object pool
		NewClass = ::new (FMemory::Malloc(sizeof(UMonoUnrealClass), 16))	UMonoUnrealClass(SuperClass, ClassInfo.NativeParentClass,
			MoveTemp(CompiledClassAsset),
			TEXT("Engine"),
			*PackageNameString,
			*Metadata.Name,
			(EClassFlags)Metadata.ClassFlags);

		ClassInfo.CreatedType = NewClass;

		RuntimeState.MonoClasses.Add(NewClass);

		// Now that NewClass is resolvable, it's safe to create UProperties and UFunctions,
		// even if there are circular references.
		NewClass->Initialize(Metadata);

#if MONO_WITH_HOT_RELOADING
		NewClass->SetClassHash(Metadata.ClassHash);

		if (IsReloading() && OldClass != nullptr)
		{
			DeferClassReinstance(*OldClass, *NewClass);
		}
#endif // MONO_WITH_HOT_RELOADING

	}

	return true;
}

bool FMonoBindings::CreateGameEnum(DeferredUnrealEnumCreationInfo& EnumInfo)
{
	check(EnumInfo.TypeReferenceMetadata);

	// This has to remain valid and unchanged for the duration of this function, Epic's class registration system assumes the package name is a constant literal
	check(EnumInfo.ScriptPackage);
	UPackage& ScriptPackage = *EnumInfo.ScriptPackage;
	const FString PackageNameString = ScriptPackage.GetName();

	const FMonoEnumMetadata& Metadata = *EnumInfo.GetMetadata();
	UEnum* NewEnum = FindObject<UEnum>(&ScriptPackage, *Metadata.Name);

	EnumInfo.CreatedType = nullptr;

	MonoClass* ManagedEnum = EnumInfo.CachedAssembly->GetClass(*Metadata.Namespace, *Metadata.Name);
	if (nullptr == ManagedEnum)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(EnumInfo.CleanMetadataFileName));
		Args.Add(TEXT("EnumName"), FText::FromString(Metadata.Name));
		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotFindManagedEnum", "'{CleanMetadataFile}': Could't find managed struct named '{EnumName}'"), Args));
		return false;
	}

	if (nullptr != NewEnum
#if MONO_WITH_HOT_RELOADING
		&& (!IsReloading() || (IsReloading() && !CurrentReloadContext->CachedRuntimeState.MonoEnums.Contains(NewEnum)))
#endif // MONO_WITH_HOT_RELOADING
		)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(EnumInfo.CleanMetadataFileName));
		Args.Add(TEXT("EnumName"), FText::FromString(Metadata.Name));
		Args.Add(TEXT("PackageName"), FText::FromString(ScriptPackage.GetName()));

		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("ExistingEnumError", "'{CleanMetadataFile}': Existing struct named '{EnumName}' in package '{PackageName}'"), Args));
		return false;
	}

#if MONO_WITH_HOT_RELOADING
	UEnum* OldEnum = nullptr;
	if (IsReloading() && nullptr != NewEnum)
	{
		const FString& EnumHash = *CurrentReloadContext->CachedRuntimeState.MonoEnums.Find(NewEnum);
		if (EnumHash != Metadata.EnumHash)
		{
			OldEnum = NewEnum;
			NewEnum = nullptr;
			RenamePreviousEnum(*OldEnum);
		}
		else
		{
			// There's no compiled class asset or functions to hot reload, so just wire up the existing 
			// UEnum and call it a day.
			EnumInfo.CreatedType = NewEnum;

#if MONO_WITH_HOT_RELOADING
			RuntimeState.MonoEnums.Add(NewEnum, Metadata.EnumHash);
#endif // MONO_WITH_HOT_RELOADING
		}
	}
	if (nullptr != NewEnum)
	{
		check(IsReloading());
		check(OldEnum == nullptr);
	}
	else
#endif // MONO_WITH_HOT_RELOADING
	{
		NewEnum = NewObject<UEnum>(&ScriptPackage, *Metadata.Name);
		NewEnum->AddToRoot();

		TArray<TPair<FName, int64>> EnumNames;
		for (int i = 0; i < Metadata.Items.Num(); i++)
		{
			const FString Name = Metadata.Items[i];
			FName NamespacedName = *FString::Printf(TEXT("%s::%s"), *Metadata.Name, *Name);
			EnumNames.Add(TPairInitializer<FName, int64>(NamespacedName, (int64)i));
		}
		NewEnum->SetEnums(EnumNames, UEnum::ECppForm::Namespaced);
		
#if WITH_METADATA
		if (Metadata.BlueprintVisible)
		{
			NewEnum->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		}
#endif //WITH_METADATA

		EnumInfo.CreatedType = NewEnum;
#if MONO_WITH_HOT_RELOADING
		if (IsReloading() && OldEnum != nullptr)
		{
			DeferEnumReinstance(*OldEnum, *NewEnum);
		}

		RuntimeState.MonoEnums.Add(NewEnum, Metadata.EnumHash);
#endif // MONO_WITH_HOT_RELOADING
	}
	return true;
}

#if DO_CHECK

const UMonoUnrealClass* FMonoBindings::GetMonoUnrealClass(const UClass* InClass) const
{
	const UMonoUnrealClass* TempClass = static_cast<const UMonoUnrealClass*>(InClass);
	if (RuntimeState.MonoClasses.Contains(const_cast<UMonoUnrealClass*>(TempClass)))
	{
		return TempClass;
	}
	return nullptr;
}

UMonoUnrealClass* FMonoBindings::GetMonoUnrealClass(UClass* InClass) const
{
	UMonoUnrealClass* TempClass = static_cast<UMonoUnrealClass*>(InClass);
	if (RuntimeState.MonoClasses.Contains(TempClass))
	{
		return TempClass;
	}
	return nullptr;

}

#endif // DO_CHECK

#undef LOCTEXT_NAMESPACE