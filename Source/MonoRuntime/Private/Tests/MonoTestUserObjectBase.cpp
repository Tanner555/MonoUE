// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "Tests/MonoTestUserObjectBase.h"
#include "Tests/MonoTestsObject.h"
#include "Tests/MonoTestSubObject.h"
#include "MonoRuntimeCommon.h"

#include "Engine/Light.h"
#include "UObject/UObjectIterator.h"

UMonoTestUserObjectBase::UMonoTestUserObjectBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMonoTestUserObjectBase::Reset(UMonoTestsObject* InTestsObject)
{
	TestsObject = InTestsObject;
	OnReset();
}

void UMonoTestUserObjectBase::RunTests()
{
	OnRunTests();
}

void UMonoTestUserObjectBase::RunOverridableFloatReturnTest()
{
	float Result = TestOverridableFloatReturn(22.f, 42.f);
	UMonoTestsObject::AssertEqualDouble(Result, 22.f, TEXT("Result"));
}

void UMonoTestUserObjectBase::AssertUProperty(UProperty* ActualProperty, UClass* ExpectedType, const FString& ExpectedName, const TCHAR* Name)
{
	TestsObject->AssertEqualUObject(ActualProperty->GetClass(), ExpectedType, *FString::Printf(TEXT("%s->GetClass()"), Name));
	TestsObject->AssertEqualString(ActualProperty->GetName(), ExpectedName, *FString::Printf(TEXT("%s->GetName()"), Name));
}

void UMonoTestUserObjectBase::RunManagedUFunctionTest()
{
	UFunction* ManagedUFunction = FindFunction(TEXT("ManagedUFunction"));
	TestsObject->AssertNotNull(ManagedUFunction, TEXT("ManagedUFunction"));
	TestsObject->AssertEqualInt(ManagedUFunction->NumParms, 4, TEXT("ManagedUFunction->NumParms"));

	UProperty* Parm1 = ManagedUFunction->PropertyLink;
	AssertUProperty(Parm1, UIntProperty::StaticClass(), TEXT("X"), TEXT("Parm1"));

	UProperty* Parm2 = Parm1->PropertyLinkNext;
	AssertUProperty(Parm2, UIntProperty::StaticClass(), TEXT("Y"), TEXT("Parm2"));

	UProperty* Parm3 = Parm2->PropertyLinkNext;
	AssertUProperty(Parm3, UStrProperty::StaticClass(), TEXT("Z"), TEXT("Parm3"));

	UProperty* Parm4 = Parm3->PropertyLinkNext;
	AssertUProperty(Parm4, UIntProperty::StaticClass(), TEXT("ReturnValue"), TEXT("Parm4"));

	uint8* Parms = reinterpret_cast<uint8*>(FMemory::Malloc(ManagedUFunction->ParmsSize));
	*(int*)(Parms + Parm1->GetOffset_ForUFunction()) = 24601;
	*(int*)(Parms + Parm2->GetOffset_ForUFunction()) = 108;
	FString ParamString("Apepe");

	TCHAR** strPtr = (TCHAR**)(Parms + Parm3->GetOffset_ForUFunction());
	*strPtr = (TCHAR*) *ParamString;

	uint8* ReturnValueMemory = Parms + Parm4->GetOffset_ForUFunction();
	Parm4->InitializeValue(ReturnValueMemory);

	ProcessEvent(ManagedUFunction, Parms);

	int ReturnValue = *(int*)ReturnValueMemory;
	TestsObject->AssertEqualInt(ReturnValue, 24601, TEXT("ReturnValue"));

	Parm4->DestroyValue(ReturnValueMemory);
}

