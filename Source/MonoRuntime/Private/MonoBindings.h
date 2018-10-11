// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Object.h"
#include "Delegates/Delegate.h"
#include "UObject/Package.h"

#include "MonoCachedAssembly.h"
#include "MonoDomain.h"
#include "IMonoRuntime.h"
#include "MonoHelpers.h"
#include "MonoObjectTable.h"
#include "MonoAssemblyMetadata.h"
#include "MonoUnrealClass.h"

#include <mono/metadata/class.h>
#include <mono/metadata/object.h>

class FMonoCompiledClassAsset;
class FMonoMainDomain;

struct FMonoClassInfo;
struct FMonoClassMetadata;
struct FMonoLoadedAssemblyMetadata;
struct FMonoTypeReferenceMetadata;

class MONORUNTIME_API FMonoBindings : public FMonoDomain
{
public:
	virtual ~FMonoBindings();

	static FMonoBindings& Get();
	
	static FMonoBindings* CreateMonoBindings(FMonoMainDomain& InMainDomain, const FString& InEngineAssemblyDirectory, const FString& InGameAssemblyDirectory);

	// open message log for any runtime errors or warnings
	static void ShowAnyErrorsOrWarnings();
	// notification when an exception has been sent to the message log
	void OnExceptionSentToMessageLog();

#if MONO_WITH_HOT_RELOADING
	// ReloadDomain - dumps current app domain, reloads all assemblies, re-creates managed companion objects
	bool ReloadDomain();
	DECLARE_DERIVED_EVENT(FMonoBindings, IMonoRuntime::FStopPIEForHotReloadEvent, FStopPIEForHotReloadEvent);
	FStopPIEForHotReloadEvent& GetOnStopPIEForHotReloadEvent() { return StopPIEForHotReloadEvent;  }

	DECLARE_DERIVED_EVENT(FMonoBindings, IMonoRuntime::FHotReloadEvent, FHotReloadEvent);
	FHotReloadEvent& GetOnHotReloadEvent() { return HotReloadEvent; }

	// hack methods for blueprint reinstancing
	void HACK_SetOldDomainAsCurrent();
	void HACK_SetNewDomainAsCurrent();

#endif // MONO_WITH_HOT_RELOADING

	bool InitializeMonoClasses();

	MonoClass* GetNameClass() const { return RuntimeState.NameClass;  }

	MonoClass* GetLifetimeReplicatedPropertyClass() const { return RuntimeState.LifetimeReplicatedPropertyClass; }
	MonoMethod* GetLifetimeReplicationListMethod() const { return RuntimeState.GetLifetimeReplicationListMethod; }
	MonoMethod* GetCustomReplicationListMethod() const { return RuntimeState.GetCustomReplicationListMethod; }

	void ThrowUnrealObjectDestroyedException(const FString& Message);

	MonoObject* GetUnrealObjectWrapper(UObject* InObject) const;

	MonoClass* GetMonoClassFromUnrealClass(const UClass& InClass) const;

	UField* GetUnrealTypeFromMonoType(MonoType* InMonoType);

	static UPackage* GetPackageFromNamespaceAndAssembly(bool& bIsBindingsAssembly, const FString& InNamespace, const FString& InAssemblyName = TEXT(""));

	UClass* GetUnrealClassFromTypeReference(const FMonoTypeReferenceMetadata& TypeReference);
	UClass* GetUnrealClassFromType(MonoType* InMonoType);

	UScriptStruct* GetUnrealStructFromTypeReference(const FMonoTypeReferenceMetadata& TypeReference);
	UScriptStruct* GetUnrealStructFromType(MonoType* InMonoType);

	void CreateCompanionObject(UObject* InObject, MonoClass* Class, MonoMethod* Method, const FObjectInitializer& ObjectInitializer);
	
	TSharedRef<FMonoDelegateHandle> CreateObjectDelegate(UObject& InOwner, MonoObject* Delegate, UObject* OptionalTargetObject);

	const FCachedAssembly& GetBindingsAssembly() const { return *RuntimeState.MonoBindingsAssembly; }
	const FCachedAssembly& GetRuntimeAssembly() const { return *RuntimeState.MonoRuntimeAssembly; }

#if DO_CHECK

