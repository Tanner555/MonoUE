// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "MonoScriptNameMapper.h"
#include "MonoScriptGeneratorLog.h"

using namespace ScriptGenUtil;

enum EPropertyUsage : uint8
{
	EPU_None = 0x00,

	EPU_Property = 0x01,
	EPU_Parameter = 0x02,
	EPU_ReturnValue = 0x04,
	EPU_ArrayInner = 0x08,
	EPU_StructProperty = 0x10,
	EPU_OverridableFunctionParameter = 0x20,
	EPU_OverridableFunctionReturnValue = 0x40,
	EPU_StaticArrayProperty = 0x80,

	EPU_Any = 0xFF,
};

class FMonoTextBuilder;
class FMonoPropertyHandler;
class FNullPropertyHandler;

class FSupportedPropertyTypes
{
public:
	FSupportedPropertyTypes(const MonoScriptNameMapper& InNameMapper, class FInclusionLists& CodeGenerator);

	const FMonoPropertyHandler& Find(const UProperty* Property) const;
	const FMonoPropertyHandler& Find(UFunction* Property) const;

	bool IsStructBlittable(const UScriptStruct& ScriptStruct) const;

	inline const MonoScriptNameMapper& GetScriptNameMapper() const { return NameMapper; }
private:
	const MonoScriptNameMapper& NameMapper;

	TUniquePtr<FNullPropertyHandler> NullHandler;

	// Map of UProperty subclass names to property handlers.
	// Even if a property's class has handlers registered, it may still go unhandled if none of the
	// available handlers returns true from CanHandleProperty.
	TMap<FName, TArray<FMonoPropertyHandler*>> HandlerMap;

	void AddPropertyHandler(UClass* PropertyClass, FMonoPropertyHandler* Handler);
	void AddBlittablePropertyHandler(UClass* PropertyClass, const TCHAR* CSharpType);
	void AddBlittableCustomStructPropertyHandler(const TCHAR* UnrealName, const TCHAR* CSharpName, FInclusionLists& Blacklist);
	void AddCustomStructPropertyHandler(const TCHAR* UnrealName, const TCHAR* CSharpName, FInclusionLists& Blacklist);

};

