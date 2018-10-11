// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoPropertyHandler.h"
#include "MonoScriptCodeGenerator.h"
#include "MonoScriptCodeGeneratorUtils.h"
#include "UObject/UObjectIterator.h"
#include "UObject/TextProperty.h"
#include "UObject/EnumProperty.h"
#include "HAL/PlatformFilemanager.h"

static const FName MD_DeprecatedFunction(TEXT("DeprecatedFunction"));
static const FName MD_DeprecationMessage(TEXT("DeprecationMessage"));
static const FName MD_BlueprintProtected(TEXT("BlueprintProtected"));

//////////////////////////////////////////////////////////////////////////
// FMonoPropertyHandler
//////////////////////////////////////////////////////////////////////////
const TCHAR* FMonoPropertyHandler::GetPropertyProtection(const UProperty* Property)
{
	const TCHAR* Protection = TEXT("");

	//properties can be RF_Public and CPF_Protected, the first takes precedence
	if (Property->HasAnyFlags(RF_Public))
	{
		Protection = TEXT("public ");
	}
	else if (Property->HasAnyPropertyFlags(CPF_Protected) || Property->HasMetaData(MD_BlueprintProtected))
	{
		Protection = TEXT("protected ");
	}
	else //it must be MD_AllowPrivateAccess
	{
		Protection = TEXT("public ");
	}

	return Protection;
}

FString FMonoPropertyHandler::GetCSharpFixedSizeArrayType(const UProperty *Property) const
{
	FString ArrayType;
	if (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
	{
		ArrayType = TEXT("FixedSizeArrayReadOnly");
	}
	else
	{
		ArrayType = TEXT("FixedSizeArrayReadWrite");
	}
	return FString::Printf(TEXT("%s<%s>"), *ArrayType, *GetCSharpType(Property));
}

void FMonoPropertyHandler::ExportWrapperProperty(FMonoTextBuilder& Builder, const UProperty* Property, bool IsGreylisted, bool IsWhitelisted) const
{
	FString CSharpPropertyName = GetScriptNameMapper().MapPropertyName(Property);
	FString NativePropertyName = Property->GetName();

	Builder.AppendLine(FString::Printf(TEXT("// %s"), *Property->GetFullName()));
	ExportPropertyVariables(Builder, Property, NativePropertyName);

	if (!IsGreylisted)
	{
		BeginWrapperPropertyAccessorBlock(Builder, Property, CSharpPropertyName, Property);
		if (Property->ArrayDim == 1)
		{
            Builder.AppendLine(TEXT("get"));
            Builder.OpenBrace();

            ExportPropertyGetter(Builder, Property, NativePropertyName);
            Builder.CloseBrace(); // get

            if (IsSetterRequired() && (IsWhitelisted || !Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly)))
            {
                Builder.AppendLine(TEXT("set"));
                Builder.OpenBrace();
                // TODO: see if blueprint makes any callback on objects when it changes a property

                ExportPropertySetter(Builder, Property, NativePropertyName);

                Builder.CloseBrace(); // set
            }
		}
		else
		{
			Builder.AppendLine(TEXT("get"));
			Builder.OpenBrace();
			Builder.AppendLine(FString::Printf(TEXT("if (%s_Wrapper == null)"), *NativePropertyName));
			Builder.OpenBrace();
			Builder.AppendLine(ExportInstanceMarshalerVariables(Property, NativePropertyName));
			Builder.AppendLine(FString::Printf(TEXT("%s_Wrapper = new %s (this, %s_Offset, %s_Length, %s);"), *NativePropertyName, *GetCSharpFixedSizeArrayType(Property), *NativePropertyName, *NativePropertyName, *ExportMarshalerDelegates(Property, NativePropertyName)));
			Builder.CloseBrace();
			Builder.AppendLine(FString::Printf(TEXT("return %s_Wrapper;"), *NativePropertyName));
			Builder.CloseBrace();
		}


		EndWrapperPropertyAccessorBlock(Builder, Property);
	}

	Builder.AppendLine();
}

void FMonoPropertyHandler::BeginWrapperPropertyAccessorBlock(FMonoTextBuilder& Builder, const UProperty* Property, const FString& CSharpPropertyName, const UField* DocCommentField) const
{
	check(Property);
	const TCHAR* Protection = GetPropertyProtection(Property);

	Builder.AppendLine();
	if (nullptr != DocCommentField)
	{
		Builder.AppendDocCommentFromMetadata(*DocCommentField);
	}
	FString PropertyType = (Property->ArrayDim == 1 ? GetCSharpType(Property) : GetCSharpFixedSizeArrayType(Property));

	Builder.AppendLine(FString::Printf(TEXT("%s%s %s"), Protection, *PropertyType, *CSharpPropertyName));
	Builder.OpenBrace();
}

void FMonoPropertyHandler::EndWrapperPropertyAccessorBlock(FMonoTextBuilder& Builder, const UProperty* Property) const
{
	Builder.CloseBrace();
}

void FMonoPropertyHandler::ExportMirrorProperty(FMonoTextBuilder& Builder, const UProperty* Property, bool IsGreylisted, bool bSuppressOffsets) const
{
	FString CSharpPropertyName = GetScriptNameMapper().MapPropertyName(Property);
	FString NativePropertyName = Property->GetName();

	Builder.AppendLine(FString::Printf(TEXT("// %s"), *Property->GetFullName()));

	if (!bSuppressOffsets)
	{
		ExportPropertyVariables(Builder, Property, NativePropertyName);
	}

	if (!IsGreylisted)
	{
		const TCHAR* Protection = GetPropertyProtection(Property);
		Builder.AppendDocCommentFromMetadata(*Property);
		if (IsSetterRequired())
		{
			Builder.AppendLine(FString::Printf(TEXT("%s%s %s;"), Protection, *GetCSharpType(Property), *CSharpPropertyName));
		}
		else
		{
			// Use an auto-property with a private setter.
			Builder.AppendLine(FString::Printf(TEXT("%s%s %s { get; private set; }"), Protection, *GetCSharpType(Property), *CSharpPropertyName));
		}
	}

	Builder.AppendLine();
}

void FMonoPropertyHandler::ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	Builder.AppendLine(FString::Printf(TEXT("%s_Offset = UnrealInterop.GetPropertyOffsetFromName(NativeClassPtr, \"%s\");"),
		*NativePropertyName,
		*NativePropertyName));

	if (Property->ArrayDim > 1)
	{
		check(IsSupportedInStaticArray());
		Builder.AppendLine(FString::Printf(TEXT("%s_Length = UnrealInterop.GetPropertyArrayDimFromName(NativeClassPtr, \"%s\");"), *NativePropertyName, *NativePropertyName));
	}
}

void FMonoPropertyHandler::ExportParameterStaticConstruction(FMonoTextBuilder& Builder, const FString& NativeMethodName, const UProperty* Parameter) const
{
	const FString ParamName = Parameter->GetName();
	Builder.AppendLine(FString::Printf(TEXT("%s_%s_Offset = UnrealInterop.GetPropertyOffsetFromName(%s_NativeFunction, \"%s\");"),
		*NativeMethodName,
		*ParamName,
		*NativeMethodName,
		*ParamName));
}

FMonoPropertyHandler::FunctionExporter::FunctionExporter(const FMonoPropertyHandler& InHandler, UFunction& InFunction, ProtectionMode InProtectionMode, OverloadMode InOverloadMode, BlueprintVisibility InBlueprintVisibility)
	: Handler(InHandler)
	, Function(InFunction)
	, OverrideClassBeingExtended(nullptr)
	, SelfParameter(nullptr)
{
	Initialize(InProtectionMode, InOverloadMode, InBlueprintVisibility);
}

FMonoPropertyHandler::FunctionExporter::FunctionExporter(const FMonoPropertyHandler& InHandler, UFunction& InFunction, const UProperty* InSelfParameter, const UClass* InOverrideClassBeingExtended)
	: Handler(InHandler)
	, Function(InFunction)
	, OverrideClassBeingExtended(InOverrideClassBeingExtended)
	, SelfParameter(InSelfParameter)
{
	Initialize(ProtectionMode::UseUFunctionProtection, OverloadMode::AllowOverloads, BlueprintVisibility::Call);
}

