// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"

#include <mono/metadata/class.h>
#include <mono/metadata/object.h>

class FJsonObject;
class FMonoBindings;

enum class EMonoAccessModifier : uint8
{
	Private,
	Protected,
	Public
};

struct FMonoKeyValuePair
{
	FName Key;
	FString Value;

	bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object);

};

struct MONORUNTIME_API FMonoTypeReferenceMetadata
{
	FString Namespace;
	FString Name;
	FString AssemblyName;

	bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object);

	FString GetQualifiedName() const;

	FMonoTypeReferenceMetadata() {}

	FMonoTypeReferenceMetadata(const FString& InNamespace, const FString& InName, const FString& InAssemblyName);
	FMonoTypeReferenceMetadata(MonoString* InNamespace, MonoString* InName, MonoString* InAssemblyName);

};

struct FMonoUnrealType
{
	FName UnrealPropertyClass;
	int32 ArrayDim;

	virtual ~FMonoUnrealType();

	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object);


	static bool ParseDerivedFromJsonObject(TUniquePtr<FMonoUnrealType>& Dest, FString& ErrorMessage, const FJsonObject& Object);
};

struct FMonoCoreStructType : public FMonoUnrealType
{
	FString StructName;

	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object) override;
};

struct FMonoStructType : public FMonoUnrealType
{
	FString NativeClassOwner;
	FMonoTypeReferenceMetadata TypeRef;

	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object) override;
};

struct FMonoUnrealEnumType : public FMonoUnrealType
{
	FString NativeClassOwner;
	FString NativeEnumName;
	FMonoTypeReferenceMetadata TypeRef;

	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object) override;
};

struct FMonoUnrealObjectType : public FMonoUnrealType
{
	FMonoTypeReferenceMetadata TypeRef;

	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object) override;
};

struct FMonoUnrealClassType : public FMonoUnrealType
{
	FMonoTypeReferenceMetadata TypeRef;

	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object) override;
};

struct FMonoUnrealWeakObjectType : public FMonoUnrealType
{
	FMonoTypeReferenceMetadata TypeRef;

	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object) override;
};

struct FMonoMetadataBase
{
	FName Name;
	FString NameCaseSensitive;
#if WITH_METADATA
	TArray<FMonoKeyValuePair> Metadata;
#endif // WITH_METADATA

protected:
	EMonoAccessModifier Protection;

public:
	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object);

	virtual ~FMonoMetadataBase() {}
};
struct FMonoPropertyMetadata : public FMonoMetadataBase
{
	TUniquePtr<FMonoUnrealType> UnrealPropertyType;
	FName RepNotifyFunctionName;

	EPropertyFlags GetPropertyFlags() const;
	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object) override;

private:
	EPropertyFlags PropertyFlags;

};

struct FMonoFunctionMetadata : public FMonoMetadataBase
{
	FMonoPropertyMetadata ReturnValueProperty;
	TArray<FMonoPropertyMetadata> ParamProperties;

	EFunctionFlags GetFunctionFlags() const;
	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object) override;

private:
	EFunctionFlags FunctionFlags;

};

struct FMonoEnumMetadata : public FMonoTypeReferenceMetadata
{
	FString EnumHash;
	TArray<FString> Items;
	bool BlueprintVisible;

	bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object);
private:
};


struct FMonoClassMetadata : public FMonoTypeReferenceMetadata
{
	TArray<FString> VirtualFunctions;
	TArray<FMonoPropertyMetadata> Properties;
	TArray<FMonoFunctionMetadata> Functions;
	FMonoTypeReferenceMetadata BaseClass;
	FMonoTypeReferenceMetadata BaseUnrealNativeClass;
	bool ChildCanTick;
	bool OverridesBindInput;
	FString ClassHash;
	FString BlueprintUse;
	FString Transience;
	FString Placeablity;
	bool Abstract;
	bool Deprecated;
	FString Group;
	FString ConfigFile;
	FString Flags;
	uint64 ClassFlags;

	bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object);

	TArray<FName> GetVirtualFunctions() const;

};

struct FMonoStructMetadata : public FMonoTypeReferenceMetadata
{
	TArray<FMonoPropertyMetadata> Properties;
	uint64 StructFlags;
	FString StructHash;
	
	bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object);
};

struct FMonoAssemblyReferenceMetadata
{
	FString AssemblyName;
	FString AssemblyPath;
	bool	Resolved;
	bool	InKnownLocation;

	bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object);
};

struct FMonoAssemblyMetadata
{
	FString AssemblyName;
	FString AssemblyPath;
	TArray<FMonoAssemblyReferenceMetadata> References;
	TArray<FMonoStructMetadata> Structs;
	TArray<FMonoClassMetadata> Classes;
	TArray<FMonoEnumMetadata> Enums;

	bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object);
};

struct FMonoLoadedAssemblyMetadata
{
	FString AssemblyFile;
	FString MetadataFile;
	FGuid   ScriptPackageGuid;
	TSharedPtr<FMonoAssemblyMetadata> AssemblyMetadata;

	static bool LoadAssemblyMetadataInDirectory(TArray<FMonoLoadedAssemblyMetadata>& Loaded, const FString& InDirectory);

private:
	static TSharedPtr<FMonoAssemblyMetadata> LoadAssemblyMetadata(FGuid& ScriptPackageGuid, const FString& InMetadataFile);

};

struct FMonoUnrealArrayType : public FMonoUnrealType
{
	FMonoPropertyMetadata InnerProperty;

	virtual bool ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object) override;
};