class FMonoPropertyHandler
{
public:
	FMonoPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers, EPropertyUsage InPropertyUsage)
	: PropertyHandlers(InPropertyHandlers)
	, SupportedPropertyUsage(InPropertyUsage)
	{

	}

	virtual ~FMonoPropertyHandler() {}

	static const TCHAR* GetPropertyProtection(const UProperty* Property);

	virtual bool CanHandleProperty(const UProperty* Property) const = 0;

	// Subclasses may override to specify any additional classes that must be exported to handle a property.
	virtual void AddReferences(const UProperty* Property, TSet<UStruct*>& References) const
	{

	}

	virtual FString GetCSharpType(const UProperty* Property) const = 0;
	virtual FString GetCSharpFixedSizeArrayType(const UProperty *Property) const;

	inline bool IsSupportedAsProperty() const { return !!(SupportedPropertyUsage & EPropertyUsage::EPU_Property); }
	inline bool IsSupportedAsParameter() const { return !!(SupportedPropertyUsage & EPropertyUsage::EPU_Parameter); }
	inline bool IsSupportedAsReturnValue() const { return !!(SupportedPropertyUsage & EPropertyUsage::EPU_ReturnValue); }
	inline bool IsSupportedAsArrayInner() const { return !!(SupportedPropertyUsage & EPropertyUsage::EPU_ArrayInner); }
	inline bool IsSupportedAsStructProperty() const { return !!(SupportedPropertyUsage & EPropertyUsage::EPU_StructProperty); }
	inline bool IsSupportedAsOverridableFunctionParameter() const { return !!(SupportedPropertyUsage & EPropertyUsage::EPU_OverridableFunctionParameter); }
	inline bool IsSupportedAsOverridableFunctionReturnValue() const { return !!(SupportedPropertyUsage & EPropertyUsage::EPU_OverridableFunctionReturnValue); }
	inline bool IsSupportedInStaticArray() const { return !!(SupportedPropertyUsage & EPropertyUsage::EPU_StaticArrayProperty); }

	virtual bool IsBlittable() const { return false; }

	// Exports a C# property which wraps a native UProperty, suitable for use in a reference type backed by a UObject.
	void ExportWrapperProperty(FMonoTextBuilder& Builder, const UProperty* Property, bool IsGreylisted, bool IsWhitelisted) const;
	virtual void ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const;
	virtual void ExportParameterStaticConstruction(FMonoTextBuilder& Builder, const FString& NativeMethodName, const UProperty* Parameter) const;
	// helpers for collapsed getter/setters
	void BeginWrapperPropertyAccessorBlock(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName, const UField* DocCommentField) const;
	void EndWrapperPropertyAccessorBlock(FMonoTextBuilder& Builder, const UProperty* Property) const;

	// Exports a C# property which mirrors a UProperty, suitable for use in a value type.
	void ExportMirrorProperty(FMonoTextBuilder& Builder, const UProperty* Property, bool IsGreylisted, bool bSuppressOffsets) const;

	enum class FunctionType : uint8
	{
		Normal,
		BlueprintEvent,
		ExtensionOnAnotherClass
	};
	void ExportFunction(FMonoTextBuilder& Builder, UFunction* Function, FunctionType FuncType) const;
	void ExportOverridableFunction(FMonoTextBuilder& Builder, UFunction* Function) const;
	void ExportExtensionMethod(FMonoTextBuilder& Builder, UFunction& Function, const UProperty* SelfParameter, const UClass* OverrideClassBeingExtended) const;

	virtual void ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const;
	virtual void ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const;
	virtual void ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const;

	// Subclasses must override to export the C# property's get accessor, if property usage is supported.
	virtual void ExportPropertyGetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const;
	
	struct FunctionOverload
	{
		FString ParamsStringAPIWithDefaults;
		FString ParamsStringCall;
		FString CSharpParamName;
		FString CppDefaultValue;
		const FMonoPropertyHandler* ParamHandler;
		UProperty* ParamProperty;
	};

	enum class ProtectionMode : uint8
	{
		UseUFunctionProtection,
		OverrideWithInternal,
		OverrideWithProtected,
	};

	enum class OverloadMode : uint8
	{
		AllowOverloads,
		SuppressOverloads,
	};

	enum class BlueprintVisibility : uint8
	{
		Call,
		Event,
	};

	class FunctionExporter
	{
	public:
		FunctionExporter(const FMonoPropertyHandler& InHandler, UFunction& InFunction, ProtectionMode InProtectionMode = ProtectionMode::UseUFunctionProtection, OverloadMode InOverloadMode = OverloadMode::AllowOverloads, BlueprintVisibility InBlueprintVisibility = BlueprintVisibility::Call);
		FunctionExporter(const FMonoPropertyHandler& InHandler, UFunction& InFunction, const UProperty* InSelfParameter, const UClass* InOverrideClassBeingExtended);

		void ExportFunctionVariables(FMonoTextBuilder& Builder) const;

		void ExportOverloads(FMonoTextBuilder& Builder) const;

		void ExportFunction(FMonoTextBuilder& Builder) const;

		void ExportGetter(FMonoTextBuilder& Builder) const;
		void ExportSetter(FMonoTextBuilder& Builder) const;

		void ExportExtensionMethod(FMonoTextBuilder& Builder) const;

		inline const MonoScriptNameMapper& GetScriptNameMapper() const { return Handler.GetScriptNameMapper(); }

	private:
		void Initialize(ProtectionMode InProtectionMode, OverloadMode InOverloadMode, BlueprintVisibility InBlueprintVisibility);

		enum class InvokeMode : uint8
		{
			Normal,
			Getter,
			Setter
		};
		void ExportInvoke(FMonoTextBuilder& Builder, InvokeMode Mode) const;

		void ExportDeprecation(FMonoTextBuilder& Builder) const;

		const FMonoPropertyHandler& Handler;
		UFunction& Function;
		const UClass* OverrideClassBeingExtended;
		const UProperty* SelfParameter;
		UProperty* ReturnProperty;
		FString CSharpMethodName;
		FString Modifiers;
		bool bProtected;
		bool bBlueprintEvent;
		FString PinvokeFunction;
		FString PinvokeFirstArg;
		FString ParamsStringCall;
		FString ParamsStringAPIWithDefaults;
		TArray<FunctionOverload> Overloads;
	};

	virtual FString ExportInstanceMarshalerVariables(const UProperty *Property, const FString &PropertyName) const { return TEXT(""); }
	virtual FString ExportMarshalerDelegates(const UProperty *Property, const FString &PropertyName) const;

	inline const MonoScriptNameMapper& GetScriptNameMapper() const { return PropertyHandlers.GetScriptNameMapper(); }

