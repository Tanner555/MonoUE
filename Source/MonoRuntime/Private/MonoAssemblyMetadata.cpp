// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoAssemblyMetadata.h"

#include "UObject/UnrealType.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Logging/MessageLog.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"

#include "MonoBindings.h"
#include "MonoRuntimeCommon.h"

#define LOCTEXT_NAMESPACE "MonoRuntime"

#define JSON_READ_BOOL(MemberName) \
	if(!ReadBoolFieldChecked(MemberName, ErrorMessage, Object, FString( "" #MemberName ))) \
	{ \
	return false; \
	}

#define JSON_READ_INT(MemberName) \
	if(!ReadIntFieldChecked(MemberName, ErrorMessage, Object, FString( "" #MemberName ))) \
	{ \
		return false; \
	}

#define JSON_READ_STRING(MemberName) \
	if(!ReadStringFieldChecked(MemberName, ErrorMessage, Object, FString( "" #MemberName ))) \
	{ \
		return false; \
	}

#define JSON_READ_OPTIONAL_STRING(MemberName) ReadStringField(MemberName, Object, FString( "" #MemberName ));

#define JSON_READ_STRING_ARRAY(MemberName) \
	if(!ReadStringArrayFieldChecked(MemberName, ErrorMessage, Object, FString( "" #MemberName))) \
	{ \
		return false; \
	}

#define JSON_READ_ENUM(MemberName, MappingFunction) \
	if(!ReadEnumFieldChecked(MemberName, MappingFunction, ErrorMessage, Object, FString( "" #MemberName))) \
	{ \
		return false; \
	}

#define JSON_PARSE_OBJECT(MemberName) \
	if(!ParseObjectFieldChecked(MemberName, ErrorMessage, Object, FString( "" #MemberName))) \
	{ \
	return false; \
	}

#define JSON_PARSE_OBJECT_ARRAY(MemberName) \
	if(!ParseObjectArrayFieldChecked(MemberName, ErrorMessage, Object, FString("" #MemberName))) \
	{ \
	return false; \
	}

static bool ReadBoolFieldChecked(bool& Dest, FString& ErrorMessage, const FJsonObject& Object, const FString& FieldName)
{
	if (!Object.HasTypedField<EJson::Boolean>(FieldName))
	{
		ErrorMessage = FString::Printf(TEXT("Assembly metadata missing or wrongly typed field %s"), *FieldName);
		return false;
	}
	Dest = Object.GetBoolField(FieldName);
	return true;
}

static bool ReadIntFieldChecked(int32& Dest, FString& ErrorMessage, const FJsonObject& Object, const FString& FieldName)
{
	if (!Object.HasTypedField<EJson::Number>(FieldName))
	{
		ErrorMessage = FString::Printf(TEXT("Assembly metadata missing or wrongly typed field %s"), *FieldName);
		return false;
	}
	Dest = (int32)Object.GetNumberField(FieldName);
	return true;
}

static bool ReadStringField(FString& Dest, const FJsonObject& Object, const FString& FieldName)
{
	if (Object.HasTypedField<EJson::String>(FieldName))
	{
		Dest = Object.GetStringField(FieldName);
		return true;
	}
	return false;
}

static bool ReadStringFieldChecked(FString& Dest, FString& ErrorMessage, const FJsonObject& Object, const FString& FieldName)
{
	if (!ReadStringField(Dest, Object, FieldName))
	{
		ErrorMessage = FString::Printf(TEXT("Assembly metadata missing or wrongly typed field %s"), *FieldName);
		return false;
	}
	return true;
}

static bool ReadStringField(FName& Dest, const FJsonObject& Object, const FString& FieldName)
{
	FString TempString;
	if (ReadStringField(TempString, Object, FieldName))
	{
		Dest = FName(*TempString);
		return true;
	}
	return false;
}

static bool ReadStringFieldChecked(FName& Dest, FString& ErrorMessage, const FJsonObject& Object, const FString& FieldName)
{
	FString TempString;
	if (!ReadStringFieldChecked(TempString, ErrorMessage, Object, FieldName))
	{
		return false;
	}
	Dest = FName(*TempString);
	return true;
}

static bool ReadStringArrayFieldChecked(TArray<FString>& Dest, FString& ErrorMessage, const FJsonObject& Object, const FString& FieldName)
{
	if (!Object.HasTypedField<EJson::Array>(FieldName))
	{
		ErrorMessage = FString::Printf(TEXT("Assembly metadata missing or wrongly typed array field %s"), *FieldName);
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>& FieldArray = Object.GetArrayField(FieldName);

	Dest.Empty(FieldArray.Num());
	for (const auto& Value : FieldArray)
	{
		if (Value->Type != EJson::String)
		{
			ErrorMessage = FString::Printf(TEXT("Assembly metadata field %s should be an array of strings, is not"), *FieldName);
			return false;
		}
		Dest.Add(Value->AsString());
	}
	return true;
}

template <class T>
static bool ReadEnumFieldChecked(T& Dest, bool (*MapFromString)(T&, FString&, const FString&), FString& ErrorMessage, const FJsonObject& Object, const FString& FieldName)
{
	FString TempString;
	if (!ReadStringFieldChecked(TempString, ErrorMessage, Object, FieldName))
	{
		return false;
	}
	if (!MapFromString(Dest, ErrorMessage, TempString))
	{
		return false;
	}
	return true;
}

template <class T>
static bool ParseObjectFieldChecked(T& Dest, FString& ErrorMessage, const FJsonObject& Object, const FString& FieldName)
{
	if (!Object.HasTypedField<EJson::Object>(FieldName))
	{
		ErrorMessage = FString::Printf(TEXT("Assembly metadata missing or wrongly typed object field %s"), *FieldName);
		return false;
	}

	return Dest.ParseFromJsonObject(ErrorMessage, *Object.GetObjectField(FieldName));
}

template <class T>
static bool ParseObjectFieldChecked(TUniquePtr<T>& Dest, FString& ErrorMessage, const FJsonObject& Object, const FString& FieldName)
{
	if (!Object.HasTypedField<EJson::Object>(FieldName))
	{
		ErrorMessage = FString::Printf(TEXT("Assembly metadata missing or wrongly typed object field %s"), *FieldName);
		return false;
	}

	if (!T::ParseDerivedFromJsonObject(Dest, ErrorMessage, *Object.GetObjectField(FieldName)))
	{
		return false;
	}

	return true;
}

template <class T>
static bool ParseObjectArrayFieldChecked(TArray<T>& Dest, FString& ErrorMessage, const FJsonObject& Object, const FString& FieldName)
{
	if (!Object.HasTypedField<EJson::Array>(FieldName))
	{
		ErrorMessage = FString::Printf(TEXT("Assembly metadata missing or wrongly typed array field %s"), *FieldName);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>& FieldArray = Object.GetArrayField(FieldName);

	Dest.Empty(FieldArray.Num());
	for (const auto& Value : FieldArray)
	{
		T& NewDest = *(new(Dest) T);

		if (Value->Type != EJson::Object)
		{
			ErrorMessage = FString::Printf(TEXT("Assembly metadata field %s should be an array of objects, is not"), *FieldName);
			return false;
		}

		if (!NewDest.ParseFromJsonObject(ErrorMessage, *Value->AsObject()))
		{
			return false;
		}
	}
	return true;
}

static bool MapMonoProtection(EMonoAccessModifier& Dest, FString& ErrorMessage, const FString& TempString)
{
	if (TempString == TEXT("Private"))
	{
		Dest = EMonoAccessModifier::Private;
		return true;
	}
	else if (TempString == TEXT("Protected"))
	{
		Dest = EMonoAccessModifier::Protected;
		return true;
	}
	else if (TempString == TEXT("Public"))
	{
		Dest = EMonoAccessModifier::Public;
		return true;
	}
	else
	{
		ErrorMessage = FString::Printf(TEXT("Unknown property protection type %s"), *TempString);
		return false;
	}
}

bool FMonoKeyValuePair::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	JSON_READ_STRING(Key);
	JSON_READ_STRING(Value);

	return true;
}

FMonoUnrealType::~FMonoUnrealType()
{
}

bool FMonoUnrealType::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	JSON_READ_STRING(UnrealPropertyClass);
	JSON_READ_INT(ArrayDim);
	return true;
}

bool FMonoUnrealType::ParseDerivedFromJsonObject(TUniquePtr<FMonoUnrealType>& Dest, FString& ErrorMessage, const FJsonObject& Object)
{
	FName PropertyClass;
	if (ReadStringFieldChecked(PropertyClass, ErrorMessage, Object, TEXT("UnrealPropertyClass")))
	{
		static const FName EnumPropertyName("EnumProperty");
		static const FName CoreStructPropertyName("CoreStructProperty");

		if (PropertyClass == EnumPropertyName)
		{
			Dest.Reset(new FMonoUnrealEnumType);
		}
		else if (PropertyClass == CoreStructPropertyName)
		{
			Dest.Reset(new FMonoCoreStructType);
		}
		else if (PropertyClass == UStructProperty::StaticClass()->GetFName())
		{
			Dest.Reset(new FMonoStructType);
		}
		else if (PropertyClass == UObjectProperty::StaticClass()->GetFName())
		{
			Dest.Reset(new FMonoUnrealObjectType);
		}
		else if (PropertyClass == UClassProperty::StaticClass()->GetFName())
		{
			Dest.Reset(new FMonoUnrealClassType);
		}
		else if (PropertyClass == UWeakObjectProperty::StaticClass()->GetFName())
		{
			Dest.Reset(new FMonoUnrealWeakObjectType);
		}
		else if (PropertyClass == UArrayProperty::StaticClass()->GetFName())
		{
			Dest.Reset(new FMonoUnrealArrayType);
		}
		else
		{
			Dest.Reset(new FMonoUnrealType);
		}

		if (!Dest->ParseFromJsonObject(ErrorMessage, Object))
		{
			return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool FMonoCoreStructType::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoUnrealType::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}
	JSON_READ_STRING(StructName);
	return true;
}

bool FMonoStructType::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoUnrealType::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}
	JSON_READ_STRING(NativeClassOwner);
	JSON_PARSE_OBJECT(TypeRef);
	return true;
}

bool FMonoUnrealEnumType::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoUnrealType::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}
	JSON_READ_STRING(NativeClassOwner);
	JSON_READ_STRING(NativeEnumName);
	JSON_PARSE_OBJECT(TypeRef);
	return true;
}

bool FMonoUnrealObjectType::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoUnrealType::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}
	JSON_PARSE_OBJECT(TypeRef);

	return true;
}

bool FMonoUnrealClassType::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoUnrealType::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}
	JSON_PARSE_OBJECT(TypeRef);

	return true;
}

bool FMonoUnrealWeakObjectType::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoUnrealType::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}
	JSON_PARSE_OBJECT(TypeRef);

	return true;
}

bool FMonoUnrealArrayType::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoUnrealType::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}

	JSON_PARSE_OBJECT(InnerProperty);

	return true;
}

bool FMonoMetadataBase::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!ReadStringFieldChecked(NameCaseSensitive, ErrorMessage, Object, FString(TEXT("Name"))))
		return false;
	Name = static_cast<FName>(*NameCaseSensitive);
	JSON_READ_ENUM(Protection, MapMonoProtection);
#if WITH_METADATA
	JSON_PARSE_OBJECT_ARRAY(Metadata);
#endif // WITH_METADATA

	return true;
}

bool FMonoPropertyMetadata::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (FMonoMetadataBase::ParseFromJsonObject(ErrorMessage, Object))
	{
		JSON_PARSE_OBJECT(UnrealPropertyType);

		FString Flags;
		JSON_READ_STRING(Flags);
		uint64 UintPropertyFlags;
		TTypeFromString<uint64>::FromString(UintPropertyFlags, *Flags);
		PropertyFlags = (EPropertyFlags)UintPropertyFlags;

		JSON_READ_OPTIONAL_STRING(RepNotifyFunctionName);

		return true;
	}

	return false;
}

EPropertyFlags FMonoPropertyMetadata::GetPropertyFlags() const
{
	EPropertyFlags Flags = PropertyFlags;

	if (Protection == EMonoAccessModifier::Protected)
	{
		Flags|= CPF_Protected;
	}
	return Flags;
}

bool FMonoFunctionMetadata::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (FMonoMetadataBase::ParseFromJsonObject(ErrorMessage, Object))
	{
		// Return value might be null, for void functions.
		if (Object.HasTypedField<EJson::Object>("ReturnValueProperty"))
		{
			JSON_PARSE_OBJECT(ReturnValueProperty);
		}

		JSON_PARSE_OBJECT_ARRAY(ParamProperties);

		FString Flags;
		JSON_READ_STRING(Flags);
		uint64 UintFunctionFlags;
		TTypeFromString<uint64>::FromString(UintFunctionFlags, *Flags);
		FunctionFlags = (EFunctionFlags)UintFunctionFlags;

		return true;
	}

	return false;
}

bool FMonoEnumMetadata::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoTypeReferenceMetadata::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}

	JSON_READ_STRING_ARRAY(Items);
	JSON_READ_STRING(EnumHash);
	JSON_READ_BOOL(BlueprintVisible);
	return true;
}

EFunctionFlags FMonoFunctionMetadata::GetFunctionFlags() const
{
	return FunctionFlags;
}

bool FMonoTypeReferenceMetadata::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	JSON_READ_STRING(Namespace);
	JSON_READ_STRING(Name);
	JSON_READ_STRING(AssemblyName);

	return true;
}

FString FMonoTypeReferenceMetadata::GetQualifiedName() const
{
	if (Namespace.Len() > 0)
	{
		return (Namespace + TEXT(".")) + Name;
	}
	else
	{
		return Name;
	}

}

FMonoTypeReferenceMetadata::FMonoTypeReferenceMetadata(const FString& InNamespace, const FString& InName, const FString& InAssemblyName)
	: Namespace(InNamespace)
	, Name(InName)
	, AssemblyName(InAssemblyName)
{
}

FMonoTypeReferenceMetadata::FMonoTypeReferenceMetadata(MonoString* InNamespace, MonoString* InName, MonoString* InAssemblyName)
{
	Mono::MonoStringToFString(Namespace, InNamespace);
	Mono::MonoStringToFString(Name, InName);
	Mono::MonoStringToFString(AssemblyName, InAssemblyName);
}

bool FMonoClassMetadata::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoTypeReferenceMetadata::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}

	JSON_READ_STRING_ARRAY(VirtualFunctions);

	JSON_PARSE_OBJECT_ARRAY(Properties);
	JSON_PARSE_OBJECT_ARRAY(Functions);

	JSON_PARSE_OBJECT(BaseClass);
	JSON_PARSE_OBJECT(BaseUnrealNativeClass);

	JSON_READ_BOOL(ChildCanTick);
	JSON_READ_BOOL(OverridesBindInput);
	JSON_READ_STRING(ClassHash);

	JSON_READ_STRING(BlueprintUse);
	JSON_READ_STRING(Transience);
	JSON_READ_STRING(Placeablity);
	JSON_READ_BOOL(Deprecated);
	JSON_READ_BOOL(Abstract);
	JSON_READ_STRING(Group);
	JSON_READ_STRING(ConfigFile);
	JSON_READ_STRING(Flags);
	TTypeFromString<uint64>::FromString(ClassFlags, *Flags);

	return true;
}

TArray<FName> FMonoClassMetadata::GetVirtualFunctions() const
{
	TArray<FName> OutVirtualFunctions;

	OutVirtualFunctions.Empty(VirtualFunctions.Num());

	for (auto Function : VirtualFunctions)
	{
		OutVirtualFunctions.Add(FName(*Function));
	}

	return OutVirtualFunctions;
}

bool FMonoStructMetadata::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	if (!FMonoTypeReferenceMetadata::ParseFromJsonObject(ErrorMessage, Object))
	{
		return false;
	}

	JSON_PARSE_OBJECT_ARRAY(Properties);

	FString Flags;
	JSON_READ_STRING(Flags);
	TTypeFromString<uint64>::FromString(StructFlags, *Flags);

	JSON_READ_STRING(StructHash);

	return true;
}


