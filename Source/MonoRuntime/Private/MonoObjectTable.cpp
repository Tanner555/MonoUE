// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoObjectTable.h"

#include "CoreMinimal.h"
#include "UObject/GarbageCollection.h"
#include "UObject/FastReferenceCollector.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Stats/StatsMisc.h"
#include "Engine/World.h"
#include "UObject/Object.h"
#include "UObject/Package.h"

#include "MonoRuntimeCommon.h"
#include "MonoHelpers.h"
#include "MonoDelegateHandle.h"

#include <mono/metadata/mono-gc.h>

FMonoObjectHandle::FMonoObjectHandle()
	: GCHandle(0)
	, State(EMonoObjectHandleState::Reset)
{

}

FMonoObjectHandle::FMonoObjectHandle(FMonoObjectHandle&& Other)
{
	GCHandle = Other.GCHandle;
	Other.GCHandle = 0;
	State = Other.State;
	Other.State = EMonoObjectHandleState::Reset;
}

FMonoObjectHandle::FMonoObjectHandle(MonoObject* TargetObject, bool InIsCompanion)
	: GCHandle(InIsCompanion ? mono_gchandle_new(TargetObject, false) : mono_gchandle_new_weakref(TargetObject, false))
	, State(InIsCompanion ? EMonoObjectHandleState::Companion_Alive : EMonoObjectHandleState::Wrapper)
{

}

FMonoObjectHandle::~FMonoObjectHandle()
{
	Reset();
}

FMonoObjectHandle& FMonoObjectHandle::operator=(FMonoObjectHandle&& Other)
{
	if (this == &Other)
	{
		return *this;
	}

	Reset();
	GCHandle = Other.GCHandle;
	Other.GCHandle = 0;
	State = Other.State;
	Other.State = EMonoObjectHandleState::Reset;

	return *this;
}

MonoObject* FMonoObjectHandle::GetTargetObject() const
{
	check(State != EMonoObjectHandleState::Reset);
	check(GCHandle != 0);
	return mono_gchandle_get_target(GCHandle);
}

bool FMonoObjectHandle::IsWrapper() const
{
	check(State != EMonoObjectHandleState::Reset);
	return State == EMonoObjectHandleState::Wrapper;
}
bool FMonoObjectHandle::IsCompanion() const 
{ 
	check(State != EMonoObjectHandleState::Reset);
	return State == EMonoObjectHandleState::Companion_Alive || State == EMonoObjectHandleState::Companion_PossiblyDead;
}

void FMonoObjectHandle::MarkCompanionAsPossiblyDead()
{
	check(State == EMonoObjectHandleState::Companion_Alive);

	// switch to weak handle
	MonoObject* Target = GetTargetObject();
	check(nullptr != Target);
	uint32_t NewWeakGCHandle = mono_gchandle_new_weakref(Target, false);
	check(GCHandle != 0);
	mono_gchandle_free(GCHandle);
	GCHandle = NewWeakGCHandle;

	State = EMonoObjectHandleState::Companion_PossiblyDead;
}

void FMonoObjectHandle::MarkCompanionAsAlive()
{
	if (State != EMonoObjectHandleState::Companion_Alive)
	{
		check(State == EMonoObjectHandleState::Companion_PossiblyDead);
		// switch back to strong handle
		MonoObject* Target = GetTargetObject();
		check(nullptr != Target);
		uint32_t NewStrongGCHandle = mono_gchandle_new(Target, false);
		check(GCHandle != 0);
		mono_gchandle_free(GCHandle);
		GCHandle = NewStrongGCHandle;

		State = EMonoObjectHandleState::Companion_Alive;
	}
}

void FMonoObjectHandle::Reset()
{
	if (GCHandle != 0)
	{
		check(State != EMonoObjectHandleState::Reset);
		mono_gchandle_free(GCHandle);
		GCHandle = 0;
		State = EMonoObjectHandleState::Reset;
	}
	else
	{
		check(State == EMonoObjectHandleState::Reset);
	}

}

FMonoObjectTable::FMonoObjectTable()
	: Domain(nullptr)
	, ClearNativePointerMethod(nullptr)
{
	AddDelegates();
}

