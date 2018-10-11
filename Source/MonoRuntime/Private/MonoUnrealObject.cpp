// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoRuntimeCommon.h"

#include "CoreMinimal.h"
#include "Templates/SharedPointerInternals.h"
#include "Atmosphere/AtmosphericFogComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Components/SkinnedMeshComponent.h"
#include "Misc/FeedbackContext.h"

#include "MonoBindings.h"
#include "PInvokeSignatures.h"

#include <mono/metadata/exception.h>

#include "MonoUnrealObjectDerived.inl"

static FMarshalledName ConvertToMarshalledName(const FName& InName)
{
	FMarshalledName ReturnName;
#if WITH_CASE_PRESERVING_NAME
	ReturnName.DisplayIndex = InName.GetDisplayIndex();
#endif
	ReturnName.ComparisonIndex = InName.GetComparisonIndex();
	ReturnName.Number = InName.GetNumber();
	return ReturnName;
}

// UnrealObject internal calls
bool UnrealObject_IsPendingKill(UObject* Object)
{
	checkSlow(Object);
	return Object->IsPendingKill();
}

MonoObject* UnrealObject_GetUnrealObjectWrapper(UObject* Object)
{
	return FMonoBindings::Get().GetUnrealObjectWrapper(Object);
}

MonoObject* UnrealObject_ConstructUnrealObject(MonoReflectionType* InReturnType, UClass* InClass, UObject* InOuter, FName InObjectName, EObjectFlags SetFlags, UObject* Template, bool bCopyTransientsFromClassDefaults, struct FObjectInstancingGraph* InstanceGraph, bool bAssumeTemplateIsArchetype)
{
	FMonoBindings& Bindings = FMonoBindings::Get();

	//if the user did not pass a specific class to be instantiated, instantiate the return type
	if (InClass == nullptr)
	{
		InClass = Bindings.GetUnrealClassFromType(mono_reflection_type_get_type(InReturnType));
#if DO_CHECK
		if (InClass == nullptr)
		{
			mono_raise_exception(mono_get_exception_argument("unrealType", "C# type does not map to an Unreal class"));
		}
#endif
	}

	//if the user passed a specific class, check it's compatible with the the return type
#if DO_CHECK
	else
	{
		UClass* ReturnClass = Bindings.GetUnrealClassFromType(mono_reflection_type_get_type(InReturnType));
		if (ReturnClass == nullptr)
		{
			mono_raise_exception(mono_get_exception_argument("unrealType", "C# type does not map to an Unreal class"));
		}
		if (!InClass->IsChildOf(ReturnClass))
		{
			mono_raise_exception(mono_get_exception_argument("unrealType", "Class is not subclass of return type"));
		}
	}
#endif

	if (nullptr == InOuter)
	{
		InOuter = (UObject*)GetTransientPackage();
	}

	UObject* Obj = StaticConstructObject_Internal(InClass, InOuter, InObjectName, SetFlags, EInternalObjectFlags::None, Template, bCopyTransientsFromClassDefaults, InstanceGraph, bAssumeTemplateIsArchetype);

	if (nullptr != Obj)
	{
		return Bindings.GetUnrealObjectWrapper(Obj);
	}
	else
	{
		return nullptr;
	}
}

MonoObject* UnrealObject_GetDefaultObjectFromUnrealClass(UClass* ThisClass)
{
	check(ThisClass);
	FMonoBindings& Bindings = FMonoBindings::Get();
	return Bindings.GetUnrealObjectWrapper(ThisClass->GetDefaultObject());
}

MonoObject* UnrealObject_GetDefaultObjectFromMonoType(MonoReflectionType* InUnrealType)
{
	FMonoBindings& Bindings = FMonoBindings::Get();

	UClass* UnrealClass = Bindings.GetUnrealClassFromType(mono_reflection_type_get_type(InUnrealType));
	if (nullptr == UnrealClass)
	{
		mono_raise_exception(mono_get_exception_argument("unrealType", "C# type does not map to an Unreal class"));
	}

	return Bindings.GetUnrealObjectWrapper(UnrealClass->GetDefaultObject());
}

MonoObject* UnrealObject_GetDefaultObjectFromUnrealObject(UObject* InObject)
{
	check(InObject);
	FMonoBindings& Bindings = FMonoBindings::Get();
	return Bindings.GetUnrealObjectWrapper(InObject->GetClass()->GetDefaultObject());
}

MonoObject* UnrealObject_GetDefaultSubobjectFromName(UObject* InObject, MonoString* SubobjectNameString)
{
	check(InObject);
	FMonoBindings& Bindings = FMonoBindings::Get();

	FName SubobjectName = Mono::MonoStringToFName(SubobjectNameString);
	return Bindings.GetUnrealObjectWrapper(InObject->GetClass()->GetDefaultSubobjectByName(SubobjectName));
}

