// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoUnrealClass.h"

#include "CoreMinimal.h"
#include "Logging/MessageLog.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Actor.h"

#include "MonoRuntimeCommon.h"
#include "MonoBindings.h"
#include "MonoAssemblyMetadata.h"
#include "MonoCompiledClassAsset.h"
#include "MonoPropertyFactory.h"

#define LOCTEXT_NAMESPACE "MonoRuntime"

static int32 GIsInManagedConstructor = 0;

static TArray<UFunction*> GetClassOveriddenFunctions(UClass& NativeParentClass, const struct FMonoClassMetadata& Metadata) 
{
	TMap<FName, UFunction*> NameToFunctionMap;

	// find all the blueprint implementable events in the parent class

	for (TFieldIterator<UFunction> It(&NativeParentClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		UFunction* Function = *It;
		if (Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
		{
			NameToFunctionMap.Add(Function->GetFName(), Function);
		}
	}

	const TArray<FName> VirtualFunctionNames = Metadata.GetVirtualFunctions();

	TArray<UFunction*> OverriddenFunctions;

	OverriddenFunctions.Empty(VirtualFunctionNames.Num());

	for (auto FunctionName : VirtualFunctionNames)
	{
		// TODO: verify signature
		UFunction* Function = NameToFunctionMap.FindRef(FunctionName);
		if (nullptr != Function)
		{
			OverriddenFunctions.Add(Function);
		}
	}

	return OverriddenFunctions;
}

UMonoUnrealClass::UMonoUnrealClass(UClass* InSuperClass,
								   UClass* InNativeParentClass,
								   TUniquePtr<FMonoCompiledClassAsset>&& InCompiledClassAsset,
								   const TCHAR* InClassConfigName,
								   const TCHAR* InPackageName,
								   const TCHAR* InClassName,
								   EClassFlags Flags)
								   : UClass(EC_StaticConstructor,
										    FName(InClassName),
										    InNativeParentClass->GetPropertiesSize(),
										    COMPILED_IN_FLAGS(Flags),
										    CASTCLASS_None,
										    InClassConfigName,
										    EObjectFlags(RF_Public | RF_Standalone | RF_Transient | RF_MarkAsNative | RF_MarkAsRootSet),
									        &UMonoUnrealClass::MonoClassConstructor,
									        &UMonoUnrealClass::MonoVTableHelperCtorCaller,
									   InNativeParentClass->ClassAddReferencedObjects)
								   , NativeParentClass(InNativeParentClass)
								   , CompiledClassAsset(MoveTemp(InCompiledClassAsset))
#if MONO_WITH_HOT_RELOADING
								   , bDeletedDuringHotReload(false)
#endif // MONO_WITH_HOT_RELOADING
								   , bHasReplicatedProperties(false)
								   , bHasCustomLifetimeReplicatedProperties(false)
{
	InitializePrivateStaticClass(InSuperClass, this, UObject::StaticClass(), InPackageName, InClassName);

	// force registration
	UObjectForceRegistration(this);
}

void UMonoUnrealClass::ApplyMetaData(const FMonoClassMetadata& Metadata)
{
	if (Metadata.Transience == TEXT("Transient"))
	{
		ClassFlags |= CLASS_Transient;
	}
	else if (Metadata.Transience == TEXT("NotTransient"))
	{
		ClassFlags &= ~CLASS_Transient;
	}

	if (Metadata.Placeablity == TEXT("Placeable"))
	{
		ClassFlags &= ~CLASS_NotPlaceable;
	}
	else if (Metadata.Transience == TEXT("NotPlaceable"))
	{
		ClassFlags |= CLASS_NotPlaceable;
	}

	if (Metadata.Abstract)
	{
		ClassFlags |= CLASS_Abstract;
	}

	if (Metadata.Deprecated)
	{
		ClassFlags |= CLASS_Deprecated;
	}

#if WITH_METADATA
	if (Metadata.BlueprintUse == TEXT("None"))
	{
		RemoveMetaData(TEXT("BlueprintType"));
		SetMetaData(TEXT("NotBlueprintType"), TEXT("true"));
		SetMetaData(TEXT("IsBlueprintBase"), TEXT("false"));
	}
	else if (Metadata.BlueprintUse == TEXT("Accessible"))
	{
		RemoveMetaData(TEXT("NotBlueprintType"));
		SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		SetMetaData(TEXT("IsBlueprintBase"), TEXT("false"));
	}
	else if (Metadata.BlueprintUse == TEXT("Derivable"))
	{
		RemoveMetaData(TEXT("NotBlueprintType"));
		SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		SetMetaData(TEXT("IsBlueprintBase"), TEXT("true"));
	}

	if (Metadata.Group.Len() != 0)
	{
		TArray<FString> Groups;

		Metadata.Group.ParseIntoArray(Groups, TEXT(","), true);

		SetMetaData(TEXT("ClassGroupNames"), *FString::Join(Groups, TEXT(" ")));
	}
#endif //WITH_METADATA

	if (Metadata.ConfigFile.Len() != 0)
	{
		if (Metadata.ConfigFile != TEXT("Inherit"))
		{
			ClassConfigName = *Metadata.ConfigFile;
		}
	}
}

void UMonoUnrealClass::Initialize(const FMonoClassMetadata& Metadata)
{
	ApplyMetaData(Metadata);

	// Generate UFunction overrides
	TArray<UFunction*> OverriddenFunctions = GetClassOveriddenFunctions(*NativeParentClass, Metadata);
	GenerateClassOverriddenFunctions(OverriddenFunctions);

	// generate properties
	GenerateClassProperties(CompiledClassAsset->GetBindings(), Metadata.Properties);

	// generate UFunctions
	GenerateClassFunctions(CompiledClassAsset->GetBindings(), Metadata.Functions);

	CompiledClassAsset->AddFunctionsToEventMap(this, NativeParentClass, Metadata.Functions);

	StaticLink(true);

	bOverrideCanTick = Metadata.ChildCanTick;
	bOverrideBindsInput = Metadata.OverridesBindInput;

#if DO_CHECK && WITH_METADATA
	if (IsChildOf(AActor::StaticClass()))
	{
		static const FName ChildCanTickName = TEXT("ChildCanTick");
		check(Metadata.ChildCanTick == (AActor::StaticClass() == NativeParentClass) || (NativeParentClass->HasMetaData(ChildCanTickName)));

	}
#endif // DO_CHECK && WITH_METADATA
}

void UMonoUnrealClass::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	// for linking purposes, pretend this is NOT a native/intrinsic class, so our properties get offsets computed
	// and we hook in any constructors/destructors for them. This is a hack, but the fact is most of the time we want to behave like a native
	// class, but this is one of the few we don't. A cleaner solution would require intrusive changes to UE4's UObject innards
	EClassFlags OldClassFlags = ClassFlags;
	ClassFlags = ClassFlags&(~(CLASS_Intrinsic | CLASS_Native));
	UClass::Link(Ar, bRelinkExistingProperties);
	ClassFlags = OldClassFlags;
}

bool UMonoUnrealClass::HasInputDelegateBindings(class UObject* InObject) const
{
	check(!bOverrideBindsInput || IsChildOf(AActor::StaticClass()));
	return bOverrideBindsInput;
}

void UMonoUnrealClass::BindInputDelegates(class UObject* InObject) const
{
	AActor* Actor = CastChecked<AActor>(InObject);
	check(Actor->InputComponent);
	check(CompiledClassAsset);
	check(HasInputDelegateBindings(InObject));

	// Let the superclass bind input if we can't, which can happen if a mono class which doesn't override BindInput() is derived from 
	// another mono class which does.  Mono method lookups won't find base class implementations of virtuals, so we have to
	// keep looking up the class hierarchy until we find one to invoke.
	if (!CompiledClassAsset->InvokeBindInput(*InObject, *Actor->InputComponent))
	{
		// Although BindInputDelegates() is a no-op for C++ classes, we should still never call it here,
		// if MonoAssemblyProcess is setting the OverridesBindInput flag correctly in the class metadata.
		check(FMonoBindings::Get().GetMonoUnrealClass(GetSuperClass()));
		GetSuperClass()->BindInputDelegates(InObject);
	}
}

bool UMonoUnrealClass::HasLifetimePropertyReplicationList(const class UObject* InObject) const
{
	if (bHasReplicatedProperties)
	{
		check(IsChildOf(AActor::StaticClass()) || IsChildOf(UActorComponent::StaticClass()));
		return true;
	}

	return false;
}

void UMonoUnrealClass::GetLifetimePropertyReplicationList(const class UObject* InObject, TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	check(bHasReplicatedProperties);
	check(CompiledClassAsset);
	TArray<FLifetimeProperty> NewLifetimeProps = CompiledClassAsset->InvokeGetLifetimeReplicationList(const_cast<UObject&>(*InObject));
	OutLifetimeProps.Reserve(OutLifetimeProps.Num() + NewLifetimeProps.Num());
	for (auto& Prop : NewLifetimeProps)
	{
		if (Prop.Condition == COND_Custom)
		{
			bHasCustomLifetimeReplicatedProperties = true;
		}

		OutLifetimeProps.Add(Prop);
	}
}

bool UMonoUnrealClass::HasCustomLifetimeReplicatedProperties(const class UObject* InObject) const
{
	if (bHasCustomLifetimeReplicatedProperties)
	{
		check(IsChildOf(AActor::StaticClass()) || IsChildOf(UActorComponent::StaticClass()));
		return true;
	}

	return false;
}

void UMonoUnrealClass::GetChangedCustomLifetimeReplicatedProperties(const class UObject* InObject, IRepChangedPropertyTracker& ChangedPropertyTracker) const
{
	check(bHasCustomLifetimeReplicatedProperties);
	check(CompiledClassAsset);
	CompiledClassAsset->InvokeUpdateCustomLifetimeReplicatedProperties(*const_cast<UObject*>(InObject), ChangedPropertyTracker);
}


UObject* UMonoUnrealClass::CreateDefaultObject()
{
	bool bDefaultObjectWillBeCreated = (nullptr == GetDefaultObject(false));

	UObject* CreatedCDO = UClass::CreateDefaultObject();

	if (nullptr != CreatedCDO && bDefaultObjectWillBeCreated)
	{
		// Set bCanEverTick if ReceiveTick is overidden on actors
		AActor* ActorCDO = Cast<AActor>(CreatedCDO);

		if (nullptr != ActorCDO)
		{
			static FName ReceiveTickName(GET_FUNCTION_NAME_CHECKED(AActor, ReceiveTick));

			if (nullptr != FindFunctionByName(ReceiveTickName, EIncludeSuperFlag::ExcludeSuper))
			{
				const bool bOverrideFlags = bOverrideCanTick;

				if (bOverrideFlags)
				{
					ActorCDO->PrimaryActorTick.bCanEverTick = true;
				}
				else if (!ActorCDO->PrimaryActorTick.bCanEverTick)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("ClassName"), FText::FromString(GetName()));
					FMessageLog(NAME_MonoErrors).Warning(FText::Format(LOCTEXT("ClassOverrideReceiveTickError", "Class '{ClassName}' overrides ReceiveTick function, but it can never tick."), Args));
				}
			}
		}

	}
	return CreatedCDO;
}