FMonoObjectTable::FMonoObjectTable(FMonoObjectTable&& Other)
{
	*this = MoveTemp(Other);
	AddDelegates();
}

FMonoObjectTable::~FMonoObjectTable()
{
	UnrealObjectToMonoObjectHandleMap.Empty();

	RemoveDelegates();
}

FMonoObjectTable& FMonoObjectTable::operator=(FMonoObjectTable&& Other)
{
	if (this == &Other)
	{
		return *this;
	}

	Domain = Other.Domain;
	Other.Domain = nullptr;
	ClearNativePointerMethod = Other.ClearNativePointerMethod;
	Other.ClearNativePointerMethod = nullptr;
	// Moving a TMap which contains a move-only value fails to compile right now, Epic is looking into it
	// Manually move over the elements
	UnrealObjectToMonoObjectHandleMap.Empty(Other.UnrealObjectToMonoObjectHandleMap.Num());

	for (auto&& Pair : Other.UnrealObjectToMonoObjectHandleMap)
	{
		UnrealObjectToMonoObjectHandleMap.Add(Pair.Key, MoveTemp(Pair.Value));
	}
	Other.UnrealObjectToMonoObjectHandleMap.Empty();

	RegisteredDelegateMap = MoveTemp(Other.RegisteredDelegateMap);

	return *this;
}

void FMonoObjectTable::Initialize(FMonoDomain& InDomain, MonoMethod* InClearNativePointerMethod)
{
	Domain = &InDomain;
	check(UnrealObjectToMonoObjectHandleMap.Num() == 0);

	check(InClearNativePointerMethod);
	ClearNativePointerMethod = InClearNativePointerMethod;
}

void FMonoObjectTable::AddWrapperObject(UObject& InObject, MonoObject* WrapperObject)
{
	check(WrapperObject);
	// see if it's already in the table
	FMonoObjectHandle* Handle = UnrealObjectToMonoObjectHandleMap.Find(&InObject);
	if (nullptr != Handle)
	{
		check(Handle->IsWrapper());
		// free the existing handle (no way to set a new value unfortunately)
		*Handle = FMonoObjectHandle(WrapperObject, false);
	}
	else
	{
		UnrealObjectToMonoObjectHandleMap.Add(&InObject, FMonoObjectHandle(WrapperObject, false));
	}
}

void FMonoObjectTable::AddCompanionObject(UObject& InObject, MonoObject* CompanionObject)
{
	check(CompanionObject);
	//if this fails, check for subobjects/components in managed CDO creation that are accessing their
	// parent (and creating a wrapper, since the parent's companion object isn't set yet)
	check(nullptr == UnrealObjectToMonoObjectHandleMap.Find(&InObject));

	// companions currently have a strong ref to their  managed object
	UnrealObjectToMonoObjectHandleMap.Add(&InObject, FMonoObjectHandle(CompanionObject, true));
}

MonoObject* FMonoObjectTable::GetManagedObject(UObject& InObject) const
{
	const FMonoObjectHandle* Handle = UnrealObjectToMonoObjectHandleMap.Find(&InObject);

	if (nullptr == Handle)
	{
		return nullptr;
	}
	else
	{
		MonoObject* ManagedObject = Handle->GetTargetObject(); ;
		// only wrappers should have weak refs that get null'd out
		check(nullptr != ManagedObject || Handle->IsWrapper());
		return ManagedObject;
	}
}

void FMonoObjectTable::RemoveObject(UObject& InObject)
{
	FMonoObjectHandle* Handle = UnrealObjectToMonoObjectHandleMap.Find(&InObject);

	// it's ok for this to be not in the table, it may have been removed during a gc
	if (nullptr != Handle)
	{
		ClearNativePointer(Handle->GetTargetObject());
		UnrealObjectToMonoObjectHandleMap.Remove(&InObject);
	}

	UnregisterObjectDelegates(InObject);
}