MonoReflectionType* UClass_GetManagedType(UClass* InNativeClass)
{
	check(InNativeClass);
	FMonoBindings& Bindings = FMonoBindings::Get();

	MonoClass* Class = Bindings.GetMonoClassFromUnrealClass(*InNativeClass);
	check(Class); // this is called internally in SubclassOf, so we can hard fail here

	MonoType* Type = mono_class_get_type(Class);
	check(Type);

	MonoReflectionType* ReflectionType = mono_type_get_object(Bindings.GetDomain(), Type);
	check(ReflectionType);

	return ReflectionType;
}

void* UClass_GetNativeClassFromType(MonoReflectionType* InReflectionType)
{
	MonoType* InternalType = mono_reflection_type_get_type(InReflectionType);

	FMonoBindings& Bindings = FMonoBindings::Get();

	UClass* NativeClass = Bindings.GetUnrealClassFromType(InternalType);

	// icalls return intptrs as a native pointer
	return NativeClass;
}

MonoObject* ObjectInitializer_CreateDefaultSubobject_Name(FObjectInitializer* ObjectInitializer, MonoReflectionType* InUnrealType, FName InObjectName, bool bIsRequired, bool bIsAbstract, bool bIsTransient)
{
	check(ObjectInitializer)

	FMonoBindings& Bindings = FMonoBindings::Get();

	UClass* UnrealClass = Bindings.GetUnrealClassFromType(mono_reflection_type_get_type(InUnrealType));
	if (nullptr == UnrealClass)
	{
		mono_raise_exception(mono_get_exception_argument("unrealType", "C# type does not map to an Unreal class"));
	}

	UObject* Obj = ObjectInitializer->CreateDefaultSubobject(ObjectInitializer->GetObj(), InObjectName, UnrealClass, UnrealClass, bIsRequired, bIsAbstract, bIsTransient);
	
	if (nullptr != Obj)
	{
		return Bindings.GetUnrealObjectWrapper(Obj);
	}
	else
	{
		return nullptr;
	}
}

MonoObject* ObjectFinder_FindNativeObject(MonoReflectionType* InType, MonoString* InSearchString)
{
	FString SearchString;
	Mono::MonoStringToFString(SearchString, InSearchString);

	FMonoBindings& Bindings = FMonoBindings::Get();

	UClass* UnrealClass = Bindings.GetUnrealClassFromType(mono_reflection_type_get_type(InType));
	if (nullptr == UnrealClass)
	{
		mono_raise_exception(mono_get_exception_argument("unrealType", "C# type does not map to an Unreal class"));
	}

	UObject* FoundObject = FindNativeObjectInternal(UnrealClass, SearchString);

	if (nullptr == FoundObject)
	{
		return nullptr;
	}
	else
	{
		return Bindings.GetUnrealObjectWrapper(FoundObject);
	}
}

// IntPtrs are returned as naked pointers in icalls
void*  ClassFinder_FindNativeClass(MonoString* InSearchString)
{
	FString SearchString;
	Mono::MonoStringToFString(SearchString, InSearchString);

	FMonoBindings& Bindings = FMonoBindings::Get();

	return FindNativeObjectInternal(UClass::StaticClass(), SearchString);
}

MonoObject* UObject_GetWorldFromContextObjectNative(UObject* NativeObject)
{
	// Higher level code handles throwing the destroyed unreal object exception
	check(NativeObject);
	check(GEngine);

	UWorld* World = GEngine->GetWorldFromContextObject(NativeObject, EGetWorldErrorMode::ReturnNull);
	if (nullptr == World)
	{
		return nullptr;
	}

	return FMonoBindings::Get().GetUnrealObjectWrapper(World);
}

bool Actor_SetRootNode(AActor* Actor, USceneComponent* NewRootComponent)
{
	check(Actor);
	return Actor->SetRootComponent(NewRootComponent);
}

ENetRole Actor_GetNetRole(AActor* Actor)
{
	check(Actor);
	return Actor->Role;
}

ENetMode Actor_GetNetMode(AActor* Actor)
{
	check(Actor);
	return Actor->GetNetMode();
}

MonoObject* Actor_GetOwner(AActor* ThisActor)
{
	check(ThisActor);
	return FMonoBindings::Get().GetUnrealObjectWrapper(ThisActor->GetOwner());
}

void Actor_SetOwner(AActor* ThisActor, AActor* NewOwner)
{
	check(ThisActor);
	ThisActor->SetOwner(NewOwner);
}

bool Actor_SetRootComponent(AActor* ThisActor, USceneComponent* NewRootComponent)
{
	return ThisActor->SetRootComponent(NewRootComponent);
}

MonoObject* Actor_GetRootComponent(AActor* ThisActor)
{
	USceneComponent* RootComponent = ThisActor->GetRootComponent();
	if (nullptr == RootComponent)
	{
		return nullptr;
	}

	return FMonoBindings::Get().GetUnrealObjectWrapper(RootComponent);
}

void InputComponent_RegisterActionInputCallback(UInputComponent* InputComponent, UObject* TargetObject, MonoString* ActionNameString, EInputEvent InputEvent, MonoObject* CallbackDelegate)
{
	// input component verified at a higher level as is CallbackDelegate
	check(InputComponent);
	check(CallbackDelegate);

	FName ActionName = Mono::MonoStringToFName(ActionNameString);
	FInputActionBinding ActionBinding(ActionName, InputEvent);
	TSharedRef<FMonoDelegateHandle> DelegateHandle = FMonoBindings::Get().CreateObjectDelegate(*InputComponent, CallbackDelegate, TargetObject);
	ActionBinding.ActionDelegate.GetDelegateForManualSet().BindSP(DelegateHandle, &FMonoDelegateHandle::Invoke<void>);
	InputComponent->AddActionBinding(ActionBinding);
}

