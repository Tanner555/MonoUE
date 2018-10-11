// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreTypes.h"
#include "MonoRuntimePrivate.h"
#include "UObject/Object.h"
#include <mono/metadata/object.h>

class FGarbageCollectionTracer;

class FMonoDelegateHandle;
class FMonoDomain;

enum EMonoObjectHandleState : uint8
{
	// object handle is reset
	Reset=0,
	// object handle is set to a wrapper
	Wrapper,
	// object handle is set to a companion that is definitely alive
	Companion_Alive,
	// object handle is set to a companion that is possibly dead (no Unreal refs, may have managed refs)
	Companion_PossiblyDead
};

struct FMonoObjectHandle
{
	FMonoObjectHandle();

	FMonoObjectHandle(MonoObject* TargetObject, bool InIsCompanion);
	FMonoObjectHandle(FMonoObjectHandle&& Other);
	~FMonoObjectHandle();

	FMonoObjectHandle& operator=(FMonoObjectHandle&& Other);

	MonoObject* GetTargetObject() const;

	bool IsWrapper() const;
	bool IsCompanion() const;
	
	void MarkCompanionAsPossiblyDead();
	void MarkCompanionAsAlive();

	FMonoObjectHandle(const FMonoObjectHandle&) = delete;
	FMonoObjectHandle& operator=(const FMonoObjectHandle&) = delete;

private:
	void Reset();

	uint32_t GCHandle;
	EMonoObjectHandleState State;
};


class FMonoObjectTable
{
public:
	FMonoObjectTable();
	FMonoObjectTable(FMonoObjectTable&& Other);
	~FMonoObjectTable();

	// move assignment
	FMonoObjectTable& operator=(FMonoObjectTable&& Other);

	void Initialize(FMonoDomain& InDomain, MonoMethod* InClearNativePointerMethod);

	void AddWrapperObject(UObject& InObject, MonoObject* WrapperObject);
	void AddCompanionObject(UObject& InObject, MonoObject* CompanionObject);

	MonoObject* GetManagedObject(UObject& InObject) const;
	void RemoveObject(UObject& InObject);

	void RegisterObjectDelegate(UObject& InObject, FMonoDelegateHandle& InDelegateHandle);
	void UnregisterObjectDelegates(UObject& InObject);
	void UnregisterAllObjectDelegates();

#if MONO_WITH_HOT_RELOADING
	void ResetForReload();

	void GetObjectsWithCompanions(TArray<UObject*>& OutObjects) const;

#endif // MONO_WITH_HOT_RELOADING
	// hide copy constructor/assignment

	FMonoObjectTable(const FMonoObjectTable&) = delete;
	FMonoObjectTable& operator=(const FMonoObjectTable&) = delete;

private:
	void ResetHandle(FMonoObjectHandle& InHandle) const;
	void ClearNativePointer(MonoObject* InObject) const;

	void OnTraceExternalRootsForReachabilityAnalysis(FGarbageCollectionTracer& Tracer, EObjectFlags KeepFlags, bool bForceSingleThreaded);

	void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);

	void AddDelegates();
	void RemoveDelegates();

	TMap<UObject*, FMonoObjectHandle> UnrealObjectToMonoObjectHandleMap;
	TMap<UObject*, TArray<TSharedRef<FMonoDelegateHandle>>> RegisteredDelegateMap;

	FMonoDomain* Domain;
	MonoMethod* ClearNativePointerMethod;

	FDelegateHandle OnWorldCleanupHandle;
	FDelegateHandle TraceRootsHandle;

};