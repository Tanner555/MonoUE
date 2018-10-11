// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ScriptGenUtil.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/CoreRedirects.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "Interfaces/IPluginManager.h"

#include "MapModuleName.inl"

//extracted from Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PyGenUtil.cpp
namespace ScriptGenUtil
{

const FName ScriptNameMetaDataKey = TEXT("ScriptName");
const FName ScriptNoExportMetaDataKey = TEXT("ScriptNoExport");
const FName ScriptMethodMetaDataKey = TEXT("ScriptMethod");
const FName ScriptMethodSelfReturnMetaDataKey = TEXT("ScriptMethodSelfReturn");
const FName ScriptOperatorMetaDataKey = TEXT("ScriptOperator");
const FName ScriptConstantMetaDataKey = TEXT("ScriptConstant");
const FName ScriptConstantHostMetaDataKey = TEXT("ScriptConstantHost");
const FName BlueprintTypeMetaDataKey = TEXT("BlueprintType");
const FName NotBlueprintTypeMetaDataKey = TEXT("NotBlueprintType");
const FName BlueprintSpawnableComponentMetaDataKey = TEXT("BlueprintSpawnableComponent");
const FName BlueprintGetterMetaDataKey = TEXT("BlueprintGetter");
const FName BlueprintSetterMetaDataKey = TEXT("BlueprintSetter");
const FName DeprecatedPropertyMetaDataKey = TEXT("DeprecatedProperty");
const FName DeprecatedFunctionMetaDataKey = TEXT("DeprecatedFunction");
const FName DeprecationMessageMetaDataKey = TEXT("DeprecationMessage");
const FName CustomStructureParamMetaDataKey = TEXT("CustomStructureParam");
const FName HasNativeMakeMetaDataKey = TEXT("HasNativeMake");
const FName HasNativeBreakMetaDataKey = TEXT("HasNativeBreak");
const FName NativeBreakFuncMetaDataKey = TEXT("NativeBreakFunc");
const FName NativeMakeFuncMetaDataKey = TEXT("NativeMakeFunc");
const FName ReturnValueKey = TEXT("ReturnValue");
const TCHAR* HiddenMetaDataKey = TEXT("Hidden");

bool IsBlueprintExposedClass(const UClass* InClass)
{
	for (const UClass* ParentClass = InClass; ParentClass; ParentClass = ParentClass->GetSuperClass())
	{
		if (ParentClass->GetBoolMetaData(BlueprintTypeMetaDataKey) || ParentClass->HasMetaData(BlueprintSpawnableComponentMetaDataKey))
		{
			return true;
		}

		if (ParentClass->GetBoolMetaData(NotBlueprintTypeMetaDataKey))
		{
			return false;
		}
	}

	return false;
}

bool IsBlueprintExposedStruct(const UScriptStruct* InStruct)
{
	for (const UScriptStruct* ParentStruct = InStruct; ParentStruct; ParentStruct = Cast<UScriptStruct>(ParentStruct->GetSuperStruct()))
	{
		if (ParentStruct->GetBoolMetaData(BlueprintTypeMetaDataKey))
		{
			return true;
		}

		if (ParentStruct->GetBoolMetaData(NotBlueprintTypeMetaDataKey))
		{
			return false;
		}
	}

	return false;
}

bool IsBlueprintExposedEnum(const UEnum* InEnum)
{
	if (InEnum->GetBoolMetaData(BlueprintTypeMetaDataKey))
	{
		return true;
	}

	if (InEnum->GetBoolMetaData(NotBlueprintTypeMetaDataKey))
	{
		return false;
	}

	return false;
}

bool IsBlueprintExposedEnumEntry(const UEnum* InEnum, int32 InEnumEntryIndex)
{
	return !InEnum->HasMetaData(HiddenMetaDataKey, InEnumEntryIndex);
}

bool IsBlueprintExposedProperty(const UProperty* InProp)
{
	return InProp->HasAnyPropertyFlags(CPF_BlueprintVisible);
}

bool IsBlueprintExposedFunction(const UFunction* InFunc)
{
	return InFunc->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintEvent)
		&& !InFunc->HasMetaData(BlueprintGetterMetaDataKey)
		&& !InFunc->HasMetaData(BlueprintSetterMetaDataKey)
		&& !InFunc->HasMetaData(CustomStructureParamMetaDataKey)
		&& !InFunc->HasMetaData(NativeBreakFuncMetaDataKey)
		&& !InFunc->HasMetaData(NativeMakeFuncMetaDataKey);
}