void FMonoPropertyHandler::FunctionExporter::Initialize(ProtectionMode InProtectionMode, OverloadMode InOverloadMode, BlueprintVisibility InBlueprintVisibility)
{
	ReturnProperty = Function.GetReturnProperty();
	CSharpMethodName = GetScriptNameMapper().MapScriptMethodName(&Function);

	check(Handler.CanHandleProperty(ReturnProperty));

	bProtected = false;

	switch (InProtectionMode)
	{
	case ProtectionMode::UseUFunctionProtection:
		if (Function.HasAnyFunctionFlags(FUNC_Public))
		{
			Modifiers = TEXT("public ");
		}
		else if (Function.HasAnyFunctionFlags(FUNC_Protected) || Function.HasMetaData (MD_BlueprintProtected))
		{
			Modifiers = TEXT("protected ");
			bProtected = true;
		}
		else
		{
			// there are a number of cases where BlueprintCallable funtions are private as they aren't intended to be used from c++
			// we need to make them available regardless
			Modifiers = TEXT("public ");
		}
		break;
	case ProtectionMode::OverrideWithInternal:
		Modifiers = TEXT("internal ");
		break;
	case ProtectionMode::OverrideWithProtected:
		Modifiers = TEXT("protected ");
		break;
	default:
		checkNoEntry();
		break;
	}

	bBlueprintEvent = (InBlueprintVisibility == BlueprintVisibility::Event);

	if (Function.HasAnyFunctionFlags(FUNC_Static))
	{
		Modifiers += TEXT("static ");
		PinvokeFunction = TEXT("InvokeStaticFunction");
		PinvokeFirstArg = TEXT("NativeClassPtr");
	}
	else
	{
		// extension methods should always be static!
		check(nullptr == SelfParameter);

		if (bBlueprintEvent)
		{
			Modifiers += TEXT("virtual ");
		}

		PinvokeFunction = TEXT("InvokeFunction");
		PinvokeFirstArg = TEXT("NativeObject");
	}

	FString ParamsStringAPI;

	bool bHasDefaultParameters = false;

	const MonoScriptNameMapper& Mapper = GetScriptNameMapper();

	// if we have a self parameter and we're exporting as a class extension method, add it as the first type
	if (SelfParameter)
	{
		const FMonoPropertyHandler& ParamHandler = Handler.PropertyHandlers.Find(SelfParameter);
		FString ParamType = OverrideClassBeingExtended? Mapper.GetQualifiedName(*OverrideClassBeingExtended) : ParamHandler.GetCSharpType(SelfParameter);

		ParamsStringAPI = FString::Printf(TEXT("this %s %s, "), *ParamType, *Mapper.MapParameterName(SelfParameter));
		ParamsStringAPIWithDefaults = ParamsStringAPI;
	}

	int paramsProcessed = 0;
	FString ParamsStringCallNative;

	for (TFieldIterator<UProperty> ParamIt(&Function); ParamIt; ++ParamIt)
	{
		UProperty* ParamProperty = *ParamIt;
		const FMonoPropertyHandler& ParamHandler = Handler.PropertyHandlers.Find(ParamProperty);
		FString CSharpParamName = Mapper.MapParameterName(ParamProperty);
		if (!ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			FString RefQualifier;
			// Ignore const-by-reference params, which will have both CPF_ReferenceParm and CPF_OutParm, 
			// but shouldn't be treated as such in the bindings.  Technically, native code could cast away
			// constness and edit them anyway, but it shouldn't, so we'll enforce it ourselves by not
			// copying the native value back after the call.  
			// As an added benefit, this gives us a cleaner API by requiring fewer "ref" and "out" qualifiers.
			if (!ParamProperty->HasAnyPropertyFlags(CPF_ConstParm))
			{
				if (ParamProperty->HasAnyPropertyFlags(CPF_ReferenceParm))
				{
					RefQualifier = TEXT("ref ");
				}
				else if (ParamProperty->HasAnyPropertyFlags(CPF_OutParm))
				{
					RefQualifier = TEXT("out ");
				}
			}

			const bool bSelfParameterForExtensionMethod = SelfParameter == ParamProperty;

			if (bSelfParameterForExtensionMethod)
			{
				FString SelfParamName = Mapper.MapParameterName(SelfParameter);
				if (ParamsStringCall.IsEmpty())
				{
					ParamsStringCall += SelfParamName;
				}
				else
				{
					ParamsStringCall = FString::Printf(TEXT("%s, "), *SelfParamName) + ParamsStringCall.Left(ParamsStringCall.Len() - 2);
				}
				ParamsStringCallNative += SelfParamName;
			}
			else
			{

				ParamsStringCall += FString::Printf(TEXT("%s%s"), *RefQualifier, *CSharpParamName);
				ParamsStringCallNative += FString::Printf(TEXT("%s%s"), *RefQualifier, *CSharpParamName);

				ParamsStringAPI += FString::Printf(TEXT("%s%s %s"),
					*RefQualifier,
					*ParamHandler.GetCSharpType(ParamProperty),
					*CSharpParamName);

				const FString CppDefaultValue = ParamHandler.GetCppDefaultParameterValue(&Function, ParamProperty);
				if ((bHasDefaultParameters || CppDefaultValue.Len()) && InOverloadMode == OverloadMode::AllowOverloads)
				{
					bHasDefaultParameters = true;
					FString CSharpDefaultValue;
					if (!CppDefaultValue.Len() || CppDefaultValue == TEXT("None"))
					{
						// UHT doesn't bother storing default params for some properties when the value is equivalent to a default-constructed value.
						CSharpDefaultValue = ParamHandler.GetNullReturnCSharpValue(ParamProperty);

						//TODO: We can't currently detect the case where the first default parameter to a function has a default-constructed value.
						//		The metadata doesn't store so much as an empty string in that case, so an explicit HasMetaData check won't tell us anything.
					}
					else if (ParamHandler.CanExportDefaultParameter())
					{
						CSharpDefaultValue = ParamHandler.ConvertCppDefaultParameterToCSharp(CppDefaultValue, &Function, ParamProperty);
					}

					if (CSharpDefaultValue.Len())
					{
						ParamsStringAPIWithDefaults += FString::Printf(TEXT("%s%s %s%s"),
							*RefQualifier,
							*ParamHandler.GetCSharpType(ParamProperty),
							*CSharpParamName,
							*FString::Printf(TEXT(" = %s"), *CSharpDefaultValue));
					}
					else
					{
						// Approximate a default parameter by outputting multiple APIs to call this UFunction.

						// remove last comma
						if (ParamsStringAPIWithDefaults.Len() > 0)
						{
							ParamsStringAPIWithDefaults = ParamsStringAPIWithDefaults.Left(ParamsStringAPIWithDefaults.Len() - 2);
						}

						FunctionOverload Overload;
						Overload.CppDefaultValue = CppDefaultValue;
						Overload.CSharpParamName = CSharpParamName;
						Overload.ParamsStringAPIWithDefaults = ParamsStringAPIWithDefaults;
						Overload.ParamsStringCall = ParamsStringCall;
						Overload.ParamHandler = &ParamHandler;
						Overload.ParamProperty = ParamProperty;

						// record overload for later
						Overloads.Add(Overload);

						// Clobber all default params so far, since we've already exported an API that includes them.
						ParamsStringAPIWithDefaults = ParamsStringAPI;
					}
				}
				else
				{
					ParamsStringAPIWithDefaults = ParamsStringAPI;
				}

				ParamsStringAPI += TEXT(", ");
				ParamsStringAPIWithDefaults += TEXT(", ");
			}

			ParamsStringCall += TEXT(", ");
			ParamsStringCallNative += TEXT(", ");

		}
		paramsProcessed++;
	}

	// After last parameter revert change in parameter order to call native function
	if (SelfParameter)
	{
		ParamsStringCall = ParamsStringCallNative;
	}

	// remove last comma
	if (ParamsStringAPIWithDefaults.Len() > 0)
	{
		ParamsStringAPIWithDefaults = ParamsStringAPIWithDefaults.Left(ParamsStringAPIWithDefaults.Len() - 2);
	}
	if (ParamsStringCall.Len() > 0)
	{
		ParamsStringCall = ParamsStringCall.Left(ParamsStringCall.Len() - 2);
	}
}

void FMonoPropertyHandler::FunctionExporter::ExportFunctionVariables(FMonoTextBuilder& Builder) const
{
	FString NativeMethodName = Function.GetName();
	Builder.AppendLine(FString::Printf(TEXT("// Function %s"), *Function.GetPathName()));
	Builder.AppendLine(FString::Printf(TEXT("%sIntPtr %s_NativeFunction;"), !bBlueprintEvent ? TEXT("static readonly ") : TEXT(""), *NativeMethodName));

	if (Function.NumParms > 0)
	{
		Builder.AppendLine(FString::Printf(TEXT("static readonly int %s_ParamsSize;"), *NativeMethodName));
	}

	for (TFieldIterator<UProperty> ParamIt(&Function); ParamIt; ++ParamIt)
	{
		UProperty* ParamProperty = *ParamIt;
		const FMonoPropertyHandler& ParamHandler = Handler.PropertyHandlers.Find(ParamProperty);
		ParamHandler.ExportParameterVariables(Builder, &Function, NativeMethodName, ParamProperty, ParamProperty->GetName());
	}
}

void FMonoPropertyHandler::FunctionExporter::ExportOverloads(FMonoTextBuilder& Builder) const
{
	for (const FunctionOverload& Overload : Overloads)
	{
		Builder.AppendLine();
		ExportDeprecation(Builder);
		Builder.AppendLine(FString::Printf(TEXT("%s%s %s(%s)"), *Modifiers, *Handler.GetCSharpType(ReturnProperty), *CSharpMethodName, *Overload.ParamsStringAPIWithDefaults));
		Builder.OpenBrace();

		FString ReturnStatement = ReturnProperty ? TEXT("return ") : TEXT("");

		Overload.ParamHandler->ExportCppDefaultParameterAsLocalVariable(Builder, *Overload.CSharpParamName, Overload.CppDefaultValue, &Function, Overload.ParamProperty);
		Builder.AppendLine(FString::Printf(TEXT("%s%s(%s);"), *ReturnStatement, *CSharpMethodName, *Overload.ParamsStringCall));

		Builder.CloseBrace(); // Overloaded function

	}
}

void FMonoPropertyHandler::FunctionExporter::ExportFunction(FMonoTextBuilder& Builder) const
{
	Builder.AppendLine();
	Builder.AppendDocCommentFromMetadata(Function);
	ExportDeprecation(Builder);
	if (bBlueprintEvent)
	{
		Builder.AppendLine(TEXT("[BlueprintImplementable]"));
	}
	Builder.AppendLine(FString::Printf(TEXT("%s%s %s(%s)"), *Modifiers, nullptr != ReturnProperty ? *Handler.GetCSharpType(ReturnProperty) : TEXT("void"), *CSharpMethodName, *ParamsStringAPIWithDefaults));
	Builder.OpenBrace();

	ExportInvoke(Builder, InvokeMode::Normal);

	Builder.CloseBrace(); // Function

	Builder.AppendLine();
}

void FMonoPropertyHandler::FunctionExporter::ExportGetter(FMonoTextBuilder& Builder) const
{
	check(ReturnProperty);
	check(Function.NumParms == 1);

	Builder.AppendLine();
	Builder.AppendLine(TEXT("get"));
	Builder.OpenBrace();
	ExportInvoke(Builder, InvokeMode::Getter);
	Builder.CloseBrace();

}

void FMonoPropertyHandler::FunctionExporter::ExportSetter(FMonoTextBuilder& Builder) const
{
	check(nullptr == ReturnProperty);
	check(Function.NumParms == 1);

	Builder.AppendLine();
	Builder.AppendLine(FString::Printf(TEXT("%s set"), bProtected?TEXT("protected "):TEXT("")));
	Builder.OpenBrace();
	ExportInvoke(Builder, InvokeMode::Setter);
	Builder.CloseBrace();

}

void FMonoPropertyHandler::FunctionExporter::ExportExtensionMethod(FMonoTextBuilder& Builder) const
{
	Builder.AppendLine();
	Builder.AppendDocCommentFromMetadata(Function);
	ExportDeprecation(Builder);
	Builder.AppendLine(FString::Printf(TEXT("%s%s %s(%s)"), *Modifiers, *Handler.GetCSharpType(ReturnProperty), *CSharpMethodName, *ParamsStringAPIWithDefaults));
	Builder.OpenBrace();

	FString ReturnStatement = ReturnProperty ? TEXT("return ") : TEXT("");

	UClass* OriginalClass = Function.GetOuterUClass();
	check(OriginalClass);

	Builder.AppendLine(FString::Printf(TEXT("%s%s.%s(%s);"), *ReturnStatement, *GetScriptNameMapper().GetQualifiedName(*OriginalClass), *CSharpMethodName, *ParamsStringCall));

	Builder.CloseBrace(); // Extension method
}