void InputComponent_RegisterKeyInputCallback(UInputComponent* InputComponent, UObject* TargetObject, FInputChord* InputChord, EInputEvent InputEvent, MonoObject* CallbackDelegate)
{
	// input component verified at a higher level as is CallbackDelegate
	check(InputComponent);
	check(CallbackDelegate);
	check(InputChord);

	FInputKeyBinding* KeyBinding = new(InputComponent->KeyBindings) FInputKeyBinding(*InputChord, InputEvent);
	TSharedRef<FMonoDelegateHandle> DelegateHandle = FMonoBindings::Get().CreateObjectDelegate(*InputComponent, CallbackDelegate, TargetObject);
	KeyBinding->KeyDelegate.GetDelegateForManualSet().BindSP(DelegateHandle, &FMonoDelegateHandle::Invoke<void>);
}


void InputComponent_RegisterTouchInputCallback(UInputComponent* InputComponent, UObject* TargetObject, EInputEvent InputEvent, MonoObject* CallbackDelegate)
{
	// input component verified at a higher level as is CallbackDelegate
	check(InputComponent);
	check(CallbackDelegate);

	FInputTouchBinding* TouchBinding = new(InputComponent->TouchBindings) FInputTouchBinding(InputEvent);
	TSharedRef<FMonoDelegateHandle> DelegateHandle = FMonoBindings::Get().CreateObjectDelegate(*InputComponent, CallbackDelegate, TargetObject);
	TouchBinding->TouchDelegate.GetDelegateForManualSet().BindSP(DelegateHandle, &FMonoDelegateHandle::Invoke<void, ETouchIndex::Type,FVector>);
}

void InputComponent_RegisterAxisInputCallback(UInputComponent* InputComponent, UObject* TargetObject, MonoString* AxisNameString, MonoObject* CallbackDelegate)
{
	// input component verified at a higher level as is CallbackDelegate
	check(InputComponent);
	check(CallbackDelegate);

	FName AxisName = Mono::MonoStringToFName(AxisNameString); 
	FInputAxisBinding* AxisBinding = new(InputComponent->AxisBindings) FInputAxisBinding(AxisName);
	TSharedRef<FMonoDelegateHandle> DelegateHandle = FMonoBindings::Get().CreateObjectDelegate(*InputComponent, CallbackDelegate, TargetObject);
	AxisBinding->AxisDelegate.GetDelegateForManualSet().BindSP(DelegateHandle, &FMonoDelegateHandle::Invoke<void,float>);
}

void InputComponent_RegisterAxisKeyInputCallback(UInputComponent* InputComponent, UObject* TargetObject, FKey* AxisKey, MonoObject* CallbackDelegate)
{
	// input component verified at a higher level as is CallbackDelegate
	check(InputComponent);
	check(CallbackDelegate);
	check(AxisKey);

	FInputAxisKeyBinding* AxisBinding = new(InputComponent->AxisKeyBindings) FInputAxisKeyBinding(*AxisKey);
	TSharedRef<FMonoDelegateHandle> DelegateHandle = FMonoBindings::Get().CreateObjectDelegate(*InputComponent, CallbackDelegate, TargetObject);
	AxisBinding->AxisDelegate.GetDelegateForManualSet().BindSP(DelegateHandle, &FMonoDelegateHandle::Invoke<void, float>);
}

void InputComponent_RegisterVectorAxisInputCallback(UInputComponent* InputComponent, UObject* TargetObject, FKey* VectorAxisKey, MonoObject* CallbackDelegate)
{
	// input component verified at a higher level as is CallbackDelegate
	check(InputComponent);
	check(CallbackDelegate);
	check(VectorAxisKey);

	FInputVectorAxisBinding* AxisBinding = new(InputComponent->VectorAxisBindings) FInputVectorAxisBinding(*VectorAxisKey);
	TSharedRef<FMonoDelegateHandle> DelegateHandle = FMonoBindings::Get().CreateObjectDelegate(*InputComponent, CallbackDelegate, TargetObject);
	AxisBinding->AxisDelegate.GetDelegateForManualSet().BindSP(DelegateHandle, &FMonoDelegateHandle::Invoke<void, FVector>);
}

void InputComponent_RegisterGestureInputCallback(UInputComponent* InputComponent, UObject* TargetObject, FKey* GestureKey, MonoObject* CallbackDelegate)
{
	// input component verified at a higher level as is CallbackDelegate
	check(InputComponent);
	check(CallbackDelegate);
	check(GestureKey);

	FInputGestureBinding* GestureBinding = new(InputComponent->GestureBindings) FInputGestureBinding(*GestureKey);
	TSharedRef<FMonoDelegateHandle> DelegateHandle = FMonoBindings::Get().CreateObjectDelegate(*InputComponent, CallbackDelegate, TargetObject);
	GestureBinding->GestureDelegate.GetDelegateForManualSet().BindSP(DelegateHandle, &FMonoDelegateHandle::Invoke<void, float>);
}