bool IsBlueprintExposedField(const UField* InField)
{
	if (const UProperty* Prop = Cast<const UProperty>(InField))
	{
		return IsBlueprintExposedProperty(Prop);
	}

	if (const UFunction* Func = Cast<const UFunction>(InField))
	{
		return IsBlueprintExposedFunction(Func);
	}

	return false;
}

bool HasBlueprintExposedFields(const UStruct* InStruct)
{
	for (TFieldIterator<const UField> FieldIt(InStruct); FieldIt; ++FieldIt)
	{
		if (IsBlueprintExposedField(*FieldIt))
		{
			return true;
		}
	}

	return false;
}

bool IsDeprecatedClass(const UClass* InClass, FString* OutDeprecationMessage)
{
	if (InClass->HasAnyClassFlags(CLASS_Deprecated))
	{
		if (OutDeprecationMessage)
		{
			*OutDeprecationMessage = InClass->GetMetaData(DeprecationMessageMetaDataKey);
			if (OutDeprecationMessage->IsEmpty())
			{
				*OutDeprecationMessage = FString::Printf(TEXT("Class '%s' is deprecated."), *InClass->GetName());
			}
		}

		return true;
	}

	return false;
}

bool IsDeprecatedProperty(const UProperty* InProp, FString* OutDeprecationMessage)
{
	if (InProp->HasMetaData(DeprecatedPropertyMetaDataKey))
	{
		if (OutDeprecationMessage)
		{
			*OutDeprecationMessage = InProp->GetMetaData(DeprecationMessageMetaDataKey);
			if (OutDeprecationMessage->IsEmpty())
			{
				*OutDeprecationMessage = FString::Printf(TEXT("Property '%s' is deprecated."), *InProp->GetName());
			}
		}

		return true;
	}

	return false;
}

bool IsDeprecatedFunction(const UFunction* InFunc, FString* OutDeprecationMessage)
{
	if (InFunc->HasMetaData(DeprecatedFunctionMetaDataKey))
	{
		if (OutDeprecationMessage)
		{
			*OutDeprecationMessage = InFunc->GetMetaData(DeprecationMessageMetaDataKey);
			if (OutDeprecationMessage->IsEmpty())
			{
				*OutDeprecationMessage = FString::Printf(TEXT("Function '%s' is deprecated."), *InFunc->GetName());
			}
		}

		return true;
	}

	return false;
}

bool ShouldExportClass(const UClass* InClass)
{
	return IsBlueprintExposedClass(InClass) || HasBlueprintExposedFields(InClass);
}

bool ShouldExportStruct(const UScriptStruct* InStruct)
{
	return IsBlueprintExposedStruct(InStruct) || HasBlueprintExposedFields(InStruct);
}

bool ShouldExportEnum(const UEnum* InEnum)
{
	return IsBlueprintExposedEnum(InEnum);
}

bool ShouldExportEnumEntry(const UEnum* InEnum, int32 InEnumEntryIndex)
{
	return IsBlueprintExposedEnumEntry(InEnum, InEnumEntryIndex);
}

bool ShouldExportProperty(const UProperty* InProp)
{
	const bool bCanScriptExport = !InProp->HasMetaData(ScriptNoExportMetaDataKey);
	return bCanScriptExport && (IsBlueprintExposedProperty(InProp) || IsDeprecatedProperty(InProp));
}

bool ShouldExportEditorOnlyProperty(const UProperty* InProp)
{
	const bool bCanScriptExport = !InProp->HasMetaData(ScriptNoExportMetaDataKey);
	return bCanScriptExport && GIsEditor && (InProp->HasAnyPropertyFlags(CPF_Edit) || IsDeprecatedProperty(InProp));
}