protected:
	// Export the variables backing the C# property accessor for a UProperty.
	// By default, this is just the UProperty's offset within the UObject, but subclasses may override
	// to export different or additional fields.
	virtual void ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const;

	// Export the variables backing a UProperty used as a function parameter.
	virtual void ExportParameterVariables(FMonoTextBuilder& Builder, UFunction* Function, const FString& BackingFunctionName, UProperty* ParamProperty, const FString& BackingPropertyName) const;

	// Subclasses may override to suppress generation of a property setter in cases where none is required.
	virtual bool IsSetterRequired() const { return true; }

	// Subclasses must override to export the C# property's set accessor, if property usage is supported and IsSetterRequired can return true.
	virtual void ExportPropertySetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const;

	virtual void ExportFunctionReturnStatement(FMonoTextBuilder& Builder, const UFunction* Function, const UProperty* ReturnProperty, const FString& FunctionName, const FString& ParamsCallString) const;

	virtual FString GetNullReturnCSharpValue(const UProperty* ReturnProperty) const = 0;

	// Subclasses may override to suppress the generation of default parameters, which may be necessary due to C#'s 
	// requirement that default values be compile-time const, and limitations on what types may be declared const.
	// When necessary, non-exportable default parameters will be approximated by generating overloaded methods.
	virtual bool CanExportDefaultParameter() const { return true; }

	virtual FString ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const;

	// Export C# code to declare and initialize a variable approximating a default parameter.
	// Subclasses must override when CanExportDefaultParameter() can return false.
	virtual void ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const;

	FSupportedPropertyTypes& PropertyHandlers;

private:
	// Returns the default value for a parameter property, or an empty string if no default is defined.
	FString GetCppDefaultParameterValue(UFunction* Function, UProperty* ParamProperty) const;

	EPropertyUsage SupportedPropertyUsage;
};

class FSimpleTypePropertyHandler : public FMonoPropertyHandler
{
public:
	FSimpleTypePropertyHandler(FSupportedPropertyTypes& InPropertyHandlers, UClass* InPropertyClass, const TCHAR* InCSharpType, const TCHAR* InMarshalerType, EPropertyUsage InPropertyUsage = EPropertyUsage::EPU_Any)
		: FMonoPropertyHandler(InPropertyHandlers, InPropertyUsage)
		, PropertyClass(InPropertyClass)
		, CSharpType(InCSharpType)
		, MarshalerType(InMarshalerType)
	{

	}

	virtual bool CanHandleProperty(const UProperty* Property) const override;
	virtual FString GetCSharpType(const UProperty* Property) const override;

	virtual FString ExportMarshalerDelegates(const UProperty *Property, const FString &PropertyName) const;
protected:
	// Alternate ctor for subclasses overriding GetMarshalerType()
	FSimpleTypePropertyHandler(FSupportedPropertyTypes& InPropertyHandlers, UClass* InPropertyClass, const TCHAR* InCSharpType, EPropertyUsage InPropertyUsage = EPropertyUsage::EPU_Any)
		: FMonoPropertyHandler(InPropertyHandlers, InPropertyUsage)
		, PropertyClass(InPropertyClass)
		, CSharpType(InCSharpType)
	{

	}
	// Alternate ctor for subclasses overriding GetCSharpType() and GetMarshalerType()
	FSimpleTypePropertyHandler(FSupportedPropertyTypes& InPropertyHandlers, UClass* InPropertyClass, EPropertyUsage InPropertyUsage = EPropertyUsage::EPU_Any)
		: FMonoPropertyHandler(InPropertyHandlers, InPropertyUsage)
		, PropertyClass(InPropertyClass)
	{

	}

	virtual FString GetMarshalerType(const UProperty *Property) const;

	virtual FString GetNullReturnCSharpValue(const UProperty* ReturnProperty) const override;
	virtual FString ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const override;