MonoObject* SkinnedMeshComponent_GetPhysicsAsset(USkinnedMeshComponent* ThisComponent)
 {
	check(ThisComponent);
	
	FMonoBindings& Bindings = FMonoBindings::Get();

#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4946)
#endif
	//cast to avoid including "PhysicsEngine/PhysicsAsset.h"
	return Bindings.GetUnrealObjectWrapper(reinterpret_cast<UObject*>(ThisComponent->GetPhysicsAsset()));
#if defined(_MSC_VER)
#pragma warning (pop)
#endif
}

MonoObject* World_SpawnActor(UWorld* ThisWorld, UClass* Class, FName Name, AActor* Template, AActor* Owner, APawn* Instigator, ULevel* OverrideLevel, ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride, bool bNoFail, bool bAllowDuringConstructionScript)
{
	check(ThisWorld);
	check(Class);
	// All other parameters may validly be NULL.

	FActorSpawnParameters Params;
	Params.Name = Name;
	Params.Template = Template;
	Params.Owner = Owner;
	Params.Instigator = Instigator;
	Params.OverrideLevel = OverrideLevel;
	Params.SpawnCollisionHandlingOverride = SpawnCollisionHandlingOverride;
	Params.bNoFail = bNoFail;
	Params.bAllowDuringConstructionScript = bAllowDuringConstructionScript;

	FMonoBindings& Bindings = FMonoBindings::Get();
	return Bindings.GetUnrealObjectWrapper(ThisWorld->SpawnActor(Class, NULL, NULL, Params));
}



MonoString* FName_ToString(FName Name)
{
	return Mono::FNameToMonoString(mono_domain_get(), Name);
}

MonoString* FName_GetPlainName(FName Name)
{
	FString PlainNameStr = Name.GetPlainNameString();
	return Mono::FStringToMonoString(mono_domain_get(), PlainNameStr);
}

MonoString* FText_ToString(const FText* ThisText)
{
	check(ThisText);
	return Mono::FStringToMonoString(mono_domain_get(), ThisText->ToString());
}

void FText_FromString(FText* ThisText, MonoString* String)
{
	check(ThisText);
	check(String);
	FString UnrealString;
	Mono::MonoStringToFString(UnrealString, String);
	(*ThisText) = FText::FromString(UnrealString);
}

void FText_CreateText(FText* ThisText, MonoString* Key, MonoString* NameSpace, MonoString* Literal)
{
	check(ThisText);
	check(NameSpace);
	check(Literal);
	FString KeyFString;
	Mono::MonoStringToFString(KeyFString, Key);
	FString NameSpaceFString;
	Mono::MonoStringToFString(NameSpaceFString, NameSpace);
	FString LiteralFString;
	Mono::MonoStringToFString(LiteralFString, Literal);
	(*ThisText) = FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*LiteralFString,*NameSpaceFString, *KeyFString);
}

void FText_CreateEmptyText(FText* ThisText)
{
	check(ThisText);
	(*ThisText) = FText();
}

void FText_FromName(FText* ThisText, FName Name)
{
	check(ThisText);
	(*ThisText) = FText::FromName(Name);
}

int FText_Compare(FText* A, FText* B)
{
	check(A);
	check(B);
	return A->CompareTo(*B);
}

bool FText_IsEmpty(FText* Text)
{
	check(Text);
	return Text->IsEmpty();
}

//there are here so we can place breakpoints to check managed use of AddSharedReference/ReleaseSharedReference
#if UE_BUILD_DEBUG
void FSharedRef_IncRefThreadSafe(SharedPointerInternals::FReferenceControllerBase* ReferenceController)
{
	SharedPointerInternals::FReferenceControllerOps<ESPMode::ThreadSafe>::AddSharedReference(ReferenceController);
}

void FSharedRef_DecRefThreadSafe(SharedPointerInternals::FReferenceControllerBase* ReferenceController)
{
	SharedPointerInternals::FReferenceControllerOps<ESPMode::ThreadSafe>::ReleaseSharedReference(ReferenceController);
}
#endif

MonoObject* FWeakObject_GetObject(FWeakObjectPtr* WeakPtr)
{
	return UnrealObject_GetUnrealObjectWrapper(WeakPtr->Get());
}

void FWeakObject_SetObject(FWeakObjectPtr* WeakPtr, UObject *object)
{
	(*WeakPtr) = object;
}

bool FWeakObject_IsValid(FWeakObjectPtr* WeakPtr, bool bThreadsafeTest = false)
{
	return WeakPtr->IsValid(false, bThreadsafeTest);
}

bool FWeakObject_IsStale(FWeakObjectPtr *WeakPtr, bool bThreadsafeTest = false)
{
	return WeakPtr->IsStale(true, bThreadsafeTest);
}

int GetBooleanSize()
{
	return sizeof(bool);
}

