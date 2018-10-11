// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoPropertyFactory.h"
#include "MonoRuntimeCommon.h"

#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "Logging/MessageLog.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "MonoRuntime"

FMonoPropertyFactory* FMonoPropertyFactory::GInstance = nullptr;

template <class T>
UProperty* CreateSimpleProperty(UObject& Outer, FMonoBindings&, const FMonoPropertyMetadata& Metadata)
{
	EPropertyFlags PropertyFlags = Metadata.GetPropertyFlags();
	return new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, RF_Public | RF_Transient | RF_MarkAsNative) T(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags);
}

static UProperty* CreateBoolProperty(UObject& Outer, FMonoBindings&, const FMonoPropertyMetadata& Metadata)
{
	EPropertyFlags PropertyFlags = Metadata.GetPropertyFlags();

	// Treat all managed bools as native - no bitfields.
	return new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, RF_Public | RF_Transient | RF_MarkAsNative) UBoolProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags, 0, sizeof(bool), true);
}

static UProperty* CreateObjectProperty(UObject& Outer, FMonoBindings& Bindings, const FMonoPropertyMetadata& Metadata)
{
	const FMonoUnrealObjectType& ObjectType = static_cast<const FMonoUnrealObjectType&>(*Metadata.UnrealPropertyType);

	UClass* UnrealClass = Bindings.GetUnrealClassFromTypeReference(ObjectType.TypeRef);
	check(UnrealClass);

	EPropertyFlags PropertyFlags = Metadata.GetPropertyFlags();
	return new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, RF_Public | RF_Transient | RF_MarkAsNative) UObjectProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags, UnrealClass);
}

static UProperty* CreateClassProperty(UObject& Outer, FMonoBindings& Bindings, const FMonoPropertyMetadata& Metadata)
{
	const FMonoUnrealClassType& ClassType = static_cast<const FMonoUnrealClassType&>(*Metadata.UnrealPropertyType);

	UClass* MetaClass = Bindings.GetUnrealClassFromTypeReference(ClassType.TypeRef);
	check(MetaClass);

	EPropertyFlags PropertyFlags = Metadata.GetPropertyFlags();
	return new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, RF_Public | RF_Transient | RF_MarkAsNative) UClassProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags, MetaClass, nullptr);
}

static UProperty* CreateWeakObjectProperty(UObject& Outer, FMonoBindings& Bindings, const FMonoPropertyMetadata& Metadata)
{
	const FMonoUnrealWeakObjectType& WeakObjectType = static_cast<const FMonoUnrealWeakObjectType&>(*Metadata.UnrealPropertyType);

	UClass* UnrealClass = Bindings.GetUnrealClassFromTypeReference(WeakObjectType.TypeRef);
	check(UnrealClass);


	EPropertyFlags PropertyFlags = Metadata.GetPropertyFlags();
	return new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, RF_Public | RF_Transient | RF_MarkAsNative) UWeakObjectProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags, UnrealClass);
}

template<class T>
T* FindPropertyUnderlyingType(const FMonoTypeReferenceMetadata& TypeReference, const FString& NativeClassOwner)
{
	bool bIsBindingsAssembly = false;
	UPackage* Package = FMonoBindings::GetPackageFromNamespaceAndAssembly(bIsBindingsAssembly, TypeReference.Namespace, TypeReference.AssemblyName);
	check(Package);

	T* TheType = nullptr;

	if (NativeClassOwner.Len() > 0)
	{
		UClass* NativeClass = FindObject<UClass>(Package, *NativeClassOwner);
		check(NativeClass);

		TheType = FindObject<T>(NativeClass, *TypeReference.Name, true);
	}
	else
	{
		TheType = FindObject<T>(Package, *TypeReference.Name, true);
	}
	return TheType;
}