void FMonoPropertyHandler::FunctionExporter::ExportInvoke(FMonoTextBuilder& Builder, InvokeMode Mode) const
{
#if DO_CHECK
	switch (Mode)
	{
	case InvokeMode::Getter:
		check(Function.NumParms == 1);
		check(ReturnProperty);
		check(Overloads.Num() == 0)
		break;
	case InvokeMode::Setter:
		check(Function.NumParms == 1);
		check(nullptr == ReturnProperty);
		check(Overloads.Num() == 0);
		break;
	case InvokeMode::Normal:
		break;
	default:
		checkNoEntry();
		break;
	}
#endif // DO_CHECK

	const FString NativeMethodName = Function.GetName();

	if (bBlueprintEvent)
	{
		// Lazy-init the instance function pointer.
		Builder.AppendLine(FString::Printf(TEXT("if (%s_NativeFunction == IntPtr.Zero)"), *NativeMethodName));
		Builder.OpenBrace();
		Builder.AppendLine(FString::Printf(TEXT("%s_NativeFunction = GetNativeFunctionFromInstanceAndName(NativeObject, \"%s\");"), *NativeMethodName, *NativeMethodName));
		Builder.CloseBrace();
	}

	if (Function.NumParms == 0)
	{
		Builder.AppendLine(FString::Printf(TEXT("%s(%s, %s_NativeFunction, IntPtr.Zero, 0);"), *PinvokeFunction, *PinvokeFirstArg, *NativeMethodName));
	}
	else
	{
		Builder.BeginUnsafeBlock();

		Builder.AppendLine(FString::Printf(TEXT("byte* ParamsBufferAllocation = stackalloc byte[%s_ParamsSize];"), *NativeMethodName));
		Builder.AppendLine(TEXT("IntPtr ParamsBuffer = new IntPtr(ParamsBufferAllocation);"));

		for (TFieldIterator<UProperty> ParamIt(&Function); ParamIt; ++ParamIt)
		{
			UProperty* ParamProperty = *ParamIt;
			const FString NativePropertyName = ParamProperty->GetName();
			// All ref params also have the CPF_Out flag, but we only need to marshal the former.
			// TODO: handle return-by-reference?  May not make much sense for structs, but we might want to use the array wrappers for array refs.
			if (!ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm) && (ParamProperty->HasAnyPropertyFlags(CPF_ReferenceParm) || !ParamProperty->HasAnyPropertyFlags(CPF_OutParm)))
			{
				const FMonoPropertyHandler& ParamHandler = Handler.PropertyHandlers.Find(ParamProperty);
				FString SourceName = Mode == InvokeMode::Setter ? TEXT("value") : GetScriptNameMapper().MapParameterName(ParamProperty);
				ParamHandler.ExportMarshalToNativeBuffer(Builder, ParamProperty, TEXT("null"), NativePropertyName, TEXT("ParamsBuffer"), FString::Printf(TEXT("%s_%s_Offset"), *NativeMethodName, *NativePropertyName), SourceName);
			}
		}

		Builder.AppendLine();
		Builder.AppendLine(FString::Printf(TEXT("%s(%s, %s_NativeFunction, ParamsBuffer, %s_ParamsSize);"), *PinvokeFunction, *PinvokeFirstArg, *NativeMethodName, *NativeMethodName));

		if (ReturnProperty || Function.HasAnyFunctionFlags(FUNC_HasOutParms))
		{
			Builder.AppendLine();
			for (TFieldIterator<UProperty> ParamIt(&Function); ParamIt; ++ParamIt)
			{
				UProperty* ParamProperty = *ParamIt;
				const FMonoPropertyHandler& ParamHandler = Handler.PropertyHandlers.Find(ParamProperty);
				if (ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm)
					|| (!ParamProperty->HasAnyPropertyFlags(CPF_ConstParm) && ParamProperty->HasAnyPropertyFlags(CPF_OutParm)))
				{
					FString NativeParamName = ParamProperty->GetName();

					FString MarshalDestination;
					if (ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						Builder.AppendLine(FString::Printf(TEXT("%s toReturn;"), *Handler.GetCSharpType(ReturnProperty)));
						MarshalDestination = TEXT("toReturn");
					}
					else
					{
						check(Mode == InvokeMode::Normal);
						MarshalDestination = GetScriptNameMapper().MapParameterName(ParamProperty);
					}
					ParamHandler.ExportMarshalFromNativeBuffer(
						Builder,
						ParamProperty, 
						TEXT("null"),
						NativeParamName,
						FString::Printf(TEXT("%s ="), *MarshalDestination),
						TEXT("ParamsBuffer"),
						FString::Printf(TEXT("%s_%s_Offset"), *NativeMethodName, *NativeParamName),
						true,
						ParamProperty->HasAnyPropertyFlags(CPF_ReferenceParm) && !ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm));
				}
			}
		}

		Builder.AppendLine();
		for (TFieldIterator<UProperty> ParamIt(&Function); ParamIt; ++ParamIt)
		{
			UProperty* ParamProperty = *ParamIt;
			if (!ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
			{
				const FMonoPropertyHandler& ParamHandler = Handler.PropertyHandlers.Find(ParamProperty);
				FString NativeParamName = ParamProperty->GetName();
				ParamHandler.ExportCleanupMarshallingBuffer(Builder, ParamProperty, NativeParamName);
			}
		}

		if (ReturnProperty)
		{
			Builder.AppendLine();
			Builder.AppendLine(TEXT("return toReturn;"));
		}

		Builder.EndUnsafeBlock();

	}
}

void FMonoPropertyHandler::FunctionExporter::ExportDeprecation(FMonoTextBuilder& Builder) const
{
	if (Function.HasMetaData(MD_DeprecatedFunction))
	{
		FString DeprecationMessage = Function.GetMetaData(MD_DeprecationMessage);
		if (DeprecationMessage.Len() == 0)
		{
			DeprecationMessage = TEXT("This function is obsolete");
		}
		Builder.AppendLine(FString::Printf(TEXT("[Obsolete(\"%s\")]"), *DeprecationMessage));
	}
}

void FMonoPropertyHandler::ExportFunction(FMonoTextBuilder& Builder, UFunction* Function, FunctionType FuncType) const
{
	ProtectionMode ProtectionBehavior = ProtectionMode::UseUFunctionProtection;
	OverloadMode OverloadBehavior = OverloadMode::AllowOverloads;
	BlueprintVisibility CallBehavior = BlueprintVisibility::Call;

	if (FuncType == FunctionType::ExtensionOnAnotherClass)
	{
		ProtectionBehavior = ProtectionMode::OverrideWithInternal;
		OverloadBehavior = OverloadMode::SuppressOverloads;
	}
	else if (FuncType == FunctionType::BlueprintEvent)
	{
		ProtectionBehavior = ProtectionMode::OverrideWithProtected;
		OverloadBehavior = OverloadMode::SuppressOverloads;
		CallBehavior = BlueprintVisibility::Event;
	}
	FunctionExporter Exporter(*this, *Function, ProtectionBehavior, OverloadBehavior, CallBehavior);

	Exporter.ExportFunctionVariables(Builder);

	Exporter.ExportOverloads(Builder);

	Exporter.ExportFunction(Builder);
}

