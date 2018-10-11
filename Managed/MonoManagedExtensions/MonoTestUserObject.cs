// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Reflection;
using UnrealEngine.Runtime;
using UnrealEngine.Core;
using UnrealEngine.MonoRuntime;
using UnrealEngine.InputCore;
using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace UnrealEngine.ManagedExtensions
{
    [UStruct]
    struct TestUserSubStructBlittable
    {
        public int X;
        public float Y;
        public bool Z;

        public override string ToString()
        {
            return String.Format("X={0}, Y={1}, Z={2}", X, Y, Z);
        }
    }

    [UStruct]
    struct TestUserSubStruct
    {
        public UnrealObject Foo;
        public SubclassOf<UnrealObject> Bar;

        public override string ToString()
        {
            return String.Format("Foo={0}, Bar={1}", Foo == null ? "null" : Foo.ToString(), Bar);
        }
    }

    [UStruct]
    struct TestUserStruct
    {
        public TestUserSubStruct Sub;
        public TestUserSubStructBlittable BlittableSub;

        public override string ToString()
        {
            return String.Format("Sub=({0}), BlittableSub=({1})", Sub, BlittableSub);
        }
    }

    [UStruct]
    public struct UserWeakRefStruct
    {
        public WeakObject<Core.Object> WeakRef1;
        public WeakObject<Core.Object> WeakRef2;

        public override string ToString()
        {
            return string.Format("WeakRef1=({0}), WeakRef2=({1})", WeakRef1, WeakRef2);
        }
    }

    [UEnum]
    public enum TestUserEnum : byte
    {
        A,
        B,
        C,
        One,
        Two,
        Three
    }

    [UStruct]
    struct TestStructConst
    {
        //test that the marshaller doesn't try to marshal the const
        const int TestConst = 7;
        public int ActualValue;

        // this makes it not blittable, so a marshaller is generated
        public UnrealObject Foo;
    }

    public class MonoTestUserObject : MonoTestUserObjectBase
    {
        [UProperty]
        float TestFloat { get; set;}

        [UProperty]
        int TestInt { get; set; }

        [UProperty]
        bool TestBool { get; set; }

        [UProperty]
        sbyte TestSByte { get; set; }

        [UProperty]
        short TestInt16 { get; set; }

        [UProperty]
        long TestInt64 { get; set; }

        [UProperty]
        byte TestByte { get; set; }

        [UProperty]
        ushort TestUInt16 { get; set; }

        [UProperty]
        uint TestUInt32 { get; set; }

        [UProperty]
        ulong TestUInt64 { get; set; }

        [UProperty]
        double TestDouble { get; set; }

        [UProperty]
        PixelFormat TestEnum { get; set; }

        [UProperty]
        ControllerHand TestEnumCpp { get; set; }

        [UProperty]
        MonoTestsObject TestTestsObject { get; set; }

        [UProperty]
        OpenTK.Vector3 TestVector3 { get; set; }

        [UProperty]
        Name TestName { get; set; }

        [UProperty]
        IList<Name> TestNameArray { get; set; }

        [UProperty]
        Rotator TestRotator { get; set; }

        [UProperty]
        Color TestColor { get; set; }

        [UProperty]
        String TestString { get; set; }

        [UProperty]
        MonoTestsStruct TestStruct { get; set; }

        [UProperty]
        IList<int> TestIntArray { get; set; }

        [UProperty]
        IList<UnrealObject> TestObjArray { get; set; }

        [UProperty]
        WeakObject<Core.Object> TestWeakObject { get; set; }

        [UProperty]
        SubclassOf<UnrealEngine.Engine.Actor> TestSubclassOf { get; set; }

        [UProperty]
        Text TestText { get; set; }

        // check assembly processor can rewrite the ctor's direct reference to the backing field
        [UProperty]
        public int TestPropertyInitializer { get; set; } = 5;

        //check assembly processor can inject a private set method for the ctor to use
        [UProperty]
        public bool TestGetOnlyPropertyInitializer { get; } = true;

        // Create object constructor
        protected MonoTestUserObject(ObjectInitializer initializer)
            : base(initializer)
        { 
        }

        // Hot-reload constructor
        protected MonoTestUserObject(IntPtr nativeObject)
            : base(nativeObject)
        {
        }

        protected override void OnReset()
        {
            TestFloat = 42.0f;
            TestDouble = 108.0;
            TestBool = false;
            TestSByte = 43;
            TestInt16 = 44;
            TestInt = 42;
            TestInt64 = 4815162342108;
            TestByte = 43;
            TestUInt16 = 44;
            TestUInt32 = 0xc001beef;
            TestUInt64 = 0xdeadbeefdeadbeef;
            TestTestsObject = null;
            TestEnum = PixelFormat.A8R8G8B8;
            TestEnumCpp = ControllerHand.Special7;
            TestVector3 = new OpenTK.Vector3(42.0f, 42.0f, 42.0f);
            TestName = new Name("Meef");
            TestRotator = new Rotator(42.0f, 42.0f, 42.0f);
            TestColor = new Color(42, 42, 42, 42);
            TestString = "Meef";
            TestText.SetFromString("Bork bork bork");
            TestSubclassOf = new SubclassOf<UnrealEngine.Engine.Actor>(typeof(UnrealEngine.Engine.Light));


            TestStruct = new MonoTestsStruct
            { 
                TestStructFloat = 42.0f, 
                TestStructInt32 = -42, 
                TestStructWeakObject = 
                TestsObject, 
                TestSubStruct = new MonoTestsSubStruct 
                { 
                    TestBool1 = true, 
                    TestBool2 = false,
                },
            };

            TestIntArray.Clear();
            TestIntArray.Add(2);
            TestIntArray.Add(4);
            TestIntArray.Add(6);
            TestIntArray.Add(0);
            TestIntArray.Add(1);

            TestObjArray.Clear();
            TestObjArray.Add(this);
            TestObjArray.Add(TestsObject);

            TestNameArray.Clear();
            TestNameArray.Add(new Name("Foo"));
            TestNameArray.Add(new Name("Bar"));
            TestNameArray.Add(new Name("Hoobajoob"));
            TestNameArray.Add(new Name("Doowacky"));

            TestWeakObject = this;
        }

        public void TestBasicProperties()
        {
            TestsObject.AssertEqual(TestFloat, 42.0f, "TestFloat");
            TestsObject.AssertEqual(TestDouble, 108.0, "TestDouble");
            TestsObject.AssertEqual(TestBool, false, "TestBool");
            TestsObject.AssertEqual(TestSByte, 43, "TestSByte");
            TestsObject.AssertEqual(TestInt16, 44, "TestInt16");
            TestsObject.AssertEqual(TestInt, 42, "TestInt");
            TestsObject.AssertEqual(TestInt64, 4815162342108, "TestInt64");
            TestsObject.AssertEqual(TestByte, 43, "TestByte");
            TestsObject.AssertEqual(TestUInt16, 44, "TestUInt16");
            TestsObject.AssertEqual(TestUInt32, 0xc001beef, "TestUInt32");
            TestsObject.AssertEqual(TestUInt64, 0xdeadbeefdeadbeef, "TestUInt64");
            TestsObject.AssertEqual(TestEnum, PixelFormat.A8R8G8B8, "TestEnum");
            TestsObject.AssertEqual(TestEnumCpp, ControllerHand.Special7, "TestEnumCpp");
            TestsObject.AssertEqual(TestText.ToString(), "Bork bork bork", "TestText");
            TestFloat = -42.0f;
            TestDouble = -108.0;
            TestInt = -42;
            TestInt64 = -4815162342108;
            TestEnum = PixelFormat.BC4;
            TestEnumCpp = ControllerHand.AnyHand;
            TestsObject.AssertEqual(TestFloat, -42.0f, "TestFloat-Write");
            TestsObject.AssertEqual(TestDouble, -108.0, "TestDouble-Write");
            TestsObject.AssertEqual(TestInt, -42, "TestInt-Write");
            TestsObject.AssertEqual(TestInt64, -4815162342108, "TestInt64-Write");
            TestsObject.AssertEqual(TestEnum, PixelFormat.BC4, "TestEnum-Write");
            TestsObject.AssertEqual(TestEnumCpp, ControllerHand.AnyHand, "TestEnumCpp-Write");
            TestsObject.AssertEqual(TestPropertyInitializer, 5, "TestPropertyInitializer");
            TestsObject.AssertEqual(TestGetOnlyPropertyInitializer, true, "TestGetOnlyPropertyInitializer");
        }
        
        public void TestStructProperties()
        {
            TestsObject.AssertEqual(TestVector3, new OpenTK.Vector3(42.0f, 42.0f, 42.0f), "TestVector");
            TestsObject.AssertEqual(TestName, new Name("Meef"), "TestName");
            TestsObject.AssertEqual(TestRotator, new Rotator(42.0f, 42.0f, 42.0f), "TestRotator");
            TestsObject.AssertEqual(TestColor, new Color(42, 42, 42, 42), "TestColor");
            TestsObject.AssertEqual(TestString, "Meef", "TestString");
            MonoTestsStruct testValue = new MonoTestsStruct
            {
                TestStructFloat = 42.0f,
                TestStructInt32 = -42,
                TestStructWeakObject = TestsObject,
                TestSubStruct = new MonoTestsSubStruct
                {
                    TestBool1 = true,
                    TestBool2 = false,
                },
            };
            TestsObject.AssertEqual(TestStruct, testValue, "TestStruct");
            TestString = "Foo";
            TestsObject.AssertEqual(TestString, "Foo", "TestString-Write");
        }

        public void TestObjectProperties()
        {
            TestsObject.AssertNull(TestTestsObject, "TestTestsObject");
            TestTestsObject = TestsObject;
            if(TestTestsObject != TestsObject)
            {
                throw AssertionFailedException.Create(TestTestsObject, TestsObject, "TestTestsObject-Write");
            }
        }

        public void TestArrayProperties()
        {
            int[] expected = new int[] { 2, 4, 6, 0, 1 };
            MonoTestsObject.TestArray(expected, new ReadOnlyCollection<int>(TestIntArray), "TestIntArray", null);

            UnrealObject[] expectedObjs = new UnrealObject[] { this, TestsObject };
            MonoTestsObject.TestArray(expectedObjs, new ReadOnlyCollection<UnrealObject>(TestObjArray), "TestObjArray", null);

            Name[] expectedNames = new Name[] { new Name("Foo"), new Name("Bar"), new Name("Hoobajoob"), new Name("Doowacky") };
            MonoTestsObject.TestArray(expectedNames, new ReadOnlyCollection<Name>(TestNameArray), "TestNameArray", null);
        }

        protected override float TestOverridableFloatReturn(float X, float Y)
        {
            TestsObject.AssertEqual(Y, 42.0f, "Y");
            return X;
        }

        public void TestOverriddenFunctionReturn()
        {
            RunOverridableFloatReturnTest();
        }

        protected override void TestOverridableParams(string str, MonoTestsStruct strukt)
        {
            TestsObject.AssertEqual(str, "Foo", "str");
            TestsObject.AssertEqual(strukt.TestStructFloat, 22.0f, "strukt.TestStructFloat");
            TestsObject.AssertEqual(strukt.TestStructInt32, 42, "strukt.TestStructInt32");
            TestsObject.AssertEqual(strukt.TestStructWeakObject.IsValid(), false, "strukt.TestStructWeakObject.IsValid()");

            TestsObject.AssertEqual(strukt.TestSubStruct.TestBool1, false, "strukt.TestSubStruct.TestBool1");
            TestsObject.AssertEqual(strukt.TestSubStruct.TestBool2, true, "strukt.TestSubStruct.TestBool2");
        }
        public void TestOverridableParams()
        {
            RunOverridableParamTest();
        }

        protected override void TestOverridableOutParams(out int x, out IList<Name> y)
        {
            x = 42;

            y = new List<Name>();
            y.Add(new Name("Warehouse_13"));
            y.Add(new Name("Reno_911"));
        }
        public void TestOverridableOutParams()
        {
            RunOverridableOutParamTest();
        }

        [UFunction, BlueprintCallable]
		public int ManagedUFunction(int X, int Y, string Z)
        {
            TestsObject.AssertEqual(Y, 108, "Y");
			TestsObject.AssertEqual(Z, "Apepe", "Z");
            return X;
        }
        public void TestManagedUFunction()
        {
            RunManagedUFunctionTest();
        }

        [UFunction]
        public SubclassOf<UnrealEngine.Engine.Actor> ManagedUFunctionSubclassOfTest (SubclassOf<UnrealEngine.Engine.Actor> param)
        {
            TestsObject.AssertEqual(param, typeof(UnrealEngine.Engine.Light), "ManagedUFunctionSubclassOfTest.param");
            return param;
        }
        public void TestManagedUFunctionSubclassOfParams()
        {
            RunManagedUFunctionSubclassOfTest();
        }

        [UFunction]
        public IList<Core.Object> ManagedUFunctionArrayTest(IList<Core.Object> param)
        {
            TestsObject.AssertEqual(param.Count, 2, "ManagedUFunctionArrayTest param.Count");
            TestsObject.AssertEqual(param[0], TestsObject, "ManagedUFunctionArrayTest param[0]");
            TestsObject.AssertEqual(param[1], this, "ManagedUFunctionArrayTest param[1]");

            IList<Core.Object> toReturn = new List<Core.Object>();
            toReturn.Add(TestsObject.TestSubObject);
            return toReturn;
        }
        public void TestManagedUFunctionarrayParams()
        {
            RunManagedUFunctionArrayTest();
        }

        [UFunction]
        public void ManagedUFunctionOutParamTest(ref string x, ref int y, out IList<Core.Object> z)
        {
            TestsObject.AssertEqual("Fahrenheit", x, "x");
            TestsObject.AssertEqual(451, y, "y");

            x = "Catch";
            y = 22;

            z = new List<Core.Object>();
            z.Add(this);
            z.Add(TestsObject.TestSubObject);
        }
        public void TestManagedUFunctionOutParams()
        {
            RunManagedUFunctionOutParamTest();
        }

        public void TestUserEnums()
        {
            string enumName = GetTestUserEnumByName(2);
            TestsObject.AssertEqual(enumName, "C", "TestUserEnum.C");
        }

        public void TestDynamicDelegate()
        {
            RunDynamicDelegateTest();
        }

        public void TestSubclassOfProperties()
        {
            TestsObject.AssertEqual(TestSubclassOf, typeof(UnrealEngine.Engine.Light), "TestSubclassOf.Class");
        }

        [UFunction]
        [RPC(Endpoint.Server, WithValidation=true)]
        public void ManagedServerFunction(bool valid, string str, MonoTestsStruct strukt)
        {
            TestsObject.AssertEqual(valid, true, "valid");
            TestsObject.AssertEqual(str, "Foo", "str");
            TestsObject.AssertEqual(strukt.TestStructFloat, 108.0f, "strukt.TestStructFloat");
            TestsObject.AssertEqual(strukt.TestStructInt32, 24601, "strukt.TestStructInt32");
            TestsObject.AssertEqual(strukt.TestSubStruct.TestBool1, false, "strukt.TestSubStruct.TestBool1");
            TestsObject.AssertEqual(strukt.TestSubStruct.TestBool2, true, "strukt.TestSubStruct.TestBool2");
        }
        public bool ManagedServerFunction_Validate(bool valid, string str, MonoTestsStruct strukt)
        {
            return valid;
        }

        public void TestRPC()
        {
            MonoTestsStruct testStruct = new MonoTestsStruct
            {
                TestStructFloat = 108.0f,
                TestStructInt32 = 24601,
                TestSubStruct = new MonoTestsSubStruct { TestBool1 = false, TestBool2 = true },
            };
            ManagedServerFunction(true, "Foo", testStruct);
            string RPCFailReason = UnrealInterop.RPCGetLastFailedReason();
            TestsObject.AssertNull(RPCFailReason, "RPCFailReason");

            ManagedServerFunction(false, "Bar", testStruct);
            RPCFailReason = UnrealInterop.RPCGetLastFailedReason();
            TestsObject.AssertEqual(RPCFailReason, "ManagedServerFunction_Validate", "RPCFailReason");

            UnrealInterop.RPCResetLastFailedReason();
        }

        [UProperty]
        TestUserStruct TestUserStruct { get; set; }

        [UProperty]
        IList<TestUserStruct> TestUserStructArray { get; set; }

        public void TestUserStructs()
        {
            TestUserStruct expected = new TestUserStruct
            {
                Sub = new TestUserSubStruct
                {
                    Foo = this,
                    Bar = typeof(UnrealEngine.Engine.Actor),
                },
                BlittableSub = new TestUserSubStructBlittable
                {
                    X = 22,
                    Y = 42.0f,
                    Z = true,
                },
            };

            TestUserStruct = expected;
            TestsObject.AssertEqual(TestUserStruct, expected, "TestUserStruct");

            // Verify the generated IUnrealArrayMarshaler, too.
            TestUserStructArray.Add(expected);
            MonoTestsObject.TestArray(new TestUserStruct[] { expected }, new ReadOnlyCollection<TestUserStruct>(TestUserStructArray), "TestUserStructArray", null);
        }

        public void TestWeakObjectProperty()
        {
            TestsObject.AssertEqual(TestWeakObject.Object, this, "TestWeakObject.Object");

            TestWeakObject = TestsObject;
            TestsObject.AssertEqual(TestWeakObject.Object, TestsObject, "TestWeakObject.Object");

            // Weakrefs can be set null as a shorthand for invalidation, but we still expect the getter to return an object.
            TestWeakObject = null;
            TestsObject.AssertNotNull(TestWeakObject, "TestWeakObject");
            TestsObject.AssertReturn(TestWeakObject.IsValid(), false, "TestWeakObject.IsValid");
        }

        [UProperty]
        UserWeakRefStruct TestWeakRefStruct { get; set; }

        public void TestUserStructWeakObjectProperty()
        {
            UserWeakRefStruct expected = new UserWeakRefStruct()
            {
                WeakRef1 = TestsObject,
                WeakRef2 = this,
            };

            TestWeakRefStruct = expected;
            TestsObject.AssertEqual(TestWeakRefStruct, expected, "TestWeakRefStruct");
        }

        [UProperty]
        TestStructConst TestStructConstProp { get; set; }

        public void TestStructConst()
        {
            var expected = new TestStructConst { ActualValue = 7, Foo = null };
            TestStructConstProp = expected;
            TestsObject.AssertEqual(TestStructConstProp, expected, "TestStructConstProp");
        }

        [UProperty(ArrayDim=5)]
        FixedSizeArrayReadWrite<int> TestFixedIntArray { get; set; }

        [UProperty(ArrayDim=3)]
        FixedSizeArrayReadWrite<UnrealObject> TestFixedObjectArray { get; set; }

        public void TestFixedArrays()
        {
            TestsObject.AssertEqual(TestFixedIntArray.Length, 5, "TestFixedIntArray.Length");
            for (int i = 0; i < TestFixedIntArray.Length; ++i)
            {
                TestFixedIntArray[i] = i * i;
                TestsObject.AssertEqual(TestFixedIntArray[i], i * i, "TestFixedIntArray[i]");
            }

            TestsObject.AssertEqual(TestFixedObjectArray.Length, 3, "TestFixedObjectArray.Length");
            TestFixedObjectArray[0] = this;
            TestsObject.AssertEqual(TestFixedObjectArray[0], this, "TestFixedObjectArray[0]");
            TestFixedObjectArray[1] = null;
            TestsObject.AssertEqual(TestFixedObjectArray[1], null, "TestFixedObjectArray[1]");
            TestFixedObjectArray[2] = TestsObject;
            TestsObject.AssertEqual(TestFixedObjectArray[2], TestsObject, "TestFixedObjectArray[2]");
        }


        protected override void OnRunTests()
        {
            int TestCount = 0;
            int FailCount = 0;
            MethodInfo[] methods = typeof(MonoTestUserObject).GetMethods(BindingFlags.Public | BindingFlags.Instance);
            foreach (MethodInfo m in methods)
            {
                if (m.GetParameters().Length == 0 && m.Name.StartsWith("Test"))
                {
                    ++TestCount;
                    try
                    {
                        this.OnReset();
                        Console.WriteLine("Running {0}...", m.Name);
                        m.Invoke(this, null);
                    }
                    // Since we're invoking via reflection, any exception thrown by the test will be repackaged.
                    catch (TargetInvocationException tie)
                    {
                        if (tie.InnerException is AssertionFailedException)
                        {
                            Console.WriteLine("{0} failed: {1}", m.Name, tie.InnerException.Message);
                            Console.WriteLine(tie.InnerException.StackTrace);
                            ++FailCount;
                        }
                        else
                        {
                            // All other exception types are considered fatal.
                            Console.WriteLine("Fatal error: " + tie.InnerException.ToString());
                            Console.WriteLine("Aborting tests.");
                            TestsObject.FailTest();
                        }
                    }
                    catch (Exception e)
                    {
                        // Other reflection exceptions are fatal, too.
                        Console.WriteLine("Fatal reflection error: " + e.ToString());
                        Console.WriteLine("Aborting tests.");
                        TestsObject.FailTest();
                    }
                }
            }

            Console.WriteLine("{0}/{1} tests passed.", TestCount - FailCount, TestCount);
            if (FailCount > 0)
            {
                TestsObject.FailTest();
            }
        
        }
    }
}
