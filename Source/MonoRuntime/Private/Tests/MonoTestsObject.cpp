// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "Tests/MonoTestsObject.h"
#include "MonoRuntimeCommon.h"
#include "MonoBindings.h"
#include "Tests/MonoTestUserObjectBase.h"
#include "Tests/MonoTestSubObject.h"

#include "Animation/SkeletalMeshActor.h"
#include "Engine/Light.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"

#include "MonoLogBridge.h"

UMonoTestsObject::UMonoTestsObject(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TestSubObject = ObjectInitializer.CreateDefaultSubobject<UMonoTestSubObject>(this, TEXT("TestSubObject"));
	TestWeakObject = TestSubObject;

	for (int32 i = 1; i <= 3; ++i)
	{
		UMonoTestSubObject* SubObject = ObjectInitializer.CreateDefaultSubobject<UMonoTestSubObject>(this, FName(TEXT("TestSubObject"), i));
		TestObjectArray.Add(SubObject);
	}

	// don't call Reset here, that causes mono module to be initialized in a weird spot (creating CDOs for module's UObjects)
}

static int32 GetSharedReferenceCount(FText &text)
{
	FMarshaledText *MarshaledTextCopy = reinterpret_cast<FMarshaledText*>(&text);
	return MarshaledTextCopy->Data.ReferenceController.GetSharedReferenceCount ();
}

void UMonoTestsObject::Reset()
{
	for (int32 i = 0; i < TestObjectArray.Num(); ++i)
	{
		TestObjectArray[i]->TestReadableInt32 = i;
	}

	TestWeakObject = TestObjectArray[1];

	TestReadableInt32 = 1000000000;

	TestReadWriteFloat = -42.0f;

	TestReadWriteInt32 = 123456789;

	TestReadWriteEnum = ETE_Something;
	TestReadWriteEnumCpp = ETestEnumCpp::Alpha;

	TestReadableBool = false;
	TestReadWriteBool = false;
	TestReadWriteBitfield1 = true;
	TestReadWriteBitfield2 = true;

	TestReadWriteString = "Foo";
	TestReadWriteName = TEXT("Catch_22");

	TestReadWriteText = FText::FromString("This is an English sentence.");
	TestReadWriteTextCopy = TestReadWriteText;

	TestArrayInt.Empty(3);
	TestArrayInt.Add(1);
	TestArrayInt.Add(2);
	TestArrayInt.Add(3);

	TestArrayFloat.Empty(3);
	TestArrayFloat.Add(1.0f);
	TestArrayFloat.Add(2.0f);
	TestArrayFloat.Add(3.0f);

	TestReadableVector2D = FVector2D(2.f, 2.f);
	TestReadableVector = FVector(4.f, 8.f, 15.f);
	TestReadableVector4 = FVector4(16.f, 23.f, 42.f, 108.f);
	TestReadWriteQuat = FQuat(2.f, 4.f, 6.f, 0.1f);
	TestReadWriteMatrix = FMatrix::Identity;
	TestReadableRotator = FRotator(45.f, 15.f, 5.f);

	TestReadWriteStruct.TestStructInt32 = 22;
	TestReadWriteStruct.bTestNotBlueprintVisible = true;
	TestReadWriteStruct.TestStructFloat = 451.f;
	TestReadWriteStruct.TestStructWeakObject = TestSubObject;
	TestReadWriteStruct.TestSubStruct.bTestBool1 = true;
	TestReadWriteStruct.TestSubStruct.bTestBool2 = false;

	TestReadWriteColor.R = 128;
	TestReadWriteColor.G = 128;
	TestReadWriteColor.B = 0;
	TestReadWriteColor.A = 0;

	TestReadWriteStructArray.Empty(1);
	TestReadWriteStructArray.Add(FMonoTestsStruct(22, 42.f, this));

	TestReadWriteClass = UMonoTestsObject::StaticClass();
	TestReadWriteActorClass = ASkeletalMeshActor::StaticClass();
	TestReadWriteActorClassArray.Empty(3);
	TestReadWriteActorClassArray.Add(AActor::StaticClass());
	TestReadWriteActorClassArray.Add(APawn::StaticClass());
	TestReadWriteActorClassArray.Add(ACharacter::StaticClass());

	if (nullptr == TestUserObject)
	{
		FMonoBindings& Bindings = FMonoBindings::Get();

		const FMonoTypeReferenceMetadata TestTypeRef(FString(MONO_UE4_NAMESPACE) + TEXT(".ManagedExtensions"), TEXT("MonoTestUserObject"), FString(MONO_UE4_NAMESPACE) + TEXT(".ManagedExtensions"));
		UClass* UserTestClass = Bindings.GetUnrealClassFromTypeReference(TestTypeRef);
		check(UserTestClass);
		TestUserObject = (UMonoTestUserObjectBase*)StaticConstructObject_Internal(UserTestClass, GetTransientPackage(), NAME_None, RF_Transient);
	}
	check(TestUserObject);
	TestUserObject->Reset(this);
}