#if !UE_BUILD_SHIPPING
void SharedPtrTheadSafe_CheckSizeof(int structSize)
{
	check(sizeof(FMarshaledSharedPtr) == structSize);
}

void Text_CheckSizeof(int structSize)
{
	check(sizeof(FMarshaledText) == structSize);
}

void Name_CheckSizeof(int structSize)
{
	check(sizeof(FMarshalledName) == structSize);
}
#endif //!UE_BUILD_SHIPPING

#if defined(__clang__)
// clang seems to need a cast here
#define MONO_ADD_INTERNAL_CALL(ManagedName, NativeFunc) \
	mono_add_internal_call(ManagedName, (void*) NativeFunc)
#else
#define MONO_ADD_INTERNAL_CALL(ManagedName, NativeFunc) \
	mono_add_internal_call(ManagedName, NativeFunc)
#endif

MonoString* UnrealInterop_MarshalIntPtrAsString(TCHAR* InString);
void UnrealInterop_MarshalToUnrealString(MonoString* InString, FMarshalledScriptArray* OutArray);

void AddUnrealObjectInternalCalls()
{
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealObject::IsPendingKillNative", UnrealObject_IsPendingKill);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealObject::GetUnrealObjectWrapperNative", UnrealObject_GetUnrealObjectWrapper);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealObject::ConstructUnrealObjectNative", UnrealObject_ConstructUnrealObject);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealObject::GetDefaultObjectFromUnrealClass", UnrealObject_GetDefaultObjectFromUnrealClass);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealObject::GetDefaultObjectFromMonoClass", UnrealObject_GetDefaultObjectFromMonoType);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealObject::GetDefaultObjectFromUnrealObject", UnrealObject_GetDefaultObjectFromUnrealObject);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealObject::GetDefaultSubobjectFromName", UnrealObject_GetDefaultSubobjectFromName);

	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealInterop::GetManagedType", UClass_GetManagedType);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealInterop::GetNativeClassFromType", UClass_GetNativeClassFromType);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealInterop::MarshalIntPtrAsString", UnrealInterop_MarshalIntPtrAsString);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealInterop::MarshalToUnrealString", UnrealInterop_MarshalToUnrealString);

	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealObject::ObjectFinder_FindNativeObject", ObjectFinder_FindNativeObject);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".UnrealObject::ClassFinder_FindNativeClass", ClassFinder_FindNativeClass);

	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".ObjectInitializer::CreateDefaultSubobject_Name", ObjectInitializer_CreateDefaultSubobject_Name);

	MONO_ADD_INTERNAL_CALL(MONO_COREUOBJECT_NAMESPACE ".Object::GetWorldFromContextObjectNative", UObject_GetWorldFromContextObjectNative);

	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".Actor::SetRootNodeOnActor", Actor_SetRootNode);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".Actor::GetNetRole", Actor_GetNetRole);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".Actor::GetNetMode", Actor_GetNetMode);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".Actor::GetOwner", Actor_GetOwner);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".Actor::SetOwner", Actor_SetOwner);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".Actor::GetRootComponent", Actor_GetRootComponent);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".Actor::SetRootComponent", Actor_SetRootComponent);

	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".InputComponent::RegisterActionInputCallback", InputComponent_RegisterActionInputCallback);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".InputComponent::RegisterKeyInputCallback", InputComponent_RegisterKeyInputCallback);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".InputComponent::RegisterTouchInputCallback", InputComponent_RegisterTouchInputCallback);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".InputComponent::RegisterAxisInputCallback", InputComponent_RegisterAxisInputCallback);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".InputComponent::RegisterAxisKeyInputCallback", InputComponent_RegisterAxisKeyInputCallback);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".InputComponent::RegisterVectorAxisInputCallback", InputComponent_RegisterVectorAxisInputCallback);
	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".InputComponent::RegisterGestureInputCallback", InputComponent_RegisterGestureInputCallback);

	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".SkinnedMeshComponent::GetPhysicsAssetNative", SkinnedMeshComponent_GetPhysicsAsset);

	MONO_ADD_INTERNAL_CALL(MONO_ENGINE_NAMESPACE ".World::SpawnActorNative", World_SpawnActor);

	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Name::FName_ToString", FName_ToString);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Name::FName_GetPlainName", FName_GetPlainName);

	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Text::FText_ToString", FText_ToString);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Text::FText_FromString", FText_FromString);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Text::FText_CreateText", FText_CreateText);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Text::FText_CreateEmptyText", FText_CreateEmptyText);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Text::FText_FromName", FText_FromName);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Text::FText_Compare", FText_Compare);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Text::FText_IsEmpty", FText_IsEmpty);