	//returns the UMonoUnrealClass if it is one, else NULL
	const UMonoUnrealClass* GetMonoUnrealClass(const UClass* InClass) const;
	UMonoUnrealClass* GetMonoUnrealClass(UClass* InClass) const;

#endif // DO_CHECK

private:
	FMonoBindings(FMonoMainDomain& InMainDomain, const FString& InEngineAssemblyDirectory, const FString& InGameAssemblyDirectory);

	class CachedUnrealClass
	{
	public:
		CachedUnrealClass();

		bool Resolve(const FCachedAssembly& CachedAssembly, MonoClass* ManagedClass, MonoClass* ManagedWrapperClass, UClass& UnrealClass);

		MonoObject* ConstructUnrealObjectWrapper(const FMonoBindings& InBindings, UObject& InObject) const;

		MonoClass* GetClass() const { return Class; }
		MonoClass* GetWrapperClass() const { return WrapperClass; }

	private:
		MonoClass* Class; // class type corresponding to the unreal class, may be abstract
		// Sometimes we need to create wrappers for abstract unreal classes. This can happen when we have the abstract base exposed in the bindings
		// but the concrete derived class is unknown to our bindings. For these cases we create an object that is a concrete wrapper of the abstract base
		// For non-abstract classes the wrapper class is the same as Class
		MonoClass* WrapperClass; 
		MonoMethod* NativeWrapperConstructor;
	};

	class DeferredUnrealTypeCreationInfo
	{
	public:
		virtual ~DeferredUnrealTypeCreationInfo();

		DeferredUnrealTypeCreationInfo(const TSharedPtr<struct FMonoAssemblyMetadata>& InAssemblyMetadata, 
										const FMonoTypeReferenceMetadata& InTypeReferenceMetadata, 
										const TSharedPtr<FCachedAssembly>& InCachedAssembly,
										const FString& InCleanMetaDataFileName);

		void AssociateWithScriptPackage(UPackage& InScriptPackage);

		UField* Resolve(FMonoBindings& InBindings);
		bool IsResolving() const { return (ResolveCount > 0); }

	protected:
		virtual void CreateType(FMonoBindings& InBindings) = 0;

	private:
		// we hold on to a reference to the assembly metadata because it owns the ClassMetadata below
		// if we didn't have this reference it would be destroyed
		TSharedPtr<struct FMonoAssemblyMetadata> AssemblyMetadata;

		const FMonoTypeReferenceMetadata* TypeReferenceMetadata;

		UPackage* ScriptPackage;
		TSharedPtr<FCachedAssembly> CachedAssembly;
		FString CleanMetadataFileName;

		UField* CreatedType;

		uint8 ResolveCount;

		friend class FMonoBindings;
	};

	class DeferredUnrealClassCreationInfo : public DeferredUnrealTypeCreationInfo
	{
	public:
		DeferredUnrealClassCreationInfo(const TSharedPtr<struct FMonoAssemblyMetadata>& InAssemblyMetadata,
										const FMonoTypeReferenceMetadata& InClassMetadata,
										UClass& InNativeParentClass,
										const TSharedPtr<FCachedAssembly>& InCachedAssembly,
										const FString& InCleanMetaDataFileName)
		: DeferredUnrealTypeCreationInfo(InAssemblyMetadata, InClassMetadata, InCachedAssembly, InCleanMetaDataFileName)
		, NativeParentClass(&InNativeParentClass)
		{

		}

		const FMonoClassMetadata* GetMetadata() const { return static_cast<const FMonoClassMetadata*>(TypeReferenceMetadata); }

	protected:
		void CreateType(FMonoBindings& InBindings) override;

	private:

		// the first native base class
		UClass* NativeParentClass;

		friend class FMonoBindings;
	};

	class DeferredUnrealStructCreationInfo : public DeferredUnrealTypeCreationInfo
	{
	public:
		DeferredUnrealStructCreationInfo(const TSharedPtr<struct FMonoAssemblyMetadata>& InAssemblyMetadata,
			const FMonoStructMetadata& InClassMetadata,
			const TSharedPtr<FCachedAssembly>& InCachedAssembly,
			const FString& InCleanMetaDataFileName)
			: DeferredUnrealTypeCreationInfo(InAssemblyMetadata, InClassMetadata, InCachedAssembly, InCleanMetaDataFileName)
		{

		}

		const FMonoStructMetadata* GetMetadata() const { return static_cast<const FMonoStructMetadata*>(TypeReferenceMetadata); }

	protected:
		void CreateType(FMonoBindings& InBindings) override;
	};

