// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "MonoTestsObject.generated.h"

// Bump this number to force headers to regenerate. This is a workaround until we get the UBT fix from epic
// which takes UHT plugins into account as dependencies for the header generation step
// BUMP_THIS_TO_FORCE_HEADER_REGEN_UNTIL_UHT_DEPENDENCIES_ARE_FIXED 109
// INITIALS OF LAST PERSON TO BUMP TO FORCE A MERGE MJH

class UMonoTestSubObject;
class UMonoTestUserObjectBase;

USTRUCT(BlueprintType)
struct FMonoTestsSubStruct
{
	GENERATED_USTRUCT_BODY()

	FMonoTestsSubStruct()
	: bTestBool1(true)
	, bTestBool2(false)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Test")
		uint8 bTestBool1 : 1;
	UPROPERTY(BlueprintReadOnly, Category = "Test")
		uint8 bTestBool2 : 1;
};

UENUM(BlueprintType)
enum ETestEnum
{
    ETE_Something,
    ETE_SomethingElse,
    ETE_StillAnotherThing,
};

UENUM(BlueprintType)
enum class ETestEnumCpp : uint8
{
	Alpha,
	Beta,
	Gamma
};

UENUM(BlueprintType)
enum ETestNumberEnum
{
	NUMBER_1,
	NUMBER_2,
	NUMBER_3
};

USTRUCT(BlueprintType)
struct FMonoTestsStruct
{
	GENERATED_USTRUCT_BODY()

	FMonoTestsStruct()
	: TestStructInt32(0)
	, bTestNotBlueprintVisible(false)
	{}

	FMonoTestsStruct(int32 InInt, float InFloat, UObject* InObj)
	: TestStructInt32(InInt)
	, bTestNotBlueprintVisible(false)
	, TestStructFloat(InFloat)
	, TestStructWeakObject(InObj)
	{

	}
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		int32 TestStructInt32;

	bool bTestNotBlueprintVisible;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		float TestStructFloat;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
		FMonoTestsSubStruct TestSubStruct;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		TWeakObjectPtr<UObject> TestStructWeakObject;
};

UCLASS(BlueprintType)
class UMonoTestsObject : public UObject
{
	GENERATED_UCLASS_BODY()
 
public:
	class FAutomationTestBase* Tester;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
		UObject* TestNullObject;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
		UMonoTestSubObject* TestSubObject;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		TWeakObjectPtr<UMonoTestSubObject> TestWeakObject; 

	UPROPERTY(BlueprintReadOnly, Category = "Test")
		TArray<UMonoTestSubObject*> TestObjectArray;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
		UMonoTestUserObjectBase* TestUserObject;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
		int32 TestReadableInt32;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		float TestReadWriteFloat;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		int32  TestReadWriteInt32;

	UPROPERTY(BlueprintReadWrite, Category = Test)
		TEnumAsByte<ETestEnum> TestReadWriteEnum;

	UPROPERTY(BlueprintReadWrite, Category = Test)
		ETestEnumCpp TestReadWriteEnumCpp;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		bool TestReadWriteBool;
	UPROPERTY(BlueprintReadOnly, Category = "Test")
		bool TestReadableBool;
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		uint32 TestReadWriteBitfield1 : 1;
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		uint32 TestReadWriteBitfield2 : 1;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		FString TestReadWriteString;
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		FName TestReadWriteName;
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		FText TestReadWriteText;

	FText TestReadWriteTextCopy;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
		TArray<int32> TestArrayInt;
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		TArray<float> TestArrayFloat;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
		FVector2D TestReadableVector2D;
	UPROPERTY(BlueprintReadOnly, Category = "Test")
		FVector TestReadableVector;
	UPROPERTY(BlueprintReadOnly, Category = "Test")
		FVector4 TestReadableVector4;
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		FQuat TestReadWriteQuat;
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		FMatrix TestReadWriteMatrix;
	UPROPERTY(BlueprintReadOnly, Category = "Test")
		FRotator TestReadableRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		FMonoTestsStruct TestReadWriteStruct;
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		FColor TestReadWriteColor;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		TArray<FMonoTestsStruct> TestReadWriteStructArray;

	UPROPERTY(BlueprintReadWrite, Category = "Test")
		UClass* TestReadWriteClass;
	UPROPERTY(BlueprintReadWrite, Category = "Test")
		TSubclassOf<AActor> TestReadWriteActorClass;
	UPROPERTY(BlueprintReadOnly, Category = "Test")
		TArray <TSubclassOf<AActor>> TestReadWriteActorClassArray;

	UFUNCTION(BlueprintCallable, Category = "Test")
		void Reset();
	UFUNCTION(BlueprintCallable, Category = "Test")
		void LogTestFailure(const FString& Message);
	UFUNCTION(BlueprintCallable, Category = "Test")
		void FailTest();

	UFUNCTION(BlueprintCallable, Category = "Test")
		void VerifySimpleTypePropertyEdits();
	UFUNCTION(BlueprintCallable, Category = "Test")
		void VerifyStringPropertyEdit();
	UFUNCTION(BlueprintCallable, Category = "Test")
		void VerifyNamePropertyEdit();
	UFUNCTION(BlueprintCallable, Category = "Test")
		void VerifyTextPropertyEdit();
	UFUNCTION(BlueprintCallable, Category = "Test")
		void VerifyMathPropertyEdits();
	UFUNCTION(BlueprintCallable, Category = "Test")
		void VerifyStructPropertyEdits();
	UFUNCTION(BlueprintCallable, Category = "Test")
		void VerifyStructArrayPropertyEdits();
	UFUNCTION(BlueprintCallable, Category = "Test")
		void VerifyClassPropertyEdits();
	UFUNCTION(BlueprintCallable, Category = "Test")
		void VerifyWeakObjectPropertyEdits();

	UFUNCTION(BlueprintCallable, Category = "Test")
		void TestOnlyInt32Args(int32 x, int32 y, int32 z);

	UFUNCTION(BlueprintCallable, Category = "Test")
		void TestOnlyFloatArgs(float x, float y, float z);

	UFUNCTION(BlueprintCallable, Category = "Test")
		void TestOnlyBoolArgs(bool x, bool y, bool z);

	UFUNCTION(BlueprintCallable, Category = "Test")
		void TestOnlyStringArgs(const FString& x, const FString& y, const FString& z);
	UFUNCTION(BlueprintCallable, Category = "Test")
		void TestOnlyNameArgs(FName x, const FName y, const FName z);

	UFUNCTION(BlueprintCallable, Category = "Test")
		void TestMixedArgs(const FString& s, FName t, int32 w, float x, int32 y, float z);

	UFUNCTION(BlueprintCallable, Category = "Test")
		UObject* TestObjectArgsAndReturn(UObject* x, UObject* y);

	UFUNCTION(BlueprintCallable, Category = "Test")
		int32 TestInt32Return(int32 x, int32 y);

	UFUNCTION(BlueprintCallable, Category = "Test")
		float TestFloatReturn(float x, float y);

	UFUNCTION(BlueprintCallable, Category = "Test")
		bool TestBoolReturn(bool x, bool y);
	UFUNCTION(BlueprintCallable, Category = "Test")
		TEnumAsByte<ETestEnum> TestEnumReturn(TEnumAsByte<ETestEnum> x, TEnumAsByte<ETestEnum> y);
	UFUNCTION(BlueprintCallable, Category = "Test")
		ETestEnumCpp TestEnumCppReturn(ETestEnumCpp x, ETestEnumCpp y);

	UFUNCTION(BlueprintCallable, Category = "Test")
		FString TestStringReturn(const FString& x, const FString& y);
	UFUNCTION(BlueprintCallable, Category = "Test")
		FName TestNameReturn(FName x, FName y);

	UFUNCTION(BlueprintCallable, Category = "Test")
		FVector TestVectorReturn(FVector x, FVector y);
	UFUNCTION(BlueprintCallable, Category = "Test")
		FQuat TestQuatReturn(const FQuat& x, const FQuat& y);
	UFUNCTION(BlueprintCallable, Category = "Test")
		FMatrix TestMatrixReturn(const FMatrix& x, const FMatrix& y);

	UFUNCTION(BlueprintCallable, Category = "Test")
		TArray<FName> TestValueTypeArrayReturn(const TArray<FName>& x, const TArray<FName>& y);
	UFUNCTION(BlueprintCallable, Category = "Test")
		TArray<UObject*> TestObjectArrayReturn(const TArray<UObject*>& x, const TArray<UObject*>& y);

	UFUNCTION(BlueprintCallable, Category = "Test")
		FMonoTestsStruct TestStructReturn(FMonoTestsStruct x, FMonoTestsStruct y);

	UFUNCTION(BlueprintCallable, Category = "Test")
		static int32 TestStaticFunction(int32 x, int32 y);

	UFUNCTION(BlueprintCallable, Category = "Test")
		void TestOutParams(UPARAM(ref) FVector& InOutParam, FVector& OutParam);

	UFUNCTION(BlueprintCallable, Category = "Test")
		int32 TestStructDefaultParams(FVector vec3 = FVector(4.f, 8.f, 15.f), FVector2D vec2 = FVector2D::ZeroVector, FLinearColor lc = FLinearColor(16.f, 23.f, 42.f), FRotator rot = FRotator::ZeroRotator, FColor Color = FColor::Red);

	static void RaiseAssertFailedException(FString Message);
	static void AssertEqualInt(int64 ActualValue, int64 ExpectedValue, const TCHAR* Name);
	static void AssertEqualUInt(uint64 ActualValue, uint64 ExpectedValue, const TCHAR* Name);
	static void AssertEqualBool(bool ActualValue, bool ExpectedValue, const TCHAR* Name);
	static void AssertEqualDouble(double ActualValue, double ExpectedValue, const TCHAR* Name);
	static void AssertEqualString(const FString& ActualValue, const FString& ExpectedValue, const TCHAR* Name);

	template<typename T>
	static void AssertEquals(const T& ActualValue, const T& ExpectedValue, const TCHAR* Name)
	{
		if (ActualValue != ExpectedValue)
		{
			RaiseAssertFailedException(FString::Printf(TEXT("Expected %s to be %s, got %s"), Name, *ExpectedValue.ToString(), *ActualValue.ToString()));
		}
	}

	static void AssertEqualUObject(UObject* ActualValue, UObject* ExpectedValue, const TCHAR* Name);

	template<typename T>
	static void AssertNotNull(T* ActualValue, const TCHAR* Name)
	{
		if (ActualValue == nullptr)
		{
			RaiseAssertFailedException(FString::Printf(TEXT("Expected %s to be non-null, got nullptr"), Name));
		}
	}
};