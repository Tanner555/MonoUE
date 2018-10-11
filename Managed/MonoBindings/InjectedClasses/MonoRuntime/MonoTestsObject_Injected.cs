// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Reflection;
using UnrealEngine.Runtime;
using UnrealEngine.Engine;

namespace UnrealEngine.MonoRuntime
{
    [Serializable]
    public class AssertionFailedException : Exception
    {
        // Zero-arg ctor required for mono_raise_exception
        public AssertionFailedException()
        { }

        public AssertionFailedException(string message)
            : base(message)
        { }

        public static AssertionFailedException Create<T>(T actualValue, T expectedValue, string name)
        {
            return new AssertionFailedException(String.Format("Expected {0} to be {1}, got {2}", name, expectedValue, actualValue));
        }
    }

    public partial class MonoTestsObject
    {

        public void AssertNull<T>(T actualValue, string name) where T : class
        {
            if (null != actualValue)
            {
                throw AssertionFailedException.Create(actualValue, null, name);
            }
        }

        public void AssertNotNull<T>(T actualValue, string name) where T : class
        {
            if (null == actualValue)
            {
                throw new AssertionFailedException(String.Format("Expected {0} to be non-null, got null.", name));
            }
        }

        public void AssertEqual<T>(T actualValue, T expectedValue, string name)
        {
            if (!object.Equals(actualValue, expectedValue))
            {
                throw AssertionFailedException.Create(actualValue, expectedValue, name);
            }
        }

        public void AssertReturn<T>(T actualReturn, T expectedReturn, string methodName, string methodArgs = "")
        {
            if (!object.Equals(expectedReturn, actualReturn))
            {
                throw new AssertionFailedException(String.Format("Expected {0}({1}) to return {2}, got {3}", methodName, methodArgs, expectedReturn, actualReturn));
            }
        }

        public static string EnumerableToString<T>(IEnumerable<T> Enumerable)
        {
            return String.Join<T>(", ", Enumerable);
        }

        public static bool SequenceCompare<T>(IEnumerable<T> source1, IEnumerable<T> source2)
        {
            if (source1 == null || source2 == null)
            {
                throw new ArgumentNullException();
            }

            using (IEnumerator<T> iterator1 = source1.GetEnumerator())
            using (IEnumerator<T> iterator2 = source2.GetEnumerator())
            {
                while (true)
                {
                    bool next1 = iterator1.MoveNext();
                    bool next2 = iterator2.MoveNext();
                    if (!next1 && !next2) // Both sequences finished
                    {
                        return true;
                    }
                    if (!next1) // Only the first sequence has finished
                    {
                        Console.WriteLine("SequenceCompare: only 1st sequence finished");
                        return false;
                    }
                    if (!next2) // Only the second sequence has finished
                    {
                        Console.WriteLine("SequenceCompare: only 2nd sequence finished");
                        return false;
                    }

                    if (iterator1.Current == null && iterator2.Current == null) // Both null is a match, keep comparing
                    {
                        continue;
                    }
                    if (iterator1.Current == null || iterator2.Current == null) // One, but not both, null is a mismatch
                    {
                        Console.WriteLine("SequenceCompare: One, but not both, null");
                        return false;
                    }

                    if (iterator1.Current != null)
                    // If elements are non-equal, we're done
                    if (!iterator1.Current.Equals(iterator2.Current))
                    {
                        Console.WriteLine("SequenceCompare: Mismatch detected");
                        return false;
                    }
                }
            }
        }

        public static void TestArray<T>(T[] Expected, IReadOnlyList<T> Actual, string ArrayName, string test)
        {
            string AnnotatedName = ArrayName;
            if (test != null)
            {
                AnnotatedName = String.Format("{0}-{1}", test, ArrayName);
            }

            if (!SequenceCompare(Expected, Actual))
            {
                throw AssertionFailedException.Create(EnumerableToString(Actual), EnumerableToString(Expected), AnnotatedName);
            }

            if (Expected.Length != Actual.Count)
            {
                throw AssertionFailedException.Create(Actual.Count, Expected.Length, AnnotatedName + ".Count");
            }

            for (int i = 0; i < Expected.Length; ++i)
            {
                if ((Expected[i] != null && !Expected[i].Equals(Actual[i]))
                    || (Expected[i] == null && Actual[i] != null))
                {
                    throw AssertionFailedException.Create(Actual[i], Expected[i], String.Format("{0}[{1}]", AnnotatedName, i));
                }
            }
        }

