// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoHelpers.h"
#include "MonoRuntimeCommon.h"
#include "MonoDomain.h"
#include "MonoBindings.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Logging/MessageLog.h"
#include "Async/TaskGraphInterfaces.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <combaseapi.h>
#endif

#include <mono/metadata/debug-helpers.h>

#define LOCTEXT_NAMESPACE "MonoRuntime"

namespace Mono
{
	MonoReflectionType* GetReflectionTypeFromClass(const FMonoDomain& Domain, MonoClass* Class)
	{
		check(Class);
		MonoType* ClassType = mono_class_get_type(Class);
		check(ClassType);
		return mono_type_get_object(Domain.GetDomain(), ClassType);
	}

	MonoClass* GetClassFromReflectionType(MonoReflectionType* ReflectionType)
	{
		check(ReflectionType);
		MonoType* ClassType = mono_reflection_type_get_type(ReflectionType);
		check(ClassType);
		MonoClass* Class = mono_class_from_mono_type(ClassType);
		return Class;
	}

	MonoMethod* LookupMethod(MonoImage* AssemblyImage, const ANSICHAR* FullyQualifiedMethodName)
	{
		MonoMethodDesc* MethodDesc = mono_method_desc_new(FullyQualifiedMethodName, true);
		check(MethodDesc);
		MonoMethod* Method = mono_method_desc_search_in_image(MethodDesc, AssemblyImage);
		mono_method_desc_free(MethodDesc);

		return Method;
	}

	MonoMethod* LookupMethodOnClass(MonoClass* Class, const ANSICHAR* MethodName )
	{
		MonoMethodDesc* MethodDesc = mono_method_desc_new(MethodName, false);
		check(MethodDesc);
		MonoMethod* Method = mono_method_desc_search_in_class(MethodDesc, Class);
		mono_method_desc_free(MethodDesc);

		return Method;
	}

	MonoProperty* LookupPropertyOnClass(MonoClass* Class, const ANSICHAR* PropertyName)
	{
		return mono_class_get_property_from_name(Class, PropertyName);
	}

	bool IsValidArrayType(MonoType* typ, const ANSICHAR* InnerTypeName, bool bAllowAnyType)
	{
		MonoArrayType* ArrayType = mono_type_get_array_type(typ);
		if (nullptr == ArrayType)
		{
			return false;
		}

		if (bAllowAnyType)
		{
			return true;
		}

		FString InnerName(InnerTypeName);
		FString ArrayName(InnerName + TEXT("[]"));

		return 0 == FCStringAnsi::Strcmp(mono_type_get_name(typ), TCHAR_TO_ANSI(*ArrayName));
	}

	MonoString* Marshal<FString, void>::Parameter(const FMonoDomain& Domain, const FString& InString)
	{
		return FStringToMonoString(Domain.GetDomain(), InString);
	}

	void MonoStringToFString(FString& Result, MonoString* InString)
	{
		int32 StringLength = mono_string_length(InString);
		Result.Empty(StringLength+1);
#if PLATFORM_TCHAR_IS_4_BYTES
		mono_unichar4* StringResult = mono_string_to_utf32(InString);
		Result = reinterpret_cast<TCHAR *>(StringResult);
		mono_free(StringResult);
#else
		Result.AppendChars((const TCHAR*)mono_string_chars(InString), StringLength);
#endif
	}

	FName MonoStringToFName(MonoString* InString)
	{
		FString TempString;
		MonoStringToFString(TempString, InString);
		return FName(*TempString);
	}

	MonoString* FStringToMonoString(MonoDomain* InDomain, const FString& InString)
	{
#if PLATFORM_TCHAR_IS_4_BYTES
		return mono_string_new_utf32(InDomain, reinterpret_cast<const mono_unichar4*>(*InString), InString.Len());
#else
		return mono_string_new_utf16(InDomain, reinterpret_cast<const mono_unichar2*>(*InString), InString.Len());
#endif
	}

	MonoString* FNameToMonoString(MonoDomain* InDomain, FName InName)
	{
		return FStringToMonoString(InDomain, InName.ToString());
	}

	FString Marshal<FString, void>::ReturnValue(const FMonoDomain& , MonoObject* Object)
	{
		FString Result;
		MonoStringToFString(Result, Object);
		return Result;
	}

	MonoArray* Marshal<TArray<FString>, void>::Parameter(const FMonoDomain& Domain, const TArray<FString>& InArray)
	{
		MonoArray* OutArray = mono_array_new(Domain.GetDomain(), mono_get_string_class(), InArray.Num());
		for (int i = 0; i < InArray.Num(); ++i)
		{
			MonoString* MarshalledString = Marshal<FString, void>::Parameter(Domain, InArray[i]);
			mono_array_setref(OutArray, i, MarshalledString);
		}
		return OutArray;
	}