void UMonoTestsObject::LogTestFailure(const FString& Message)
{
	check(Tester);
	FMonoLogBridge::Get().Write(*Message, Message.Len());
	Tester->AddError(FString::Printf(TEXT("MonoRuntime - %s"), *Message));
}

void UMonoTestsObject::FailTest()
{
	// only hard fail in debug builds, otherwise the automation test commandlet won't be able to report
#if DO_GUARD_SLOW
	checkNoEntry();
#endif // DO_GUARD_SLOW
}



void UMonoTestsObject::VerifySimpleTypePropertyEdits()
{
	// Values should have been changed in MonoTestsObject_Injected.cs before this call
	AssertEqualDouble(TestReadWriteFloat, 42.0f, TEXT("TestReadWriteFloat"));
	AssertEqualInt(TestReadWriteInt32, 123456789, TEXT("TestReadWriteInt32"));

	AssertEqualBool(TestReadWriteBool, true, TEXT("TestReadWriteBool"));
	AssertEqualBool(TestReadableBool, false, TEXT("TestReadableBool"));

	AssertEqualBool(TestReadWriteBitfield1, false, TEXT("TestReadWriteBitfield1"));
	AssertEqualBool(TestReadWriteBitfield2, true, TEXT("TestReadWriteBitfield2"));
	AssertEqualUInt(TestReadWriteEnum, ETE_SomethingElse, TEXT("TestReadWriteEnum"));
	AssertEqualUInt((uint64)TestReadWriteEnumCpp, (uint64)ETestEnumCpp::Beta, TEXT("TestReadWriteEnumCpp"));

	// Values should be unchanged
	AssertEqualInt(TestReadableInt32, 1000000000, TEXT("TestReadableInt32"));

	AssertEqualBool(TestReadWriteBool, true, TEXT("TestReadWriteBool"));
	AssertEqualBool(TestReadableBool, false, TEXT("TestReadableBool"));

	AssertEqualString(TestReadWriteString, TEXT("Foo"), TEXT("TestReadWriteString"));
	AssertEqualString(TestReadWriteName.ToString(), TEXT("Catch_22"), TEXT("TestReadWriteName.ToString()"));
	AssertEqualString(TestReadWriteText.ToString(), TEXT("This is an English sentence."), TEXT("TestReadWriteText.ToString()"));
}

void UMonoTestsObject::VerifyStringPropertyEdit()
{
	// Value should have been changed in MonoTestsObject_Injected.cs before this call
	AssertEqualString(TestReadWriteString, TEXT("Bar"), TEXT("TestReadWriteString"));

	// Values should be unchanged
	AssertEqualInt(TestReadableInt32, 1000000000, TEXT("TestReadableInt32"));

	AssertEqualDouble(TestReadWriteFloat, -42.0f, TEXT("TestReadWriteFloat"));
	AssertEqualInt(TestReadWriteInt32, 123456789, TEXT("TestReadWriteInt32"));

	AssertEqualUInt(TestReadWriteEnum, ETE_Something, TEXT("TestReadWriteEnum"));
	AssertEqualUInt((uint64)TestReadWriteEnumCpp, (uint64)ETestEnumCpp::Alpha, TEXT("TestReadWriteEnumCpp"));

	AssertEqualBool(TestReadWriteBool, false, TEXT("TestReadWriteBool"));
	AssertEqualBool(TestReadableBool, false, TEXT("TestReadableBool"));
	AssertEqualBool(TestReadWriteBitfield1, true, TEXT("TestReadWriteBitfield1"));
	AssertEqualBool(TestReadWriteBitfield2, true, TEXT("TestReadWriteBitfield2"));

	AssertEqualString(TestReadWriteName.ToString(), TEXT("Catch_22"), TEXT("TestReadWriteName.ToString()"));
	AssertEqualString(TestReadWriteText.ToString(), TEXT("This is an English sentence."), TEXT("TestReadWriteText.ToString()"));
	AssertEqualInt(GetSharedReferenceCount(TestReadWriteTextCopy), 2, TEXT("TestReadWriteTextCopy.SharedReferenceCount"));

}

