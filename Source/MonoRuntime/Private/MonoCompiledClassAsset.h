// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/CoreNet.h"

#include "MonoRuntimePrivate.h"

#include <mono/metadata/class.h>
#include "MonoAssemblyMetadata.h"

class FMonoBindings;
class UInputComponent;
class UMonoUnrealClass;

class FMonoCompiledClassAsset
{
public:
	static TUniquePtr<FMonoCompiledClassAsset> CreateCompiledClassAsset(FString& ErrorString, FMonoBindings& InBindings, MonoClass* InAssetClass);

	void CreateCompanionObject(UObject* NativeObject, const FObjectInitializer& ObjectInitializer) const;

	void InvokeMonoEvent(UObject* Object, FFrame& TheStack, RESULT_DECL);
	bool InvokeBindInput(UObject& Object, UInputComponent& InputComponent);
	TArray<FLifetimeProperty> InvokeGetLifetimeReplicationList(UObject& Object);
	void InvokeUpdateCustomLifetimeReplicatedProperties(UObject& Object, IRepChangedPropertyTracker& ChangedPropertyTracker);

	FString GetQualifiedName() const;
	FString GetName() const;
	FString GetNamespace() const;

	MonoClass* GetAssetClass() const { return AssetClass;  }

#if MONO_WITH_HOT_RELOADING
	MonoMethod* GetAssetNativeConstructor() const { return AssetNativeConstructor;  }
#endif // MONO_WITH_HOT_RELOADING

	FMonoBindings& GetBindings() const { return Bindings;  }

	void AddFunctionsToEventMap(UMonoUnrealClass* Class, UClass* NativeParentClass, const TArray<FMonoFunctionMetadata>& FunctionMetadata);

private:
	FMonoCompiledClassAsset(FMonoBindings& InBindings, 
							MonoClass* InAssetClass, 
							MonoMethod* InAssetConstructor
#if MONO_WITH_HOT_RELOADING
							, MonoMethod* InAssetNativeConstructor
#endif // MONO_WITH_HOT_RELOADING
							);

	FMonoBindings& Bindings;
	MonoClass* AssetClass;
	MonoMethod* AssetConstructor;
	MonoMethod* BindInputMethod;
#if MONO_WITH_HOT_RELOADING
	MonoMethod* AssetNativeConstructor;
#endif // MONO_WITH_HOT_RELOADING
	TMap<UFunction*, MonoMethod*> MonoEventMap;
};