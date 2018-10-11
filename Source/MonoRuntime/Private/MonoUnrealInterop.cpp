// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoRuntimeCommon.h"
#include "MonoRuntimePrivate.h"
#include "IMonoRuntime.h"
#include "MonoHelpers.h"
#include "PInvokeSignatures.h"

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectMacros.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/Object.h"

#include <mono/metadata/object.h>

MONO_PINVOKE_FUNCTION(void) Bindings_OnUnhandledExceptionNative(const UTF16CHAR* InMessage, const UTF16CHAR* InStackTrace)
{
	UE_LOG(LogMono, Fatal, TEXT("Unhandled managed exception: '%s' Stack trace: %s"), StringCast<TCHAR>(InMessage).Get(), StringCast<TCHAR>(InStackTrace).Get());
}

// Property exposure
MONO_PINVOKE_FUNCTION(UClass*) UnrealInterop_GetNativeClassFromName(const UTF16CHAR* InClassName)
{
    FString ClassName(StringCast<TCHAR>(InClassName).Get());
	return FindObject<UClass>(ANY_PACKAGE, StringCast<TCHAR>(InClassName).Get(), true);
}

MONO_PINVOKE_FUNCTION(UStruct*) UnrealInterop_GetNativeStructFromName(const UTF16CHAR* InStructName)
{
	return FindObject<UScriptStruct>(ANY_PACKAGE, StringCast<TCHAR>(InStructName).Get(), true);
}

MONO_PINVOKE_FUNCTION(int32) UnrealInterop_GetNativeStructSize(UScriptStruct* ScriptStruct)
{
	check(ScriptStruct);
	if (ScriptStruct->GetCppStructOps())
	{
		return ScriptStruct->GetCppStructOps()->GetSize();
	}
	else
	{
		return ScriptStruct->GetStructureSize();
	}
}

MONO_PINVOKE_FUNCTION(int) UnrealInterop_GetPropertyOffsetFromName(UStruct* InStruct, const UTF16CHAR* InPropertyName)
{
	check(InStruct);

	UProperty* InProperty = FindField<UProperty>(InStruct, StringCast<TCHAR>(InPropertyName).Get());
	check(InProperty);

	// we need a dummy address, it's not actually accessed just used for pointer math
	uint8 DummyContainer;
	uint8* DummyPointer = &DummyContainer;
	uint8* OffsetPointer = InProperty->ContainerPtrToValuePtr<uint8>(DummyPointer);

	return OffsetPointer - DummyPointer;
}

MONO_PINVOKE_FUNCTION(UProperty*) UnrealInterop_GetNativePropertyFromName(UStruct* Struct, const UTF16CHAR* PropertyName)
{
	check(Struct);
	UProperty* Property = FindField<UProperty>(Struct, StringCast<TCHAR>(PropertyName).Get());
	check(Property);

	return Property;
}

MONO_PINVOKE_FUNCTION(uint16) UnrealInterop_GetPropertyRepIndexFromName(UStruct* Struct, const UTF16CHAR* PropertyName)
{
	check(Struct);
	UProperty* Property = FindField<UProperty>(Struct, StringCast<TCHAR>(PropertyName).Get());
	check(Property);
	check(Property->HasAllPropertyFlags(CPF_Net));

	return Property->RepIndex;
}

MONO_PINVOKE_FUNCTION(int32) UnrealInterop_GetArrayElementSize(UStruct* Struct, const UTF16CHAR* PropertyName)
{
	check(Struct);
	UProperty* Property = FindField<UProperty>(Struct, StringCast<TCHAR>(PropertyName).Get());
	UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(Property);
	UProperty* InnerProperty = ArrayProperty->Inner;
	check(InnerProperty);
	return InnerProperty->GetSize();
}

MONO_PINVOKE_FUNCTION(int32) UnrealInterop_GetPropertyArrayDimFromName(UStruct* InStruct, const UTF16CHAR* InPropertyName)
{
	check(InStruct);

	UProperty* InProperty = FindField<UProperty>(InStruct, StringCast<TCHAR>(InPropertyName).Get());
	check(InProperty);

	return InProperty->ArrayDim;
}