	class DeferredUnrealEnumCreationInfo : public DeferredUnrealTypeCreationInfo
	{
	public:
		DeferredUnrealEnumCreationInfo(const TSharedPtr<struct FMonoAssemblyMetadata>& InAssemblyMetadata,
			const FMonoEnumMetadata& InClassMetadata,
			const TSharedPtr<FCachedAssembly>& InCachedAssembly,
			const FString& InCleanMetaDataFileName)
			: DeferredUnrealTypeCreationInfo(InAssemblyMetadata, InClassMetadata, InCachedAssembly, InCleanMetaDataFileName)
		{

		}

		const FMonoEnumMetadata* GetMetadata() const { return static_cast<const FMonoEnumMetadata*>(TypeReferenceMetadata); }

	protected:
		void CreateType(FMonoBindings& InBindings) override;
	};

	class UnrealTypeReference
	{
	public:
		UnrealTypeReference();

		explicit UnrealTypeReference(UField& InUnrealType);

		explicit UnrealTypeReference(const TSharedPtr<DeferredUnrealTypeCreationInfo>& DeferredCreation);

		UField* Resolve(FMonoBindings& InBindings);

	private:
		UField* UnrealType;
		TSharedPtr<DeferredUnrealTypeCreationInfo> DeferredCreationInfo;
	};

	// wrap this in a struct so we can cache runtime state before a hot reload
	// this allows us to restore it in case the hot reload fails
	// If you add anything to this class be sure to update the default constructor and move assignment operator!
	struct MonoRuntimeState
	{
		TSharedPtr<FCachedAssembly> MonoBindingsAssembly;
		TSharedPtr<FCachedAssembly> MonoRuntimeAssembly;
		// Map from assembly name to cached assembly (1 to 1)
		TMap<FString, TSharedPtr<FCachedAssembly>> AllAssemblies;
		// Map from script package name to cached bindings assembly (many to 1)
		TMap<FName, TSharedPtr<FCachedAssembly>> ScriptPackageToBindingsAssemblyMap;
		// Map from native class to managed wrapper
		TMap<UClass*, CachedUnrealClass> NativeWrapperMap;
		// Map from mono type to unreal type
		TMap<MonoType*, UnrealTypeReference>		  MonoTypeToUnrealTypeMap;
		// Our mono unreal classes
		TSet<UMonoUnrealClass*>			   MonoClasses;

#if MONO_WITH_HOT_RELOADING
		// Map of user UStructs to struct hashes
		TMap<UScriptStruct*, FString> MonoStructs;
		TMap<UEnum*, FString> MonoEnums;
#endif

		uint32_t	BindingsGCHandle;

		MonoClass*  NameClass;
		MonoClass*	LifetimeReplicatedPropertyClass;
		MonoMethod* LoadAssemblyMethod;
		MonoMethod* FindUnrealClassesInAssemblyMethod;
		MonoMethod* GetLifetimeReplicationListMethod;
		MonoMethod* GetCustomReplicationListMethod;
		int32		ExceptionCount;

		mutable FMonoObjectTable MonoObjectTable; 

		MonoRuntimeState();
		MonoRuntimeState(MonoRuntimeState&& Other);
		~MonoRuntimeState();

		MonoRuntimeState&operator=(MonoRuntimeState&& Other);

		MonoRuntimeState(const MonoRuntimeState&) = delete;
		MonoRuntimeState&operator=(const MonoRuntimeState&) = delete;
	};

#if MONO_WITH_HOT_RELOADING
	template<class T> class TReloadType
	{
	public:
		inline explicit TReloadType(T* InOldType);
		
		T* GetOldType() const { return OldType;  }
		T* GetNewType() const { return NewType; }

		bool IsReinstancedType() const { return bWasMovedToTransientPackage;  }

		inline virtual void MoveToTransientPackage();
		inline void SetNewType(T& InNewType);
		inline virtual void CancelReload();

	protected:
		inline virtual void InternalMoveToTransientPackage(T& InType, const TCHAR* Prefix);

	private:
		T* OldType;
		T* NewType;
		FName PreviousName;
		FName PreviousPackageName;
		EObjectFlags PreviousFlags;
		bool bWasMovedToTransientPackage;
	};

	typedef TReloadType<UScriptStruct> ReloadStruct;

	class ReloadEnum : public TReloadType<UEnum>
	{
	public:
		explicit ReloadEnum(UEnum* InOldEnum);
		virtual void MoveToTransientPackage() override;
		virtual void CancelReload() override;
		void FinishReload();
	private:
		void FixEnumNames();
	};