static UProperty* CreateEnumProperty(UObject& Outer, FMonoBindings& Bindings, const FMonoPropertyMetadata& Metadata)
{
	const FMonoUnrealEnumType& EnumType = static_cast<const FMonoUnrealEnumType&>(*Metadata.UnrealPropertyType);

	const FMonoTypeReferenceMetadata ModifiedTypeRef(EnumType.TypeRef.Namespace, EnumType.NativeEnumName, EnumType.TypeRef.AssemblyName);

	UEnum* TheEnum = FindPropertyUnderlyingType<UEnum>(ModifiedTypeRef, EnumType.NativeClassOwner);
	check(TheEnum);

	EPropertyFlags PropertyFlags = Metadata.GetPropertyFlags();
	EObjectFlags ObjectFlags = RF_Public | RF_Transient | RF_MarkAsNative;

	if (TheEnum->GetCppForm() == UEnum::ECppForm::EnumClass)
	{
		UEnumProperty* EnumProp = new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, ObjectFlags) UEnumProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags, TheEnum);
		//FIXME: non uint8 properties. would need to UEnum to have the underlying type or look it up from the C# type.
		UNumericProperty* UnderlyingProp = new(EC_InternalUseOnlyConstructor, EnumProp, TEXT("UnderlyingType"), ObjectFlags) UByteProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags);
		return EnumProp;
	}

	return new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, ObjectFlags) UByteProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags, TheEnum);
}

static UProperty* CreateStructProperty(UObject& Outer, FMonoBindings& Bindings, const FMonoPropertyMetadata& Metadata)
{
	const FMonoStructType& StructType = static_cast<const FMonoStructType&>(*Metadata.UnrealPropertyType);

	UScriptStruct* TheStruct = FindPropertyUnderlyingType<UScriptStruct>(StructType.TypeRef, StructType.NativeClassOwner);
	if (!TheStruct)
	{
		// Property may be referencing a user struct that hasn't been initialized yet,
		// so give the bindings a chance to resolve it for us.
		TheStruct = Bindings.GetUnrealStructFromTypeReference(StructType.TypeRef);
	}

	check(TheStruct);
	EPropertyFlags PropertyFlags = Metadata.GetPropertyFlags();
	return new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, RF_Public | RF_Transient | RF_MarkAsNative) UStructProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags, TheStruct);
}

static UProperty* CreateCoreStructProperty(UObject& Outer, FMonoBindings& Bindings, const FMonoPropertyMetadata& Metadata)
{
	const FMonoCoreStructType& StructType = static_cast<const FMonoCoreStructType&>(*Metadata.UnrealPropertyType);

	// WORKAROUND: Using the UObject package means that we might run into an "Ambiguous search" (see StaticFindObjectFastInternalThreadSafe). The ideal solution would be to specify package in the metadata, which will require changes in several places throughout MonoUE.
	UScriptStruct* TheStruct = FindObject<UScriptStruct>(UObject::StaticClass()->GetOutermost(), *StructType.StructName, true);
	check(TheStruct);
	EPropertyFlags PropertyFlags = Metadata.GetPropertyFlags();
	return new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, RF_Public | RF_Transient | RF_MarkAsNative) UStructProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags, TheStruct);
}

static UProperty* CreateArrayProperty(UObject& Outer, FMonoBindings& Bindings, const FMonoPropertyMetadata& Metadata)
{
	const FMonoUnrealArrayType& ArrayType = static_cast<const FMonoUnrealArrayType&>(*Metadata.UnrealPropertyType);

	EPropertyFlags PropertyFlags = Metadata.GetPropertyFlags();
	UArrayProperty* ArrayProp = new(EC_InternalUseOnlyConstructor, &Outer, Metadata.Name, RF_Public | RF_Transient | RF_MarkAsNative) UArrayProperty(FObjectInitializer(), EC_CppProperty, 0, PropertyFlags);

	UProperty* InnerProp = FMonoPropertyFactory::Get().Create(*ArrayProp, Bindings, ArrayType.InnerProperty);
	ArrayProp->Inner = InnerProp;
	return ArrayProp;
}

