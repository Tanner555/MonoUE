// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "MonoRuntimePrivate.h"
#include "MonoAssemblyMetadata.h"
#include <mono/metadata/object.h>
#include <mono/metadata/class.h>

class FMonoBindings;
class FMonoCompiledClassAsset;
struct FMonoClassMetadata;

// HERE LIES EVIL
// At startup, we generate UClasses for managed class deriving from unreal ones.
// We want these classes to effectively be the same as native UClasses, which for the rest of UE4
// means they ARE a UClass. But we also need to extend UClass functionality and add a few members
// Here we take advantage of the fact that UMonoUnrealClasses are allocated with placement new in memory
// we allocated manually, and the constructor is invoked manually (just like UClass is in the generated glue).
// Thus we can derive from UClass C++ side and get all the proper behavior we expect, but from the UE4
// reflection system's point of view this is a UClass. This all works because UMonoUnrealClasses never get ConstructObject<>'d
// or DuplicateObject'd
class UMonoUnrealClass : public UClass
{

public:
	UMonoUnrealClass(UClass* InSuperClass,
					UClass* InNativeParentClass, 
					TUniquePtr<FMonoCompiledClassAsset>&& InCompiledClassAsset, 
					const TCHAR* InClassConfigName,
					const TCHAR* InPackageName,
					const TCHAR* InClassName,
					EClassFlags Flags);

	
	void Initialize(const FMonoClassMetadata& Metadata);

	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;

	virtual bool HasInputDelegateBindings(class UObject* InObject) const override;
	virtual void BindInputDelegates(class UObject* InObject) const override;
	virtual bool HasLifetimePropertyReplicationList(const class UObject* InObject) const override;
	virtual void GetLifetimePropertyReplicationList(const class UObject* InObject, TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	virtual bool HasCustomLifetimeReplicatedProperties(const class UObject* InObject) const;
	virtual void GetChangedCustomLifetimeReplicatedProperties(const class UObject* InObject, IRepChangedPropertyTracker& ChangedPropertyTracker) const;

#if MONO_WITH_HOT_RELOADING
	void SetClassHash(const FString& InHash);
	void SetDeletedDuringHotReload(bool bIsDeleted);
	bool WasDeletedDuringHotReload() const { return bDeletedDuringHotReload;  }

	const FString& GetClassHash() const { return ClassHash;  }

	void HotReload(UClass* InSuperClass,
					UClass* InNativeParentClass, 
					TUniquePtr<FMonoCompiledClassAsset>&& InCompiledClassAsset,
					const FMonoClassMetadata& InMetadata);

	const FMonoCompiledClassAsset& GetCompiledClassAsset() const { check(CompiledClassAsset);  return *CompiledClassAsset; }

#endif // MONO_WITH_HOT_RELOADING

	static void CheckIfObjectFindIsInConstructor(const TCHAR* SearchString);

	static UMonoUnrealClass& GetMonoUnrealClassFromClass(UClass* Class);

	MonoClass* GetMonoClass() const;

#if WITH_EDITOR

	//returns the UMonoUnrealClass if it is one, else NULL
	FString GetQualifiedName() const;
	FString GetNamespace() const;

#endif // WITH_EDITOR

protected:
	virtual UObject* CreateDefaultObject() override;

private:

	void ApplyMetaData(const FMonoClassMetadata& Metadata);

	// Here's the deal. UFunctions contain a pointer to a native member function of UObject
	// When calling into C# land, I want to hijack this routing into my own general purpose handler
	// If it were a non-member function pointer, I could just set it to my own function and be done with it
	// But because it expects to be a pointer to UObject (or UObject derived type), I do some hackery.
	// So in managed UFunctions we set InvokeMonoEvent as the UFunction function pointer, *even though the underlying object is NOT a UMonoUnrealClass. 
	// This works, but it is critical to remember you do not have an FMonoUnrealClass in the this pointer, and instead
	// an arbitrary UObject - i.e. treat this as a static function which can not access any members or member functions safely
	DECLARE_FUNCTION(InvokeMonoEvent);

#if MONO_WITH_HOT_RELOADING
	void HotReloadClassFunctions(UClass* InNativeParentClass, const FMonoClassMetadata& InMetadata);
#endif // MONO_WITH_HOT_RELOADING

	static void MonoClassConstructor(const FObjectInitializer& ObjectInitializer);
	static UObject* MonoVTableHelperCtorCaller(FVTableHelper& Helper);

	UFunction* CreateOverriddenFunction(UFunction* ParentFunction);
	void GenerateClassOverriddenFunctions(const TArray<UFunction*>& OverriddenFunctions);

	void GenerateClassProperties(FMonoBindings& InBindings, const TArray<FMonoPropertyMetadata>& PropertyMetadata);

	UFunction* CreateFunction(FMonoBindings& InBindings, const FMonoFunctionMetadata& FunctionInfo);
	void GenerateClassFunctions(FMonoBindings& InBindings, const TArray<FMonoFunctionMetadata>& FunctionMetadata);

	UClass* NativeParentClass;
	TUniquePtr<FMonoCompiledClassAsset> CompiledClassAsset;
#if MONO_WITH_HOT_RELOADING
	FString ClassHash;
	bool bDeletedDuringHotReload;
#endif // MONO_WITH_HOT_RELOADING
	bool	bOverrideCanTick;
	bool	bOverrideBindsInput;
	bool	bHasReplicatedProperties;
	mutable bool	bHasCustomLifetimeReplicatedProperties;
};
