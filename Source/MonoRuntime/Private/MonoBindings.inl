// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoDelegateHandle.h"

// These are implemented here because they are dependent on FMonoBindings being fully declared
namespace Mono
{
    
    template<class T>
	MonoObject* Marshal<T*, typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived>::Type>::Parameter(const FMonoBindings& Bindings, UObject* Object)
    {
        return Bindings.GetUnrealObjectWrapper(Object);
    }
    
    template<class T>
    MonoArray* Marshal<TArray<T*>, typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived>::Type>::Parameter(const FMonoBindings& Bindings, const TArray<T*>& InArray)
    {
        MonoArray* OutArray = mono_array_new(Bindings.GetDomain(), Bindings.GetMonoClassFromUnrealClass(T::StaticClass()), InArray.Num());
        for (int i = 0; i < InArray.Num(); ++i)
        {
            MonoObject* MarshalledValue = Marshal<T*>::Parameter(Bindings, InArray[i]);
            mono_array_setref(OutArray, i, MarshalledValue);
        }
        return OutArray;
        
    }

}

template <class ReturnValue>
ReturnValue FMonoDelegateHandle::Invoke()
{
	if (!bTargetObjectBound || TargetObject.Get() != nullptr)
	{
		MonoObject* DelegateObject = mono_gchandle_get_target(DelegateGCHandle);

		if (nullptr != DelegateObject)
		{
			return Mono::InvokeDelegate<ReturnValue>(Bindings, DelegateObject);
		}
	}
	return ReturnValue();
}

template <class ReturnValue, class Arg1Type>
ReturnValue FMonoDelegateHandle::Invoke(Arg1Type argOne)
{
	if (!bTargetObjectBound || TargetObject.Get() != nullptr)
	{
		MonoObject* DelegateObject = mono_gchandle_get_target(DelegateGCHandle);

		if (nullptr != DelegateObject)
		{
			return Mono::InvokeDelegate<ReturnValue>(Bindings, DelegateObject, argOne);
		}
	}
	return ReturnValue();
}

template <class ReturnValue, class Arg1Type, class Arg2Type>
ReturnValue FMonoDelegateHandle::Invoke(Arg1Type argOne, Arg2Type argTwo)
{
	if (!bTargetObjectBound || TargetObject.Get() != nullptr)
	{
		MonoObject* DelegateObject = mono_gchandle_get_target(DelegateGCHandle);

		if (nullptr != DelegateObject)
		{
			return Mono::InvokeDelegate<ReturnValue>(Bindings, DelegateObject, argOne, argTwo);
		}
	}
	return ReturnValue();
}

#if MONO_WITH_HOT_RELOADING

template<class T>
FMonoBindings::TReloadType<T>::TReloadType(T* InOldType)
	: OldType(InOldType)
	, NewType(nullptr)
	, PreviousFlags(RF_NoFlags)
	, bWasMovedToTransientPackage(false)
{

}

template<class T>
void FMonoBindings::TReloadType<T>::InternalMoveToTransientPackage(T& InType, const TCHAR* Prefix)
{
	InType.ClearFlags(RF_Standalone | RF_Public | RF_Transactional);
	InType.RemoveFromRoot();
	const FName OldClassRename = MakeUniqueObjectName(GetTransientPackage(), InType.GetClass(), *FString::Printf(TEXT("%s_%s"), Prefix, *InType.GetName()));
	InType.Rename(*OldClassRename.ToString(), GetTransientPackage());

	//Mono classes and all heir sub-properties get put in the RootSet by
	//Obj.cpp's MarkObjectsToDisregardForGC(), this causes the "Old" classes to not get GCed.
	//Additionally, all CPP UProperties are constructed with RF_Native. This is a GARBAGE_COLLECTION_KEEP_FLAG
	//And also prevents duplicate classes from being GCed. Here we remove these flags.
	TArray<UObject*> ChildObjects;
	FReferenceFinder References(ChildObjects, &InType, false, false, true);
	References.FindReferences(&InType);
	for (UObject* Obj : ChildObjects)
	{
		Obj->ClearFlags(GARBAGE_COLLECTION_KEEPFLAGS);
		Obj->RemoveFromRoot();
	}
}

template<class T>
void FMonoBindings::TReloadType<T>::MoveToTransientPackage()
{
	check(!bWasMovedToTransientPackage);
	check(OldType);
	check(nullptr == NewType);

	PreviousFlags = OldType->GetFlags() | RF_Standalone | RF_Public;
	PreviousName = OldType->GetFName();
	PreviousPackageName = OldType->GetOutermost()->GetFName();


	InternalMoveToTransientPackage(*OldType, TEXT("MONOHOTRELOAD"));
	OldType->AddToRoot();

	// TODO: de-register enums (call UEnum::RemoveNamesFromMasterList )

	bWasMovedToTransientPackage = true;
}

template<class T>
void FMonoBindings::TReloadType<T>::SetNewType(T& InNewType)
{
	check(NewType == nullptr);
	NewType = &InNewType;
}
template<class T>
void FMonoBindings::TReloadType<T>::CancelReload()
{
	if (bWasMovedToTransientPackage)
	{
		if (NewType)
		{
			InternalMoveToTransientPackage(*NewType, TEXT("MONOABORTEDHOTRELOAD"));
		}
		// restore
		OldType->ClearFlags(RF_Standalone | RF_Public);
		OldType->RemoveFromRoot();

		UPackage* Package = FindPackage(nullptr, *PreviousPackageName.ToString());
		checkf(Package, TEXT("Could not find package %s"), *PreviousPackageName.ToString());

		OldType->Rename(*PreviousName.ToString(), Package);
		OldType->AddToRoot();
		OldType->SetFlags(PreviousFlags);

		// TODO: enum re-registration

		bWasMovedToTransientPackage = false;
	}
}
#endif