	MonoArray* Marshal<TArray<FName>>::Parameter(const FMonoBindings& Bindings, const TArray<FName>& InArray)
	{
		MonoArray* OutArray = mono_array_new(Bindings.GetDomain(), Bindings.GetNameClass(), InArray.Num());
		for (int i = 0; i < InArray.Num(); ++i)
		{
			mono_array_set(OutArray, FName, i, InArray[i]);
		}
		return OutArray;
	}

	TArray<FName> Marshal<TArray<FName>, void>::ReturnValue(const FMonoBindings& Bindings, MonoObject* Object)
	{
		TArray<FName> Ret;
		MonoValueArrayToTArray(Ret, Object);
		return Ret;
	}

	MonoArray* Marshal<TArray<FLifetimeProperty>>::Parameter(const FMonoBindings& Bindings, const TArray<FLifetimeProperty>& InArray)
	{
		MonoArray* OutArray = mono_array_new(Bindings.GetDomain(), Bindings.GetLifetimeReplicatedPropertyClass(), InArray.Num());
		for (int i = 0; i < InArray.Num(); ++i)
		{
			mono_array_set(OutArray, FLifetimeProperty, i, InArray[i]);
		}
		return OutArray;
	}

	TArray<FLifetimeProperty> Marshal<TArray<FLifetimeProperty>, void>::ReturnValue(const FMonoBindings& Bindings, MonoObject* Object)
	{
		TArray<FLifetimeProperty> Ret;
		MonoValueArrayToTArray(Ret, Object);
		return Ret;
	}

	static void SendErrorToMessageLog(FText InError)
	{
		FMessageLog(NAME_MonoErrors).Error(InError);
		FMonoBindings::Get().OnExceptionSentToMessageLog();
	}

	static void LogExceptionToMessageLog(MonoObject* Exception)
	{
		FText ExceptionError;
		MonoObject* ExceptionInStringConversion = nullptr;
		MonoString* MonoExceptionString = mono_object_to_string(Exception, &ExceptionInStringConversion);
		if (nullptr != MonoExceptionString)
		{
			FString ExceptionString;
			MonoStringToFString(ExceptionString, (MonoObject*) MonoExceptionString);
			FFormatNamedArguments Args;
			Args.Add(TEXT("ExceptionMessage"), FText::FromString(ExceptionString));
			ExceptionError = FText::Format(LOCTEXT("ExceptionError", "Managed exception: {ExceptionMessage}"), Args);
		}
		else
		{
			check(ExceptionInStringConversion);
			// Can't really get much out of the original exception with the public API, so just note that two exceptions were thrown
			FString ExceptionString;
			MonoExceptionString = mono_object_to_string(ExceptionInStringConversion, nullptr);
			check(MonoExceptionString);
			MonoStringToFString(ExceptionString, (MonoObject*) MonoExceptionString);
			FFormatNamedArguments Args;
			MonoClass* ExceptionClass = mono_object_get_class(Exception);
			Args.Add(TEXT("OriginalExceptionType"), FText::FromString(mono_class_get_name(ExceptionClass)));
			Args.Add(TEXT("NestedExceptionMessage"), FText::FromString(ExceptionString));
			ExceptionError = FText::Format(LOCTEXT("NestedExceptionError", "Nested exception! Original exception was of type '{OriginalExceptionType}'. Nested Exception: {NestedExceptionMessage}"), Args);
		}

		if (IsInGameThread())
		{
			SendErrorToMessageLog(ExceptionError);
		}
		else
		{
			// dispatch to game thread
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(SendErrorToMessageLog, ExceptionError)
				, NULL
				, NULL
				, ENamedThreads::GameThread
				);
		}
	}

	MonoObject* Invoke(bool& bThrewException, InvokeExceptionBehavior ExceptionBehavior, MonoDomain* Domain, MonoMethod* Method, MonoObject* Object, void** Arguments)
	{
		check(Method);
		// mono_runtime_invoke doesn't handle invoking on boxed value types, so make sure we're not doing that
		checkSlow(Object == nullptr || !mono_class_is_valuetype(mono_object_get_class(Object)));
#if MONO_WITH_HOT_RELOADING
		mono_domain_set(Domain,false);
#endif // MONO_WITH_HOT_RELOADING
		MonoObject* Exception = nullptr;
		MonoObject* ReturnValue = mono_runtime_invoke(Method, Object, Arguments, &Exception);

		bThrewException = nullptr != Exception;
		if (nullptr == Exception)
		{
			return ReturnValue;
		}
		else
		{
			if (ExceptionBehavior == InvokeExceptionBehavior::OutputToMessageLog)
			{
				LogExceptionToMessageLog(Exception);
			}
			else
			{
				check(ExceptionBehavior == InvokeExceptionBehavior::OutputToLog);
				mono_print_unhandled_exception(Exception);
			}
			return nullptr;
		}
	}

	MonoObject* InvokeDelegate(bool& bThrewException, InvokeExceptionBehavior ExceptionBehavior, MonoDomain* Domain, MonoObject* Delegate, void** Arguments)
	{
		check(Delegate);
#if MONO_WITH_HOT_RELOADING
		mono_domain_set(Domain, false);
#endif // MONO_WITH_HOT_RELOADING
		MonoObject* Exception = nullptr;
		MonoObject* ReturnValue = mono_runtime_delegate_invoke(Delegate, Arguments, &Exception);

		bThrewException = nullptr != Exception;
		if (nullptr == Exception)
		{
			return ReturnValue;
		}
		else
		{
			if (ExceptionBehavior == InvokeExceptionBehavior::OutputToMessageLog)
			{
				LogExceptionToMessageLog(Exception);
			}
			else
			{
				check(ExceptionBehavior == InvokeExceptionBehavior::OutputToLog);
				mono_print_unhandled_exception(Exception);
			}
			return nullptr;
		}
	}

	MonoObject* ConstructObject(const FMonoDomain& Domain, MonoClass* Class)
	{
		check(Class);
		// TODO: capture and log exception
		MonoObject* Object = mono_object_new(Domain.GetDomain(), Class);
		MonoMethod *ConstructorMethod = mono_class_get_method_from_name(Class, ".ctor", 0);
		check(ConstructorMethod);
		
		bool bThrewException = false;
		Invoke(bThrewException, Domain.GetExceptionBehavior(), Domain.GetDomain(), ConstructorMethod, Object, nullptr);

		if (!bThrewException)
		{
			return Object;
		}
		else
		{
			return nullptr;
		}
	}