void FMonoObjectTable::RegisterObjectDelegate(UObject& InObject, FMonoDelegateHandle& InDelegateHandle)
{
	// This is only supported on actors and components right now (things that are marked pending kill)
	check(InObject.IsA(AActor::StaticClass()) || InObject.IsA(UActorComponent::StaticClass()));

	TArray<TSharedRef<FMonoDelegateHandle>>* DelegateArray = RegisteredDelegateMap.Find(&InObject);

	if (nullptr == DelegateArray)
	{
		DelegateArray = &RegisteredDelegateMap.Add(&InObject);
	}
	DelegateArray->Add(InDelegateHandle.AsShared());
}

void FMonoObjectTable::UnregisterObjectDelegates(UObject& InObject)
{
	RegisteredDelegateMap.Remove(&InObject);
}

void FMonoObjectTable::UnregisterAllObjectDelegates()
{
	RegisteredDelegateMap.Empty();
}

#if MONO_WITH_HOT_RELOADING
void FMonoObjectTable::ResetForReload()
{
	// toss wrappers before saving state, but leave companions. Wrappers will be reconstructed on demand
	for (TMap<UObject*, FMonoObjectHandle>::TIterator It(UnrealObjectToMonoObjectHandleMap); It; ++It)
	{
		FMonoObjectHandle& Handle = It.Value();
		// toss wrapper objects, preserve companions
		if (Handle.IsWrapper())
		{
			// release the GC handle
			// do not clear native pointer (object is still valid)
			// do not unregister delegates
			// TODO: should we just treat wrappers and companions the same? Probably
			It.RemoveCurrent();
		}
	}
}

void FMonoObjectTable::GetObjectsWithCompanions(TArray<UObject*>& OutObjects) const
{
	for (TMap<UObject*, FMonoObjectHandle>::TConstIterator It(UnrealObjectToMonoObjectHandleMap); It; ++It)
	{
		const FMonoObjectHandle& Handle = It.Value();
		// only companions should be left
		check(Handle.IsCompanion());
		// temporarily add native object to root set
		UObject* Object = It.Key();
		// we shouldn't have any pending kill objects left in our map, GC should have been run beforehand!
		check(!Object->IsPendingKill());
		OutObjects.Add(Object);
	}
	
}

#endif // MONO_WITH_HOT_RELOADING

void FMonoObjectTable::ClearNativePointer(MonoObject* Target) const
{
	if (nullptr != Target)
	{
		check(Domain);
		check(ClearNativePointerMethod);
		// clear the native point on the mono object, it has been destroyed
		Mono::Invoke<void>(*Domain, ClearNativePointerMethod, Target);
	}
}