        public static void TestWriteableArray<T>(T[] Expected, IList<T> Actual, string ArrayName, string test) where T : struct
        {
            // test read-only
            TestArray(Expected, new ReadOnlyCollection<T>(Actual), ArrayName, test);

            // test write operations
            Actual.Clear();

            if (0 != Actual.Count)
            {
                throw new AssertionFailedException(String.Format("Expected {0} to be empty after clear", ArrayName));
            }

            // re-add expected
            foreach (T item in Expected)
            {
                Actual.Add(item);
            }

            TestArray(Expected, new ReadOnlyCollection<T>(Actual), ArrayName, "post-readd");

            // remove second  element
            Actual.RemoveAt(1);

            if (Actual.Count != Expected.Length - 1)
            {
                throw new AssertionFailedException(String.Format("Expected {0} to have {1} elements after removing single element, had {2}", ArrayName, Expected.Length, Actual.Count));
            }

            // re-add second element
            Actual.Insert(1, Expected[1]);

            TestArray(Expected, new ReadOnlyCollection<T>(Actual), ArrayName, "post-removeinsert");
        }

        void RunTests()
        {
            int TestCount = 0;
            int FailCount = 0;
            MethodInfo[] methods = typeof(MonoTestsObject).GetMethods(BindingFlags.Public | BindingFlags.Instance);
            foreach (MethodInfo m in methods)
            {
                if (m.GetParameters().Length == 0 && m.Name.StartsWith("Test"))
                {
                    ++TestCount;
                    try
                    {
                        Reset();
                        Console.WriteLine("Running {0}...", m.Name);
                        m.Invoke(this, null);
                    }
                    // Since we're invoking via reflection, any exception thrown by the test will be repackaged.
                    catch (TargetInvocationException tie)
                    {
                        if (tie.InnerException is AssertionFailedException)
                        {
                            LogTestFailure(String.Format("{0} failed: {1}", m.Name, tie.InnerException.Message));
                            LogTestFailure(tie.InnerException.StackTrace);
                            ++FailCount;
                        }
                        else
                        {
                            // All other exception types are considered fatal.
                            LogTestFailure(String.Format("Fatal error: " + tie.InnerException.ToString()));
                            FailTest();
                        }
                    }
                    catch (Exception e)
                    {
                        // Other reflection exceptions are fatal, too.
                        LogTestFailure(String.Format("Fatal reflection error: " + e.ToString()));
                        FailTest();
                    }
                }
            }

            Console.WriteLine("{0}/{1} tests passed.", TestCount - FailCount, TestCount);
            if (FailCount > 0)
            {
                FailTest();
            }
        }

        public void TestArray()
        {
            int[] TestInts = { 1, 2, 3 };
            TestArray(TestInts, TestArrayInt, "TestArrayInt", "toplevel");
        }

        public void TestWriteableArray()
        {
            float[] TestFloats = { 1.0f, 2.0f, 3.0f };
            TestWriteableArray(TestFloats, TestArrayFloat, "TestArrayFloat", "toplevel");
        }

        public void TestObjectArrayProperty()
        {
            int i = 0;
            foreach (MonoTestSubObject SubObj in TestObjectArray)
            {
                AssertEqual(TestObjectArray[i].TestReadableInt32, i, String.Format("TestObjectArray[{0}].TestReadableInt32", i));
                ++i;
            }
        }

        public void TestWeakObjectProperties()
        {
            AssertEqual(TestWeakObject.Object, TestObjectArray[1],"TestWeakObject.Object");
            TestWeakObject = TestObjectArray[0];
            AssertEqual(TestWeakObject.Object, TestObjectArray[0], "TestWeakObject.Object");

            AssertEqual(TestReadWriteStruct.TestStructWeakObject.Object, TestSubObject, "TestReadWriteStruct.TestStructWeakObject.Object");
            MonoTestsStruct modifiedStruct = TestReadWriteStruct;
            modifiedStruct.TestStructWeakObject = null;
            TestReadWriteStruct = modifiedStruct;
            AssertEqual(TestReadWriteStruct.TestStructWeakObject.IsValid(), false, "TestReadWriteStruct.TestStructWeakObject.IsValid");

            VerifyWeakObjectPropertyEdits();
        }