#if MONO_WITH_HOT_RELOADING
void UMonoUnrealClass::SetClassHash(const FString& InHash)
{
	check(InHash.Len() > 0);
	ClassHash = InHash;
}

void UMonoUnrealClass::SetDeletedDuringHotReload(bool bIsDeleted)
{
	bDeletedDuringHotReload = bIsDeleted;
}

void UMonoUnrealClass::HotReload(UClass* InSuperClass,
	UClass* InNativeParentClass,
	TUniquePtr<FMonoCompiledClassAsset>&& InCompiledClassAsset,
	const FMonoClassMetadata& InMetadata)
{
	// this function is only used when nothing has changed in the class metadata
	// therefore we can assert this is the fact
	check(InNativeParentClass == NativeParentClass);
	check(InSuperClass == GetSuperClass());

	CompiledClassAsset.Reset(InCompiledClassAsset.Release());
	

	HotReloadClassFunctions(InNativeParentClass, InMetadata);
}

void UMonoUnrealClass::HotReloadClassFunctions(UClass* InNativeParentClass, const FMonoClassMetadata& InMetadata)
{
	TArray<UFunction*> OverriddenFunctions = GetClassOveriddenFunctions(*InNativeParentClass, InMetadata);

	TArray<UFunction*> NewOveriddenFunctions;
	TArray<UFunction*> NewManagedFunctions;
	TMap<FName, UFunction*> ExistingOveriddenFunctions;
	TMap<FName, UFunction*> ExistingManagedFunctions;

	for (TFieldIterator<UFunction> It(this, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UFunction* NewFunction = *It;
		if (nullptr != NewFunction->GetSuperFunction())
		{
			ExistingOveriddenFunctions.Add(NewFunction->GetFName(), NewFunction);
			// if a function was deleted, we should have gone down the reinstancing path
			check(OverriddenFunctions.Contains(NewFunction->GetSuperFunction()));
			check(NewFunction->GetNativeFunc() == (FNativeFuncPtr) &UMonoUnrealClass::InvokeMonoEvent);

			NewOveriddenFunctions.Add(NewFunction);
		}
		else
		{
			FName FuncName = NewFunction->GetFName();
			ExistingManagedFunctions.Add(FuncName, NewFunction);
			// if a managed function was deleted, ClassHash should have changed, taking the re-instancing path
			check(nullptr != InMetadata.Functions.FindByPredicate([=](const FMonoFunctionMetadata& CurMetadata) { return CurMetadata.Name == FuncName; }));
			check(NewFunction->GetNativeFunc() == (FNativeFuncPtr) &UMonoUnrealClass::InvokeMonoEvent);
			NewManagedFunctions.Add(NewFunction);
		}
	}

	// verify no new overrides - this should have resulted in a new ClassHash and the reinstancing path being taken
	for (auto&& OverriddenSuperFunction : OverriddenFunctions)
	{
		check(nullptr != ExistingOveriddenFunctions.FindRef(OverriddenSuperFunction->GetFName()));
	}

	// verify no new functions - should have resulted in a new ClassHash and the reinstancing path being taken
	for (auto&& ManagedFunction : InMetadata.Functions)
	{
		check(nullptr != ExistingManagedFunctions.FindRef(ManagedFunction.Name));
	}

	CompiledClassAsset->AddFunctionsToEventMap(this, InNativeParentClass, InMetadata.Functions);
}

#endif // MONO_WITH_HOT_RELOADING

DEFINE_FUNCTION(UMonoUnrealClass::InvokeMonoEvent)
{
	check(Context);

	UMonoUnrealClass& MonoUnrealClass = GetMonoUnrealClassFromClass(Context->GetClass());

#if MONO_WITH_HOT_RELOADING
	if (MonoUnrealClass.bDeletedDuringHotReload)
	{
		// class was deleted
		if (Stack.Node->GetSuperFunction() != nullptr)
		{
			// this is an override, forward to super function
			Stack.Object->ProcessEvent(Stack.Node->GetSuperFunction(), Stack.Locals);
		}
		else
		{
			// we don't need to do anything here. The caller sets up the return value/out parameters to a zeroed state
			// so we'll just always return that
		}
	}
	else
#endif // MONO_WITH_HOT_RELOADING
	{
		check(MonoUnrealClass.CompiledClassAsset);
		MonoUnrealClass.CompiledClassAsset->InvokeMonoEvent(Context, Stack, RESULT_PARAM);
	}
}

void UMonoUnrealClass::MonoClassConstructor(const FObjectInitializer& ObjectInitializer)
{
	UMonoUnrealClass& MonoUnrealClass = GetMonoUnrealClassFromClass(ObjectInitializer.GetClass());

	check(MonoUnrealClass.NativeParentClass);

	// Pull the class flags switcheroo, pretend we're not a native class so our properties and subobjects get initialized properly
	// This is a hack, but the fact is most of the time we want to behave like a native class, 
	// but this is one of the few we don't. A cleaner solution would require intrusive changes to UE4's UObject innards
	EClassFlags OldClassFlags = MonoUnrealClass.ClassFlags;
	MonoUnrealClass.ClassFlags = MonoUnrealClass.ClassFlags&(~(CLASS_Intrinsic | CLASS_Native));

	// call the most derived native class constructor
	(*(MonoUnrealClass.NativeParentClass->ClassConstructor))(ObjectInitializer);

	UObject* Obj = ObjectInitializer.GetObj();
	UClass* Class = Obj->GetClass();

	//the PrimaryActorTick doesn't seem to be copied from the CDO unless in the editor, so do it ourselves
	if (!Obj->HasAnyFlags(RF_ClassDefaultObject))
	{
		AActor* ActorObj = Cast<AActor>(Obj);
		if (nullptr != ActorObj)
		{
			AActor* ActorArchetype = Cast<AActor>(ObjectInitializer.GetArchetype());
			ActorObj->PrimaryActorTick.bCanEverTick = ActorArchetype->PrimaryActorTick.bCanEverTick;
		}
	}

	MonoUnrealClass.ClassFlags = OldClassFlags;

	GIsInManagedConstructor++;
	
#if MONO_WITH_HOT_RELOADING
	// deleted classes there's no managed companion to create!
	if (MonoUnrealClass.bDeletedDuringHotReload)
	{
		UE_LOG(LogMono, Log, TEXT("Attempted to create instance of deleted managed class %s. Creating native-only instance."), *MonoUnrealClass.GetPathName());
	}
	else
#endif // MONO_WITH_HOT_RELOADING
	{
#if MONO_WITH_HOT_RELOADING
		// When blueprint recompiles it will duplicate CDOs using *old* classes, which are implemented in the old domain.
		// This is a UE4 bug, since even with native hot reloading this will lead to executing *old* class ObjectInitializer constructors.
		//
		// It's not a trivial fix (might require a large rewrite of the Blueprint compile path), and until Epic addresses it, we
		// have to swap in the old domain 
		bool bSwapDomain = GIsDuplicatingClassForReinstancing && MonoUnrealClass.GetOutermost() == GetTransientPackage();

		if (bSwapDomain)
		{
			FMonoBindings::Get().HACK_SetOldDomainAsCurrent();
		}
#endif // MONO_WITH_HOT_RELOADING

		check(MonoUnrealClass.CompiledClassAsset);
		// Initialize any non-native properties before calling the mono constructor.
		// This makes sure their data is in a "good state" before the mono constructor 
		// potentially accesses them. We don't call InitProperties because we still have 
		// ObjectInitializer operations to complete in the mono constructor. Instead we simply call
		// InitializeValue, but only if the property isn't zero initialized
		for (UProperty* P = Class->GetClass()->PropertyLink; P; P = P->PropertyLinkNext)
		{
			UMonoUnrealClass *MonoClazz = MonoUnrealClass.CompiledClassAsset->GetBindings().GetMonoUnrealClass(P->GetOwnerClass());
			//Check if property is defined in a mono class and needs init
			if (MonoClazz && !P->HasAnyPropertyFlags(CPF_ZeroConstructor))
			{
				P->InitializeValue_InContainer(Obj);
			}
		}

		// create mono object and call constructor. Registers it with the companion object table
		MonoUnrealClass.CompiledClassAsset->CreateCompanionObject(Obj, ObjectInitializer);

#if MONO_WITH_HOT_RELOADING
		if (bSwapDomain)
		{
			FMonoBindings::Get().HACK_SetNewDomainAsCurrent();
		}
#endif // MONO_WITH_HOT_RELOADING
	}

	check(GIsInManagedConstructor > 0);
	GIsInManagedConstructor--;
}

UObject* UMonoUnrealClass::MonoVTableHelperCtorCaller(FVTableHelper& Helper)
{
	//TODO what else do we need to do?
	//this returns new (EC_InternalUseOnlyConstructor, (UObject*)GetTransientPackage(), NAME_None, RF_NeedLoad | RF_ClassDefaultObject | RF_TagGarbageTemp) UObject(Helper);
	return __VTableCtorCaller(Helper);
}

void UMonoUnrealClass::CheckIfObjectFindIsInConstructor(const TCHAR* SearchString)
{
	UE_CLOG(!GIsInManagedConstructor, LogMono, Fatal, TEXT("ObjectFinders can't be used outside of creation constructors to find %s"), SearchString);
}

UMonoUnrealClass& UMonoUnrealClass::GetMonoUnrealClassFromClass(UClass* Class)
{
	check(Class);

	// UMonoUnrealClass isn't actually included in UE4's reflection system for hacky reasons.
	// Therefore, we can't compare Class' own Class to UMonoUnrealClass.
	// But we can compare the class of the class
	// (e.g. Class->GetClass() is UBlueprintGeneratedClass, which is a UClass, therefore Class->GetClass()->GetClass() == UClass::StaticClass())
	// It's safe to assume that AnyOtherUClassDerivedType::StaticClass()->GetClass() != UMonoUnrealClass::StaticClass()->GetClass()
	// as long as it's not possible to extend a Mono class with an unreflected class.
	const auto NotReflectedClass = StaticClass()->GetClass();
	while (Class->GetClass() != NotReflectedClass)
	{
		Class = Class->GetSuperClass();
		check(Class);
	}

	return *static_cast<UMonoUnrealClass*>(Class);
}

MonoClass* UMonoUnrealClass::GetMonoClass() const
{
	check(CompiledClassAsset);
	return CompiledClassAsset->GetAssetClass();
}

UFunction* UMonoUnrealClass::CreateOverriddenFunction(UFunction* ParentFunction)
{
	FNativeFunctionRegistrar::RegisterFunction(this, TCHAR_TO_ANSI(*ParentFunction->GetName()), (FNativeFuncPtr) &UMonoUnrealClass::InvokeMonoEvent);

	EFunctionFlags FuncFlags = (ParentFunction->FunctionFlags & (FUNC_NetFuncFlags | FUNC_FuncInherit | FUNC_Public | FUNC_Protected | FUNC_Private));
	FuncFlags |= FUNC_Native;

	UFunction* NewFunction = new(EC_InternalUseOnlyConstructor, this, *ParentFunction->GetName(), RF_Public | RF_Transient | RF_MarkAsNative)
		UFunction(FObjectInitializer(), ParentFunction, FuncFlags, ParentFunction->ParmsSize);

	// create parameters.
	// AddCppProperty inserts at the beginning of the property list, so we need to add them backwards to ensure a matching function signature.
	TArray<UProperty*> NewProperties;
	for (TFieldIterator<UProperty> It(ParentFunction, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_Parm))
		{
			UProperty* NewProperty = CastChecked<UProperty>(StaticDuplicateObject(Property, NewFunction, *Property->GetName()));
			NewProperty->ClearPropertyFlags(CPF_AllFlags);
			NewProperty->SetPropertyFlags(Property->GetPropertyFlags());
			NewProperties.Add(NewProperty);

			if (NewProperty->HasAnyPropertyFlags(CPF_OutParm))
			{
				NewFunction->FunctionFlags |= FUNC_HasOutParms;
			}
		}
	}
	for (int i = NewProperties.Num() - 1; i >= 0; --i)
	{
		NewFunction->AddCppProperty(NewProperties[i]);
	}

	NewFunction->Bind();
	NewFunction->StaticLink(true);
#if WITH_METADATA
	UMetaData::CopyMetadata(ParentFunction, NewFunction);
#endif // WITH_METADATA

	verify(NewFunction->IsSignatureCompatibleWith(ParentFunction));

	NewFunction->Next = Children;
	Children = NewFunction;

	AddFunctionToFunctionMap(NewFunction, NewFunction->GetFName());
	return NewFunction;
}