FMonoPropertyFactory::FMonoPropertyFactory()
{
	// NOTE: If you add new property types here, you must update the IL rewriting in MonoAssemblyProcess to handle them
#define ADD_SIMPLE_PROPERTY(TypeName) PropertyFactoryMap.Add(TypeName::StaticClass()->GetFName(), PropertyFactoryFunctor(&CreateSimpleProperty<TypeName>))
	ADD_SIMPLE_PROPERTY(UDoubleProperty);
	ADD_SIMPLE_PROPERTY(UFloatProperty);

	ADD_SIMPLE_PROPERTY(UInt8Property);
	ADD_SIMPLE_PROPERTY(UInt16Property);
	ADD_SIMPLE_PROPERTY(UIntProperty);
	ADD_SIMPLE_PROPERTY(UInt64Property);

	ADD_SIMPLE_PROPERTY(UByteProperty);
	ADD_SIMPLE_PROPERTY(UUInt16Property);
	ADD_SIMPLE_PROPERTY(UUInt32Property);
	ADD_SIMPLE_PROPERTY(UUInt64Property);
	ADD_SIMPLE_PROPERTY(UNameProperty);
	ADD_SIMPLE_PROPERTY(UStrProperty);
	ADD_SIMPLE_PROPERTY(UTextProperty);
#undef ADD_SIMPLE_PROPERTY

	PropertyFactoryMap.Add(UBoolProperty::StaticClass()->GetFName(), PropertyFactoryFunctor(&CreateBoolProperty));
	PropertyFactoryMap.Add(UObjectProperty::StaticClass()->GetFName(), PropertyFactoryFunctor(&CreateObjectProperty));
	PropertyFactoryMap.Add(UClassProperty::StaticClass()->GetFName(), PropertyFactoryFunctor(&CreateClassProperty));
	PropertyFactoryMap.Add(UEnumProperty::StaticClass()->GetFName(), PropertyFactoryFunctor(&CreateEnumProperty));
	// CoreStructProperty isn't a real property type name, but it's how we differentiate between math/core structs and normal structs
	PropertyFactoryMap.Add(FName("CoreStructProperty"), PropertyFactoryFunctor(&CreateCoreStructProperty));
	PropertyFactoryMap.Add(UStructProperty::StaticClass()->GetFName(), PropertyFactoryFunctor(&CreateStructProperty));
	PropertyFactoryMap.Add(UArrayProperty::StaticClass()->GetFName(), PropertyFactoryFunctor(&CreateArrayProperty));
	PropertyFactoryMap.Add(UWeakObjectProperty::StaticClass()->GetFName(), PropertyFactoryFunctor(&CreateWeakObjectProperty));
}

UProperty* FMonoPropertyFactory::Create(UObject& Outer, FMonoBindings& Bindings, const FMonoPropertyMetadata& Metadata) const
{
	const PropertyFactoryFunctor* pFactory = PropertyFactoryMap.Find(Metadata.UnrealPropertyType->UnrealPropertyClass);
	if (nullptr == pFactory)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PropertyType"), FText::FromName(Metadata.UnrealPropertyType->UnrealPropertyClass));
		Args.Add(TEXT("ClassName"), FText::FromString(Outer.GetName()));
		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("NoPropertyFactoryFound", "No property factory found for property type '{PropertyType}' in class '{ClassName}'"), Args));
		return nullptr;
	}
	else
	{
		UProperty* Property = (*pFactory)(Outer, Bindings, Metadata);
		Property->ArrayDim = Metadata.UnrealPropertyType->ArrayDim;
#if WITH_METADATA
		for (auto Pair : Metadata.Metadata)
		{
			Property->SetMetaData(Pair.Key, *Pair.Value);
		}
#endif // WITH_METADATA

		return Property;
	}
}

#undef LOCTEXT_NAMESPACE