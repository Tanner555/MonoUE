// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoRuntimePrivate.h"
#include "MonoBindings.h"
#include "MonoHelpers.h"
#include "Tests/MonoTestsObject.h"
#include "Misc/AutomationTest.h"

// Disable GC tests until we can disable consideration of main thread's stack
#define MONO_HACK_GC_TEST_UNTIL_HAVE_MORE_CONTROL 1

#define MONO_TEST_TEXT( Format, ... ) FString::Printf(TEXT("%s - %d: %s"), TEXT(__FILE__) , __LINE__ , *FString::Printf(TEXT(Format), ##__VA_ARGS__) )

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMonoRuntimeBindingTests, "MonoRuntime.Mono Binding Tests", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMonoRuntimeBindingTests::RunTest(const FString& Parameters)
{
#if MONOUE_STANDALONE
	//if (!IMonoRuntime::Get().IsAvailable() || !IMonoRuntime::Get().IsLoaded())
	{
		return false;
	}
#endif

	// run simple runtime tests
	UMonoTestsObject* TestsObject = NewObject<UMonoTestsObject>();
	TestsObject->Tester = this;

	TestsObject->Reset();

	FMonoBindings& Bindings = FMonoBindings::Get();
	MonoClass* TestsObjectClass = Bindings.GetMonoClassFromUnrealClass(*UMonoTestsObject::StaticClass());
	check(TestsObjectClass);

	MonoMethod* RunTestsMethod = Mono::LookupMethodOnClass(TestsObjectClass, ":RunTests");
	check(RunTestsMethod);

	Mono::Invoke<void>(Bindings, RunTestsMethod, Bindings.GetUnrealObjectWrapper(TestsObject));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMonoRuntimeGCTests, "MonoRuntime.Mono GC Tests", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMonoRuntimeGCTests::RunTest(const FString& Parameters)
{
	UMonoTestsObject* TestsObject = NewObject<UMonoTestsObject>();

	TestsObject->AddToRoot();
	TWeakObjectPtr<UMonoTestsObject> WeakPointer = TestsObject;

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	TestTrue(MONO_TEST_TEXT("Survived garbage collection as root"), WeakPointer.Get() == TestsObject);

	TestsObject->RemoveFromRoot();

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

#if MONO_HACK_GC_TEST_UNTIL_HAVE_MORE_CONTROL
	// Mono's gc conservatively traces the stack, which still has a C++ reference to this object in the weak pointer even though
	// we *know* we don't have any actual references. 
	// What we really need to do is tell the manually invoked mono GC inside of CollectGarbage that the main thread's stack should be ignored for GC
	// But there's no way to do that currently.
	if (WeakPointer.Get() != nullptr)
	{
		UE_LOG(LogMono, Warning, TEXT("MonoTestObject not destroyed during GC (perhaps due to conservative stack tracing)"));
	}
#else
	TestTrue(MONO_TEST_TEXT("Destroyed during garbage collection"), WeakPointer.Get() == nullptr);
#endif

	return true;
}