#if UE_BUILD_DEBUG
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".SharedPtrTheadSafe::FSharedRef_IncRefThreadSafe", FSharedRef_IncRefThreadSafe);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".SharedPtrTheadSafe::FSharedRef_DecRefThreadSafe", FSharedRef_DecRefThreadSafe);
#else
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".SharedPtrTheadSafe::FSharedRef_IncRefThreadSafe", SharedPointerInternals::FReferenceControllerOps<ESPMode::ThreadSafe>::AddSharedReference);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".SharedPtrTheadSafe::FSharedRef_DecRefThreadSafe", SharedPointerInternals::FReferenceControllerOps<ESPMode::ThreadSafe>::ReleaseSharedReference);
#endif

	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".WeakObjectData::GetObject", FWeakObject_GetObject);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".WeakObjectData::SetObject", FWeakObject_SetObject);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".WeakObjectData::IsValid", FWeakObject_IsValid);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".WeakObjectData::IsStale", FWeakObject_IsStale);

	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".BoolMarshaler::GetBooleanSize", GetBooleanSize);

#if !UE_BUILD_SHIPPING
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".SharedPtrTheadSafe::CheckSizeof", SharedPtrTheadSafe_CheckSizeof);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Text::CheckSizeof", Text_CheckSizeof);
	MONO_ADD_INTERNAL_CALL(MONO_BINDINGS_NAMESPACE ".Name::CheckSizeof", Name_CheckSizeof);
#endif
}

#undef MONO_ADD_INTERNAL_CALL

// UnrealObject pinvoke functions

// Name access
MONO_PINVOKE_FUNCTION(FMarshalledName) UnrealObject_GetFName(UObject* InObject)
{
	// this is verified at a higher level
	check(InObject);
	return ConvertToMarshalledName(InObject->GetFName());
}

// UFunction exposure
MONO_PINVOKE_FUNCTION(UFunction*) UnrealObject_GetNativeFunctionFromClassAndName(UClass* Class, const UTF16CHAR* FunctionName)
{
	check(Class);
	UFunction* Function = FindField<UFunction>(Class, StringCast<TCHAR>(FunctionName).Get());
	check(Function);

	return Function;
}

MONO_PINVOKE_FUNCTION(UFunction*) UnrealObject_GetNativeFunctionFromInstanceAndName(UObject* Obj, const UTF16CHAR* FunctionName)
{
	check(Obj);
	return Obj->FindFunctionChecked(StringCast<TCHAR>(FunctionName).Get());
}

MONO_PINVOKE_FUNCTION(int16) UnrealObject_GetNativeFunctionParamsSize(UFunction* NativeFunction)
{
	check(NativeFunction);
	return NativeFunction->ParmsSize;
}

bool IsOutParam(UProperty* Property)
{
	return Property->HasAnyPropertyFlags(CPF_ReturnParm) || (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ReferenceParm));
}

MONO_PINVOKE_FUNCTION(void) UnrealObject_InvokeFunction(UObject* NativeObject, UFunction* NativeFunction, void* Arguments, int ArgumentsSize)
{
	check(NativeFunction);
	check(ArgumentsSize == NativeFunction->ParmsSize);
	if (nullptr == NativeObject)
	{
		FMonoBindings::Get().ThrowUnrealObjectDestroyedException(FString::Printf(TEXT("Trying to call function %s on destroyed unreal object"), *NativeFunction->GetPathName()));
	}

	for (TFieldIterator<UProperty> ParamIt(NativeFunction); ParamIt; ++ParamIt)
	{
		UProperty* ParamProperty = *ParamIt;
		if (IsOutParam(ParamProperty))
		{
			uint8* ParamMemory = reinterpret_cast<uint8*>(Arguments) + ParamProperty->GetOffset_ForUFunction();
			ParamProperty->InitializeValue(ParamMemory);
		}
	}

	NativeObject->ProcessEvent(NativeFunction, Arguments);

	for (TFieldIterator<UProperty> ParamIt(NativeFunction); ParamIt; ++ParamIt)
	{
		UProperty* ParamProperty = *ParamIt;
		if (IsOutParam(ParamProperty))
		{
			uint8* ParamMemory = reinterpret_cast<uint8*>(Arguments)+ParamProperty->GetOffset_ForUFunction();
			if (UStrProperty* StringProperty = Cast<UStrProperty>(ParamProperty))
			{
				FString* String = reinterpret_cast<FString*>(ParamMemory);
				int32 Length = String->Len() + 1;
				TCHAR* ReturnBuffer = reinterpret_cast<TCHAR*>(Mono::CoTaskMemAlloc(Length * sizeof(TCHAR)));
				FCString::Strcpy(ReturnBuffer, Length, **String);

				ParamProperty->DestroyValue(ParamMemory);

				FMarshalledScriptArray* MarshalledString = reinterpret_cast<FMarshalledScriptArray*>(ParamMemory);
				MarshalledString->Data = ReturnBuffer;
				MarshalledString->ArrayNum = MarshalledString->ArrayMax = Length;
			}
			else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(ParamProperty))
			{
				FScriptArray* ScriptArray = reinterpret_cast<FScriptArray*>(ParamMemory);
				int ArrayNum = ScriptArray->Num();

				//TODO: handle non-blittable inner properties.
				//      Currently, only simple types, structs, and UObject*s are permitted by the code generator.
				UProperty* InnerProperty = ArrayProperty->Inner;
				size_t BufferSize = InnerProperty->ElementSize * ArrayNum;
				void* ReturnBuffer = Mono::CoTaskMemAlloc(BufferSize);
				FMemory::Memcpy(ReturnBuffer, ScriptArray->GetData(), BufferSize);

				ParamProperty->DestroyValue(ParamMemory);

				FMarshalledScriptArray* MarshalledScriptArray = reinterpret_cast<FMarshalledScriptArray*>(ParamMemory);
				MarshalledScriptArray->Data = ReturnBuffer;
				MarshalledScriptArray->ArrayNum = MarshalledScriptArray->ArrayMax = ArrayNum;
			}
			else
			{
				ParamProperty->DestroyValue(ParamMemory);
			}
		}
	}
}

