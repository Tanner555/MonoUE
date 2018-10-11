// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/CoreNet.h"
#include "UObject/Object.h"
#include "Templates/EnableIf.h"
#include "Templates/UnrealTypeTraits.h"
#include "Components/InputComponent.h"

#include "MonoRuntimePrivate.h"
#include "MonoDomain.h"
#include "MonoHelpersShared.h"

#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>

class FMonoDomain;
class FMonoBindings;

extern "C"
{
	// our template helpers can't deal with undefined types, so define dummy versions of these
	struct _MonoReflectionType
	{
		// LLVM doesn't like empty structs
		int dummy;
	};

	struct _MonoReflectionAssembly
	{
		int dummy;
	};

	struct _MonoAppDomain
	{
		int dummy;
	};
}

//these are mirrored in C#
extern "C"
{
	struct FMarshalledScriptArray
	{
		void* Data;
		int32 ArrayNum;
		int32 ArrayMax;
	};

	struct FMarshaledSharedPtr
	{
		void *ObjectPtr;
		SharedPointerInternals::FSharedReferencer<ESPMode::ThreadSafe> ReferenceController;
	};

	struct FMarshaledText
	{
		FMarshaledSharedPtr Data;
		uint32 Flags;
	};

	struct FMarshaledWeakObjectPtr
	{
		int32 ObjectIndex;
		int32 ObjectSerialNumber;
	};

	// Use a wrapper to return FNames, since FName itself has C++ linkage and will cause compiler warnings
	// if returned from an extern "C" API.
	// TODO: can we use FScriptName here?
	struct FMarshalledName
	{
#if WITH_CASE_PRESERVING_NAME
		NAME_INDEX DisplayIndex;
#endif
		NAME_INDEX ComparisonIndex;
		int32 Number;
	};

	static_assert(sizeof(FScriptArray) == sizeof(FMarshalledScriptArray), "FMarshalledScriptArray must be the same size as FScriptArray");
	static_assert(sizeof(FString) == sizeof(FScriptArray), "FString must be same size as FScriptArray");
	static_assert(sizeof(FText) == sizeof(FMarshaledText), "FText must be the same size as FMarshaledText. Has FText's implementation changed?");
	static_assert(sizeof(FWeakObjectPtr) == sizeof(FMarshaledWeakObjectPtr), "FWeakObjectPtr must be the same size as FMarshaledWeakObjectPtr. Has FWeakObjectPtr's implementation changed?");
	static_assert(sizeof(FMarshaledSharedPtr) == sizeof(TSharedRef<FString>), "FMarshaledSharedPtr must be the same size as TSharedRef. Has TSharedRef's implementation changed?");
	static_assert(sizeof(FMarshalledName) == sizeof(FName), "FMarshalledName must be the same size as FName. Has FName's implementation changed?");
}


namespace Mono
{
	// Reflection helpers
	MonoReflectionType* GetReflectionTypeFromClass(const FMonoDomain& Domain, MonoClass* Class);
	MonoClass* GetClassFromReflectionType(MonoReflectionType* ReflectionType);

	// Method lookup
	MonoMethod* LookupMethod(MonoImage* AssemblyImage, const ANSICHAR* FullyQualifiedMethodName);
	MONORUNTIME_API MonoMethod* LookupMethodOnClass(MonoClass* Class, const ANSICHAR* MethodName);

	// Property lookup
	MonoProperty* LookupPropertyOnClass(MonoClass* Class, const ANSICHAR* PropertyName);

	// String conversion
	void MonoStringToFString(FString& Result, MonoString* InString);
	inline void MonoStringToFString(FString& Result, MonoObject* InObject)
	{
		check(mono_object_get_class(InObject) == mono_get_string_class());
		MonoStringToFString(Result, (MonoString*) InObject);
	}

	FName MonoStringToFName(MonoString* InString);

	MonoString* FStringToMonoString(MonoDomain* InDomain, const FString& InString);
	MonoString* FNameToMonoString(MonoDomain* InDomain, FName InName);

	// type verification helpers
	bool IsValidArrayType(MonoType* typ, const ANSICHAR* InnerTypeName, bool bAllowAnyType);

	template <class T> 
	void MonoValueArrayToTArray(TArray<T>& Ret, MonoArray* ReturnArray)
	{
		if (nullptr != ReturnArray)
		{
			uintptr_t ArrayLength = mono_array_length(ReturnArray);

			Ret.Empty(ArrayLength);
			Ret.AddZeroed(ArrayLength);

			for (uintptr_t i = 0; i < ArrayLength; ++i)
			{
				Ret[i] = mono_array_get(ReturnArray, T, i);
			}
		}
	}