        public void TestObjectProperties()
        {
            // subobject
            AssertNull(TestNullObject, "TestNullObject");
            AssertEqual(TestSubObject.TestReadableInt32, 42, "TestSubObject.TestReadableInt32");
        }

        public void TestSimpleTypeProperties()
        {
            AssertEqual(TestReadableInt32, 1000000000, "TestReadableInt32");

            AssertEqual(TestReadWriteFloat, -42.0f, "TestReadWriteFloat");
            AssertEqual(TestReadWriteBool, false, "TestReadWriteBool");

            TestReadWriteFloat = 42.0f;
            TestReadWriteBool = true;


            AssertEqual(TestReadWriteEnum, TestEnum.Something, "TestReadWriteEnum");
            AssertEqual(TestReadWriteEnumCpp, TestEnumCpp.Alpha, "TestReadWriteEnumCpp");

            TestReadWriteEnum = TestEnum.SomethingElse;
            TestReadWriteEnumCpp = TestEnumCpp.Beta;

            AssertEqual(TestReadableBool, false, "TestReadWriteBool");
            AssertEqual(TestReadWriteBitfield2, true, "TestReadWriteBitfield2");
            AssertEqual(TestReadWriteBitfield1, true, "TestReadWriteBitfield1");

            TestReadWriteBitfield1 = false;

            VerifySimpleTypePropertyEdits();
        }

        public void TestStringProperties()
        {
            AssertEqual(TestReadWriteString, "Foo", "TestReadWriteString");
            TestReadWriteString = "Bar";

            VerifyStringPropertyEdit();
        }

        public void TestNameProperties()
        {
            AssertEqual(TestReadWriteName.ToString(), "Catch_22", "TestReadWriteName");
            AssertEqual(TestReadWriteName.Number, 23, "TestReadWriteName.Number");
            AssertEqual(TestReadWriteName.PlainName, "Catch", "TestReadWriteName.PlainName");
            TestReadWriteName = new Name("Jim");

            VerifyNamePropertyEdit();
        }

        public void TestTextProperties()
        {
            AssertEqual(TestReadWriteText.ToString(), "This is an English sentence.", "TestReadWriteName");

            TestReadWriteText.SetFromString("This is still an English sentence.");

            VerifyTextPropertyEdit();
        }

        public void TestMathProperties()
        {
            AssertEqual(TestReadableVector2D, new OpenTK.Vector2(2.0f, 2.0f), "TestReadableVector2D");
            AssertEqual(TestReadableVector, new OpenTK.Vector3(4.0f, 8.0f, 15.0f), "TestReadableVector");
            AssertEqual(TestReadableVector4, new OpenTK.Vector4(16.0f, 23.0f, 42.0f, 108.0f), "TestReadableVector4");

            AssertEqual(TestReadWriteQuat, new OpenTK.Quaternion(2, 4, 6, 0.1f), "TestReadWriteQuat");
            TestReadWriteQuat = new OpenTK.Quaternion(1, 2, 3, 4);

            AssertEqual(TestReadWriteMatrix, OpenTK.Matrix4.Identity, "TestReadWriteMatrix");
            TestReadWriteMatrix = new OpenTK.Matrix4(
                0, 1, 2, 3,
                4, 5, 6, 7, 
                8, 9, 10, 11, 
                12, 13, 14, 15);

            AssertEqual(TestReadableRotator, new Rotator(45.0f, 15.0f, 5.0f), "TestReadableRotator");
        }