	class ReloadClass : public TReloadType<UMonoUnrealClass>
	{
	public:
		explicit ReloadClass(UMonoUnrealClass* InOldClass);

		virtual void MoveToTransientPackage() override;
		virtual void CancelReload() override;
		void FinishReload(TArray<UObject*>& ExistingManagedObjects);
		int32 GetChildCount() const;
	protected:
		void InternalMoveToTransientPackage(UMonoUnrealClass& InType, const TCHAR* Prefix) override;

	private:
		FName PreviousCDOName;
		EObjectFlags PreviousCDOFlags;
		int32 ChildCount;
	};

	enum class HACK_CurrentActiveDomain : uint8
	{
		OldDomain,
		NewDomain
	};

	struct ReloadContext
	{
		TArray<UObject*> ManagedObjects;
		MonoRuntimeState CachedRuntimeState;
		MonoDomain*		 CachedPreviousDomain;
		TArray<ReloadStruct> ReloadStructs;
		TArray<ReloadClass> ReloadClasses;
		TArray<ReloadEnum> ReloadEnums;
		TSet<AActor*>		BoundInputActors;
		HACK_CurrentActiveDomain HACK_DomainInMonoBindings;
	};
	void BeginReload(ReloadContext& Context, bool bReinstancing);
	void EndReload(ReloadContext& Context);
	void CancelReload(ReloadContext& Context);
	bool IsReloading() const { return CurrentReloadContext != nullptr;  }
	void RenamePreviousStruct(UScriptStruct& OldStruct);
	void DeferStructReinstance(UScriptStruct& OldStruct, UScriptStruct& NewStruct);
	void RenamePreviousClass(UMonoUnrealClass& OldClass);
	void DeferClassReinstance(UMonoUnrealClass& OldClass, UMonoUnrealClass& NewClass);
	void RenamePreviousEnum(UEnum& OldEnum);
	void DeferEnumReinstance(UEnum& OldEnum, UEnum& NewEnum);

	bool HotReloadRequiresReinstancing(const TArray<FMonoLoadedAssemblyMetadata>& NewMetadata);

	void ReloadDomainCommand();	
#endif // MONO_WITH_HOT_RELOADING
#if WITH_EDITOR
	void BuildMissingAssemblies();
#endif // WITH_EDITOR
	bool InitializeDomain(const TArray<FMonoLoadedAssemblyMetadata>& EngineAssemblyMetadata, const TArray<FMonoLoadedAssemblyMetadata>& GameAssemblyMetadata);

	MonoType* ResolveTypeReference(const FMonoTypeReferenceMetadata& TypeReference) const;

	MonoObject* ConstructUnrealObjectWrapper(UObject& InObject) const;

	void LoadBindingsForScriptPackages(const TSet<FName>& ScriptPackages);
	TSharedPtr<FCachedAssembly> LoadAssembly(FString& ErrorString, const FString& AssemblyName);

	TMap<FName, FString> GetUnloadedScriptPackageBindings(const TSet<FName>& ScriptPackageSet) const;
	void CacheUnrealClassesForAssembly(FName ScriptPackageName, const FCachedAssembly& CachedAssembly);

	bool LoadGameAssemblies(const TArray<FMonoLoadedAssemblyMetadata>& DirectoryMetadata);
	bool LoadGameAssembly(const FMonoLoadedAssemblyMetadata& LoadedMetadata);

	bool CreateGameStruct(DeferredUnrealStructCreationInfo& StructInfo);
	bool CreateGameClass(DeferredUnrealClassCreationInfo& ClassInfo);
	bool CreateGameEnum(DeferredUnrealEnumCreationInfo& EnumInfo);
	static FMonoBindings* GInstance;

	FMonoMainDomain& MainDomain;
	FString EngineAssemblyDirectory;
	FString GameAssemblyDirectory;

	MonoRuntimeState		RuntimeState;

#if MONO_WITH_HOT_RELOADING
	ReloadContext*			 CurrentReloadContext;
	FAutoConsoleCommand					   HotReloadCommand;
	FStopPIEForHotReloadEvent			   StopPIEForHotReloadEvent;
	FHotReloadEvent						   HotReloadEvent;
#endif // MONO_WITH_HOT_RELOADS
};

#include "MonoBindings.inl"