bool FMonoAssemblyReferenceMetadata::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	JSON_READ_STRING(AssemblyName);
	JSON_READ_STRING(AssemblyPath);
	JSON_READ_BOOL(Resolved);
	JSON_READ_BOOL(InKnownLocation);

	return true;
}

bool FMonoAssemblyMetadata::ParseFromJsonObject(FString& ErrorMessage, const FJsonObject& Object)
{
	JSON_READ_STRING(AssemblyName);
	JSON_READ_STRING(AssemblyPath);

	JSON_PARSE_OBJECT_ARRAY(References);

	JSON_PARSE_OBJECT_ARRAY(Structs);
	JSON_PARSE_OBJECT_ARRAY(Classes);
	JSON_PARSE_OBJECT_ARRAY(Enums);

	return true;
}

#undef JSON_READ_BOOL
#undef JSON_READ_STRING
#undef JSON_READ_STRING_ARRAY
#undef JSON_READ_ENUM
#undef JSON_PARSE_OBJECT
#undef JSON_PARSE_OBJECT_ARRAY

bool FMonoLoadedAssemblyMetadata::LoadAssemblyMetadataInDirectory(TArray<FMonoLoadedAssemblyMetadata>& Loaded, const FString& InDirectory)
{
	bool bAnyFailed = false;
	TArray<FString> MetadataFiles;
	IFileManager::Get().FindFiles(MetadataFiles, *FPaths::Combine(*InDirectory, TEXT("*.json")), true, false);

	for (const FString& MetadataFileName : MetadataFiles)
	{
		const FString MetadataFile = FPaths::Combine(*InDirectory, *MetadataFileName);
		const FString AssemblyFile = FPaths::GetBaseFilename(MetadataFile, false) + TEXT(".dll");

		if (FPaths::FileExists(*AssemblyFile))
		{
			FGuid ScriptPackageGuid;
			TSharedPtr<FMonoAssemblyMetadata> Metadata = LoadAssemblyMetadata(ScriptPackageGuid, MetadataFile);

			if (Metadata.IsValid())
			{
				FMonoLoadedAssemblyMetadata LoadedAssemblyMetadata;
				LoadedAssemblyMetadata.AssemblyFile = AssemblyFile;
				LoadedAssemblyMetadata.MetadataFile = MetadataFile;
				LoadedAssemblyMetadata.AssemblyMetadata = Metadata;
				LoadedAssemblyMetadata.ScriptPackageGuid = ScriptPackageGuid;
				Loaded.Add(LoadedAssemblyMetadata);
			}
			else
			{
				bAnyFailed = true;
			}
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("MetadataFile"), FText::FromString(MetadataFile));
			Args.Add(TEXT("AssemblyFile"), FText::FromString(AssemblyFile));
			FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("NoGameAssemblyFound", "Found game assembly metadata file '{MetadataFile}' but no assembly '{AssemblyFile}'"), Args));
			bAnyFailed = true;
		}
	}

	return !bAnyFailed;
}