void FMonoObjectTable::OnTraceExternalRootsForReachabilityAnalysis(FGarbageCollectionTracer& Tracer, EObjectFlags KeepFlags, bool bForceSingleThreaded)
{
	FGCArrayStruct* ArrayStruct = FGCArrayPool::Get().GetArrayStructFromPool();
	TArray<UObject*>& ObjectsToSerialize = ArrayStruct->ObjectsToSerialize;

	ObjectsToSerialize.Empty(UnrealObjectToMonoObjectHandleMap.Num());

	double TraceExternalRootsTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(TraceExternalRootsTime);

		bool bAnyPossiblyDead = false;

		// this is called after UE4's gc has done a full reachability analysis of its graph.
		// process our object table. Any companions which are reachable must be roots in GC (by default they have strong references)
		// Any companions which are unreachable should be converted to weak refs
		for (TMap<UObject*, FMonoObjectHandle>::TIterator It(UnrealObjectToMonoObjectHandleMap); It; ++It)
		{
			UObject* ReferencedObject = It.Key();
			FMonoObjectHandle& Handle = It.Value();


			if (ReferencedObject->IsPendingKill())
			{
				// pending kill objects have been forcibly killed by unreal's gc
				// so we always kill them
				// clear the native pointer on the object
				ClearNativePointer(Handle.GetTargetObject());
				// remove any registered delegates
				UnregisterObjectDelegates(*ReferencedObject);
				It.RemoveCurrent();
			}
			else if (Handle.IsCompanion())
			{
				// these should have been dealt with in AddReferencedObjects
				if (ReferencedObject->IsUnreachable ())
				{
					Handle.MarkCompanionAsPossiblyDead();
					bAnyPossiblyDead = true;
				}
			}
		}

		// run the full mono GC
		double MonoGCTime = 0.0;
		{
			SCOPE_SECONDS_COUNTER(MonoGCTime)
				// not using a time limit, force a full mono GC
				mono_gc_collect(mono_gc_max_generation());
		}
		if (MonoGCTime > 0.0)
		{
			UE_LOG(LogMono, Log, TEXT("Managed garbage collection took %g ms"), MonoGCTime*1000.0);
		}

		// now mono has run its gc, check if any of our companions or wrappers died
		for (TMap<UObject*, FMonoObjectHandle>::TIterator It(UnrealObjectToMonoObjectHandleMap); It; ++It)
		{
			FMonoObjectHandle& Handle = It.Value();
			UObject* ReferencedObject = It.Key();
			checkSlow(!ReferencedObject->IsPendingKill());

			MonoObject* Target = Handle.GetTargetObject();
			if (nullptr == Target)
			{
				// this is dead, remove it
				if (Handle.IsCompanion())
				{
					UnregisterObjectDelegates(*ReferencedObject);
				}
				It.RemoveCurrent();
			}
			else
			{
				// do not remove it, add it to objects to serialize
				if (Handle.IsCompanion())
				{
					Handle.MarkCompanionAsAlive();
				}
				ObjectsToSerialize.Add(ReferencedObject);
			}
		}
	}

	// now trace in UE4 land
	Tracer.PerformReachabilityAnalysisOnObjects(ArrayStruct, bForceSingleThreaded);

	// see if any objects with delegates were unreachable
	for (TMap<UObject*, TArray<TSharedRef<FMonoDelegateHandle>>>::TIterator It(RegisteredDelegateMap); It; ++It)
	{
		if (It.Key()->IsUnreachable())
		{
			It.RemoveCurrent();
		}
	}

	FGCArrayPool::Get().ReturnToPool(ArrayStruct);

	if (TraceExternalRootsTime > 0.0)
	{
		UE_LOG(LogMono, Log, TEXT("Mono TraceExternalRootsForReachabilityAnalysis took %g ms"), TraceExternalRootsTime*1000.0);
	}
}

void FMonoObjectTable::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	// called when world is cleaning up. Explicitly let go of our references to managed objects in this world.
	// This handles cases like ending PIE where Epic does not mark things pending kill (if Epic was consistent about marking things pending kill, we wouldn't need this)
	// Theoretically, we shouldn't need this, but in practice, we do because Mono's GC does conservative tracing of stacks, objects may hang around that are actually unreferenced
	// We are going to do some things to reduce the cases where this happens (for example, when we run the full mono gc we know the main thread does not have any references on its stack)
	// but that won't remove the need for doing this
	check(InWorld);
	UPackage* Outermost = InWorld->GetOutermost();
	// release objects that are in this world
	for (TMap<UObject*, FMonoObjectHandle>::TIterator It(UnrealObjectToMonoObjectHandleMap); It; ++It)
	{
		UObject* Object = It.Key();		
		if (Object->IsIn(Outermost))
		{
			FMonoObjectHandle& Handle = It.Value();
			// clear out the managed object's reference to this object
			ClearNativePointer(Handle.GetTargetObject());
			UnregisterObjectDelegates(*Object);
			It.RemoveCurrent();
		}
	}
}

void FMonoObjectTable::AddDelegates()
{
	OnWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FMonoObjectTable::OnWorldCleanup);
	TraceRootsHandle = FCoreUObjectDelegates::TraceExternalRootsForReachabilityAnalysis.AddRaw(this, &FMonoObjectTable::OnTraceExternalRootsForReachabilityAnalysis);
}

void FMonoObjectTable::RemoveDelegates()
{
	FWorldDelegates::OnWorldCleanup.Remove(OnWorldCleanupHandle);
	FCoreUObjectDelegates::TraceExternalRootsForReachabilityAnalysis.Remove(TraceRootsHandle);
}