void UMonoUnrealClass::GenerateClassOverriddenFunctions(const TArray<UFunction*>& OverriddenFunctions)
{
	TArray<UFunction*> NewFunctions;

	NewFunctions.Empty(OverriddenFunctions.Num());

	for (auto ParentFunction : OverriddenFunctions)
	{
		UFunction* NewFunction = CreateOverriddenFunction(ParentFunction);
		NewFunctions.Add(NewFunction);
	}
}

void UMonoUnrealClass::GenerateClassProperties(FMonoBindings& InBindings, const TArray<FMonoPropertyMetadata>& PropertyMetadata)
{
	const FMonoPropertyFactory& PropertyFactory = FMonoPropertyFactory::Get();
	// Parameter creation order dictates the order of insertion at the head of the property list during linking, so create
	// in reverse order to ensure that the native property order matches the manged declaration order.
	TArray<UProperty*> ParamProperties;
	for (int i = PropertyMetadata.Num() - 1; i >= 0; --i)
	{
		const FMonoPropertyMetadata& Metadata = PropertyMetadata[i];
		UProperty* Property = PropertyFactory.Create(*this, InBindings, Metadata);
		if (Property)
		{
			if (Property->ContainsInstancedObjectProperty())
			{
				ClassFlags |= CLASS_HasInstancedReference;
			}
			if (Property->HasAllPropertyFlags(CPF_Net))
			{
				bHasReplicatedProperties = true;
				if (Property->HasAllPropertyFlags(CPF_RepNotify))
				{
					check(Metadata.RepNotifyFunctionName != NAME_None);
					Property->RepNotifyFunc = Metadata.RepNotifyFunctionName;
				}
			}
			else
			{
				check(!Property->HasAnyPropertyFlags(CPF_RepNotify));
			}
		}
	}
}