void FMonoPropertyHandler::ExportOverridableFunction(FMonoTextBuilder& Builder, UFunction* Function) const
{
	UProperty* ReturnProperty = Function->GetReturnProperty();
	check(CanHandleProperty(ReturnProperty));

	FString ParamsStringAPI;
	FString ParamsCallString;

	FString NativeMethodName = *Function->GetName();

	for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		UProperty* ParamProperty = *ParamIt;
		if (!ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			const FMonoPropertyHandler& ParamHandler = PropertyHandlers.Find(ParamProperty);
			FString CSharpParamName = GetScriptNameMapper().MapParameterName(ParamProperty);
			FString CSharpParamType = PropertyHandlers.Find(ParamProperty).GetCSharpType(ParamProperty);

			// Don't generate ref or out bindings for const-reference params.
			// While the extra qualifiers would only clutter up the generated invoker,  not user code,
			// it would still give an incorrect impression that the user's implementation of the UFunction
			// is meant to change those parameters.
			FString RefQualifier;
			if (!ParamProperty->HasAnyPropertyFlags(CPF_ConstParm))
			{
				if (ParamProperty->HasAnyPropertyFlags(CPF_ReferenceParm))
				{
					RefQualifier = "ref ";
				}
				else if (ParamProperty->HasAnyPropertyFlags(CPF_OutParm))
				{
					RefQualifier = "out ";
				}
			}

			ParamsStringAPI += FString::Printf(TEXT("%s%s %s"), *RefQualifier, *CSharpParamType, *CSharpParamName);
			ParamsStringAPI += TEXT(", ");
			ParamsCallString += FString::Printf(TEXT("%s%s, "), *RefQualifier, *CSharpParamName);
		}
	}

	// remove last comma
	if (ParamsStringAPI.Len() > 0)
	{
		ParamsStringAPI = ParamsStringAPI.Left(ParamsStringAPI.Len() - 2);
	}
	if (ParamsCallString.Len() > 0)
	{
		ParamsCallString = ParamsCallString.Left(ParamsCallString.Len() - 2);
	}

	ExportFunction(Builder, Function, FunctionType::BlueprintEvent);

	//the rewriter moves user overrides from the original method to this method - users should not see it in intellisense
	Builder.AppendLine(TEXT("[System.ComponentModel.EditorBrowsable(System.ComponentModel.EditorBrowsableState.Never)]"));
	Builder.AppendLine(FString::Printf(TEXT("protected virtual %s %s_Implementation(%s)"), *GetCSharpType(ReturnProperty), *NativeMethodName, *ParamsStringAPI));
	Builder.OpenBrace();

	// Out params must be initialized before we return, since there may not be any override to do it.
	for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		UProperty* ParamProperty = *ParamIt;
		if (ParamProperty->HasAnyPropertyFlags(CPF_OutParm) && !ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm | CPF_ConstParm | CPF_ReferenceParm))
		{
			const FMonoPropertyHandler& ParamHandler = PropertyHandlers.Find(ParamProperty);
			FString CSharpParamName = GetScriptNameMapper().MapParameterName(ParamProperty);
			FString CSharpDefaultValue = ParamHandler.GetNullReturnCSharpValue(ParamProperty);
			Builder.AppendLine(FString::Printf(TEXT("%s = %s;"), *CSharpParamName, *CSharpDefaultValue));
		}
	}
	if (nullptr != ReturnProperty)
	{
		Builder.AppendLine(FString::Printf(TEXT("return %s;"), *GetNullReturnCSharpValue(ReturnProperty)));
	}
	Builder.CloseBrace(); // Function

	// Export the native invoker
	Builder.AppendLine(FString::Printf(TEXT("void Invoke_%s(IntPtr buffer, IntPtr returnBuffer)"), *NativeMethodName));
	Builder.OpenBrace();
	Builder.BeginUnsafeBlock();

	FString ReturnAssignment;
	for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		UProperty* ParamProperty = *ParamIt;
		const FMonoPropertyHandler& ParamHandler = PropertyHandlers.Find(ParamProperty);
		FString NativeParamName = ParamProperty->GetName();
		FString CSharpParamName = GetScriptNameMapper().MapParameterName(ParamProperty);
		FString ParamType = ParamHandler.GetCSharpType(ParamProperty);
		if (ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			ReturnAssignment = FString::Printf(TEXT("%s returnValue = "), *ParamType);
		}
		else if (!ParamProperty->HasAnyPropertyFlags(CPF_ConstParm) && ParamProperty->HasAnyPropertyFlags(CPF_OutParm))
		{
			Builder.AppendLine(FString::Printf(TEXT("%s %s;"), *ParamType, *CSharpParamName));
		}
		else
		{
			ParamHandler.ExportMarshalFromNativeBuffer(
				Builder,
				ParamProperty, 
				TEXT("null"),
				NativeParamName,
				FString::Printf(TEXT("%s %s ="), *ParamType, *CSharpParamName),
				TEXT("buffer"),
				FString::Printf(TEXT("%s_%s_Offset"), *NativeMethodName, *NativeParamName),
				false,
				false);
		}
	}

	Builder.AppendLine(FString::Printf(TEXT("%s%s_Implementation(%s);"), *ReturnAssignment, *NativeMethodName, *ParamsCallString));

	if (nullptr != ReturnProperty)
	{
		const FMonoPropertyHandler& ReturnValueHandler = PropertyHandlers.Find(ReturnProperty);
		ReturnValueHandler.ExportMarshalToNativeBuffer(
			Builder,
			ReturnProperty, 
			TEXT("null"),
			GetScriptNameMapper().MapPropertyName(ReturnProperty),
			TEXT("returnBuffer"),
			TEXT("0"),
			TEXT("returnValue"));
	}
	for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		UProperty* ParamProperty = *ParamIt;
		const FMonoPropertyHandler& ParamHandler = PropertyHandlers.Find(ParamProperty);
		FString NativePropertyName = ParamProperty->GetName();
		FString CSharpParamName = GetScriptNameMapper().MapParameterName(ParamProperty);
		FString ParamType = ParamHandler.GetCSharpType(ParamProperty);
		if (!ParamProperty->HasAnyPropertyFlags(CPF_ReturnParm | CPF_ConstParm) && ParamProperty->HasAnyPropertyFlags(CPF_OutParm))
		{
			ParamHandler.ExportMarshalToNativeBuffer(
				Builder,
				ParamProperty, 
				TEXT("null"),
				NativePropertyName,
				TEXT("buffer"),
				FString::Printf(TEXT("%s_%s_Offset"), *NativeMethodName, *NativePropertyName),
				CSharpParamName);
		}
	}

	Builder.EndUnsafeBlock();
	Builder.CloseBrace(); // Invoker

	Builder.AppendLine();
}

void FMonoPropertyHandler::ExportExtensionMethod(FMonoTextBuilder& Builder, UFunction& Function, const UProperty* SelfParameter, const UClass* OverrideClassBeingExtended) const
{
	FunctionExporter Exporter(*this, Function, SelfParameter, OverrideClassBeingExtended);

	Exporter.ExportOverloads(Builder);

	Exporter.ExportExtensionMethod(Builder);
}

void FMonoPropertyHandler::ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	Builder.AppendLine(FString::Printf(TEXT("static readonly int %s_Offset;"), *NativePropertyName));
	if (Property->ArrayDim > 1)
	{
		check(IsSupportedInStaticArray());
		Builder.AppendLine(FString::Printf(TEXT("static readonly int %s_Length;"), *NativePropertyName));
		Builder.AppendLine(FString::Printf(TEXT("%s %s_Wrapper;"), *GetCSharpFixedSizeArrayType(Property), *NativePropertyName));
	}
}

void FMonoPropertyHandler::ExportPropertyGetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	Builder.AppendLine(TEXT("CheckDestroyedByUnrealGC();"));

	ExportMarshalFromNativeBuffer(
		Builder,
		Property, 
		TEXT("this"),
		NativePropertyName,
		TEXT("return"),
		TEXT("NativeObject"),
		FString::Printf(TEXT("%s_Offset"), *NativePropertyName),
		false,
		false);
}

void FMonoPropertyHandler::ExportPropertySetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	Builder.AppendLine(TEXT("CheckDestroyedByUnrealGC();"));

	ExportMarshalToNativeBuffer(
		Builder, 
		Property, 
		TEXT("this"), 
		NativePropertyName,
		TEXT("NativeObject"), 
		FString::Printf(TEXT("%s_Offset"), *NativePropertyName),
		TEXT("value"));
}

void FMonoPropertyHandler::ExportFunctionReturnStatement(FMonoTextBuilder& Builder, const UFunction* Function, const UProperty* ReturnProperty, const FString& NativeFunctionName, const FString& ParamsCallString) const
{
	const TCHAR* ReturnStatement = nullptr == ReturnProperty ? TEXT("") : TEXT("return ");
	Builder.AppendLine(FString::Printf(TEXT("%sInvoke_%s(NativeObject, %s_NativeFunction%s);"), ReturnStatement, *NativeFunctionName, *NativeFunctionName, *ParamsCallString));
}

void FMonoPropertyHandler::ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const
{
	checkNoEntry();
}

void FMonoPropertyHandler::ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& NativeParamName) const
{
	checkNoEntry();
}

void FMonoPropertyHandler::ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const
{
	checkNoEntry();
}

FString FMonoPropertyHandler::GetCppDefaultParameterValue(UFunction* Function, UProperty* ParamProperty) const
{
	//TODO: respect defaults specified as metadata, not C++ default params?
	//		The syntax for those seems to be a bit looser, but they're pretty rare...
	//		When specified that way, the key will just be the param name.

	// Return the default value exactly as specified for C++.
	// Subclasses may intercept it if it needs to be massaged for C# purposes.
	const FString MetadataCppDefaultValueKey = FString::Printf(TEXT("CPP_Default_%s"), *ParamProperty->GetName());
	return Function->GetMetaData(*MetadataCppDefaultValueKey);
}

FString FMonoPropertyHandler::ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	checkNoEntry();
	return TEXT("");
}


void FMonoPropertyHandler::ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	checkNoEntry();
}

FString FMonoPropertyHandler::ExportMarshalerDelegates(const UProperty *Property, const FString &PropertyName) const
{
	checkNoEntry();
	return TEXT("");
}

void FMonoPropertyHandler::ExportParameterVariables(FMonoTextBuilder& Builder, UFunction* Function, const FString& NativeMethodName, UProperty* ParamProperty, const FString& NativePropertyName) const
{
	Builder.AppendLine(FString::Printf(TEXT("static readonly int %s_%s_Offset;"), *NativeMethodName, *NativePropertyName));
}

//////////////////////////////////////////////////////////////////////////
// FSimpleTypePropertyHandler
//////////////////////////////////////////////////////////////////////////

bool FSimpleTypePropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	return Property->IsA(PropertyClass);
}

FString FSimpleTypePropertyHandler::GetCSharpType(const UProperty* Property) const
{
	return CSharpType;
}

FString FSimpleTypePropertyHandler::GetNullReturnCSharpValue(const UProperty* ReturnProperty) const
{
	return FString::Printf(TEXT("default(%s)"), *GetCSharpType(ReturnProperty));
}

FString FSimpleTypePropertyHandler::ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	if (CppDefaultValue == TEXT("None")) {
		return GetNullReturnCSharpValue(ParamProperty);
	}
	return CppDefaultValue;
}

void FSimpleTypePropertyHandler::ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const
{
	Builder.AppendLine(FString::Printf(TEXT("%s.ToNative(IntPtr.Add(%s, %s), 0, %s, %s);"), *GetMarshalerType(Property), *DestinationBuffer, *Offset, *Owner, *Source));
}

void FSimpleTypePropertyHandler::ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const
{
	// No cleanup required for simple types
}

FString FSimpleTypePropertyHandler::GetMarshalerType(const UProperty *Property) const
{
	check(!MarshalerType.IsEmpty());
	return MarshalerType;
}

void FSimpleTypePropertyHandler::ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const
{
	// The returned handle is just a pointer to the return value memory in the parameter buffer.
	Builder.AppendLine(FString::Printf(TEXT("%s %s.FromNative(IntPtr.Add(%s, %s), 0, %s);"), *AssignmentOrReturn, *GetMarshalerType(Property), *SourceBuffer, *Offset, *Owner));
}

FString FSimpleTypePropertyHandler::ExportMarshalerDelegates(const UProperty *Property, const FString &PropertyName) const
{
	return FString::Printf(TEXT("%s.ToNative, %s.FromNative"), *GetMarshalerType(Property), *GetMarshalerType(Property));
}


//////////////////////////////////////////////////////////////////////////
// FBlittableTypePropertyHandler
//////////////////////////////////////////////////////////////////////////

FString FBlittableTypePropertyHandler::GetMarshalerType(const UProperty *Property) const
{
	return FString::Printf(TEXT("BlittableTypeMarshaler<%s>"), *GetCSharpType(Property));
}


//////////////////////////////////////////////////////////////////////////
// FFloatPropertyHandler
//////////////////////////////////////////////////////////////////////////

FString FFloatPropertyHandler::ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	// Trailing f will have been stripped for blueprint, but C# won't auto-convert literal constants from double to float.
	return CppDefaultValue + TEXT("f");
}


//////////////////////////////////////////////////////////////////////////
// FEnumPropertyHandler
//////////////////////////////////////////////////////////////////////////

TMap<FName, FString> FEnumPropertyHandler::StrippedPrefixes;