	virtual void ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const override;
	virtual void ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const override final;
	virtual void ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const override;

private:
	UClass *PropertyClass;
	FString CSharpType;
	FString MarshalerType;
};

class FBlittableTypePropertyHandler : public FSimpleTypePropertyHandler
{
public:
	FBlittableTypePropertyHandler(FSupportedPropertyTypes& InPropertyHandlers, UClass* InPropertyClass, const TCHAR* InCSharpType, EPropertyUsage InPropertyUsage = EPropertyUsage::EPU_Any)
		: FSimpleTypePropertyHandler(InPropertyHandlers, InPropertyClass, InCSharpType, InPropertyUsage)
	{

	}

	virtual bool IsBlittable() const override { return true; }
protected:

	virtual FString GetMarshalerType(const UProperty *Property) const override;
};

class FFloatPropertyHandler : public FBlittableTypePropertyHandler
{
public:
	explicit FFloatPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
		: FBlittableTypePropertyHandler(InPropertyHandlers, UFloatProperty::StaticClass(), TEXT("float"))
	{

	}

protected:

	virtual FString ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const override;
};

class FEnumPropertyHandler : public FBlittableTypePropertyHandler
{
public:
	explicit FEnumPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
		: FBlittableTypePropertyHandler(InPropertyHandlers, UByteProperty::StaticClass(), TEXT(""), EPU_Any)
	{}

	virtual bool CanHandleProperty(const UProperty* Property) const override;
	virtual FString GetCSharpType(const UProperty* Property) const override;
	virtual FString ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const override;

	static void AddStrippedPrefix(const UEnum* Enum, const FString& Prefix)
	{
		check(!StrippedPrefixes.Contains(Enum->GetFName()));
		StrippedPrefixes.Add(Enum->GetFName(), Prefix);
	}
protected:
	virtual FString GetMarshalerType(const UProperty *Property) const override;
private:
	static TMap<FName, FString> StrippedPrefixes;
};

class FNamePropertyHandler : public FBlittableTypePropertyHandler
{
public:
	explicit FNamePropertyHandler(FSupportedPropertyTypes& InPropertyHandlers) : FBlittableTypePropertyHandler(InPropertyHandlers, UNameProperty::StaticClass(), TEXT("Name"))
	{}

protected:
	virtual FString GetNullReturnCSharpValue(const UProperty* ReturnProperty) const override;

	virtual bool CanExportDefaultParameter() const override { return false; }

	virtual void ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const override;
};

class FTextPropertyHandler : public FMonoPropertyHandler
{
public:
	explicit FTextPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers) 
		: FMonoPropertyHandler(InPropertyHandlers, static_cast<EPropertyUsage>(EPU_Property | EPU_StaticArrayProperty))
	{}
	virtual bool CanHandleProperty(const UProperty* Property) const override;
	virtual FString GetCSharpType(const UProperty* Property) const override;

	virtual void ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const override;
	
	virtual FString ExportInstanceMarshalerVariables(const UProperty *Property, const FString &PropertyName) const;
	virtual FString ExportMarshalerDelegates(const UProperty *Property, const FString &PropertyName) const;
protected:
	virtual void ExportPropertyGetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const override;
	virtual bool IsSetterRequired() const override { return false; }
	virtual void ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const override;

	virtual FString GetNullReturnCSharpValue(const UProperty* ReturnProperty) const override;

	//TODO, for params
	//virtual void ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const override;
	//virtual void ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const override;
	//virtual void ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const override;

};

class FWeakObjectPropertyHandler : public FSimpleTypePropertyHandler
{
public:
	explicit FWeakObjectPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
		: FSimpleTypePropertyHandler(InPropertyHandlers, UWeakObjectProperty::StaticClass(), static_cast<EPropertyUsage>(EPU_Property | EPU_StructProperty | EPU_StaticArrayProperty))
	{

	}

	virtual FString GetCSharpType(const UProperty* Property) const override;

protected:
	virtual bool CanExportDefaultParameter() const override { return false; }

	virtual FString GetMarshalerType(const UProperty *Property) const override;
};