        public void TestStructProperties()
        {
            AssertEqual(TestReadWriteStruct.TestStructInt32, 22, "TestReadWriteStruct.TestStructInt32");
            AssertEqual(TestReadWriteStruct.TestStructFloat, 451.0f, "TestReadWriteStruct.TestStructFloat");
            AssertEqual(TestReadWriteStruct.TestSubStruct.TestBool1, true, "TestReadWriteStruct.TestSubStruct.bTestBool1");
            AssertEqual(TestReadWriteStruct.TestSubStruct.TestBool2, false, "TestReadWriteStruct.TestSubStruct.bTestBool2");

            MonoTestsStruct ModifiedStruct = TestReadWriteStruct;
            ModifiedStruct.TestStructInt32 = 42;
            ModifiedStruct.TestStructFloat = 24601.0f;
            TestReadWriteStruct = ModifiedStruct;

            AssertEqual(TestReadWriteColor.R, 128, "TestReadWriteColor.R");
            AssertEqual(TestReadWriteColor.G, 128, "TestReadWriteColor.G");
            AssertEqual(TestReadWriteColor.B, 0, "TestReadWriteColor.B");
            AssertEqual(TestReadWriteColor.A, 0, "TestReadWriteColor.A");

            UnrealEngine.Core.Color ModifiedColor = TestReadWriteColor;
            ModifiedColor.G = 0;
            ModifiedColor.B = 128;
            ModifiedColor.A = 128;
            TestReadWriteColor= ModifiedColor;

            VerifyStructPropertyEdits();
        }

        public void TestStructArray()
        {
            MonoTestsStruct Expected;
            Expected.TestStructInt32 = 22;
            Expected.TestStructFloat = 42.0f;
            Expected.TestStructWeakObject = this;
            Expected.TestSubStruct.TestBool1 = true;
            Expected.TestSubStruct.TestBool2 = false;
            TestArray(new MonoTestsStruct[] { Expected }, new ReadOnlyCollection<MonoTestsStruct>(TestReadWriteStructArray), "TestReadWriteStructArray", null);

            MonoTestsStruct Temp = TestReadWriteStructArray[0];
            Temp.TestStructFloat = 54.0f;
            TestReadWriteStructArray[0] = Temp;
            TestReadWriteStructArray.Add(new MonoTestsStruct { TestStructInt32 = 451, TestStructFloat = 24601.0f, TestStructWeakObject = this });

            VerifyStructArrayPropertyEdits();
        }

        public void TestClassProperties()
        {
            AssertEqual(TestReadWriteClass, typeof(MonoTestsObject), "TestReadWriteClass");
            AssertEqual(TestReadWriteActorClass, typeof(SkeletalMeshActor), "TestReadWriteActorClass");

            AssertEqual(TestReadWriteActorClass.IsChildOf(typeof(Actor)), true, "IsChildOf(Actor)");
            AssertEqual(TestReadWriteActorClass.IsChildOf(typeof(SkeletalMeshActor)), true, "IsChildOf(SkeletalMeshActor)");
            AssertEqual(TestReadWriteActorClass.IsChildOf(typeof(StaticMeshActor)), false, "IsChildOf(StaticMeshActor)");

            TestReadWriteClass = typeof(SceneComponent);
            try
            {
                TestReadWriteActorClass = typeof(MonoTestsObject);
                throw new AssertionFailedException("Unexpected success setting TestReadWriteActorClass to incompatible UClass type.");
            }
            catch (System.ArgumentException)
            {
            	// success!
            }
            TestReadWriteActorClass = typeof(Light);

            VerifyClassPropertyEdits();

            SubclassOf<Actor>[] expected = new SubclassOf<Actor>[]
            {
                typeof(Actor),
                typeof(Pawn),
                typeof(Character),
            };
            TestArray(expected, TestReadWriteActorClassArray, "TestReadWriteActorClassArray", null);
        }

        public void TestOnlyInt32Args()
        {
            TestOnlyInt32Args(1, 2, 3);
        }

        public void TestOnlyFloatArgs()
        {
            TestOnlyFloatArgs(1.0f, 2.0f, 3.0f);
        }

        public void TestOnlyBoolArgs()
        {
            TestOnlyBoolArgs(true, false, true);
        }

        public void TestOnlyStringArgs()
        {
            TestOnlyStringArgs("Foo", "Bar", "Baz");
        }

        public void TestOnlyNameArgs()
        {
            TestOnlyNameArgs(new Name("Joseph"), new Name("Heller"), new Name("Catch_22"));
        }

        public void TestMixedArgs()
        {
            TestMixedArgs("Foo", new Name("Bar"), 1, 42.0f, 108, 22.0f);
        }