bool ShouldExportFunction(const UFunction* InFunc)
{
	const bool bCanScriptExport = !InFunc->HasMetaData(ScriptNoExportMetaDataKey);
	return bCanScriptExport && IsBlueprintExposedFunction(InFunc);
}

FString StripPropertyPrefix(const FString& InName)
{
	int32 NameOffset = 0;

	for (;;)
	{
		// Strip the "b" prefix from bool names
		if (InName.Len() - NameOffset >= 2 && InName[NameOffset] == TEXT('b') && FChar::IsUpper(InName[NameOffset + 1]))
		{
			NameOffset += 1;
			continue;
		}

		// Strip the "In" prefix from names
		if (InName.Len() - NameOffset >= 3 && InName[NameOffset] == TEXT('I') && InName[NameOffset + 1] == TEXT('n') && FChar::IsUpper(InName[NameOffset + 2]))
		{
			NameOffset += 2;
			continue;
		}

		// Strip the "Out" prefix from names
		//if (InName.Len() - NameOffset >= 4 && InName[NameOffset] == TEXT('O') && InName[NameOffset + 1] == TEXT('u') && InName[NameOffset + 2] == TEXT('t') && FChar::IsUpper(InName[NameOffset + 3]))
		//{
		//	NameOffset += 3;
		//	continue;
		//}

		// Nothing more to strip
		break;
	}
	return NameOffset ? InName.RightChop(NameOffset) : InName;
}

FString ScriptNameMapper::ScriptifyName(const FString& InName, const EScriptNameKind InNameKind) const
{
	switch (InNameKind)
	{
	case EScriptNameKind::Property:
	case EScriptNameKind::Parameter:
		return StripPropertyPrefix (InName);
	}
	return InName;
}

FString ScriptNameMapper::GetFieldModule(const UField* InField) const
{
	UPackage* ScriptPackage = InField->GetOutermost();
	
	const FString PackageName = ScriptPackage->GetName();
	if (PackageName.StartsWith(TEXT("/Script/")))
	{
		return PackageName.RightChop(8); // Chop "/Script/" from the name
	}

	check(PackageName[0] == TEXT('/'));
	int32 RootNameEnd = 1;
	for (; PackageName[RootNameEnd] != TEXT('/'); ++RootNameEnd) {}
	return PackageName.Mid(1, RootNameEnd - 1);
}

FString ScriptNameMapper::GetFieldPlugin(const UField* InField) const
{
	static const TMap<FName, FString> ModuleNameToPluginMap = []()
	{
		IPluginManager& PluginManager = IPluginManager::Get();

		// Build up a map of plugin modules -> plugin names
		TMap<FName, FString> PluginModules;
		{
			TArray<TSharedRef<IPlugin>> Plugins = PluginManager.GetDiscoveredPlugins();
			for (const TSharedRef<IPlugin>& Plugin : Plugins)
			{
				for (const FModuleDescriptor& PluginModule : Plugin->GetDescriptor().Modules)
				{
					PluginModules.Add(PluginModule.Name, Plugin->GetName());
				}
			}
		}
		return PluginModules;
	}();

	const FString* FieldPluginNamePtr = ModuleNameToPluginMap.Find(*GetFieldModule(InField));
	return FieldPluginNamePtr ? *FieldPluginNamePtr : FString();
}

FName ScriptNameMapper::MapModuleName(const FName InModuleName) const
{
	return MapModuleNameToScriptModuleName(InModuleName);
}

bool GetFieldScriptNameFromMetaDataImpl(const UField* InField, const FName InMetaDataKey, FString& OutFieldName)
{
	// See if we have a name override in the meta-data
	if (!InMetaDataKey.IsNone())
	{
		OutFieldName = InField->GetMetaData(InMetaDataKey);

		// This may be a semi-colon separated list - the first item is the one we want for the current name
		if (!OutFieldName.IsEmpty())
		{
			int32 SemiColonIndex = INDEX_NONE;
			if (OutFieldName.FindChar(TEXT(';'), SemiColonIndex))
			{
				OutFieldName.RemoveAt(SemiColonIndex, OutFieldName.Len() - SemiColonIndex, /*bAllowShrinking*/false);
			}

			return true;
		}
	}

	return false;
}