class FBitfieldPropertyHandler : public FMonoPropertyHandler
{
public:
	explicit FBitfieldPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
		: FMonoPropertyHandler(InPropertyHandlers, static_cast<EPropertyUsage>(EPU_Any & (~EPU_StaticArrayProperty)))
	{

	}

	virtual bool CanHandleProperty(const UProperty* Property) const override;
	virtual FString GetCSharpType(const UProperty* Property) const override;

	virtual void ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const override;

protected:
	virtual void ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const override;

	virtual FString GetNullReturnCSharpValue(const UProperty* ReturnProperty) const override;

	virtual void ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const override;
	virtual void ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const override;
	virtual void ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const override;
};

class FBoolPropertyHandler : public FSimpleTypePropertyHandler
{
public:
	explicit FBoolPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
		: FSimpleTypePropertyHandler(InPropertyHandlers, UBoolProperty::StaticClass(), TEXT("bool"), TEXT("BoolMarshaler"), EPU_Any)
	{

	}

protected:
};

class FStringPropertyHandler : public FMonoPropertyHandler
{
public:
	explicit FStringPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
	: FMonoPropertyHandler(InPropertyHandlers, static_cast<EPropertyUsage>(EPU_Property | EPU_Parameter | EPU_ReturnValue | EPU_OverridableFunctionParameter | EPU_OverridableFunctionReturnValue | EPU_StaticArrayProperty))
	{

	}

	virtual bool CanHandleProperty(const UProperty* Property) const override;
	virtual FString GetCSharpType(const UProperty* Property) const override;
	virtual void ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const override;
	virtual FString ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const override;

	virtual FString ExportMarshalerDelegates(const UProperty *Property, const FString &PropertyName) const;
protected:
	virtual void ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const override;
	virtual void ExportPropertySetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const override;
	virtual void ExportPropertyGetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const override;
	virtual void ExportFunctionReturnStatement(FMonoTextBuilder& Builder, const UFunction* Function, const UProperty* ReturnProperty, const FString& FunctionName, const FString& ParamsCallString) const override;
	virtual FString GetNullReturnCSharpValue(const UProperty* ReturnProperty) const override;

	virtual void ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const override;
	virtual void ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const override;
	virtual void ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const override;

	
};

class FObjectPropertyHandler : public FSimpleTypePropertyHandler
{
public:
	explicit FObjectPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
		: FSimpleTypePropertyHandler(InPropertyHandlers, UObjectProperty::StaticClass(), EPU_Any)
	{

	}

	virtual void AddReferences(const UProperty* Property, TSet<UStruct*>& References) const override;

	virtual FString GetCSharpType(const UProperty* Property) const override;
protected:
	virtual FString GetMarshalerType(const UProperty *Property) const override;
};

// UClassProperty is a subclass of UObjectProperty, but we don't have and likely don't need a direct managed representation
// of UClass, so we use a custom handler to map things to System.Type.
class FClassPropertyHandler : public FSimpleTypePropertyHandler
{
public:
	explicit FClassPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
		: FSimpleTypePropertyHandler(InPropertyHandlers, UClassProperty::StaticClass(), EPU_Any)
	{

	}
	virtual void AddReferences(const UProperty* Property, TSet<UStruct*>& References) const override;

	virtual FString GetCSharpType(const UProperty* Property) const override;
protected:
	virtual FString GetMarshalerType(const UProperty *Property) const override;
};

class FArrayPropertyHandler : public FMonoPropertyHandler
{
public:
	explicit FArrayPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
	: FMonoPropertyHandler(InPropertyHandlers, static_cast<EPropertyUsage>(EPU_Property | EPU_Parameter | EPU_ReturnValue | EPU_OverridableFunctionParameter | EPU_OverridableFunctionReturnValue | EPU_StaticArrayProperty))
	{

	}

	virtual bool CanHandleProperty(const UProperty* Property) const override;
	virtual void AddReferences(const UProperty* Property, TSet<UStruct*>& References) const override;

	virtual FString GetCSharpType(const UProperty* Property) const override;
	virtual void ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const override;
	virtual void ExportParameterStaticConstruction(FMonoTextBuilder& Builder, const FString& CSharpMethodName, const UProperty* Parameter) const override;

