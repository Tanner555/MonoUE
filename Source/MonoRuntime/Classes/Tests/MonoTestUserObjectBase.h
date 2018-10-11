// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MonoTestsObject.h"
#include "MonoTestUserObjectBase.generated.h"

class UMonoTestsObject;

DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(int32, FManagedUFunctionSignature, int32, X, int32, Y, FString, Z);

UCLASS(abstract,Blueprintable)
class UMonoTestUserObjectBase : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Test")
		UMonoTestsObject* TestsObject;
public:

	void Reset(UMonoTestsObject* InTestsObject);

	UFUNCTION(BlueprintCallable, Category="Test")
		void RunTests();

	UFUNCTION(BlueprintImplementableEvent)
		void OnReset();

	UFUNCTION(BlueprintImplementableEvent)
		void OnRunTests();

	UFUNCTION(BlueprintImplementableEvent)
		float TestOverridableFloatReturn(float x, float y);
	UFUNCTION(BlueprintCallable, Category="Test")
		virtual void RunOverridableFloatReturnTest();

	UFUNCTION(BlueprintCallable, Category = "Test")
		virtual void RunManagedUFunctionTest();

	UFUNCTION(BlueprintCallable, Category = "Test")
		virtual void RunManagedUFunctionSubclassOfTest();

	UFUNCTION(BlueprintCallable, Category = "Test")
		virtual void RunManagedUFunctionArrayTest();

	UFUNCTION(BlueprintCallable, Category = "Test")
		virtual void RunDynamicDelegateTest();

	UFUNCTION(BlueprintImplementableEvent)
		void TestOverridableParams(const FString& Str, FMonoTestsStruct TestStruct);
	UFUNCTION(BlueprintCallable, Category = "Test")
		void RunOverridableParamTest();

	UFUNCTION(BlueprintImplementableEvent)
		void TestOverridableOutParams(int32& X, TArray<FName>& Y);
	UFUNCTION(BlueprintCallable, Category = "Test")
		void RunOverridableOutParamTest();
	UFUNCTION(BlueprintImplementableEvent)
		TArray<UObject*> TestOverridableArrayParams(const TArray<UObject*>& parm);
	UFUNCTION(BlueprintCallable, Category = "Test")
		void RunManagedUFunctionOutParamTest();
	UFUNCTION(BlueprintCallable, Category = "Test")
		FString GetTestUserEnumByName(int32 index);

private:
	void AssertUProperty(UProperty* ActualProperty, UClass* ExpectedType, const FString& ExpectedName, const TCHAR* Name);
};