MONO_PINVOKE_FUNCTION(void) UnrealObject_InvokeStaticFunction(UClass* NativeClass, UFunction* NativeFunction, void* Arguments, int ArgumentsSize)
{
	check(NativeClass);
	UnrealObject_InvokeFunction(NativeClass->ClassDefaultObject, NativeFunction, Arguments, ArgumentsSize);
}

MONO_PINVOKE_FUNCTION(void) FName_FromString(FName* Name, UTF16CHAR* Value, EFindName FindType)
{
	check(Name);
	*Name = FName(StringCast<TCHAR>(Value).Get(), FindType);
}

MONO_PINVOKE_FUNCTION(void) FName_FromStringAndNumber(FName* Name, UTF16CHAR* Value, int Number, EFindName FindType)
{
	check(Name);
	*Name = FName(StringCast<TCHAR>(Value).Get(), Number, FindType);
}

MONO_PINVOKE_FUNCTION(void) FRotator_FromQuat(FRotator* OutRotator, FQuatArg QuatArg)
{
	check(OutRotator);
	const FQuat Quat(QuatArg.X, QuatArg.Y, QuatArg.Z, QuatArg.W);
	*OutRotator = FRotator(Quat);
}

MONO_PINVOKE_FUNCTION(void) FRotator_FromMatrix(FRotator* OutRotator, const FMatrix* RotationMatrixArg)
{
	check(OutRotator);
	FMatrix RotationMatrix;
	// we can't be sure of alignment of input matrix, so do a memory copy 
	FMemory::Memcpy(RotationMatrix, *RotationMatrixArg);
	*OutRotator = RotationMatrix.Rotator();
}

MONO_PINVOKE_FUNCTION(void) FQuat_FromRotator(FQuat* OutQuat, FRotator Rotator)
{
	check(OutQuat);
	const FQuat TempQuat = Rotator.Quaternion();
	// we can't be sure of alignment of output quat, so do a memory copy 
	FMemory::Memcpy(*OutQuat, TempQuat);
}

MONO_PINVOKE_FUNCTION(void) FMatrix_FromRotator(FMatrix* OutRotationMatrix, FRotator Rotator)
{
	check(OutRotationMatrix);
	FRotationMatrix TempMatrix(Rotator);
	// we can't be sure of alignment of output matrix, so do a memory copy
	FMemory::Memcpy(OutRotationMatrix, &TempMatrix, sizeof(FMatrix));
}

MONO_PINVOKE_FUNCTION(void) FVector_FromRotator(FVector* OutDirection, FRotator Rotator)
{
	check(OutDirection);
	*OutDirection = Rotator.Vector();
}

MONO_PINVOKE_FUNCTION(void) FVector_SafeNormal(FVector* OutVector, FVector InVector, float tolerance)
{
	check(OutVector);
	*OutVector = InVector.GetSafeNormal(tolerance);
}

MONO_PINVOKE_FUNCTION(void) FVector_SafeNormal2D(FVector* OutVector, FVector InVector, float tolerance)
{
	check(OutVector);
	*OutVector = InVector.GetSafeNormal2D(tolerance);
}

MONO_PINVOKE_FUNCTION(void) FVector_ToRotator(FRotator* OutRotator, FVector InVector)
{
	check(OutRotator);
	*OutRotator = InVector.Rotation();
}

MONO_PINVOKE_FUNCTION(void) Actor_GetComponentsBoundingBoxNative(AActor* InActor, FBox* OutBox, bool bNonColliding)
{
	check(InActor);
	check(OutBox);
	*OutBox = InActor->GetComponentsBoundingBox(bNonColliding);
}

MONO_PINVOKE_FUNCTION(ETickingGroup) Actor_GetTickGroup(AActor* ThisActor)
{
	check(ThisActor);
	return ThisActor->PrimaryActorTick.TickGroup;
}

MONO_PINVOKE_FUNCTION(void) Actor_SetTickGroup(AActor* ThisActor, ETickingGroup TickGroup)
{
	check(ThisActor);
	return ThisActor->SetTickGroup(TickGroup);
}

MONO_PINVOKE_FUNCTION(bool) Actor_GetActorTickEnabled(AActor* ThisActor)
{
	check(ThisActor);
	return ThisActor->IsActorTickEnabled();
}

MONO_PINVOKE_FUNCTION(void) Actor_SetActorTickEnabled(AActor* ThisActor, bool bEnabled)
{
	check(ThisActor);
	return ThisActor->SetActorTickEnabled(bEnabled);
}