	virtual FString ExportInstanceMarshalerVariables(const UProperty *Property, const FString &PropertyName) const override;
	virtual FString ExportMarshalerDelegates(const UProperty *Property, const FString &PropertyName) const override;
protected:
	virtual void ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const override;
	virtual void ExportParameterVariables(FMonoTextBuilder& Builder, UFunction* Function, const FString& CSharpMethodName, UProperty* ParamProperty, const FString& CSharpPropertyName) const override;
	virtual void ExportPropertyGetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& PropertyName) const override;
	virtual void ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const override;
	virtual void ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const override;
	virtual void ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& PropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const override;

	// Array properties don't need a setter - all modifications should occur through the IList interface of the wrapper class.
	virtual bool IsSetterRequired() const override { return false; }

	virtual FString GetNullReturnCSharpValue(const UProperty* ReturnProperty) const override;

	FString GetWrapperInterface(const UProperty* Property) const;
	FString GetWrapperType(const UProperty* Property) const;


};

class FBlittableCustomStructTypePropertyHandler : public FBlittableTypePropertyHandler
{
public:
	FBlittableCustomStructTypePropertyHandler(FSupportedPropertyTypes& InPropertyHandlers, const TCHAR* InUnrealStructName, const TCHAR* CSharpStructName)
	: FBlittableTypePropertyHandler(InPropertyHandlers, UStructProperty::StaticClass(), CSharpStructName, EPU_Any)
	, UnrealStructName(InUnrealStructName)
	{
	}

	virtual bool CanHandleProperty(const UProperty* Property) const override;

protected:
	virtual bool CanExportDefaultParameter() const override { return false; }
	virtual void ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const override;
private:
	FName UnrealStructName;
};

class FBlittableStructPropertyHandler : public FBlittableTypePropertyHandler
{
public:
	explicit FBlittableStructPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
		: FBlittableTypePropertyHandler(InPropertyHandlers, UStructProperty::StaticClass(), TEXT(""))
	{

	}

	static bool IsStructBlittable(const FSupportedPropertyTypes& PropertyHandlers, const UScriptStruct& ScriptStruct);

	virtual bool CanHandleProperty(const UProperty* Property) const override;
	virtual FString GetCSharpType(const UProperty* Property) const override;

	virtual void AddReferences(const UProperty* Property, TSet<UStruct*>& References) const override;

protected:
	virtual bool CanExportDefaultParameter() const override { return false; }
	virtual void ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const override;
};

class FStructPropertyHandler : public FSimpleTypePropertyHandler
{
public:
	explicit FStructPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers)
		: FSimpleTypePropertyHandler(InPropertyHandlers, UStructProperty::StaticClass(), EPU_Any)
	{
	}

	virtual FString GetCSharpType(const UProperty* Property) const override;

	virtual void AddReferences(const UProperty* Property, TSet<UStruct*>& References) const override;
protected:
	virtual FString GetMarshalerType(const UProperty *Property) const;

	virtual bool CanExportDefaultParameter() const override { return false; }
	virtual void ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const override;
};

class FCustomStructTypePropertyHandler : public FSimpleTypePropertyHandler
{
public:
	FCustomStructTypePropertyHandler(FSupportedPropertyTypes& InPropertyHandlers, const TCHAR* InUnrealStructName, const TCHAR* InCSharpStructName)
		: FSimpleTypePropertyHandler(InPropertyHandlers, UStructProperty::StaticClass(), InCSharpStructName)
		, UnrealStructName(InUnrealStructName)
	{

	}

	virtual bool CanHandleProperty(const UProperty* Property) const override;

	virtual void AddReferences(const UProperty* Property, TSet<UStruct*>& References) const override;
protected:
	virtual FString GetMarshalerType(const UProperty *Property) const override;

	virtual bool CanExportDefaultParameter() const override { return false; }
	virtual void ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const override;
private:
	FName UnrealStructName;
};

class FNullPropertyHandler : public FMonoPropertyHandler
{
public:
	explicit FNullPropertyHandler(FSupportedPropertyTypes& InPropertyHandlers) : FMonoPropertyHandler(InPropertyHandlers, EPU_None) { }

	virtual bool CanHandleProperty(const UProperty* Property) const override;
	virtual FString GetCSharpType(const UProperty* Property) const override;

protected:
	virtual FString GetNullReturnCSharpValue(const UProperty* ReturnProperty) const override;

};