	template <class T>
	void MonoValueArrayToTArray(TArray<T>& Ret, MonoObject* ReturnArray)
	{
		MonoValueArrayToTArray(Ret, (MonoArray*) ReturnArray);
	}

	// Helpers so we can use Epic's TEnableIf. Replace with Epic versions if they ever make them
	struct TrueType
	{
		static const bool Value = true;
	};

	struct FalseType
	{
		static const bool Value = false;
	};

	// Pass through types
	template <class T>
	struct PassThroughType : public FalseType
	{
	};

#define DECLARE_MONO_PASSTHROUGH_TYPE(X,CLASSNAME,ALLOWANYTYPE) \
	template <> \
	struct PassThroughType<X> : public TrueType \
	{ \
		static inline bool AllowAnyType() \
		{ \
			return ALLOWANYTYPE; \
		} \
		static inline const ANSICHAR* GetMonoTypeName() \
		{ \
			return CLASSNAME; \
		} \
	}

	DECLARE_MONO_PASSTHROUGH_TYPE(MonoObject*, "System.Object", true);
	DECLARE_MONO_PASSTHROUGH_TYPE(MonoReflectionType*, "System.Type", false);
	DECLARE_MONO_PASSTHROUGH_TYPE(MonoReflectionAssembly*, "System.Reflection.Assembly", false);
	DECLARE_MONO_PASSTHROUGH_TYPE(MonoAppDomain*, "System.AppDomain", false);

#undef DECLARE_MONO_PASSTHROUGH_TYPE

	// Value types
	template <class T>
	struct ValueType : public FalseType
	{
		typedef void Type;
	};

#define DECLARE_MONO_VALUE_TYPE(X, MonoTypeFunc, IsPointerTypeValue) \
	template <> \
	struct ValueType<X> : public TrueType \
		{ \
		typedef X Type; \
		\
		static inline MonoClass* GetMonoType() \
			{ \
			return MonoTypeFunc(); \
			} \
		static inline const ANSICHAR* GetMonoTypeName() \
			{ \
			return mono_type_get_name(mono_class_get_type(GetMonoType())); \
			} \
		static inline bool IsPointerType() \
			{ \
			return IsPointerTypeValue; \
		} \
	}

	DECLARE_MONO_VALUE_TYPE(bool, mono_get_boolean_class, false);
	DECLARE_MONO_VALUE_TYPE(uint8, mono_get_byte_class, false);
	DECLARE_MONO_VALUE_TYPE(float, mono_get_single_class, false);
	DECLARE_MONO_VALUE_TYPE(int32, mono_get_int32_class, PLATFORM_32BITS);
	DECLARE_MONO_VALUE_TYPE(int64, mono_get_int64_class, PLATFORM_64BITS);

#undef DECLARE_MONO_VALUE_TYPE

	// Struct value types (no array support yet)
	// Value types
	template <class T>
	struct StructValueType : public FalseType
	{
		typedef void Type;
	};

#define DECLARE_MONO_STRUCT_VALUE_TYPE(X, QualifiedTypeName) \
	template <> \
	struct StructValueType<X> : public TrueType \
				{ \
		typedef X Type; \
		\
		static inline const ANSICHAR* GetMonoTypeName() \
					{ \
			return QualifiedTypeName; \
					} \
			}

	DECLARE_MONO_STRUCT_VALUE_TYPE(FVector, "OpenTK.Vector3");

#undef DECLARE_MONO_STRUCT_VALUE_TYPE

	template <class T, class Enable = void>
	struct Marshal
	{

	};

	// void
	template <>
	struct Marshal<void, void>
	{
		static inline void ReturnValue(const FMonoDomain&, MonoObject*)
		{

		}

		static inline bool IsValidReturnType(MonoType*)
		{
			// void is a special case, can be used when throwing away return value, so accept all types
			return true;
		}
	};

	// built-in types (do not box)
	template <class T>
	struct Marshal<T, typename TEnableIf<ValueType<T>::Value>::Type>
	{
		static inline T* Parameter(const FMonoDomain& , const T& Value)
		{
			// this is safe because we use perfect forwarding in the Invoke wrappers
			// and no temporaries are created (but if I can figure out a way to do it in the wrapper I would just to be extra safe)
			return const_cast<T*>(&Value);
		}

		static inline bool IsValidParameterType(MonoType* typ)
		{
			return 0 == FCStringAnsi::Strcmp(ValueType<T>::GetMonoTypeName(), mono_type_get_name(typ))
				|| (ValueType<T>::IsPointerType() && 0 == FCStringAnsi::Strcmp("System.IntPtr", mono_type_get_name(typ)));
		}
	};

	template<class T>
	struct Marshal<TArray<T>, typename TEnableIf<ValueType<T>::Value>::Type>
	{
		static MonoArray* Parameter(const FMonoDomain& Domain, const TArray<T>& InArray);
		
		static inline bool IsValidParameterType(MonoType* typ)
		{
			return IsValidArrayType(typ, ValueType<T>::GetMonoTypeName(), false);
		}
	};

	// struct types
	template <class T>
	struct Marshal < T, typename TEnableIf<StructValueType<T>::Value>::Type >
	{
		static inline T* Parameter(const FMonoDomain&, const T& Value)
		{
			// this is safe because we use perfect forwarding in the Invoke wrappers
			// and no temporaries are created (but if I can figure out a way to do it in the wrapper I would just to be extra safe)
			return const_cast<T*>(&Value);
		}

		static inline bool IsValidParameterType(MonoType* typ)
		{
			return 0 == FCStringAnsi::Strcmp(StructValueType<T>::GetMonoTypeName(), mono_type_get_name(typ));
		}
	};

	// enum types
	template <class T>
	struct Marshal < T, typename TEnableIf<TIsEnum<T>::Value>::Type >
	{
		static inline T* Parameter(const FMonoDomain&, const T& Value)
		{
			// this is safe because we use perfect forwarding in the Invoke wrappers
			// and no temporaries are created (but if I can figure out a way to do it in the wrapper I would just to be extra safe)
			return const_cast<T*>(&Value);
		}

		static inline bool IsValidParameterType(MonoType* typ)
		{
			return mono_class_is_enum(mono_type_get_class(typ))?true:false;
		}
	};


	// pass-through types
	template<class T>
	struct Marshal<T, typename TEnableIf<PassThroughType<T>::Value>::Type>
	{
		static inline T Parameter(const FMonoDomain&, T Object)
		{
			return Object;
		}
		static inline T ReturnValue(const FMonoDomain&, MonoObject* Object)
		{
			return (T) Object;
		}

		static inline bool IsValidParameterType(MonoType* typ)
		{
			const ANSICHAR* PassThroughTypeName = PassThroughType<T>::GetMonoTypeName();
			return PassThroughType<T>::AllowAnyType() 
				|| 0 == FCStringAnsi::Strcmp(mono_type_get_name(typ), PassThroughTypeName);
		}

		static inline bool IsValidReturnType(MonoType* typ)
		{
			return PassThroughType<T>::AllowAnyType() || 0 == FCStringAnsi::Strcmp(mono_type_get_name(typ), PassThroughType<T>::GetMonoTypeName());
		}
	};

	template<class T>
	struct Marshal<TArray<T>, typename TEnableIf<PassThroughType<T>::Value>::Type>
	{
		static TArray<T> ReturnValue(const FMonoDomain& , MonoObject* Object)
		{
			TArray<T> Ret;
			MonoValueArrayToTArray(Ret, Object);
			return Ret;
		}

		static inline bool IsValidReturnType(MonoType* typ)
		{
			return IsValidArrayType(typ, PassThroughType<T>::GetMonoTypeName(), PassThroughType<T>::AllowAnyType());
		}
	};


	// boxed types
	template<>
	struct Marshal<FString, void>
	{
		static MonoString* Parameter(const FMonoDomain& Domain, const FString& InString);
		static FString ReturnValue(const FMonoDomain& Domain, MonoObject* Object);

		static inline bool IsValidParameterType(MonoType* typ)
		{
			return 0 == FCStringAnsi::Strcmp(mono_type_get_name(typ), "System.String");
		}
		static inline bool IsValidReturnType(MonoType* typ)
		{
			return 0 == FCStringAnsi::Strcmp(mono_type_get_name(typ), "System.String");
		}
	};

	template<class T>
	struct Marshal<T*, typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived>::Type>
	{
		static MonoObject* Parameter(const FMonoBindings& Bindings, UObject* Object);

		static inline bool IsValidParameterType(MonoType* typ)
		{
			// TODO: implement me
			return true;
		}
	};

	template<class T>
	struct Marshal<TArray<T*>, typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived>::Type>
	{
		static MonoArray* Parameter(const FMonoBindings& Bindings, const TArray<T*>& InArray);
	};

	template<>
	struct Marshal<TArray<FString>, void>
	{
		static MonoArray* Parameter(const FMonoDomain& Domain, const TArray<FString>& InArray);

		static inline bool IsValidParameterType(MonoType* typ)
		{
			return IsValidArrayType(typ, "System.String", false);
		}
	};

	template<>
	struct Marshal<TArray<FName>, void>
	{
		static MonoArray* Parameter(const FMonoBindings& Bindings, const TArray<FName>& InArray);
		static TArray<FName> ReturnValue(const FMonoBindings& Bindings, MonoObject* Object);

		static inline bool IsValidParameterType(MonoType* typ)
		{
			return IsValidArrayType(typ, MONO_BINDINGS_NAMESPACE ".Name", false);
		}

		static inline bool IsValidReturnType(MonoType* typ)
		{
			return IsValidArrayType(typ, MONO_BINDINGS_NAMESPACE ".Name", false);
		}
	};

	template<>
	struct Marshal < TArray<FLifetimeProperty>, void >
	{
		static MonoArray* Parameter(const FMonoBindings& Bindings, const TArray<FLifetimeProperty>& InArray);
		static TArray<FLifetimeProperty> ReturnValue(const FMonoBindings& Bindings, MonoObject* Object);

		static inline bool IsValidParameterType(MonoType* typ)
		{
			return IsValidArrayType(typ, MONO_BINDINGS_NAMESPACE ".LifetimeReplicatedProperty", false);
		}

		static inline bool IsValidReturnType(MonoType* typ)
		{
			return IsValidArrayType(typ, MONO_BINDINGS_NAMESPACE ".LifetimeReplicatedProperty", false);
		}
	};

	template<class T>
	MonoArray* Marshal<TArray<T>, typename TEnableIf<ValueType<T>::Value>::Type>::Parameter(const FMonoDomain& Domain, const TArray<T>& InArray)
	{
		MonoArray* OutArray = mono_array_new(Domain.GetDomain(), ValueType<T>::GetMonoType(), InArray.Num());
		FMemory::Memcpy(mono_array_addr(OutArray, T, 0), InArray.GetData(), InArray.Num() * sizeof(T));
		return OutArray;
	}

	// Invoke
	MONORUNTIME_API MonoObject* Invoke(bool& bThrewException, InvokeExceptionBehavior ExceptionBehavior, MonoDomain* Domain, MonoMethod* Method, MonoObject* Object, void** Arguments);
	MonoObject* InvokeDelegate(bool& bThrewException, InvokeExceptionBehavior ExceptionBehavior, MonoDomain* Domain, MonoObject* Delegate, void** Arguments);

	template <class ReturnValue>
	inline void VerifyReturnSignature(MonoMethod* Method, int ExpectedParamCount)
	{
#if DO_GUARD_SLOW
		check(Method);
		MonoMethodSignature* Signature = mono_method_signature(Method);
		check(Signature);
		check(mono_signature_get_param_count(Signature) == ExpectedParamCount);
		check(Marshal<ReturnValue>::IsValidReturnType(mono_signature_get_return_type(Signature)));
#endif
	}

#if DO_GUARD_SLOW
	template<typename ParameterType>
	static void VerifyParameter(MonoType* p, int paramNum)
	{
		checkf(Marshal<typename TRemoveCV<typename TRemoveReference<ParameterType>::Type>::Type>::IsValidParameterType(p), TEXT("Type mismatch in parameter %d, expected %s"), paramNum, ANSI_TO_TCHAR(mono_type_get_name(p)));
	}
	
	typedef void(*VerifyParameterFunc)(MonoType*, int);
#endif // DO_GUARD_SLOW

	template <typename... ArgTypes>
	inline void VerifyParameterSignature(MonoMethod* Method)
	{
#if DO_GUARD_SLOW
		check(Method);
		MonoMethodSignature* Signature = mono_method_signature(Method);
		check(Signature);
		const int ArgCount = sizeof...(ArgTypes);
		// this is nasty but I can't think of another way to do it that garauntees we verify params in correct order
		const VerifyParameterFunc VerifyFuncs[ArgCount] = { (&VerifyParameter<ArgTypes>)... };
		void* ParamIterator = nullptr;
		MonoType* ExpectedArgType = nullptr;
		int ParamCount = 0;

		for (; ParamCount < ArgCount; ++ParamCount)
		{
			ExpectedArgType = mono_signature_get_params(Signature, &ParamIterator);
			check(ExpectedArgType);
			VerifyFuncs[ParamCount](ExpectedArgType, ParamCount);
		}
#endif
	}

	// no-argument version
	template <class ReturnValue, class DomainType>
	inline ReturnValue Invoke(const DomainType& Domain, MonoMethod* Method, MonoObject* Object)
	{
		bool bThrewException=false;
		VerifyReturnSignature<ReturnValue>(Method,0);
		MonoObject* ReturnObject = Invoke(bThrewException, Domain.GetExceptionBehavior(), Domain.GetDomain(), Method, Object, nullptr);
		if (bThrewException)
		{
			ReturnObject = nullptr;
		}
		return Marshal<ReturnValue>::ReturnValue(Domain, ReturnObject);
	}

	// no-argument delegate version
	template <class ReturnValue, class DomainType>
	inline ReturnValue InvokeDelegate(const DomainType& Domain, MonoObject* Delegate)
	{
		bool bThrewException = false;
		check(Delegate);
#if DO_GUARD_SLOW
		MonoClass* DelegateClass = mono_object_get_class(Delegate);
		check(DelegateClass);
		MonoMethod* DelegateMethod = mono_get_delegate_invoke(DelegateClass);
		check(DelegateMethod);
		VerifyReturnSignature<ReturnValue>(DelegateMethod, 0);
#endif // DO_GUARD_SLOW
		MonoObject* ReturnObject = InvokeDelegate(bThrewException, Domain.GetExceptionBehavior(), Domain.GetDomain(), Delegate, nullptr);
		if (bThrewException)
		{
			ReturnObject = nullptr;
		}
		return Marshal<ReturnValue>::ReturnValue(Domain, ReturnObject);
	}

	// automatic argument marshalling version
	template <class ReturnValue, class DomainType, typename... ArgTypes>
	ReturnValue Invoke(const DomainType& Domain, MonoMethod* Method, MonoObject* Object, ArgTypes&&... Arguments)
	{
		const int ArgCount = sizeof...(Arguments);
		VerifyReturnSignature<ReturnValue>(Method, ArgCount);
		VerifyParameterSignature<ArgTypes...>(Method);
		void* ArgumentArray[ArgCount] = { Marshal<typename TRemoveCV<typename TRemoveReference<ArgTypes>::Type>::Type>::Parameter(Domain, Forward<ArgTypes>(Arguments))... };
		bool bThrewException = false;
		MonoObject* ReturnObject = Invoke(bThrewException, Domain.GetExceptionBehavior(), Domain.GetDomain(), Method, Object, ArgumentArray);
		if (bThrewException)
		{
			ReturnObject = nullptr;
		}
		return Marshal<ReturnValue>::ReturnValue(Domain, ReturnObject);
	}

	// automatic argument marshalling version for delegates
	template <class ReturnValue, class DomainType, typename... ArgTypes>
	ReturnValue InvokeDelegate(const DomainType& Domain, MonoObject* Delegate, ArgTypes&&... Arguments)
	{
		const int ArgCount = sizeof...(Arguments);
#if DO_GUARD_SLOW
		MonoClass* DelegateClass = mono_object_get_class(Delegate);
		check(DelegateClass);
		MonoMethod* DelegateMethod = mono_get_delegate_invoke(DelegateClass);
		check(DelegateMethod);
		VerifyReturnSignature<ReturnValue>(DelegateMethod, ArgCount);
		VerifyParameterSignature<ArgTypes...>(DelegateMethod);
#endif // DO_GUARD_SLOW
		void* ArgumentArray[ArgCount] = { Marshal<typename TRemoveCV<typename TRemoveReference<ArgTypes>::Type>::Type>::Parameter(Domain, Forward<ArgTypes>(Arguments))... };
		bool bThrewException = false;
		MonoObject* ReturnObject = InvokeDelegate(bThrewException, Domain.GetExceptionBehavior(), Domain.GetDomain(), Delegate, ArgumentArray);
		if (bThrewException)
		{
			ReturnObject = nullptr;
		}
		return Marshal<ReturnValue>::ReturnValue(Domain, ReturnObject);
	}

	// Object creation
	// construct object calling default constructor
	MonoObject* ConstructObject(const FMonoDomain& Domain, MonoClass* Class);

	template<typename DomainType, typename... ArgTypes>
	MonoObject* ConstructObject(const DomainType& Domain, MonoClass* Class, MonoMethod* ConstructorMethod, ArgTypes&&... Arguments)
	{
		check(Class);
		check(ConstructorMethod);

		MonoObject* Object = mono_object_new(Domain.GetDomain(), Class);
		Invoke<void>(Domain, ConstructorMethod, Object, Forward<ArgTypes>(Arguments)...);
		return Object;
	}

#if MONO_IS_DYNAMIC_LIB
	void LoadMonoDLL();
	void UnloadMonoDLL();
#endif //MONO_IS_DYNAMIC_LIB
}