void UMonoTestUserObjectBase::RunManagedUFunctionSubclassOfTest()
{
	UFunction* ManagedUFunction = FindFunction(TEXT("ManagedUFunctionSubclassOfTest"));
	TestsObject->AssertNotNull(ManagedUFunction, TEXT("ManagedUFunctionSubclassOfTest"));
	TestsObject->AssertEqualInt(ManagedUFunction->NumParms, 2, TEXT("ManagedUFunctionSubclassOfTest->NumParms"));

	UProperty* Parm1 = ManagedUFunction->PropertyLink;
	AssertUProperty(Parm1, UClassProperty::StaticClass(), TEXT("param"), TEXT("Parm1"));

	UProperty* Parm2 = Parm1->PropertyLinkNext;
	AssertUProperty(Parm2, UClassProperty::StaticClass(), TEXT("ReturnValue"), TEXT("Parm2"));

	uint8* Parms = reinterpret_cast<uint8*>(FMemory::Malloc(ManagedUFunction->ParmsSize));
	*((TSubclassOf<AActor>*)Parms + Parm1->GetOffset_ForUFunction()) = TSubclassOf<AActor>(ALight::StaticClass());

	uint8* ReturnValueMemory = Parms + Parm2->GetOffset_ForUFunction();
	Parm2->InitializeValue(ReturnValueMemory);

	ProcessEvent(ManagedUFunction, Parms);

	TSubclassOf<AActor> ReturnValue = *(TSubclassOf<AActor>*)ReturnValueMemory;
	TestsObject->AssertEqualUObject(*ReturnValue, ALight::StaticClass(), TEXT("ReturnValue"));

	Parm2->DestroyValue(ReturnValueMemory);
}

void UMonoTestUserObjectBase::RunManagedUFunctionArrayTest()
{
	UFunction* ManagedUFunction = FindFunction(TEXT("ManagedUFunctionArrayTest"));
	TestsObject->AssertNotNull(ManagedUFunction, TEXT("ManagedUFunctionArrayTest"));
	TestsObject->AssertEqualInt(ManagedUFunction->NumParms, 2, TEXT("ManagedUFunctionArrayTest->NumParms"));

	UProperty* Parm1 = ManagedUFunction->PropertyLink;
	AssertUProperty(Parm1, UArrayProperty::StaticClass(), TEXT("param"), TEXT("Parm1"));

	UProperty* Parm2 = Parm1->PropertyLinkNext;
	AssertUProperty(Parm2, UArrayProperty::StaticClass(), TEXT("ReturnValue"), TEXT("Parm2"));

	uint8* Parms = reinterpret_cast<uint8*>(FMemory::Malloc(ManagedUFunction->ParmsSize));
	Parm1->InitializeValue_InContainer(Parms);
	TArray<UObject*>& ArrayParam = *((TArray<UObject*>*)(Parms + Parm1->GetOffset_ForUFunction()));
	ArrayParam.Add(TestsObject);
	ArrayParam.Add(this);

	uint8* ReturnValueMemory = Parms + Parm2->GetOffset_ForUFunction();
	Parm2->InitializeValue(ReturnValueMemory);

	ProcessEvent(ManagedUFunction, Parms);

	TArray<UObject*> ReturnValue = *(TArray<UObject*>*)ReturnValueMemory;
	TestsObject->AssertEqualInt(ReturnValue.Num(), 1, TEXT("ReturnValue.Num()"));
	TestsObject->AssertEqualUObject(ReturnValue[0], TestsObject->TestSubObject, TEXT("ReturnValue[0]"));

	Parm1->DestroyValue_InContainer(Parms);
	Parm2->DestroyValue(ReturnValueMemory);
}

void UMonoTestUserObjectBase::RunDynamicDelegateTest()
{
	FManagedUFunctionSignature Delegate;
	Delegate.BindUFunction(this, "ManagedUFunction");

	float Result = Delegate.Execute(24601, 108, TEXT("Apepe"));

	Delegate.Unbind();

	TestsObject->AssertEqualInt(Result, 24601, TEXT("Result"));
}

void UMonoTestUserObjectBase::RunOverridableParamTest()
{
	FMonoTestsStruct TestStruct(42, 22.f, nullptr);
	TestStruct.TestSubStruct.bTestBool1 = false;
	TestStruct.TestSubStruct.bTestBool2 = true;

	TestOverridableParams(TEXT("Foo"), TestStruct);
}

void UMonoTestUserObjectBase::RunOverridableOutParamTest()
{
	int x;
	TArray<FName> y;
	TestOverridableOutParams(x, y);

	TestsObject->AssertEqualInt(x, 42, TEXT("x"));
	TestsObject->AssertEqualInt(y.Num(), 2, TEXT("y.Num()"));
	TestsObject->AssertEqualString(y[0].ToString(), TEXT("Warehouse_13"), TEXT("y[0].ToString()"));
	TestsObject->AssertEqualString(y[1].ToString(), TEXT("Reno_911"), TEXT("y[1].ToString()"));
}
void UMonoTestUserObjectBase::RunManagedUFunctionOutParamTest()
{
	UFunction* ManagedUFunction = FindFunction(TEXT("ManagedUFunctionOutParamTest"));
	TestsObject->AssertNotNull(ManagedUFunction, TEXT("ManagedUFunctionOutParamTest"));
	TestsObject->AssertEqualInt(ManagedUFunction->NumParms, 3, TEXT("ManagedUFunctionOutParamTest->NumParms"));
	TestsObject->AssertEqualBool(ManagedUFunction->HasAllFunctionFlags(FUNC_HasOutParms), true, TEXT("FUNC_HasOutParms"));

	UProperty* Parm1 = ManagedUFunction->PropertyLink;
	AssertUProperty(Parm1, UStrProperty::StaticClass(), TEXT("x"), TEXT("Parm1"));

	UProperty* Parm2 = Parm1->PropertyLinkNext;
	AssertUProperty(Parm2, UIntProperty::StaticClass(), TEXT("y"), TEXT("Parm2"));

	UProperty* Parm3 = Parm2->PropertyLinkNext;
	AssertUProperty(Parm3, UArrayProperty::StaticClass(), TEXT("z"), TEXT("Parm3"));

	uint8* Parms = reinterpret_cast<uint8*>(FMemory::Malloc(ManagedUFunction->ParmsSize));

	Parm1->InitializeValue_InContainer(Parms);
	FString& StringRefParm = *((FString*)(Parms + Parm1->GetOffset_ForUFunction()));
	StringRefParm = TEXT("Fahrenheit");

	int& IntRefParm = *((int*)(Parms + Parm2->GetOffset_ForUFunction()));
	IntRefParm = 451;

	TArray<UObject*>& ArrayOutParm = *((TArray<UObject*>*)(Parms + Parm3->GetOffset_ForUFunction()));

	ProcessEvent(ManagedUFunction, Parms);

	TestsObject->AssertEqualString(StringRefParm, TEXT("Catch"), TEXT("StringRefParm"));
	TestsObject->AssertEqualInt(IntRefParm, 22, TEXT("IntRefParm"));
	TestsObject->AssertEqualInt(ArrayOutParm.Num(), 2, TEXT("ArrayOutParm.Num()"));
	TestsObject->AssertEqualUObject(ArrayOutParm[0], this, TEXT("ArrayOutParm[0]"));
	TestsObject->AssertEqualUObject(ArrayOutParm[1], TestsObject->TestSubObject, TEXT("ArrayOutParm[1]"));

	Parm1->DestroyValue_InContainer(Parms);
	Parm3->DestroyValue_InContainer(Parms);
}


FString UMonoTestUserObjectBase::GetTestUserEnumByName(int index)
{
	UEnum *TestUserEnum = nullptr;
	for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
	{
		UEnum *Enum = *EnumIt;
		if (Enum->GetName() == TEXT("TestUserEnum"))
		{
			TestUserEnum = Enum;
			break;
		}
	}

	TestsObject->AssertNotNull(TestUserEnum, TEXT("TestUserEnum"));

	return TestUserEnum->GetNameStringByIndex(index);
}