#if PLATFORM_WINDOWS
	void* CoTaskMemAlloc(int32 Bytes)
	{
		return ::CoTaskMemAlloc(Bytes);
	}

	void* CoTaskMemRealloc(void* Ptr, int32 Bytes)
	{
		return ::CoTaskMemRealloc(Ptr, Bytes);
	}

	void CoTaskMemFree(void* Ptr)
	{
		::CoTaskMemFree(Ptr);
	}
#else
	void* CoTaskMemAlloc(int32 Bytes)
	{
		return malloc(Bytes);
	}

	void* CoTaskMemRealloc(void* Ptr, int32 Bytes)
	{
		return realloc(Ptr, Bytes);
	}

	void CoTaskMemFree(void* Ptr)
	{
		free(Ptr);
	}
#endif

#if MONO_IS_DYNAMIC_LIB
	static void* MonoDLLHandle = nullptr;

	void LoadMonoDLL()
	{
#if PLATFORM_WINDOWS
		FString LibName = TEXT("mono-2.0-sgen.dll");
#elif PLATFORM_MAC
		FString LibName = TEXT("libmonosgen-2.0.dylib");
#else
#error Platform not supported
#endif

		//the library will be copied here for staged builds
		FString LibPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Mono") / FPlatformProcess::GetBinariesSubdirectory() / LibName;
		MonoDLLHandle = FPlatformProcess::GetDllHandle(*LibPath);

		IFileManager& FileManager = IFileManager::Get();
		LibPath = FileManager.ConvertToAbsolutePathForExternalAppForRead(*LibPath);

#if !UE_BUILD_SHIPPING
		if (nullptr == MonoDLLHandle)
		{
			//try to load libmono from the original dev-time location
			FString DevLibPath = FPaths::EnginePluginsDir() / TEXT("MonoUE/ThirdParty/mono/lib") / FPlatformProcess::GetBinariesSubdirectory() / LibName;
			MonoDLLHandle = FPlatformProcess::GetDllHandle(*DevLibPath);
		}
#endif

		check(nullptr != MonoDLLHandle);
	}

	void UnloadMonoDLL()
	{
		if (nullptr != MonoDLLHandle)
		{
			FPlatformProcess::FreeDllHandle(MonoDLLHandle);
			MonoDLLHandle = nullptr;
		}
	}
#endif //MONO_IS_DYNAMIC_LIB
}

#undef LOCTEXT_NAMESPACE