bool FEnumPropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	return Property->IsA(UEnumProperty::StaticClass())
		|| (Property->IsA(UByteProperty::StaticClass()) && Cast<const UByteProperty>(Property)->Enum);
}

UEnum* GetEnum(const UProperty* Property)
{
	UEnum* Enum;
	if (const UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Enum = EnumProperty->GetEnum();
	}
	else
	{
		const UByteProperty* ByteProperty = CastChecked<UByteProperty>(Property);
		Enum = ByteProperty->GetIntPropertyEnum();
	}

	check(Enum);
	return Enum;
}

FString FEnumPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	UEnum* Enum = GetEnum(Property);

	// Fully qualify the enum name - we may be pulling it from a different package's bindings.
	return GetScriptNameMapper().GetQualifiedName(*Enum);
}

FString FEnumPropertyHandler::GetMarshalerType(const UProperty *Property) const
{
	return FString::Printf(TEXT("EnumMarshaler<%s>"), *GetCSharpType(Property));
}

FString FEnumPropertyHandler::ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	// Default value may be namespaced in C++, and must be in C#.
	int32 Pos = CppDefaultValue.Find(TEXT("::"));
	FString EnumValue = (Pos >= 0 ? CppDefaultValue.Right(CppDefaultValue.Len() - Pos - 2) : CppDefaultValue);

	UEnum* Enum = GetEnum(ParamProperty);
	const FString* Prefix = StrippedPrefixes.Find(Enum->GetFName());
	if (Prefix)
	{
		EnumValue.RemoveFromStart(*Prefix, ESearchCase::CaseSensitive);
	}
	EnumValue = GetScriptNameMapper().ScriptifyName(EnumValue, EScriptNameKind::EnumValue);

	return FString::Printf(TEXT("%s.%s"), *GetCSharpType(ParamProperty), *EnumValue);
}

//////////////////////////////////////////////////////////////////////////
// FNamePropertyHandler
//////////////////////////////////////////////////////////////////////////

FString FNamePropertyHandler::GetNullReturnCSharpValue(const UProperty* ReturnProperty) const
{
	return TEXT("default(Name)");
}

void FNamePropertyHandler::ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	if (CppDefaultValue == TEXT("None"))
	{
		Builder.AppendLine(FString::Printf(TEXT("Name %s = Name.None;"), *VariableName));
	}
	else
	{
		Builder.AppendLine(FString::Printf(TEXT("Name %s = new Name(\"%s\");"), *VariableName, *CppDefaultValue));
	}
}

//////////////////////////////////////////////////////////////////////////
// FTextPropertyHandler
//////////////////////////////////////////////////////////////////////////

bool FTextPropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	check(Property->IsA(UTextProperty::StaticClass()));
	return true;
}

FString FTextPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	return TEXT("Text");
}

void FTextPropertyHandler::ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	FMonoPropertyHandler::ExportPropertyStaticConstruction(Builder, Property, NativePropertyName);
	Builder.AppendLine(FString::Printf(TEXT("%s_NativeProperty = UnrealInterop.GetNativePropertyFromName(NativeClassPtr, \"%s\");"), *NativePropertyName, *NativePropertyName));
}

void FTextPropertyHandler::ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	FMonoPropertyHandler::ExportPropertyVariables(Builder, Property, NativePropertyName);
	Builder.AppendLine(FString::Printf(TEXT("static readonly IntPtr %s_NativeProperty;"), *NativePropertyName));
	if (Property->ArrayDim == 1)
	{
		Builder.AppendLine(FString::Printf(TEXT("TextMarshaler %s_Wrapper;"), *NativePropertyName));
	}
}

void FTextPropertyHandler::ExportPropertyGetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	Builder.AppendLine(FString::Printf(TEXT("if (%s_Wrapper == null)"), *NativePropertyName));
	Builder.OpenBrace();
	check(Property->ArrayDim == 1);
	Builder.AppendLine(FString::Printf(TEXT("%s_Wrapper  = new TextMarshaler(1);"), *NativePropertyName));
	Builder.CloseBrace();
	Builder.AppendLine(FString::Printf(TEXT("return %s_Wrapper.FromNative(this.NativeObject + %s_Offset, 0, this);"), *NativePropertyName, *NativePropertyName));
}

FString FTextPropertyHandler::GetNullReturnCSharpValue(const UProperty* ReturnProperty) const
{
	return TEXT("null");
}

/*
//TODO
void FTextPropertyHandler::ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const
{

}

void FTextPropertyHandler::ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const
{
	
}

void FTextPropertyHandler::ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const
{
	
}
*/
FString FTextPropertyHandler::ExportInstanceMarshalerVariables(const UProperty *Property, const FString &PropertyName) const
{
	return FString::Printf(TEXT("TextMarshaler InstanceMarshaler = new TextMarshaler(%s_Length);"), *PropertyName);
}

FString FTextPropertyHandler::ExportMarshalerDelegates(const UProperty *Property, const FString &PropertyName) const
{
	return TEXT("InstanceMarshaler.ToNative, InstanceMarshaler.FromNative");//FString::Printf(TEXT("new TextFixedSizeArrayMarshaler (%s_Length)"),*PropertyName);
}

//////////////////////////////////////////////////////////////////////////
// FWeakObjectPropertyHandler
//////////////////////////////////////////////////////////////////////////

FString FWeakObjectPropertyHandler::GetCSharpType(const UProperty *Property) const
{
	const UWeakObjectProperty* ObjectProperty = CastChecked<UWeakObjectProperty>(Property);
	check(ObjectProperty->PropertyClass);
	return FString::Printf(TEXT("WeakObject<%s>"), *GetScriptNameMapper().GetQualifiedName(*ObjectProperty->PropertyClass));
}

FString FWeakObjectPropertyHandler::GetMarshalerType(const UProperty *Property) const
{
	const UWeakObjectProperty* ObjectProperty = CastChecked<UWeakObjectProperty>(Property);
	check(ObjectProperty->PropertyClass);
	FString InnerType = *GetScriptNameMapper().GetQualifiedName(*ObjectProperty->PropertyClass);
	return FString::Printf(TEXT("WeakObjectMarshaler<%s>"), *InnerType);
}

//////////////////////////////////////////////////////////////////////////
// FBitfieldPropertyHandler
//////////////////////////////////////////////////////////////////////////

void FBitfieldPropertyHandler::ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	FMonoPropertyHandler::ExportPropertyStaticConstruction(Builder, Property, NativePropertyName);
	Builder.AppendLine(FString::Printf(TEXT("%s_NativeProperty = UnrealInterop.GetNativePropertyFromName(NativeClassPtr, \"%s\");"), *NativePropertyName, *NativePropertyName));
}

bool FBitfieldPropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	const UBoolProperty* BoolProperty = CastChecked<UBoolProperty>(Property);
	return !BoolProperty->IsNativeBool();
}

FString FBitfieldPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	return TEXT("bool");
}

void FBitfieldPropertyHandler::ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	FMonoPropertyHandler::ExportPropertyVariables(Builder, Property, NativePropertyName);
	Builder.AppendLine(FString::Printf(TEXT("static readonly IntPtr %s_NativeProperty;"), *NativePropertyName));
}

void FBitfieldPropertyHandler::ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const
{
	Builder.AppendLine(FString::Printf(TEXT("%s UnrealInterop.GetBitfieldValueFromProperty(%s, %s_NativeProperty, %s);"), *AssignmentOrReturn, *SourceBuffer, *NativePropertyName, *Offset));
}

void FBitfieldPropertyHandler::ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const
{

}

void FBitfieldPropertyHandler::ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const
{
	Builder.AppendLine(FString::Printf(TEXT("UnrealInterop.SetBitfieldValueForProperty(%s, %s_NativeProperty, %s, %s);"), *DestinationBuffer, *NativePropertyName, *Offset, *Source));
}

FString FBitfieldPropertyHandler::GetNullReturnCSharpValue(const UProperty* ReturnProperty) const
{
	return TEXT("false");
}

//////////////////////////////////////////////////////////////////////////
// FStringPropertyHandler
//////////////////////////////////////////////////////////////////////////

bool FStringPropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	check(Property->IsA(UStrProperty::StaticClass()));
	return true;
}

FString FStringPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	return TEXT("string");
}

void FStringPropertyHandler::ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	FMonoPropertyHandler::ExportPropertyStaticConstruction(Builder, Property, NativePropertyName);
	Builder.AppendLine(FString::Printf(TEXT("%s_NativeProperty = UnrealInterop.GetNativePropertyFromName(NativeClassPtr, \"%s\");"), *NativePropertyName, *NativePropertyName));
}

void FStringPropertyHandler::ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	FMonoPropertyHandler::ExportPropertyVariables(Builder, Property, NativePropertyName);
	Builder.AppendLine(FString::Printf(TEXT("static readonly IntPtr %s_NativeProperty;"), *NativePropertyName));
}

void FStringPropertyHandler::ExportPropertySetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	Builder.AppendLine(TEXT("CheckDestroyedByUnrealGC();"));
	Builder.AppendLine(FString::Printf(TEXT("StringMarshaler.ToNative(IntPtr.Add(NativeObject,%s_Offset),0,this,value);"),*NativePropertyName));
}


void FStringPropertyHandler::ExportPropertyGetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	Builder.AppendLine(TEXT("CheckDestroyedByUnrealGC();"));
	Builder.AppendLine(FString::Printf(TEXT("return StringMarshaler.FromNative(IntPtr.Add(NativeObject,%s_Offset),0,this);"), *NativePropertyName));
}


void FStringPropertyHandler::ExportFunctionReturnStatement(FMonoTextBuilder& Builder, const UFunction* Function, const UProperty* ReturnProperty, const FString& FunctionName, const FString& ParamsCallString) const
{
	Builder.AppendLine(FString::Printf(TEXT("return UnrealInterop.MarshalIntPtrAsString(Invoke_%s(NativeObject, %s_NativeFunction%s));"), *FunctionName, *FunctionName, *ParamsCallString));
}

FString FStringPropertyHandler::GetNullReturnCSharpValue(const UProperty* ReturnProperty) const
{
	//we can't use string.empty as this may be used for places where it must be a compile-time constant
	return TEXT("\"\"");
}

void FStringPropertyHandler::ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const
{
	Builder.AppendLine(FString::Printf(TEXT("IntPtr %s_NativePtr = IntPtr.Add(%s,%s);"), *NativePropertyName, *DestinationBuffer, *Offset));
	Builder.AppendLine(FString::Printf(TEXT("StringMarshalerWithCleanup.ToNative(%s_NativePtr,0,%s,%s);"),*NativePropertyName, *Owner, *Source));
}