UFunction* UMonoUnrealClass::CreateFunction(FMonoBindings& InBindings, const FMonoFunctionMetadata& FunctionInfo)
{
	FNativeFunctionRegistrar::RegisterFunction(this, TCHAR_TO_ANSI(*FunctionInfo.NameCaseSensitive), (FNativeFuncPtr) &UMonoUnrealClass::InvokeMonoEvent);

	EFunctionFlags FuncFlags = FunctionInfo.GetFunctionFlags();

	UFunction* NewFunction = new(EC_InternalUseOnlyConstructor, this, FunctionInfo.Name, RF_Public | RF_Transient | RF_MarkAsNative)
		UFunction(FObjectInitializer(), nullptr, FuncFlags);

	const FMonoPropertyFactory& PropertyFactory = FMonoPropertyFactory::Get();

	UProperty* ReturnProperty = nullptr;
	if (FunctionInfo.ReturnValueProperty.UnrealPropertyType)
	{
		ReturnProperty = PropertyFactory.Create(*NewFunction, InBindings, FunctionInfo.ReturnValueProperty);
		check(ReturnProperty);
		ReturnProperty->PropertyFlags |= CPF_Parm | CPF_OutParm | CPF_ReturnParm;
	}

	// Parameter creation order dictates the order of insertion at the head of the property list during linking, so create
	// in reverse order to get a list of all parameters, in order, followed by the return value.
	TArray<UProperty*> ParamProperties;
	for (int i = FunctionInfo.ParamProperties.Num() - 1; i >= 0; --i)
	{
		const FMonoPropertyMetadata& Parameter = FunctionInfo.ParamProperties[i];
		UProperty* ParamProperty = PropertyFactory.Create(*NewFunction, InBindings, Parameter);
		check(ParamProperty);
		ParamProperty->PropertyFlags |= CPF_Parm;
		//TODO: ref/out params
		ParamProperties.Add(ParamProperty);
	}

	NewFunction->Bind();
	NewFunction->StaticLink(true);

#if WITH_METADATA
	for (auto Pair : FunctionInfo.Metadata)
	{
		NewFunction->SetMetaData(Pair.Key, *Pair.Value);
	}
#endif // WITH_METADATA

	NewFunction->Next = Children;
	Children = NewFunction;

	AddFunctionToFunctionMap(NewFunction, NewFunction->GetFName());
	return NewFunction;
}

void UMonoUnrealClass::GenerateClassFunctions(FMonoBindings& InBindings, const TArray<FMonoFunctionMetadata>& FunctionMetadata)
{
	TArray<UFunction*> NewFunctions;
	for (auto& FunctionInfo : FunctionMetadata)
	{
		UFunction* NewFunction = CreateFunction(InBindings, FunctionInfo);
		NewFunctions.Add(NewFunction);
	}
}

#if WITH_EDITOR

FString UMonoUnrealClass::GetQualifiedName() const
{
	return CompiledClassAsset->GetQualifiedName();
}

FString UMonoUnrealClass::GetNamespace() const
{
	return CompiledClassAsset->GetNamespace();
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE