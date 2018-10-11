// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

// TODO: share with MonoRuntime
#define MONO_UE4_NAMESPACE TEXT("UnrealEngine")
#define MONO_BINDINGS_NAMESPACE MONO_UE4_NAMESPACE TEXT(".Runtime")
#define MONO_RUNTIME_NAMESPACE MONO_UE4_NAMESPACE TEXT(".MonoRuntime")
#define MONO_ENGINE_NAMESPACE MONO_UE4_NAMESPACE TEXT(".Engine")
#define BUILTIN_MODULES_PROJECT_NAME TEXT("BuiltinModules")

// mirrored from EdGraphSchema_K2.cpp (we can't bring in Kismet into a program plugin)
extern const FName MD_IsBlueprintBase;
extern const FName MD_BlueprintFunctionLibrary;
extern const FName MD_AllowableBlueprintVariableType;
extern const FName MD_NotAllowableBlueprintVariableType;
extern const FName MD_BlueprintInternalUseOnly;
extern const FName MD_BlueprintSpawnableComponent;
extern const FName MD_FunctionCategory;
extern const FName MD_DefaultToSelf;
extern const FName MD_Latent;

class FMonoTextBuilder
{
public:
	enum class IndentType
	{
		Spaces,
		Tabs
	};
	explicit FMonoTextBuilder(IndentType InIndentMode)
	: UnsafeBlockCount(0)
	, IndentCount(0)
	, IndentMode(InIndentMode)
	{

	}

	void Indent()
	{
		++IndentCount;
	}

	void Unindent()
	{
		--IndentCount;
	}

	void AppendLine()
	{
		if (!Report.IsEmpty())
		{
			Report += LINE_TERMINATOR;
		}

		if (IndentMode == IndentType::Spaces)
		{
			for (int32 Index = 0; Index < IndentCount; Index++)
			{
				Report += TEXT("    ");
			}
		}
		else
		{
			for (int32 Index = 0; Index < IndentCount; Index++)
			{
				Report += TEXT("\t");
			}
		}
	}

	void AppendLine(const FText& Text)
	{
		AppendLine();
		Report += Text.ToString();
	}

	void AppendLine(const FString& String)
	{
		AppendLine();
		Report += String;
	}

	void AppendLine(const FName& Name)
	{
		AppendLine();
		Report += Name.ToString();
	}

	void AppendLine(const TCHAR* Line)
	{
		AppendLine(FString(Line));
	}

	void OpenBrace()
	{
		AppendLine(TEXT("{"));
		Indent();
	}

	void CloseBrace()
	{
		Unindent();
		AppendLine(TEXT("}"));
	}

	void BeginUnsafeBlock()
	{
		if (!UnsafeBlockCount++)
		{
			AppendLine(TEXT("unsafe"));
			OpenBrace();
		}
	}

	void EndUnsafeBlock()
	{
		check(UnsafeBlockCount >= 0);
		if (!--UnsafeBlockCount)
		{
			CloseBrace();
		}
	}

	void AppendUnsafeLine(const FString& Line)
	{
		if (!UnsafeBlockCount)
		{
			AppendLine(FString::Printf(TEXT("unsafe { %s }"), *Line));
		}
		else
		{
			AppendLine(Line);
		}
	}

	void AppendUnsafeLine(const TCHAR* Line)
	{
		AppendUnsafeLine(FString(Line));
	}

	void Clear()
	{
		Report.Empty();
	}

	FText ToText() const
	{
		return FText::FromString(Report);
	}

	void AppendDocCommentFromMetadata(const UField& InField);
	void AppendDocCommentSummary(const FString& SummaryText);

private:

	FString Report;
	int32 UnsafeBlockCount;
	int32 IndentCount;
	IndentType IndentMode;
};

class FMonoCSharpPropertyBuilder
{
public:
	FMonoCSharpPropertyBuilder()
	{
		String = FString::Printf(TEXT("["));
		State = AttributeState::Open;
	}

	void AddAttribute(const FString& AttributeName)
	{
		switch (State)
		{
		case AttributeState::Open:
			break;
		case AttributeState::InAttribute:
			String += TEXT(", ");
			break;
		case AttributeState::InAttributeParams:
			String += TEXT("), ");
			break;
		default:
			checkNoEntry();
			break;
		}
		String += AttributeName;
		State = AttributeState::InAttribute;
	}

	void AddArgument(const FString& Arg)
	{
		switch (State)
		{
		case AttributeState::InAttribute:
			String += TEXT("(");
			break;
		case AttributeState::InAttributeParams:
			String += TEXT(", ");
			break;
		default:
			checkNoEntry();
			break;
		}
		String += Arg;
		State = AttributeState::InAttributeParams;
	}

	void AddMetaData(const UObject& InObject)
	{
		TMap<FName, FString>* MetaDataMap = UMetaData::GetMapForObject(&InObject);

		if (nullptr != MetaDataMap)
		{
			for (TMap<FName, FString>::TIterator It(*MetaDataMap); It; ++It)
			{
				AddAttribute(TEXT("UMetaData"));
				AddArgument(FString::Printf(TEXT("\"%s\""),*It.Key().ToString()));
				if (It.Value().Len() > 0)
				{
					FString Value = It.Value();
					// ReplaceCharWithEscapedChar doesn't seem to do what we want, it'll replace "\r" with "\\\\r"
					Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
					Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
					Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
					Value.ReplaceInline(TEXT("\t"), TEXT("\\t"));
					Value.ReplaceInline(TEXT("\""), TEXT("\\\""));

					AddArgument(FString::Printf(TEXT("\"%s\""), *Value));
			}
			}
		}
	}

	void Finish()
	{
		switch (State)
		{
		case AttributeState::InAttribute:
			String += TEXT("]");
			break;
		case AttributeState::InAttributeParams:
			String += TEXT(")]");
			break;
		default:
			checkNoEntry();
			break;
		}

		State = AttributeState::Closed;
	}

	const FString& ToString() const
	{
		check(State == AttributeState::Closed);
		return String;
	}

private:
	FString String;
	enum class AttributeState : uint8
	{
		Open,
		Closed,
		InAttribute,
		InAttributeParams
	};
	AttributeState State;
};

// Helper methods, used by both the property handlers and the script code generator
namespace MonoScriptCodeGeneratorUtils
{
	inline FName GetModuleFName(const UObject& Obj)
	{
		return FPackageName::GetShortFName(Obj.GetOutermost()->GetFName());
	}

	inline FString GetModuleName(const UObject& Obj)
	{
		return GetModuleFName(Obj).ToString();
	}

	void InitializeToolTipLocalization();
	FString GetEnumValueMetaData(const UEnum& InEnum, const TCHAR* MetadataKey, int32 ValueIndex);
	FString GetEnumValueToolTip(const UEnum& InEnum, int32 ValueIndex);
	FString GetFieldToolTip(const UField& InField);

	UProperty* GetFirstParam(UFunction* Function);

	enum class BoolHierarchicalMetaDataMode : uint8
	{
		// any value stops the hierarchical search
		SearchStopAtAnyValue,
		// search stops when it encounters first true value, ignores false ones
		SearchStopAtTrueValue
	};
	bool GetBoolMetaDataHeirarchical(const UClass* TestClass, FName KeyName, BoolHierarchicalMetaDataMode Mode);

	bool IsBlueprintFunctionLibrary(const UClass* InClass);

	bool ParseGuidFromProjectFile(FGuid& ResultGuid, const FString& ProjectPath);
}