bool GetDeprecatedFieldScriptNamesFromMetaDataImpl(const UField* InField, const FName InMetaDataKey, TArray<FString>& OutFieldNames)
{
	// See if we have a name override in the meta-data
	if (!InMetaDataKey.IsNone())
	{
		const FString FieldName = InField->GetMetaData(InMetaDataKey);

		// This may be a semi-colon separated list - everything but the first item is deprecated
		if (!FieldName.IsEmpty())
		{
			FieldName.ParseIntoArray(OutFieldNames, TEXT(";"), false);

			// Remove the non-deprecated entry
			if (OutFieldNames.Num() > 0)
			{
				OutFieldNames.RemoveAt(0, 1, /*bAllowShrinking*/false);
			}

			// Trim whitespace and remove empty items
			OutFieldNames.RemoveAll([](FString& InStr)
			{
				InStr.TrimStartAndEndInline();
				return InStr.IsEmpty();
			});

			return true;
		}
	}

	return false;
}

FString GetFieldScriptNameImpl(const UField* InField, const FName InMetaDataKey)
{
	FString FieldName;

	// First see if we have a name override in the meta-data
	if (GetFieldScriptNameFromMetaDataImpl(InField, InMetaDataKey, FieldName))
	{
		return FieldName;
	}

	// Just use the field name if we have no meta-data
	if (FieldName.IsEmpty())
	{
		FieldName = InField->GetName();

		// Strip the "E" prefix from enum names
		if (InField->IsA<UEnum>() && FieldName.Len() >= 2 && FieldName[0] == TEXT('E') && FChar::IsUpper(FieldName[1]))
		{
			FieldName.RemoveAt(0, 1, /*bAllowShrinking*/false);
		}
	}

	return FieldName;
}

TArray<FString> GetDeprecatedFieldScriptNamesImpl(const UField* InField, const FName InMetaDataKey)
{
	TArray<FString> FieldNames;

	// First see if we have a name override in the meta-data
	if (GetDeprecatedFieldScriptNamesFromMetaDataImpl(InField, InMetaDataKey, FieldNames))
	{
		return FieldNames;
	}

	// Just use the redirects if we have no meta-data
	ECoreRedirectFlags RedirectFlags = ECoreRedirectFlags::None;
	if (InField->IsA<UFunction>())
	{
		RedirectFlags = ECoreRedirectFlags::Type_Function;
	}
	else if (InField->IsA<UProperty>())
	{
		RedirectFlags = ECoreRedirectFlags::Type_Property;
	}
	else if (InField->IsA<UClass>())
	{
		RedirectFlags = ECoreRedirectFlags::Type_Class;
	}
	else if (InField->IsA<UScriptStruct>())
	{
		RedirectFlags = ECoreRedirectFlags::Type_Struct;
	}
	else if (InField->IsA<UEnum>())
	{
		RedirectFlags = ECoreRedirectFlags::Type_Enum;
	}
	
	const FCoreRedirectObjectName CurrentName = FCoreRedirectObjectName(InField);
	TArray<FCoreRedirectObjectName> PreviousNames;
	FCoreRedirects::FindPreviousNames(RedirectFlags, CurrentName, PreviousNames);

	FieldNames.Reserve(PreviousNames.Num());
	for (const FCoreRedirectObjectName& PreviousName : PreviousNames)
	{
		// Redirects can be used to redirect outers
		// We want to skip those redirects as we only care about changes within the current scope
		if (!PreviousName.OuterName.IsNone() && PreviousName.OuterName != CurrentName.OuterName)
		{
			continue;
		}

		// Redirects can often keep the same name when updating the path
		// We want to skip those redirects as we only care about name changes
		if (PreviousName.ObjectName == CurrentName.ObjectName)
		{
			continue;
		}
		
		FString FieldName = PreviousName.ObjectName.ToString();

		// Strip the "E" prefix from enum names
		if (InField->IsA<UEnum>() && FieldName.Len() >= 2 && FieldName[0] == TEXT('E') && FChar::IsUpper(FieldName[1]))
		{
			FieldName.RemoveAt(0, 1, /*bAllowShrinking*/false);
		}

		FieldNames.Add(MoveTemp(FieldName));
	}

	return FieldNames;
}

FString ScriptNameMapper::MapClassName(const UClass* InClass) const
{
	return GetFieldScriptNameImpl(InClass, ScriptNameMetaDataKey);
}

TArray<FString> ScriptNameMapper::GetDeprecatedClassScriptNames(const UClass* InClass) const
{
	return GetDeprecatedFieldScriptNamesImpl(InClass, ScriptNameMetaDataKey);
}

FString ScriptNameMapper::MapStructName(const UScriptStruct* InStruct) const
{
	return GetFieldScriptNameImpl(InStruct, ScriptNameMetaDataKey);
}

TArray<FString> ScriptNameMapper::GetDeprecatedStructScriptNames(const UScriptStruct* InStruct) const
{
	return GetDeprecatedFieldScriptNamesImpl(InStruct, ScriptNameMetaDataKey);
}

FString ScriptNameMapper::MapEnumName(const UEnum* InEnum) const
{
	return GetFieldScriptNameImpl(InEnum, ScriptNameMetaDataKey);
}

TArray<FString> ScriptNameMapper::GetDeprecatedEnumScriptNames(const UEnum* InEnum) const
{
	return GetDeprecatedFieldScriptNamesImpl(InEnum, ScriptNameMetaDataKey);
}

FString ScriptNameMapper::MapEnumEntryName(const UEnum* InEnum, const int32 InEntryIndex) const
{
	FString EnumEntryName;

	// First see if we have a name override in the meta-data
	{
		EnumEntryName = InEnum->GetMetaData(TEXT("ScriptName"), InEntryIndex);

		// This may be a semi-colon separated list - the first item is the one we want for the current name
		if (!EnumEntryName.IsEmpty())
		{
			int32 SemiColonIndex = INDEX_NONE;
			if (EnumEntryName.FindChar(TEXT(';'), SemiColonIndex))
			{
				EnumEntryName.RemoveAt(SemiColonIndex, EnumEntryName.Len() - SemiColonIndex, /*bAllowShrinking*/false);
			}
		}
	}
	
	// Just use the entry name if we have no meta-data
	if (EnumEntryName.IsEmpty())
	{
		EnumEntryName = InEnum->GetNameStringByIndex(InEntryIndex);
	}

	return ScriptifyName(EnumEntryName, EScriptNameKind::Enum);
}

FString ScriptNameMapper::MapDelegateName(const UFunction* InDelegateSignature) const
{
	FString DelegateName = InDelegateSignature->GetName().LeftChop(19); // Trim the "__DelegateSignature" suffix from the name
	return ScriptifyName(DelegateName, EScriptNameKind::Function);
}

FString ScriptNameMapper::MapFunctionName(const UFunction* InFunc) const
{
	FString FuncName = GetFieldScriptNameImpl(InFunc, ScriptNameMetaDataKey);
	return ScriptifyName(FuncName, EScriptNameKind::Function);
}

TArray<FString> ScriptNameMapper::GetDeprecatedFunctionScriptNames(const UFunction* InFunc) const
{
	const UClass* FuncOwner = InFunc->GetOwnerClass();
	check(FuncOwner);

	TArray<FString> FuncNames = GetDeprecatedFieldScriptNamesImpl(InFunc, ScriptNameMetaDataKey);
	for (auto FuncNamesIt = FuncNames.CreateIterator(); FuncNamesIt; ++FuncNamesIt)
	{
		FString& FuncName = *FuncNamesIt;

		// Remove any deprecated names that clash with an existing Script exposed function
		const UFunction* DeprecatedFunc = FuncOwner->FindFunctionByName(*FuncName);
		if (DeprecatedFunc && ShouldExportFunction(DeprecatedFunc))
		{
			FuncNamesIt.RemoveCurrent();
			continue;
		}

		FuncName = ScriptifyName(FuncName, EScriptNameKind::Function);
	}

	return FuncNames;
}

FString ScriptNameMapper::MapScriptMethodName(const UFunction* InFunc) const
{
	FString ScriptMethodName;
	if (GetFieldScriptNameFromMetaDataImpl(InFunc, ScriptMethodMetaDataKey, ScriptMethodName))
	{
		return ScriptifyName(ScriptMethodName, EScriptNameKind::ScriptMethod);
	}
	return MapFunctionName(InFunc);
}

TArray<FString> ScriptNameMapper::GetDeprecatedScriptMethodScriptNames(const UFunction* InFunc) const
{
	TArray<FString> ScriptMethodNames;
	if (GetDeprecatedFieldScriptNamesFromMetaDataImpl(InFunc, ScriptMethodMetaDataKey, ScriptMethodNames))
	{
		for (FString& ScriptMethodName : ScriptMethodNames)
		{
			ScriptMethodName = ScriptifyName(ScriptMethodName, EScriptNameKind::ScriptMethod);
		}
		return ScriptMethodNames;
	}
	return GetDeprecatedFunctionScriptNames(InFunc);
}

FString ScriptNameMapper::MapScriptConstantName(const UFunction* InFunc) const
{
	FString ScriptConstantName;
	if (!GetFieldScriptNameFromMetaDataImpl(InFunc, ScriptConstantMetaDataKey, ScriptConstantName))
	{
		ScriptConstantName = GetFieldScriptNameImpl(InFunc, ScriptNameMetaDataKey);
	}
	return ScriptifyName(ScriptConstantName, EScriptNameKind::Constant);
}

TArray<FString> ScriptNameMapper::GetDeprecatedScriptConstantScriptNames(const UFunction* InFunc) const
{
	TArray<FString> ScriptConstantNames;
	if (!GetDeprecatedFieldScriptNamesFromMetaDataImpl(InFunc, ScriptConstantMetaDataKey, ScriptConstantNames))
	{
		ScriptConstantNames = GetDeprecatedFieldScriptNamesImpl(InFunc, ScriptNameMetaDataKey);
	}
	for (FString& ScriptConstantName : ScriptConstantNames)
	{
		ScriptConstantName = ScriptifyName(ScriptConstantName, EScriptNameKind::Constant);
	}
	return ScriptConstantNames;
}

FString ScriptNameMapper::MapPropertyName(const UProperty* InProp) const
{
	FString PropName = GetFieldScriptNameImpl(InProp, ScriptNameMetaDataKey);
	return ScriptifyName(PropName, EScriptNameKind::Property);
}

TArray<FString> ScriptNameMapper::GetDeprecatedPropertyScriptNames(const UProperty* InProp) const
{
	const UStruct* PropOwner = InProp->GetOwnerStruct();
	check(PropOwner);

	TArray<FString> PropNames = GetDeprecatedFieldScriptNamesImpl(InProp, ScriptNameMetaDataKey);
	for (auto PropNamesIt = PropNames.CreateIterator(); PropNamesIt; ++PropNamesIt)
	{
		FString& PropName = *PropNamesIt;

		// Remove any deprecated names that clash with an existing script exposed property
		const UProperty* DeprecatedProp = PropOwner->FindPropertyByName(*PropName);
		if (DeprecatedProp && ShouldExportProperty(DeprecatedProp))
		{
			PropNamesIt.RemoveCurrent();
			continue;
		}

		PropName = ScriptifyName(PropName, EScriptNameKind::Property);
	}

	return PropNames;
}

FString ScriptNameMapper::MapParameterName(const UProperty* InProp) const
{
	FString PropName = GetFieldScriptNameImpl(InProp, ScriptNameMetaDataKey);
	return ScriptifyName(PropName, EScriptNameKind::Parameter);
}
}