void UMonoTestsObject::VerifyNamePropertyEdit()
{
	// Value should have been changed in MonoTestsObject_Injected.cs before this call
	AssertEqualString(TestReadWriteName.ToString(), TEXT("Jim"), TEXT("TestReadWriteName.ToString()"));

	// Values should be unchanged
	AssertEqualInt(TestReadableInt32, 1000000000, TEXT("TestReadableInt32"));

	AssertEqualDouble(TestReadWriteFloat, -42.0f, TEXT("TestReadWriteFloat"));
	AssertEqualInt(TestReadWriteInt32, 123456789, TEXT("TestReadWriteInt32"));

	AssertEqualUInt(TestReadWriteEnum, ETE_Something, TEXT("TestReadWriteEnum"));
	AssertEqualUInt((uint64)TestReadWriteEnumCpp, (uint64)ETestEnumCpp::Alpha, TEXT("TestReadWriteEnumCpp"));

	AssertEqualBool(TestReadWriteBool, false, TEXT("TestReadWriteBool"));
	AssertEqualBool(TestReadableBool, false, TEXT("TestReadableBool"));
	AssertEqualBool(TestReadWriteBitfield1, true, TEXT("TestReadWriteBitfield1"));
	AssertEqualBool(TestReadWriteBitfield2, true, TEXT("TestReadWriteBitfield2"));

	AssertEqualString(TestReadWriteString, TEXT("Foo"), TEXT("TestReadWriteString"));
	AssertEqualString(TestReadWriteText.ToString(), TEXT("This is an English sentence."), TEXT("TestReadWriteText.ToString()"));
	AssertEqualInt(GetSharedReferenceCount(TestReadWriteTextCopy), 2, TEXT("TestReadWriteTextCopy.SharedReferenceCount"));

}

void UMonoTestsObject::VerifyTextPropertyEdit()
{
	AssertEqualString(TestReadWriteText.ToString(), TEXT("This is still an English sentence."), TEXT("TestReadWriteText.ToString()"));
	AssertEqualInt(GetSharedReferenceCount(TestReadWriteTextCopy), 1, TEXT("TestReadWriteTextCopy.SharedReferenceCount"));

	// Values should be unchanged
	AssertEqualInt(TestReadableInt32, 1000000000, TEXT("TestReadableInt32"));

	AssertEqualDouble(TestReadWriteFloat, -42.0f, TEXT("TestReadWriteFloat"));
	AssertEqualInt(TestReadWriteInt32, 123456789, TEXT("TestReadWriteInt32"));

	AssertEqualUInt(TestReadWriteEnum, ETE_Something, TEXT("TestReadWriteEnum"));
	AssertEqualUInt((uint64)TestReadWriteEnumCpp, (uint64)ETestEnumCpp::Alpha, TEXT("TestReadWriteEnumCpp"));

	AssertEqualBool(TestReadWriteBool, false, TEXT("TestReadWriteBool"));
	AssertEqualBool(TestReadableBool, false, TEXT("TestReadableBool"));
	AssertEqualBool(TestReadWriteBitfield1, true, TEXT("TestReadWriteBitfield1"));
	AssertEqualBool(TestReadWriteBitfield2, true, TEXT("TestReadWriteBitfield2"));

	AssertEqualString(TestReadWriteName.ToString(), TEXT("Catch_22"), TEXT("TestReadWriteName.ToString()"));
	AssertEqualString(TestReadWriteString, TEXT("Foo"), TEXT("TestReadWriteString"));
}

void UMonoTestsObject::VerifyMathPropertyEdits()
{
	AssertEquals(TestReadWriteQuat, FQuat(1, 2, 3, 4), TEXT("TestReadWriteQuat"));

	for (int i = 0; i <= 15; ++i)
	{
		int row = i / 4;
		int col = i % 4;
		AssertEqualDouble(TestReadWriteMatrix.M[row][col], i, *FString::Printf(TEXT("TestReadWriteMatrix.M[%d][%d]"), row, col));
	}
}

void UMonoTestsObject::VerifyStructPropertyEdits()
{
	AssertEqualInt(TestReadWriteStruct.TestStructInt32, 42, TEXT("TestReadWriteStruct.TestStructInt32"));
	AssertEqualBool(TestReadWriteStruct.bTestNotBlueprintVisible, true, TEXT("TestReadWriteStruct.bTestNotBlueprintVisible"));
	AssertEqualDouble(TestReadWriteStruct.TestStructFloat, 24601.f, TEXT("TestReadWriteStruct.TestStructFloat"));

	AssertEqualInt(TestReadWriteColor.R, 128, TEXT("TestReadWriteColor.R"));
	AssertEqualInt(TestReadWriteColor.G, 0, TEXT("TestReadWriteColor.G"));
	AssertEqualInt(TestReadWriteColor.B, 128, TEXT("TestReadWriteColor.B"));
	AssertEqualInt(TestReadWriteColor.A, 128, TEXT("TestReadWriteColor.A"));
}

void UMonoTestsObject::VerifyStructArrayPropertyEdits()
{
	AssertEqualInt(TestReadWriteStructArray.Num(), 2, TEXT("TestReadWriteStructArray.Num()"));

	AssertEqualInt(TestReadWriteStructArray[0].TestStructInt32, 22, TEXT("TestReadWriteStructArray[0].TestStructInt32"));
	AssertEqualDouble(TestReadWriteStructArray[0].TestStructFloat, 54.f, TEXT("TestReadWriteStructArray[0].TestStructFloat"));

	AssertEqualInt(TestReadWriteStructArray[1].TestStructInt32, 451, TEXT("TestReadWriteStructArray[1].TestStructInt32"));
	AssertEqualDouble(TestReadWriteStructArray[1].TestStructFloat, 24601.f, TEXT("TestReadWriteStructArray[1].TestStructFloat"));
}

void UMonoTestsObject::VerifyClassPropertyEdits()
{
	AssertEqualUObject(TestReadWriteClass, USceneComponent::StaticClass(), TEXT("TestReadWriteClass"));
	AssertEqualUObject(*TestReadWriteActorClass, ALight::StaticClass(), TEXT("TestReadWriteActorClass"));
}

void UMonoTestsObject::VerifyWeakObjectPropertyEdits()
{
	AssertEqualUObject(TestWeakObject.Get(), TestObjectArray[0], TEXT("TestWeakObject->TestReadableInt32"));
	AssertEqualBool(TestReadWriteStruct.TestStructWeakObject.IsValid(), false, TEXT("TestReadWriteStruct.TestWeakObject.IsValid()"));
}

void UMonoTestsObject::TestOnlyInt32Args(int32 x, int32 y, int32 z)
{
	AssertEqualInt(x, 1, TEXT("x"));
	AssertEqualInt(y, 2, TEXT("y"));
	AssertEqualInt(z, 3, TEXT("z"));
}

void UMonoTestsObject::TestOnlyFloatArgs(float x, float y, float z)
{
	AssertEqualDouble(x, 1.0f, TEXT("x"));
	AssertEqualDouble(y, 2.0f, TEXT("y"));
	AssertEqualDouble(z, 3.0f, TEXT("z"));
}

void UMonoTestsObject::TestOnlyBoolArgs(bool x, bool y, bool z)
{
	AssertEqualBool(x, true, TEXT("x"));
	AssertEqualBool(y, false, TEXT("y"));
	AssertEqualBool(z, true, TEXT("z"));
}

void UMonoTestsObject::TestOnlyStringArgs(const FString& x, const FString& y, const FString& z)
{
	AssertEqualString(x, TEXT("Foo"), TEXT("x"));
	AssertEqualString(y, TEXT("Bar"), TEXT("y"));
	AssertEqualString(z, TEXT("Baz"), TEXT("z"));
}

void UMonoTestsObject::TestOnlyNameArgs(FName x, const FName y, const FName z)
{
	AssertEqualString(x.ToString(), TEXT("Joseph"), TEXT("x.ToString()"));
	AssertEqualString(y.ToString(), TEXT("Heller"), TEXT("y.ToString()"));
	AssertEqualString(z.ToString(), TEXT("Catch_22"), TEXT("z.ToString()"));
}

void UMonoTestsObject::TestMixedArgs(const FString& s, FName t, int32 w, float x, int32 y, float z)
{
	AssertEqualString(s, "Foo", TEXT("s"));
	AssertEqualString(t.ToString(), "Bar", TEXT("t.ToString()"));
	AssertEqualInt(w, 1, TEXT("w"));
	AssertEqualDouble(x, 42.0, TEXT("X"));
	AssertEqualInt(y, 108, TEXT("y"));
	AssertEqualDouble(z, 22.0f, TEXT("z"));
}

UObject* UMonoTestsObject::TestObjectArgsAndReturn(UObject* x, UObject* y)
{
	AssertEqualUObject(y, nullptr, TEXT("y"));
	return x;
}

int32 UMonoTestsObject::TestInt32Return(int32 x, int32 y)
{
	AssertEqualInt(y, 2, TEXT("y"));
	return x;
}

float UMonoTestsObject::TestFloatReturn(float x, float y)
{
	AssertEqualDouble(y, 2.0f, TEXT("y"));
	return x;
}

bool UMonoTestsObject::TestBoolReturn(bool x, bool y)
{
	AssertEqualBool(y, true, TEXT("y"));
	return x;
}

TEnumAsByte<ETestEnum> UMonoTestsObject::TestEnumReturn(TEnumAsByte<ETestEnum> x, TEnumAsByte<ETestEnum> y)
{
	AssertEqualUInt(y, ETestEnum::ETE_SomethingElse, TEXT("y"));
	return x;
}

ETestEnumCpp UMonoTestsObject::TestEnumCppReturn(ETestEnumCpp x, ETestEnumCpp y)
{
	AssertEqualUInt((uint64)y, (uint64)ETestEnumCpp::Beta, TEXT("y"));
	return x;
}


FString UMonoTestsObject::TestStringReturn(const FString& x, const FString& y)
{
	AssertEqualString(y, TEXT("Bar"), TEXT("y"));
	return x;
}

FName UMonoTestsObject::TestNameReturn(FName x, FName y)
{
	AssertEqualString(y.ToString(), TEXT("Catch_22"), TEXT("y"));
	return x;
}

FVector UMonoTestsObject::TestVectorReturn(FVector x, FVector y)
{
	AssertEquals(y, FVector(1.f, 0.f, 8.f), TEXT("y"));
	return x;
}

FQuat UMonoTestsObject::TestQuatReturn(const FQuat& x, const FQuat& y)
{
	AssertEquals(y, FQuat(2, 4, 6, 0.1f), TEXT("y"));
	return x;
}

FMatrix UMonoTestsObject::TestMatrixReturn(const FMatrix& x, const FMatrix& y)
{
	AssertEquals(y, FMatrix::Identity, TEXT("y"));
	return x;
}

TArray<FName> UMonoTestsObject::TestValueTypeArrayReturn(const TArray<FName>& x, const TArray<FName>& y)
{
	AssertEqualInt(y.Num(), 3, TEXT("y.Num()"));
	AssertEquals(y[0], FName(TEXT("Catch_22")), TEXT("y[0]"));
	AssertEquals(y[1], FName(TEXT("Slaughterhouse_5")), TEXT("y[1]"));
	AssertEquals(y[2], FName(TEXT("Fahrenheit_451")), TEXT("y[2]"));

	return x;
}

TArray<UObject*> UMonoTestsObject::TestObjectArrayReturn(const TArray<UObject*>& x, const TArray<UObject*>& y)
{
	AssertEqualInt(y.Num(), 3, TEXT("y.Num()"));
	AssertEqualUObject(y[0], this, TEXT("y[0]"));
	AssertEqualUObject(y[1], nullptr, TEXT("y[1]"));
	AssertEqualUObject(y[2], TestSubObject, TEXT("y[2]"));

	return x;
}

FMonoTestsStruct UMonoTestsObject::TestStructReturn(FMonoTestsStruct x, FMonoTestsStruct y)
{
	AssertEqualInt(y.TestStructInt32, 42, TEXT("y.TestStructInt32"));
	AssertEqualDouble(y.TestStructFloat, 54.f, TEXT("y.TestStructFloat"));

	return x;
}

int32 UMonoTestsObject::TestStaticFunction(int32 x, int32 y)
{
	AssertEqualInt(y, 42, TEXT("y"));
	return x;
}

void UMonoTestsObject::TestOutParams(FVector& InOutVector, FVector& OutVector)
{
	AssertEqualDouble(InOutVector.X, 16.f, TEXT("InOutVector.X"));
	AssertEqualDouble(InOutVector.Y, 23.f, TEXT("InOutVector.Y"));
	AssertEqualDouble(InOutVector.Z, 42.f, TEXT("InOutVector.Z"));

	OutVector = InOutVector;

	InOutVector.X = 4.f;
	InOutVector.Y = 8.f;
	InOutVector.Z = 15.f;
}

int32 UMonoTestsObject::TestStructDefaultParams(FVector vec3, FVector2D vec2, FLinearColor lc, FRotator rot, FColor color)
{
	int32 MatchCount = 0;
	if (vec3 == FVector(4.f, 8.f, 15.f))
	{
		++MatchCount;
	}

	if (vec2 == FVector2D::ZeroVector)
	{
		++MatchCount;
	}

	if (lc == FLinearColor(16.f, 23.f, 42.f))
	{
		++MatchCount;
	}

	if (rot == FRotator::ZeroRotator)
	{
		++MatchCount;
	}

	if (color == FColor::Red)
	{
		++MatchCount;
	}

	return MatchCount;
}



void UMonoTestsObject::RaiseAssertFailedException(FString Message)
{
	FMonoBindings& Bindings = FMonoBindings::Get();
	const FCachedAssembly& RuntimeAssembly = Bindings.GetRuntimeAssembly();

	MonoException* AssertionFailedException = RuntimeAssembly.CreateExceptionByName(MONO_RUNTIME_NAMESPACE, "AssertionFailedException", Message);
	mono_raise_exception(AssertionFailedException);
}

void UMonoTestsObject::AssertEqualInt(int64 ActualValue, int64 ExpectedValue, const TCHAR* Name)
{
	if (ActualValue != ExpectedValue)
	{
		RaiseAssertFailedException(FString::Printf(TEXT("Expected %s to be %lli, got %lli"), Name, ExpectedValue, ActualValue));
	}
}

void UMonoTestsObject::AssertEqualUInt(uint64 ActualValue, uint64 ExpectedValue, const TCHAR* Name)
{
	if (ActualValue != ExpectedValue)
	{
		RaiseAssertFailedException(FString::Printf(TEXT("Expected %s to be %llu, got %llu"), Name, ExpectedValue, ActualValue));
	}
}

void UMonoTestsObject::AssertEqualBool(bool ActualValue, bool ExpectedValue, const TCHAR* Name)
{
	if (ActualValue != ExpectedValue)
	{
		RaiseAssertFailedException(FString::Printf(TEXT("Expected %s to be %i, got %i"), Name, ExpectedValue ? TEXT("true") : TEXT("false"), ActualValue ? TEXT("true") : TEXT("false")));
	}
}

void UMonoTestsObject::AssertEqualDouble(double ActualValue, double ExpectedValue, const TCHAR* Name)
{
	if (ActualValue != ExpectedValue)
	{
		RaiseAssertFailedException(FString::Printf(TEXT("Expected %s to be %g, got %g"), Name, ExpectedValue, ActualValue));
	}
}

void UMonoTestsObject::AssertEqualString(const FString& ActualValue, const FString& ExpectedValue, const TCHAR* Name)
{
	if (ActualValue != ExpectedValue)
	{
		RaiseAssertFailedException(FString::Printf(TEXT("Expected %s to be %s, got %s"), Name, *ExpectedValue, *ActualValue));
	}
}

void UMonoTestsObject::AssertEqualUObject(UObject* ActualValue, UObject* ExpectedValue, const TCHAR* Name)
{
	if (ActualValue != ExpectedValue)
	{
		RaiseAssertFailedException(FString::Printf(TEXT("Expected %s to be %s, got %s"), 
			Name, 
			ExpectedValue ? *ExpectedValue->GetName() : TEXT("nullptr"), 
			ActualValue ? *ActualValue->GetName() : TEXT("nullptr")));
	}
}
