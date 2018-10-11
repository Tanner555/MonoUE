// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/NameTypes.h"

//extracted from Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PyGenUtil.h
namespace ScriptGenUtil
{
	extern const FName ScriptNameMetaDataKey;
	extern const FName ScriptNoExportMetaDataKey;
	extern const FName ScriptMethodMetaDataKey;
	extern const FName ScriptMethodSelfReturnMetaDataKey;
	extern const FName ScriptOperatorMetaDataKey;
	extern const FName ScriptConstantMetaDataKey;
	extern const FName ScriptConstantHostMetaDataKey;
	extern const FName BlueprintTypeMetaDataKey;
	extern const FName NotBlueprintTypeMetaDataKey;
	extern const FName BlueprintSpawnableComponentMetaDataKey;
	extern const FName BlueprintGetterMetaDataKey;
	extern const FName BlueprintSetterMetaDataKey;
	extern const FName CustomStructureParamMetaDataKey;
	extern const FName HasNativeMakeMetaDataKey;
	extern const FName HasNativeBreakMetaDataKey;
	extern const FName NativeBreakFuncMetaDataKey;
	extern const FName NativeMakeFuncMetaDataKey;
	extern const FName DeprecatedPropertyMetaDataKey;
	extern const FName DeprecatedFunctionMetaDataKey;
	extern const FName DeprecationMessageMetaDataKey;

	/** Is the given class marked as deprecated? */
	bool IsDeprecatedClass(const UClass* InClass, FString* OutDeprecationMessage = nullptr);

	/** Is the given property marked as deprecated? */
	bool IsDeprecatedProperty(const UProperty* InProp, FString* OutDeprecationMessage = nullptr);

	/** Is the given function marked as deprecated? */
	bool IsDeprecatedFunction(const UFunction* InFunc, FString* OutDeprecationMessage = nullptr);

	/** Should the given class be exported to scripts? */
	bool ShouldExportClass(const UClass* InClass);

	/** Should the given struct be exported to scripts? */
	bool ShouldExportStruct(const UScriptStruct* InStruct);

	/** Should the given enum be exported to scripts? */
	bool ShouldExportEnum(const UEnum* InEnum);

	/** Should the given enum entry be exported to scripts? */
	bool ShouldExportEnumEntry(const UEnum* InEnum, int32 InEnumEntryIndex);

	/** Should the given property be exported to scripts? */
	bool ShouldExportProperty(const UProperty* InProp);

	/** Should the given property be exported to scripts as editor-only data? */
	bool ShouldExportEditorOnlyProperty(const UProperty* InProp);

	/** Should the given function be exported to scripts? */
	bool ShouldExportFunction(const UFunction* InFunc);

	enum EScriptNameKind : uint8
	{
		Class,
		Function,
		Property,
		Enum,
		ScriptMethod,
		Constant,
		Parameter,
		EnumValue
	};

	class ScriptNameMapper
	{
	public:
		virtual ~ScriptNameMapper() {}

		virtual FString ScriptifyName(const FString& InName, const EScriptNameKind InNameKind) const;

		/** Get the native module the given field belongs to */
		FString GetFieldModule(const UField* InField) const;

		/** Get the plugin module the given field belongs to (if any) */
		FString GetFieldPlugin(const UField* InField) const;

		/** Given a native module name, get the script module we should use */
		FName MapModuleName(const FName InModuleName) const;

		/** Get the script name of the given class */
		FString MapClassName(const UClass* InClass) const;

		/** Get the deprecated script names of the given class */
		TArray<FString> GetDeprecatedClassScriptNames(const UClass* InClass) const;

		/** Get the script name of the given struct */
		FString MapStructName(const UScriptStruct* InStruct) const;

		/** Get the deprecated script names of the given struct */
		TArray<FString> GetDeprecatedStructScriptNames(const UScriptStruct* InStruct) const;

		/** Get the script name of the given enum */
		FString MapEnumName(const UEnum* InEnum) const;

		/** Get the deprecated script names of the given enum */
		TArray<FString> GetDeprecatedEnumScriptNames(const UEnum* InEnum) const;

		/** Get the script name of the given enum entry */
		FString MapEnumEntryName(const UEnum* InEnum, const int32 InEntryIndex) const;

		/** Get the script name of the given delegate signature */
		FString MapDelegateName(const UFunction* InDelegateSignature) const;

		/** Get the script name of the given function */
		FString MapFunctionName(const UFunction* InFunc) const;

		/** Get the deprecated script names of the given function */
		TArray<FString> GetDeprecatedFunctionScriptNames(const UFunction* InFunc) const;

		/** Get the script name of the given function when it's hoisted as a script method */
		FString MapScriptMethodName(const UFunction* InFunc) const;

		/** Get the deprecated script names of the given function it's hoisted as a script method */
		TArray<FString> GetDeprecatedScriptMethodScriptNames(const UFunction* InFunc) const;

		/** Get the script name of the given function when it's hoisted as a script constant */
		FString MapScriptConstantName(const UFunction* InFunc) const;

		/** Get the deprecated script names of the given function it's hoisted as a script constant */
		TArray<FString> GetDeprecatedScriptConstantScriptNames(const UFunction* InFunc) const;

		/** Get the script name of the given property */
		FString MapPropertyName(const UProperty* InProp) const;

		/** Get the deprecated script names of the given property */
		TArray<FString> GetDeprecatedPropertyScriptNames(const UProperty* InProp) const;

		/** Get the script name of the given function parameter */
		FString MapParameterName(const UProperty* InProp) const;
	};

	/** Case sensitive hashing function for TSet */
	struct FCaseSensitiveStringSetFuncs : BaseKeyFuncs<FString, FString>
	{
		static FORCEINLINE const FString& GetSetKey(const FString& Element)
		{
			return Element;
		}
		static FORCEINLINE bool Matches(const FString& A, const FString& B)
		{
			return A.Equals(B, ESearchCase::CaseSensitive);
		}
		static FORCEINLINE uint32 GetKeyHash(const FString& Key)
		{
			return FCrc::StrCrc32<TCHAR>(*Key);
		}
	};
}