        public void TestObjectArgsAndReturn()
        {
            AssertReturn(TestObjectArgsAndReturn(TestSubObject, null), TestSubObject, "TestObjectArgsAndReturn");
        }

        public void TestFloatReturn()
        {
        	AssertReturn(TestFloatReturn(42.0f, 2.0f), 42.0f, "TestFloatReturn");
        }

        public void TestInt32Return()
        {
            AssertReturn(TestInt32Return(42, 2), 42, "TestInt32Return");
        }

        public void TestBoolReturn()
        {
            AssertReturn(TestBoolReturn(false, true), false, "TestBoolReturn");
        }

        public void TestEnumReturn()
        {
            AssertReturn(TestEnumReturn(TestEnum.Something, TestEnum.SomethingElse), TestEnum.Something, "TestEnumReturn");
        }

        public void TestEnumCppReturn()
        {
            AssertReturn(TestEnumCppReturn(TestEnumCpp.Alpha, TestEnumCpp.Beta), TestEnumCpp.Alpha, "TestEnumCppReturn");
        }

        public void TestStringReturn()
        {
            AssertReturn(TestStringReturn("Foo", "Bar"), "Foo", "TestStringReturn");
        }

        public void TestNameReturn()
        {
            Name Author = new Name("Joseph Heller");
            Name Title = new Name("Catch_22");
            AssertReturn(TestNameReturn(Author, Title), Author, "TestNameReturn");
        }

        public void TestVectorReturn()
        {
            OpenTK.Vector3 x = new OpenTK.Vector3(4.0f, 5.0f, 1.0f);
            OpenTK.Vector3 y = new OpenTK.Vector3(1.0f, 0.0f, 8.0f);
            AssertReturn(TestVectorReturn(x, y), x, "TestVectorReturn");
        }

        public void TestQuatReturn()
        {
            OpenTK.Quaternion x = new OpenTK.Quaternion(1, 2, 3, 4);
            OpenTK.Quaternion y = new OpenTK.Quaternion(2, 4, 6, 0.1f);
            AssertReturn(TestQuatReturn(x, y), x, "TestQuatReturn");
        }

        public void TestMatrixReturn()
        {
            OpenTK.Matrix4 x = OpenTK.Matrix4.CreateRotationX((float)System.Math.PI / 2);
            AssertReturn(TestMatrixReturn(x, OpenTK.Matrix4.Identity), x, "TestMatrixReturn");
        }

        public void TestValueTypeArrayReturn()
        {
            Name[] expected = new Name[]
            {
                new Name("Electric_6"),
                new Name("Zero_7"),
            };
            IList<Name> x = new List<Name>(expected);

            IList<Name> y = new List<Name>();
            y.Add(new Name("Catch_22"));
            y.Add(new Name("Slaughterhouse_5"));
            y.Add(new Name("Fahrenheit_451"));

            IList<Name> actual = TestValueTypeArrayReturn(x, y);

            TestArray(expected, new ReadOnlyCollection<Name>(actual), "TestValueTypeArrayReturn", null);
        }

        public void TestObjectArrayReturn()
        {
            UnrealEngine.Core.Object[] expected = new UnrealEngine.Core.Object[]
            {
                NewObject<MonoTestSubObject>("NewTestSubobject"),
            };
            IList<UnrealEngine.Core.Object> x = new List<UnrealEngine.Core.Object>(expected);

            IList<UnrealEngine.Core.Object> y = new List<UnrealEngine.Core.Object>();
            y.Add(this);
            y.Add(null);
            y.Add(TestSubObject);

            IList<UnrealEngine.Core.Object> actual = TestObjectArrayReturn(x, y);
            TestArray(expected, new ReadOnlyCollection<UnrealEngine.Core.Object>(actual), "TestObjectArrayReturn", null);
        }