void FStringPropertyHandler::ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const
{
	Builder.AppendLine(FString::Printf(TEXT("StringMarshalerWithCleanup.DestructInstance(%s_NativePtr, 0);"), *ParamName));
}

void FStringPropertyHandler::ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const
{
	//if it was a "ref" parameter, we set this pointer up before calling the function. if not, create one.
	if (!reuseRefMarshallers)
	{
		Builder.AppendLine(FString::Printf(TEXT("IntPtr %s_NativePtr = IntPtr.Add(%s,%s);"), *NativePropertyName, *SourceBuffer, *Offset));
	}
	// The mirror struct references a temp string buffer which we must clean up.
	Builder.AppendLine(FString::Printf(TEXT("%s StringMarshalerWithCleanup.FromNative(%s_NativePtr,0,%s);"),*AssignmentOrReturn, *NativePropertyName, *Owner));
	if (bCleanupSourceBuffer)
	{
		// Ensure we're not generating unreachable cleanup code.
		check(AssignmentOrReturn != TEXT("return"));
		Builder.AppendLine(FString::Printf(TEXT("StringMarshalerWithCleanup.DestructInstance(%s_NativePtr, 0);"), *NativePropertyName));
	}
}

FString FStringPropertyHandler::ExportMarshalerDelegates(const UProperty *Property, const FString &NativePropertyName) const
{
	return TEXT("StringMarshaler.ToNative, StringMarshaler.FromNative");
}

FString FStringPropertyHandler::ConvertCppDefaultParameterToCSharp(const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	return TEXT("\"") + CppDefaultValue + TEXT("\"");
}


//////////////////////////////////////////////////////////////////////////
// FObjectPropertyHandler
//////////////////////////////////////////////////////////////////////////

void FObjectPropertyHandler::AddReferences(const UProperty* Property, TSet<UStruct*>& References) const
{
	const UObjectProperty& ObjectProperty = *CastChecked<UObjectProperty>(Property);
	References.Add(ObjectProperty.PropertyClass);
}

FString FObjectPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	const UObjectProperty* ObjectProperty = CastChecked<UObjectProperty>(Property);
	check(ObjectProperty->PropertyClass);
	if (Property->HasAnyPropertyFlags(CPF_SubobjectReference))
	{
		return FString::Printf(TEXT("Subobject<%s>"), *GetScriptNameMapper().GetQualifiedName(*ObjectProperty->PropertyClass));
	}
	else
	{
		return GetScriptNameMapper().GetQualifiedName(*ObjectProperty->PropertyClass);
	}
}

FString FObjectPropertyHandler::GetMarshalerType(const UProperty *Property) const
{
	if (Property->HasAnyPropertyFlags(CPF_SubobjectReference))
	{
		const UObjectProperty* ObjectProperty = CastChecked<UObjectProperty>(Property);
		check(ObjectProperty->PropertyClass);
		FString UObjectType = GetScriptNameMapper().GetQualifiedName(*ObjectProperty->PropertyClass);
		return FString::Printf(TEXT("SubobjectMarshaler<%s>"), *UObjectType);
	}
	else
	{
		return FString::Printf(TEXT("UnrealObjectMarshaler<%s>"), *GetCSharpType(Property));
	}
}

//////////////////////////////////////////////////////////////////////////
// FClassPropertyHandler
//////////////////////////////////////////////////////////////////////////


void FClassPropertyHandler::AddReferences(const UProperty* Property, TSet<UStruct*>& References) const
{
	const UClassProperty& ClassProperty = *CastChecked<UClassProperty>(Property);
	References.Add(ClassProperty.MetaClass);
}

FString FClassPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	// We always use a SubclassOf<T> wrapper, even for class properties not declared with TSubclassOf.
	// We don't have a managed representation of UClass, so we use SubclassOf<UnrealObject> in that case.
	const UClassProperty& ClassProperty = *CastChecked<UClassProperty>(Property);
	check(ClassProperty.MetaClass);
	return FString::Printf(TEXT("SubclassOf<%s>"), *GetScriptNameMapper().GetQualifiedName(*ClassProperty.MetaClass));
}

FString FClassPropertyHandler::GetMarshalerType(const UProperty *Property) const
{
	const UClassProperty& ClassProperty = *CastChecked<UClassProperty>(Property);
	check(ClassProperty.MetaClass);
	return FString::Printf(TEXT("SubclassOfMarshaler<%s>"), *GetScriptNameMapper().GetQualifiedName(*ClassProperty.MetaClass));
}

//////////////////////////////////////////////////////////////////////////
// FArrayPropertyHandler
//////////////////////////////////////////////////////////////////////////

bool FArrayPropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	const UArrayProperty& ArrayProperty = *CastChecked<UArrayProperty>(Property);
	const UProperty* InnerProperty = ArrayProperty.Inner;
	const FMonoPropertyHandler& Handler = PropertyHandlers.Find(InnerProperty);
	return Handler.IsSupportedAsArrayInner();
}

void FArrayPropertyHandler::AddReferences(const UProperty* Property, TSet<UStruct*>& References) const
{
	const UArrayProperty& ArrayProperty = *CastChecked<UArrayProperty>(Property);
	const UProperty* InnerProperty = ArrayProperty.Inner;
	const FMonoPropertyHandler& Handler = PropertyHandlers.Find(InnerProperty);
	return Handler.AddReferences(InnerProperty, References);
}

FString FArrayPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	return GetWrapperInterface(Property);
}

void FArrayPropertyHandler::ExportPropertyStaticConstruction(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	FMonoPropertyHandler::ExportPropertyStaticConstruction(Builder, Property, NativePropertyName);
	Builder.AppendLine(FString::Printf(TEXT("%s_NativeProperty = UnrealInterop.GetNativePropertyFromName(NativeClassPtr, \"%s\");"), *NativePropertyName, *NativePropertyName));
}

void FArrayPropertyHandler::ExportParameterStaticConstruction(FMonoTextBuilder& Builder, const FString& NativeMethodName, const UProperty* Parameter) const
{
	FMonoPropertyHandler::ExportParameterStaticConstruction(Builder, NativeMethodName, Parameter);
	const FString ParamName = Parameter->GetName();
	Builder.AppendLine(FString::Printf(TEXT("%s_%s_ElementSize = UnrealInterop.GetArrayElementSize(%s_NativeFunction, \"%s\");"), *NativeMethodName, *ParamName, *NativeMethodName, *ParamName));
}

void FArrayPropertyHandler::ExportPropertyVariables(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	FMonoPropertyHandler::ExportPropertyVariables(Builder, Property, NativePropertyName);
	Builder.AppendLine(FString::Printf(TEXT("static readonly IntPtr %s_NativeProperty;"), *NativePropertyName));
	Builder.AppendLine(FString::Printf(TEXT("%s %s_Wrapper = null;"), *GetWrapperType(Property), *NativePropertyName));
}

void FArrayPropertyHandler::ExportParameterVariables(FMonoTextBuilder& Builder, UFunction* Function, const FString& NativeMethodName, UProperty* ParamProperty, const FString& NativePropertyName) const
{
	FMonoPropertyHandler::ExportParameterVariables(Builder, Function, NativeMethodName, ParamProperty, NativePropertyName);
	Builder.AppendLine(FString::Printf(TEXT("static readonly int %s_%s_ElementSize;"), *NativeMethodName, *NativePropertyName));
}

void FArrayPropertyHandler::ExportPropertyGetter(FMonoTextBuilder& Builder, const UProperty* Property, const FString& NativePropertyName) const
{
	Builder.AppendLine(FString::Printf(TEXT("if(%s_Wrapper == null)"), *NativePropertyName));
	Builder.OpenBrace();

	const UArrayProperty& ArrayProperty = *CastChecked<UArrayProperty>(Property);
	const UProperty* InnerProperty = ArrayProperty.Inner;
	const FMonoPropertyHandler& Handler = PropertyHandlers.Find(InnerProperty);

	Builder.AppendLine(FString::Printf(TEXT("%s_Wrapper = new %s(1, %s_NativeProperty, %s);"), *NativePropertyName, *GetWrapperType(Property), *NativePropertyName, *Handler.ExportMarshalerDelegates(InnerProperty, NativePropertyName)));

	Builder.CloseBrace();

	Builder.AppendLine();
	Builder.AppendLine(FString::Printf(TEXT("return %s_Wrapper.FromNative(IntPtr.Add(NativeObject,%s_Offset),0,this);"), *NativePropertyName, *NativePropertyName));
}

void FArrayPropertyHandler::ExportMarshalToNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& DestinationBuffer, const FString& Offset, const FString& Source) const
{
	const UArrayProperty& ArrayProperty = *CastChecked<UArrayProperty>(Property);
	const UProperty* InnerProperty = ArrayProperty.Inner;
	const FMonoPropertyHandler& Handler = PropertyHandlers.Find(InnerProperty);

	FString ElementSize = NativePropertyName + TEXT("_ElementSize");
	if (UFunction* Function = Cast<UFunction>(Property->GetOuter()))
	{
		FString NativeFunctionName = Function->GetName();
		ElementSize = NativeFunctionName + TEXT("_") + ElementSize;
	}

	FString InnerType = Handler.GetCSharpType(InnerProperty);
	//Native buffer variable used in cleanup
	Builder.AppendLine(FString::Printf(TEXT("IntPtr %s_NativeBuffer = IntPtr.Add(%s, %s);"), *NativePropertyName, *DestinationBuffer, *Offset));
	Builder.AppendLine(FString::Printf(TEXT("UnrealArrayCopyMarshaler<%s> %s_Marshaler = new UnrealArrayCopyMarshaler<%s>(1, %s, %s);"), *InnerType, *NativePropertyName, *InnerType, *Handler.ExportMarshalerDelegates(InnerProperty, NativePropertyName),*ElementSize));
	Builder.AppendLine(FString::Printf(TEXT("%s_Marshaler.ToNative(%s_NativeBuffer, 0, null, %s);"), *NativePropertyName, *NativePropertyName, *Source));
}