MONO_PINVOKE_FUNCTION(bool) UnrealInterop_GetBitfieldValueFromProperty(uint8* NativeBuffer, UProperty* Property, int32 Offset)
{
	// NativeBuffer won't necessarily correspond to a UObject.  It might be the beginning of a native struct, for example.
	check(NativeBuffer);
	uint8* OffsetPointer = NativeBuffer + Offset;
	check(OffsetPointer == Property->ContainerPtrToValuePtr<uint8>(NativeBuffer));
	UBoolProperty* BoolProperty = CastChecked<UBoolProperty>(Property);
	return BoolProperty->GetPropertyValue(OffsetPointer);
}

MONO_PINVOKE_FUNCTION(void) UnrealInterop_SetBitfieldValueForProperty(uint8* NativeObject, UProperty* Property, int32 Offset, bool Value)
{
	// NativeBuffer won't necessarily correspond to a UObject.  It might be the beginning of a native struct, for example.
	check(NativeObject);
	uint8* OffsetPointer = NativeObject + Offset;
	check(OffsetPointer == Property->ContainerPtrToValuePtr<uint8>(NativeObject));
	UBoolProperty* BoolProperty = CastChecked<UBoolProperty>(Property);
	BoolProperty->SetPropertyValue(OffsetPointer, Value);
}

MONO_PINVOKE_FUNCTION(void) UnrealInterop_SetStringValueForProperty(UObject* NativeObject, UProperty* Property, int32 Offset, const UTF16CHAR* Value)
{
	check(NativeObject);
	uint8* OffsetPointer = reinterpret_cast<uint8*>(NativeObject)+Offset;
	check(OffsetPointer == Property->ContainerPtrToValuePtr<uint8>(NativeObject));

	UStrProperty* StringProperty = CastChecked<UStrProperty>(Property);
	StringProperty->SetPropertyValue(OffsetPointer, Value? StringCast<TCHAR>(Value).Get() : FString());
}

MONO_PINVOKE_FUNCTION(void) UnrealInterop_SetStringValue(FString *NativeString, const UTF16CHAR* Value)
{
	check(NativeString);
	(*NativeString) = Value? StringCast<TCHAR>(Value).Get() : FString();
}

MonoString* UnrealInterop_MarshalIntPtrAsString(TCHAR* InString)
{
#if PLATFORM_TCHAR_IS_4_BYTES
    return mono_string_from_utf32(reinterpret_cast<mono_unichar4*>(InString));
#else
    return mono_string_from_utf16(reinterpret_cast<mono_unichar2*>(InString));
#endif
}

void UnrealInterop_MarshalToUnrealString(MonoString* InString, FMarshalledScriptArray* OutArray)
{
#if PLATFORM_TCHAR_IS_4_BYTES
    OutArray->Data = mono_string_to_utf32(InString); // NOTE: mono_string_to_utf32 uses the same allocation handling as CoTaskMemAlloc so it's safe to pair with Marshal.FreeCoTaskMem
    OutArray->ArrayNum = FCString::Strlen(reinterpret_cast<TCHAR*>(OutArray->Data)) + 1;
    OutArray->ArrayMax = OutArray->ArrayNum;
#else
	// include null terminator
    OutArray->ArrayNum = mono_string_length(InString)+1;
    OutArray->Data = Mono::CoTaskMemAlloc(OutArray->ArrayNum * sizeof(TCHAR));
    FMemory::Memcpy(OutArray->Data, mono_string_chars(InString), OutArray->ArrayNum * sizeof(TCHAR));
    OutArray->ArrayMax = OutArray->ArrayNum;
#endif
}

MONO_PINVOKE_FUNCTION(void) UnrealInterop_RPC_ResetLastFailedReason()
{
	RPC_ResetLastFailedReason();
}

MONO_PINVOKE_FUNCTION(void) UnrealInterop_RPC_ValidateFailed(const UTF16CHAR* Reason)
{
	static FString ManagedLastFailedReason;
	ManagedLastFailedReason = StringCast<TCHAR>(Reason).Get();
	RPC_ValidateFailed(*ManagedLastFailedReason);
}

//FIXME this is broken on Mac
MONO_PINVOKE_FUNCTION(const TCHAR*) UnrealInterop_RPC_GetLastFailedReason()
{
	return RPC_GetLastFailedReason();
}

MONO_PINVOKE_FUNCTION(int32) UnrealInterop_RandHelper(int32 Max)
{
	return FMath::RandHelper(Max);
}