        public void TestStructReturn()
        {
            MonoTestsStruct X;
            X.TestStructInt32 = 22;
            X.TestStructFloat = 451.0f;
            X.TestStructWeakObject = TestSubObject;
            X.TestSubStruct.TestBool1 = false;
            X.TestSubStruct.TestBool2 = true;
                
            MonoTestsStruct Y;
            Y.TestStructInt32 = 42;
            Y.TestStructFloat = 54.0f;
            Y.TestStructWeakObject = null;
            Y.TestSubStruct.TestBool1 = false;
            Y.TestSubStruct.TestBool2 = true;

            MonoTestsStruct Result = TestStructReturn(X, Y);
            AssertEqual(Result.TestStructInt32, X.TestStructInt32, "Test.TestStructInt32");
            AssertEqual(Result.TestStructFloat, X.TestStructFloat, "Test.TestStructFloat");
            AssertEqual(Result.TestStructWeakObject, X.TestStructWeakObject, "Result.TestStructWeakObject");
            AssertEqual(Result.TestSubStruct.TestBool1, X.TestSubStruct.TestBool1, "Result.TestSubStruct.bTestBool1");
            AssertEqual(Result.TestSubStruct.TestBool2, X.TestSubStruct.TestBool2, "Result.TestSubStruct.bTestBool2");
        }

        public void TestStaticFunction()
        {
            AssertReturn(TestStaticFunction(22, 42), 22, "TestStaticFunction");
        }

        public void TestOutParams()
        {
            OpenTK.Vector3 InOutVec = new OpenTK.Vector3(16.0f, 23.0f, 42.0f);
            OpenTK.Vector3 OutVec;
            TestOutParams(ref InOutVec, out OutVec);

            AssertEqual(InOutVec.X, 4.0f, "InOutVec.X");
            AssertEqual(InOutVec.Y, 8.0f, "InOutVec.Y");
            AssertEqual(InOutVec.Z, 15.0f, "InOutVec.Z");
            AssertEqual(OutVec.X, 16.0f, "OutVec.X");
            AssertEqual(OutVec.Y, 23.0f, "OutVec.Y");
            AssertEqual(OutVec.Z, 42.0f, "OutVec.Z");
        }

        public void TestUObjectCreation()
        {
            MonoTestSubObject TestObject = NewObject<MonoTestSubObject>();
            AssertNotNull(TestObject, "TestObject");
            AssertEqual(TestObject.TestReadableInt32, TestSubObject.TestReadableInt32, "TestObject.TestReadableInt32");
        }

        public void TestDefaultParams()
        {
            AssertReturn(TestStructDefaultParams(), 5, "TestStructDefaultParams");

            OpenTK.Vector3 ZeroVec3 = OpenTK.Vector3.Zero;
            AssertReturn(TestStructDefaultParams(ZeroVec3), 4, "TestStructDefaultParams", "ZeroVec3");

            OpenTK.Vector2 NonzeroVec2 = new OpenTK.Vector2(22.0f, 108.0f);
            AssertReturn(TestStructDefaultParams(ZeroVec3, NonzeroVec2), 3, "TestStructDefaultParams", "ZeroVec3, NonzeroVec2");

            UnrealEngine.Core.LinearColor Black = new UnrealEngine.Core.LinearColor
            {
                R = 0.0f,
                G = 0.0f,
                B = 0.0f,
                A = 1.0f
            };
            AssertReturn(TestStructDefaultParams(ZeroVec3, NonzeroVec2, Black), 2, "TestStructDefaultParams", "ZeroVec3, NonzeroVec2, Black");

            Rotator NonzeroRotation = new Rotator(24.0f, 60.0f, 1.0f);
            AssertReturn(TestStructDefaultParams(ZeroVec3, NonzeroVec2, Black, NonzeroRotation), 1, "TestStructDefaultParams", "ZeroVec3, NonzeroVec2, Black, NonzeroRotation");

            UnrealEngine.Core.Color Yellow = new UnrealEngine.Core.Color (255, 255, 0, 255);
            AssertReturn(TestStructDefaultParams(ZeroVec3, NonzeroVec2, Black, NonzeroRotation, Yellow), 0, "TestStructDefaultParams", "ZeroVec3, NonzeroVec2, Black, NonzeroRotation, Yellow");
        }

        public void TestUserObjects()
        {
            AssertNotNull(TestUserObject, "TestUserObject");
            AssertEqual(TestUserObject.IsDestroyedOrPendingKill, false, "TestUserObject.IsDestroyedOrPendingKill");
            TestUserObject.RunTests();
        }

    }
}