void FArrayPropertyHandler::ExportCleanupMarshallingBuffer(FMonoTextBuilder& Builder, const UProperty* ParamProperty, const FString& ParamName) const
{
	const UArrayProperty& ArrayProperty = *CastChecked<UArrayProperty>(ParamProperty);
	const UProperty* InnerProperty = ArrayProperty.Inner;
	const FMonoPropertyHandler& Handler = PropertyHandlers.Find(InnerProperty);
	FString InnerType = Handler.GetCSharpType(InnerProperty);
	FString MarshalerType = FString::Printf(TEXT("UnrealArrayCopyMarshaler<%s>"), *InnerType);
	Builder.AppendLine(FString::Printf(TEXT("%s.DestructInstance(%s_NativeBuffer, 0);"), *MarshalerType, *ParamName));
}

void FArrayPropertyHandler::ExportMarshalFromNativeBuffer(FMonoTextBuilder& Builder, const UProperty* Property, const FString &Owner, const FString& NativePropertyName, const FString& AssignmentOrReturn, const FString& SourceBuffer, const FString& Offset, bool bCleanupSourceBuffer, bool reuseRefMarshallers) const
{
	const UArrayProperty& ArrayProperty = *CastChecked<UArrayProperty>(Property);
	const UProperty* InnerProperty = ArrayProperty.Inner;
	const FMonoPropertyHandler& Handler = PropertyHandlers.Find(InnerProperty);

	FString InnerType = Handler.GetCSharpType(InnerProperty);
	FString MarshalerType = FString::Printf(TEXT("UnrealArrayCopyMarshaler<%s>"), *InnerType);

	//if it was a "ref" parameter, we set the marshaler up before calling the function. if not, create one.
	if (!reuseRefMarshallers)
	{
		FString ElementSize = NativePropertyName + TEXT("_ElementSize");
		if (UFunction* Function = Cast<UFunction>(Property->GetOuter()))
		{
			FString NativeFunctionName = Function->GetName();
			ElementSize = NativeFunctionName + TEXT("_") + ElementSize;
		}

		//Native buffer variable used in cleanup
		Builder.AppendLine(FString::Printf(TEXT("IntPtr %s_NativeBuffer = IntPtr.Add(%s, %s);"), *NativePropertyName, *SourceBuffer, *Offset));
		Builder.AppendLine(FString::Printf(TEXT("%s %s_Marshaler = new %s (1, %s, %s);"), *MarshalerType, *NativePropertyName, *MarshalerType, *Handler.ExportMarshalerDelegates(InnerProperty, NativePropertyName), *ElementSize));
	}
	Builder.AppendLine(FString::Printf(TEXT("%s %s_Marshaler.FromNative(%s_NativeBuffer, 0, null);"), *AssignmentOrReturn, *NativePropertyName, *NativePropertyName));

	if (bCleanupSourceBuffer)
	{
		// Ensure we're not generating unreachable cleanup code.
		check(AssignmentOrReturn != TEXT("return"));

		Builder.AppendLine(FString::Printf(TEXT("%s.DestructInstance(%s_NativeBuffer, 0);"), *MarshalerType, *NativePropertyName));
	}
}


FString FArrayPropertyHandler::GetWrapperInterface(const UProperty* Property) const
{
	const UArrayProperty& ArrayProperty = *CastChecked<UArrayProperty>(Property);
	const UProperty* InnerProperty = ArrayProperty.Inner;
	const FMonoPropertyHandler& Handler = PropertyHandlers.Find(InnerProperty);
	check(Handler.IsSupportedAsArrayInner());

	FString InnerCSharpType = Handler.GetCSharpType(InnerProperty);

	return FString::Printf(TEXT("System.Collections.Generic.%s<%s>"), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly) ? TEXT("IReadOnlyList") : TEXT("IList"), *InnerCSharpType);
}

FString FArrayPropertyHandler::GetWrapperType(const UProperty* Property) const
{
	const UArrayProperty& ArrayProperty = *CastChecked<UArrayProperty>(Property);
	const UProperty* InnerProperty = ArrayProperty.Inner;
	const FMonoPropertyHandler& Handler = PropertyHandlers.Find(InnerProperty);
	check(Handler.IsSupportedAsArrayInner());
	FString UnrealArrayType = (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly) ? TEXT("UnrealArrayReadOnlyMarshaler") : TEXT("UnrealArrayReadWriteMarshaler"));

	return FString::Printf(TEXT("%s<%s>"), *UnrealArrayType, *Handler.GetCSharpType(InnerProperty));
}

FString FArrayPropertyHandler::GetNullReturnCSharpValue(const UProperty* ReturnProperty) const
{
	return TEXT("null");
}

FString FArrayPropertyHandler::ExportInstanceMarshalerVariables(const UProperty *Property, const FString &NativePropertyName) const
{
	FString MarshalerType = (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly) ? TEXT("UnrealArrayReadOnlyMarshaler") : TEXT("UnrealArrayReadWriteMarshaler"));
	const UArrayProperty& ArrayProperty = *CastChecked<UArrayProperty>(Property);
	const UProperty* InnerProperty = ArrayProperty.Inner;
	const FMonoPropertyHandler& Handler = PropertyHandlers.Find(InnerProperty);
	FString InnerType = Handler.GetCSharpType(InnerProperty);
	return FString::Printf(TEXT("%s %s_Marshaler = %s(%s_Length, %s_NativeProperty, %s);"),*GetWrapperType(Property), *NativePropertyName, *GetWrapperType(Property), *NativePropertyName, *NativePropertyName, *Handler.ExportMarshalerDelegates(InnerProperty, NativePropertyName));
}

FString FArrayPropertyHandler::ExportMarshalerDelegates(const UProperty *Property, const FString &NativePropertyName) const
{
	checkNoEntry();
	return TEXT("");
}

//////////////////////////////////////////////////////////////////////////
// Struct property helpers
//////////////////////////////////////////////////////////////////////////

// Export the default value for a struct parameter as a local variable.
void ExportDefaultStructParameter(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UProperty* ParamProperty, const FMonoPropertyHandler& Handler)
{
	check(Handler.CanHandleProperty(ParamProperty));

	UStructProperty* StructProperty = CastChecked<UStructProperty>(ParamProperty);

	FString StructName = StructProperty->Struct->GetName();

	// UE only permits these structs for default params, see FHeaderParser::DefaultValueStringCppFormatToInnerFormat
	// all of them except Color consist only of floats, and Color consists only of ints
	// tbqh we could probably just hardcode them all like HeaderParser does
	bool isKnownStruct = StructName == TEXT("Vector")
		|| StructName == TEXT("Vector2D")
		|| StructName == TEXT("Rotator")
		|| StructName == TEXT("LinearColor")
		|| StructName == TEXT("Color");

	if (!isKnownStruct)
	{
		MONOUE_GENERATOR_ISSUE(Error, "Cannot export default initializer for struct '%s'", *StructName);
		return;
	}

	FString FieldInitializerList;
	if (CppDefaultValue.StartsWith(TEXT("(")) && CppDefaultValue.EndsWith(TEXT(")")))
	{
		FieldInitializerList = CppDefaultValue.Mid(1, CppDefaultValue.Len() - 2);
	}
	else
	{
		FieldInitializerList = CppDefaultValue;
	}

	TArray<FString> FieldInitializers;
	FString FieldInitializerSplit;
	while (FieldInitializerList.Split(TEXT(","), &FieldInitializerSplit, &FieldInitializerList))
	{
		FieldInitializers.Add(FieldInitializerSplit);
	}
	if (FieldInitializerList.Len())
	{
		FieldInitializers.Add(FieldInitializerList);
	}

	FString CSharpType = Handler.GetCSharpType(ParamProperty);
	Builder.AppendLine(FString::Printf(TEXT("%s %s = new %s"), *CSharpType, *VariableName, *CSharpType));
	Builder.AppendLine(TEXT("{"));
	Builder.Indent();

	bool isFloat = true;
	if (StructName == TEXT("Color"))
	{
		isFloat = false;

		check(FieldInitializers.Num()== 4);
		// RGBA -> BGRA
		FString tmp = FieldInitializers[0];
		FieldInitializers[0] = FieldInitializers[2];
		FieldInitializers[2] = tmp;
	}

	TFieldIterator<UProperty> StructPropIt(StructProperty->Struct);
	for (int i = 0; i < FieldInitializers.Num(); ++i, ++StructPropIt)
	{
		check(StructPropIt);
		UProperty* Prop = *StructPropIt;
		const FString& FieldInitializer = FieldInitializers[i];

		int32 Pos = FieldInitializer.Find(TEXT("="));
		if (Pos < 0)
		{
			Builder.AppendLine(isFloat
				? FString::Printf(TEXT("%s=%sf,"), *Prop->GetName(), *FieldInitializer)
				: FString::Printf(TEXT("%s=%s,"), *Prop->GetName(), *FieldInitializer));
		}
		else
		{
			check(Prop->GetName() == FieldInitializer.Left(Pos));
			Builder.AppendLine(isFloat
				? FString::Printf(TEXT("%sf,"), *FieldInitializer)
				: FString::Printf(TEXT("%s,"), *FieldInitializer));
		}
	}

	// We should have found a field initializer for every property.
	// UHT enforces this even if the ctor used to specify the C++ default relies on some default parameters, itself.
	check(!StructPropIt);

	Builder.Unindent();
	Builder.AppendLine(TEXT("};"));
}

//////////////////////////////////////////////////////////////////////////
// FMathTypePropertyHandler
//////////////////////////////////////////////////////////////////////////

bool FBlittableCustomStructTypePropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	const UStructProperty& StructProperty = *CastChecked<UStructProperty>(Property);
	return StructProperty.Struct->GetFName() == UnrealStructName;
}

void FBlittableCustomStructTypePropertyHandler::ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	ExportDefaultStructParameter(Builder, VariableName, CppDefaultValue, ParamProperty, *this);
}

//////////////////////////////////////////////////////////////////////////
// FBlittableStructPropertyHandler
//////////////////////////////////////////////////////////////////////////
bool FBlittableStructPropertyHandler::IsStructBlittable(const FSupportedPropertyTypes& PropertyHandlers, const UScriptStruct& Struct)
{
	// CPP info is created by the IMPLEMENT_STRUCT macro, which, unfortunately, isn't
	// mandatory for UStructs.  For now, assume unblittable in that case.
	UScriptStruct::ICppStructOps* CppStructOps = const_cast<UScriptStruct&>(Struct).GetCppStructOps();
	if (!CppStructOps)
	{
		return false;
	}

	int32 CppSize = CppStructOps->GetSize();
	int32 CalculatedPropertySize = 0;
	for (TFieldIterator<UProperty> PropIt(&Struct); PropIt; ++PropIt)
	{
		UProperty* StructProperty = *PropIt;
		if (StructProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)
			&& PropertyHandlers.Find(StructProperty).IsBlittable())
		{
			CalculatedPropertySize += StructProperty->ElementSize;
		}
		else
		{
			return false;
		}
	}

	check(CalculatedPropertySize <= CppSize);
	return (CalculatedPropertySize == CppSize);

}