MONO_PINVOKE_FUNCTION(void) FQuat_ScaleVector(FVector* OutVector, FQuatArg InQuat, FVector InVector)
{
	check(OutVector);
	const FQuat Quat(InQuat.X, InQuat.Y, InQuat.Z, InQuat.W);
	*OutVector = Quat * InVector;
}

MONO_PINVOKE_FUNCTION(void) Actor_TearOff(AActor* ThisActor)
{
	check(ThisActor);
	ThisActor->TearOff();
}

MONO_PINVOKE_FUNCTION(void) Controller_GetPlayerViewPoint(AController* Controller, FVector* OutLocation, FRotator* OutRotation)
{
	check(Controller);
	check(OutLocation);
	check(OutRotation);
	Controller->GetPlayerViewPoint(*OutLocation, *OutRotation);
}

MONO_PINVOKE_FUNCTION(ETickingGroup) ActorComponent_GetTickGroup(UActorComponent* ThisComponent)
 {
	check(ThisComponent);
	return ThisComponent->PrimaryComponentTick.TickGroup;
}

MONO_PINVOKE_FUNCTION(void) ActorComponent_SetTickGroup(UActorComponent* ThisComponent, ETickingGroup TickGroup)
{
	check(ThisComponent);
	return ThisComponent->SetTickGroup(TickGroup);
}

MONO_PINVOKE_FUNCTION(bool) ActorComponent_GetComponentTickEnabled(UActorComponent* ThisComponent)
{
	check(ThisComponent);
	return ThisComponent->PrimaryComponentTick.IsTickFunctionEnabled();
}

MONO_PINVOKE_FUNCTION(void) ActorComponent_SetComponentTickEnabled(UActorComponent* ThisComponent, bool bEnabled)
 {
	check(ThisComponent);
	return ThisComponent->SetComponentTickEnabled(bEnabled);
}

MONO_PINVOKE_FUNCTION(void) CharacterMovementComponent_ForceReplicationUpdate(UCharacterMovementComponent* ThisComponent)
{
	check(ThisComponent);
	ThisComponent->ForceReplicationUpdate();
}

MONO_PINVOKE_FUNCTION(void) Pawn_GetViewRotation(APawn* NativePawn, FRotator* OutRotator)
{
	check(NativePawn);
	check(OutRotator);
	*OutRotator = NativePawn->GetViewRotation();
}


MONO_PINVOKE_FUNCTION(void) Pawn_TurnOff(APawn* ThisPawn)
{
	check(ThisPawn);
	ThisPawn->TurnOff();
}

MONO_PINVOKE_FUNCTION(ECollisionChannel) CollisionChannel_FromTraceType(ETraceTypeQuery TraceType)
{
	return UEngineTypes::ConvertToCollisionChannel(TraceType);
}

MONO_PINVOKE_FUNCTION(ECollisionChannel) CollisionChannel_FromObjectType(EObjectTypeQuery ObjectType)
{
	return UEngineTypes::ConvertToCollisionChannel(ObjectType);
}

MONO_PINVOKE_FUNCTION(ETraceTypeQuery) TraceType_FromCollisionChannel(ECollisionChannel CollisionChannel)
{
	return UEngineTypes::ConvertToTraceType(CollisionChannel);
}

MONO_PINVOKE_FUNCTION(EObjectTypeQuery) ObjectType_FromCollisionChannel(ECollisionChannel CollisionChannel)
{
	return UEngineTypes::ConvertToObjectType(CollisionChannel);
}

MONO_PINVOKE_FUNCTION(float) FRandomStream_GetFraction(FRandomStream* SelfParameter)
{
	return SelfParameter->GetFraction();
}

MONO_PINVOKE_FUNCTION(uint32) FRandomStream_GetUnsignedInt(FRandomStream* SelfParameter)
{
	return SelfParameter->GetUnsignedInt();
}

MONO_PINVOKE_FUNCTION(void) FRandomStream_GetUnitVector(FRandomStream* SelfParameter, FVector* OutVector)
{
	*OutVector = SelfParameter->GetUnitVector();
}

MONO_PINVOKE_FUNCTION(int) FRandomStream_RandRange(FRandomStream* SelfParameter, int32 Min, int32 Max)
{
	return SelfParameter->RandRange(Min, Max);
}

MONO_PINVOKE_FUNCTION(void) FRandomStream_VRandCone(FRandomStream* SelfParameter, FVector* OutVector, FVector Dir, float ConeHalfAngleRad)
{
	*OutVector = SelfParameter->VRandCone(Dir, ConeHalfAngleRad);
}

MONO_PINVOKE_FUNCTION(void) FRandomStream_VRandCone2(FRandomStream* SelfParameter, FVector* OutVector, FVector Dir, float HorizontalConeHalfAngleRad, float VerticalConeHalfAngleRad)
{
	*OutVector = SelfParameter->VRandCone(Dir, HorizontalConeHalfAngleRad, VerticalConeHalfAngleRad);
}

MONO_PINVOKE_FUNCTION(void) SceneComponent_SetupAttachment(USceneComponent* Self, USceneComponent* Parent, FName Socket)
{
	Self->SetupAttachment(Parent, Socket);
}