TSharedPtr<FMonoAssemblyMetadata> FMonoLoadedAssemblyMetadata::LoadAssemblyMetadata(FGuid& ScriptPackageGuid, const FString& MetadataFile)
{
	FString CleanMetadataFile(FPaths::GetCleanFilename(MetadataFile));

	FString MetadataJsonString;
	if (!FFileHelper::LoadFileToString(MetadataJsonString, *MetadataFile))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MetadataFile"), FText::FromString(MetadataFile));
		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotLoadMetadata", "Could not load game assembly metadata file '{MetadataFile}'"), Args));
		return TSharedPtr<FMonoAssemblyMetadata>();
	}

	TSharedPtr<FJsonObject> MetadataObject = nullptr;
	TSharedRef<TJsonReader<> > Reader = TJsonReaderFactory<TCHAR>::Create(MetadataJsonString);
	if (!FJsonSerializer::Deserialize(Reader, MetadataObject) || !MetadataObject.IsValid())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(CleanMetadataFile));
		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotParseMetadata", "Could not parse metadata file '{CleanMetadataFile}'"), Args));
		return TSharedPtr<FMonoAssemblyMetadata>();
	}

	TSharedPtr<FMonoAssemblyMetadata> Metadata(new FMonoAssemblyMetadata());

	FString ErrorMessage;
	if (!Metadata->ParseFromJsonObject(ErrorMessage, *MetadataObject))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanMetadataFile"), FText::FromString(CleanMetadataFile));
		Args.Add(TEXT("ErrorMessage"), FText::FromString(ErrorMessage));
		FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("ErrorParsingMetadata", "Error parsing metadata file '{CleanMetadataFile}': '{ErrorMessage}'"), Args));
		return TSharedPtr<FMonoAssemblyMetadata>();
	}

	// Metadata verification
	// Make sure all assembly references are either system references, or bindings references, and were resolved correctly. We don't yet support class libs referencing other class libs, 
	// or general references to non-class lib assemblies. (This is a TODO)
	bool bFailedResolve = false;

	for (const auto& Reference : Metadata->References)
	{
		if (!Reference.Resolved)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("CleanMetadataFile"), FText::FromString(CleanMetadataFile));
			Args.Add(TEXT("AssemblyName"), FText::FromString(Reference.AssemblyName));
			FMessageLog(NAME_MonoErrors).Error(FText::Format(LOCTEXT("CouldNotResolveAssembly", "'{CleanMetadataFile}': Assembly reference '{AssemblyName}' could not be resolved."), Args));
			bFailedResolve = true;
		}
	}

	if (bFailedResolve)
	{
		return TSharedPtr<FMonoAssemblyMetadata>();
	}

	// generate a package guid from a hash of the manifest file
	// I'm not sure how globally unique this actually is, but this mirrors what Epic does for script packages in the code generator
	// we use MD5 because it has a hash size of 16 bytes, which fits in a guid
	FString UpperCaseMetadataText = MetadataJsonString.ToUpper();
	FMD5  Hash;
	Hash.Update(reinterpret_cast<uint8*>(UpperCaseMetadataText.GetCharArray().GetData()), UpperCaseMetadataText.Len()*sizeof(TCHAR));
	Hash.Final(reinterpret_cast<uint8*>(&ScriptPackageGuid));

	return Metadata;
}

#undef LOCTEXT_NAMESPACE