bool FBlittableStructPropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	const UStructProperty* StructProperty = CastChecked<UStructProperty>(Property);
	check(StructProperty->Struct);
	const UScriptStruct& Struct = *StructProperty->Struct;

	return IsStructBlittable(PropertyHandlers, Struct);
}

FString FBlittableStructPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	const UStructProperty* StructProperty = CastChecked<UStructProperty>(Property);
	check(StructProperty->Struct);
	return GetScriptNameMapper().GetQualifiedName(*StructProperty->Struct);
}

void FBlittableStructPropertyHandler::AddReferences(const UProperty* Property, TSet<UStruct*>& References) const
{
	const UStructProperty* StructProperty = CastChecked<UStructProperty>(Property);
	References.Add(StructProperty->Struct);
}

void FBlittableStructPropertyHandler::ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	ExportDefaultStructParameter(Builder, VariableName, CppDefaultValue, ParamProperty, *this);
}


//////////////////////////////////////////////////////////////////////////
// FStructPropertyHandler
//////////////////////////////////////////////////////////////////////////

FString FStructPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	const UStructProperty* StructProperty = CastChecked<UStructProperty>(Property);
	check(StructProperty->Struct);
	return GetScriptNameMapper().GetQualifiedName(*StructProperty->Struct);
}

void FStructPropertyHandler::AddReferences(const UProperty* Property, TSet<UStruct*>& References) const
{
	const UStructProperty* StructProperty = CastChecked<UStructProperty>(Property);
	References.Add(StructProperty->Struct);
}

FString FStructPropertyHandler::GetMarshalerType(const UProperty *Property) const
{
	return FString::Printf(TEXT("%sMarshaler"), *GetCSharpType(Property));
}

void FStructPropertyHandler::ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	ExportDefaultStructParameter(Builder, VariableName, CppDefaultValue, ParamProperty, *this);
}

//////////////////////////////////////////////////////////////////////////
// FCustomStructTypePropertyHandler
//////////////////////////////////////////////////////////////////////////

bool FCustomStructTypePropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	const UStructProperty* StructProperty = CastChecked<UStructProperty>(Property);
	return StructProperty->Struct->GetFName() == UnrealStructName;
}

void FCustomStructTypePropertyHandler::AddReferences(const UProperty* Property, TSet<UStruct*>& References) const
{
	// Do nothing - we're just hiding the base class version, which would export a default version
	// of the property's struct.
}

FString FCustomStructTypePropertyHandler::GetMarshalerType(const UProperty *Property) const
{
	return FString::Printf(TEXT("%sMarshaler"), *GetCSharpType(Property));
}

void FCustomStructTypePropertyHandler::ExportCppDefaultParameterAsLocalVariable(FMonoTextBuilder& Builder, const FString& VariableName, const FString& CppDefaultValue, UFunction* Function, UProperty* ParamProperty) const
{
	ExportDefaultStructParameter(Builder, VariableName, CppDefaultValue, ParamProperty, *this);
}

//////////////////////////////////////////////////////////////////////////
// FSupportedPropertyTypes
//////////////////////////////////////////////////////////////////////////

FSupportedPropertyTypes::FSupportedPropertyTypes(const MonoScriptNameMapper& InNameMapper, class FInclusionLists& Blacklist)
	: NameMapper(InNameMapper)
{
	NullHandler.Reset(new FNullPropertyHandler(*this));

	AddBlittablePropertyHandler(UInt8Property::StaticClass(), TEXT("sbyte"));
	AddBlittablePropertyHandler(UInt16Property::StaticClass(), TEXT("short"));
	AddBlittablePropertyHandler(UIntProperty::StaticClass(), TEXT("int"));
	AddBlittablePropertyHandler(UInt64Property::StaticClass(), TEXT("long"));
	// Byte properties require special handling due to enums.
	AddBlittablePropertyHandler(UUInt16Property::StaticClass(), TEXT("ushort"));
	AddBlittablePropertyHandler(UUInt32Property::StaticClass(), TEXT("uint"));
	AddBlittablePropertyHandler(UUInt64Property::StaticClass(), TEXT("ulong"));
	AddPropertyHandler(UFloatProperty::StaticClass(), new FFloatPropertyHandler(*this));
	AddBlittablePropertyHandler(UDoubleProperty::StaticClass(), TEXT("double"));

	auto EnumPropertyHandler = new FEnumPropertyHandler(*this);
	AddPropertyHandler(UEnumProperty::StaticClass(), EnumPropertyHandler);

	AddPropertyHandler(UByteProperty::StaticClass(), EnumPropertyHandler);
	AddBlittablePropertyHandler(UByteProperty::StaticClass(), TEXT("byte"));

	AddPropertyHandler(UBoolProperty::StaticClass(), new FBitfieldPropertyHandler(*this));
	AddPropertyHandler(UBoolProperty::StaticClass(), new FBoolPropertyHandler(*this));

	AddPropertyHandler(UStrProperty::StaticClass(), new FStringPropertyHandler(*this));
	AddPropertyHandler(UNameProperty::StaticClass(), new FNamePropertyHandler(*this));
	AddPropertyHandler(UTextProperty::StaticClass(), new FTextPropertyHandler(*this));

	AddPropertyHandler(UWeakObjectProperty::StaticClass(), new FWeakObjectPropertyHandler(*this));
	AddPropertyHandler(UObjectProperty::StaticClass(), new FObjectPropertyHandler(*this));
	AddPropertyHandler(UClassProperty::StaticClass(), new FClassPropertyHandler(*this));

	AddPropertyHandler(UArrayProperty::StaticClass(), new FArrayPropertyHandler(*this));

	AddBlittableCustomStructPropertyHandler(TEXT("Vector2D"), TEXT("OpenTK.Vector2"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("Vector"), TEXT("OpenTK.Vector3"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("Vector_NetQuantize"), TEXT("OpenTK.Vector3"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("Vector_NetQuantize10"), TEXT("OpenTK.Vector3"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("Vector_NetQuantize100"), TEXT("OpenTK.Vector3"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("Vector_NetQuantizeNormal"), TEXT("OpenTK.Vector3"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("Vector4"), TEXT("OpenTK.Vector4"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("Quat"), TEXT("OpenTK.Quaternion"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("Matrix"), TEXT("OpenTK.Matrix4"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("Rotator"), MONO_BINDINGS_NAMESPACE TEXT(".Rotator"), Blacklist);
	AddBlittableCustomStructPropertyHandler(TEXT("RandomStream"), MONO_BINDINGS_NAMESPACE TEXT(".RandomStream"), Blacklist);

	AddCustomStructPropertyHandler(TEXT("Key"), MONO_BINDINGS_NAMESPACE TEXT(".Key"), Blacklist);

	// For structs without custom handlers, prefer the blittable handler when possible.
	// For non-POD structs or structs that don't expose all their properties to blueprint,
	// we'll have to use the generic handerl and manually marshal each property.
	AddPropertyHandler(UStructProperty::StaticClass(), new FBlittableStructPropertyHandler(*this));
	AddPropertyHandler(UStructProperty::StaticClass(), new FStructPropertyHandler(*this));

}

const FMonoPropertyHandler& FSupportedPropertyTypes::Find(const UProperty* Property) const
{
	check(Property);
	const TArray<FMonoPropertyHandler*>* Handlers = HandlerMap.Find(Property->GetClass()->GetFName());
	if (Handlers)
	{
		for (FMonoPropertyHandler* Handler : *Handlers)
		{
			check(Handler);
			if (Handler->CanHandleProperty(Property))
			{
				return *Handler;
			}
		}
	}

	return *NullHandler;
}

const FMonoPropertyHandler& FSupportedPropertyTypes::Find(UFunction* Function) const
{
	UProperty* ReturnProperty = Function->GetReturnProperty();
	if (ReturnProperty)
	{
		return Find(ReturnProperty);
	}
	else
	{
		// The NULL handler is suitable for UFunctions with no return, since it inherits all
		// the necessary infrastructure to export functions and its C# type is void.
		return *NullHandler;
	}
}

bool FSupportedPropertyTypes::IsStructBlittable(const UScriptStruct& ScriptStruct) const
{
	return FBlittableStructPropertyHandler::IsStructBlittable(*this, ScriptStruct);
}

void FSupportedPropertyTypes::AddPropertyHandler(UClass* PropertyClass, FMonoPropertyHandler* Handler)
{
	check(PropertyClass->IsChildOf(UProperty::StaticClass()));

	TArray<FMonoPropertyHandler*>& Handlers = HandlerMap.FindOrAdd(PropertyClass->GetFName());
	Handlers.Add(Handler);
}

void FSupportedPropertyTypes::AddBlittablePropertyHandler(UClass* PropertyClass, const TCHAR* CSharpType)
{
	AddPropertyHandler(PropertyClass, new FBlittableTypePropertyHandler(*this, PropertyClass, CSharpType));
}

void FSupportedPropertyTypes::AddBlittableCustomStructPropertyHandler(const TCHAR* UnrealName, const TCHAR* CSharpName, FInclusionLists& Blacklist)
{
	AddPropertyHandler(UStructProperty::StaticClass(), new FBlittableCustomStructTypePropertyHandler(*this, UnrealName, CSharpName));
	Blacklist.AddStruct(FName(UnrealName));
}

void FSupportedPropertyTypes::AddCustomStructPropertyHandler(const TCHAR* UnrealName, const TCHAR* CSharpName, FInclusionLists& Blacklist)
{
	AddPropertyHandler(UStructProperty::StaticClass(), new FCustomStructTypePropertyHandler(*this, UnrealName, CSharpName));
	Blacklist.AddStruct(FName(UnrealName));
}

bool FNullPropertyHandler::CanHandleProperty(const UProperty* Property) const
{
	return true;
}

FString FNullPropertyHandler::GetCSharpType(const UProperty* Property) const
{
	// In general, the NULL handler should be a no-op, but we need to return a useful value for function
	// return properties to ensure void method signatures are generated correctly.
	return TEXT("void");
}

FString FNullPropertyHandler::GetNullReturnCSharpValue(const UProperty* ReturnProperty) const
{
	checkNoEntry();
	return TEXT("");
}
