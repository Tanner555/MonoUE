// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Linq;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace UnrealEngine.Runtime
{
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public struct ScriptArray
    {
        public IntPtr Data;
        public int ArrayNum;
        public int ArrayMax;
    }


    class UnrealArrayEnumerator<T> : IEnumerator<T>
    {
        int Index;
        UnrealArrayBase<T> Array;

        public UnrealArrayEnumerator(UnrealArrayBase<T> array)
        {
            Array = array;
            Index = -1;
        }

        public T Current
        {
            get { return Array.Get(Index); }
        }

        public void Dispose()
        {
            
        }

        object System.Collections.IEnumerator.Current
        {
            get { return Current; }
        }

        public bool MoveNext()
        {
            ++Index;
            return Index < Array.Count;
        }

        public void Reset()
        {
            Index = -1;
        }
    }

    // workaround for BXC23802 - Incorrect CS7042 error
    // mcs does not like DllImports on generic types
    class UnrealArrayBaseNativeMethods
    {
        [DllImport("__MonoRuntime", EntryPoint = "ScriptArrayBase_EmptyArray")]
        public extern static void EmptyArray(IntPtr nativeUnrealProperty, IntPtr scriptArrayPointer);

        [DllImport("__MonoRuntime", EntryPoint = "ScriptArrayBase_AddToArray")]
        public extern static void AddToArray(IntPtr nativeUnrealProperty, IntPtr scriptArrayPointer);

        [DllImport("__MonoRuntime", EntryPoint = "ScriptArrayBase_InsertInArray")]
        public extern static void InsertInArray(IntPtr nativeUnrealProperty, IntPtr scriptArrayPointer, int index);

        [DllImport("__MonoRuntime", EntryPoint = "ScriptArrayBase_RemoveFromArray")]
        public extern static void RemoveFromArray(IntPtr nativeUnrealProperty, IntPtr scriptArrayPointer, int index);
    }

    public abstract class UnrealArrayBase<T> : IEnumerable<T>
    {
        protected readonly UnrealObject OwnerObject;
        readonly IntPtr NativeUnrealProperty;
        readonly IntPtr NativeBuffer_;
        protected MarshalingDelegates<T>.FromNative FromNative;
        protected MarshalingDelegates<T>.ToNative ToNative;

        [CLSCompliant(false)]
        public UnrealArrayBase(UnrealObject ownerObject, IntPtr nativeUnrealProperty, IntPtr nativeBuffer, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative)
        {
            OwnerObject = ownerObject;
            NativeUnrealProperty = nativeUnrealProperty;
            NativeBuffer_ = nativeBuffer;
            FromNative = fromNative;
            ToNative = toNative;
        }

        private void CheckOwner(string message)
        {
            if (OwnerObject == null || OwnerObject.IsDestroyed)
            {
                throw new UnrealObjectDestroyedException(message);
            }
        }

        private IntPtr NativeBuffer
        {
            get
            {
                CheckOwner("Trying to access array on destroyed object of type " + OwnerObject.GetType().ToString());
                return NativeBuffer_;
            }
        }

        public int Count
        {
            get
            {
                unsafe
                {
                    ScriptArray* nativeArray = (ScriptArray*)NativeBuffer.ToPointer();
                    return nativeArray->ArrayNum;
                }
            }
        }

        protected IntPtr NativeArrayBuffer
        {
            get
            {
                unsafe
                {
                    ScriptArray* nativeArray = (ScriptArray*)NativeBuffer.ToPointer();
                    return nativeArray->Data;
                }
            }
        }

        protected void ClearInternal()
        {
            CheckOwner("Trying to Clear on an array on a destroyed Unreal Object");

            UnrealArrayBaseNativeMethods.EmptyArray(NativeUnrealProperty, NativeBuffer);
        }

        protected void AddInternal()
        {
            // adds a single value, not initialized
            // caller should have checked if we're destroyed
            CheckOwner("Trying to Add on an array on a destroyed Unreal Object");
            unsafe
            {
                ScriptArray* sa = (ScriptArray*)NativeBuffer;
            }
            UnrealArrayBaseNativeMethods.AddToArray(NativeUnrealProperty, NativeBuffer);
        }

        protected void InsertInternal(int index)
        {
            // insert a single value, not initialized, at index
            // caller should have checked if we're destroyed
            CheckOwner("Trying to Insert into an array on a destroyed Unreal Object");

            UnrealArrayBaseNativeMethods.InsertInArray(NativeUnrealProperty, NativeBuffer, index);
        }

        protected void RemoveAtInternal(int index)
        {
            CheckOwner("Trying to RemoveAt on an array on a destroyed Unreal Object");

            UnrealArrayBaseNativeMethods.RemoveFromArray(NativeUnrealProperty, NativeBuffer, index);
        }

        public T Get(int index)
        {
            if (index < 0 || index >= Count)
            {
                throw new IndexOutOfRangeException(string.Format("Index {0} out of bounds. Array is size {1}", index, Count));
            }
            return FromNative(NativeArrayBuffer, index, OwnerObject);
        }

        public bool Contains(T item)
        {
            foreach (T element in this)
            {
                if (element.Equals(item))
                {
                    return true;
                }
            }
            return false;
        }

        public IEnumerator<T> GetEnumerator()
        {
            return new UnrealArrayEnumerator<T>(this);
        }

        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }
    }

    public class UnrealArrayReadOnly<T> : UnrealArrayBase<T>, IReadOnlyList<T>
    {
        [CLSCompliant(false)]
        public UnrealArrayReadOnly(UnrealObject baseObject, IntPtr nativeUnrealProperty, IntPtr nativeBuffer, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative)
            : base(baseObject, nativeUnrealProperty, nativeBuffer, toNative, fromNative)
        {
        }

        public T this[int index]
        {
            get
            {
                return Get(index);
            }
        }

       
    }

    public class UnrealArrayReadWrite<T> : UnrealArrayBase<T>, IList<T>
    {
        [CLSCompliant(false)]
        public UnrealArrayReadWrite(UnrealObject baseObject, IntPtr nativeUnrealProperty, IntPtr nativeBuffer, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative)
            : base(baseObject, nativeUnrealProperty, nativeBuffer, toNative, fromNative)
        {
        }


        public T this[int index]
        {
            get
            {
                return Get(index);
            }
            set
            {
                if (index < 0 || index >= Count)
                {
                    throw new IndexOutOfRangeException(string.Format("Index {0} out of bounds. Array is size {1}", index, Count));
                }
                ToNative(NativeArrayBuffer, index, OwnerObject, value);
            }
        }

        public void Add(T item)
        {
            int newIndex = Count;
            AddInternal();
            this[newIndex] = item;
        }

        public void Clear()
        {
            ClearInternal();
        }

        public void CopyTo(T[] array, int arrayIndex)
        {
            // TODO: probably a faster way to do this
            int numElements = Count;
            for (int i = 0; i < numElements; ++i)
            {
                array[i + arrayIndex] = this[i];
            }
        }

        public bool IsReadOnly
        {
            get { return false; }
        }

        public bool Remove(T item)
        {
            int index = IndexOf(item);
            if (index != -1)
            {
                RemoveAt(index);
            }
            return index != -1;
        }

        public int IndexOf(T item)
        {
            int numElements = Count;
            for (int i = 0; i < numElements; ++i)
            {
                if (this[i].Equals(item))
                {
                    return i;
                }
            }
            return -1;
        }

        public void Insert(int index, T item)
        {
            InsertInternal(index);
            this[index] = item;
        }

        public void RemoveAt(int index)
        {
            RemoveAtInternal(index);
        }
    }

    public class UnrealArrayReadWriteMarshaler<T>
    {
        IntPtr NativeProperty;
        UnrealArrayReadWrite<T>[] Wrappers;
        MarshalingDelegates<T>.ToNative InnerTypeToNative;
        MarshalingDelegates<T>.FromNative InnerTypeFromNative;

        public UnrealArrayReadWriteMarshaler(int length, IntPtr nativeProperty, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative)
        {
            NativeProperty = nativeProperty;
            Wrappers = new UnrealArrayReadWrite<T> [length];
            InnerTypeFromNative = fromNative;
            InnerTypeToNative = toNative;
        }

        public void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, UnrealArrayReadWrite<T> obj)
        {
            throw new NotImplementedException("Copying UnrealArrays from managed memory to native memory is unsupported.");
        }

        public UnrealArrayReadWrite<T> FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            if (Wrappers[arrayIndex] == null)
            {
                Wrappers[arrayIndex] = new UnrealArrayReadWrite<T>(owner, NativeProperty, nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)), InnerTypeToNative, InnerTypeFromNative);
            }
            return Wrappers[arrayIndex];
        }
    }

    public class UnrealArrayReadOnlyMarshaler<T>
    {
        IntPtr NativeProperty;
        UnrealArrayReadOnly<T>[] Wrappers;
        MarshalingDelegates<T>.FromNative InnerTypeFromNative;

        public UnrealArrayReadOnlyMarshaler(int length, IntPtr nativeProperty, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative)
        {
            NativeProperty = nativeProperty;
            Wrappers = new UnrealArrayReadOnly<T>[length];
            InnerTypeFromNative = fromNative;
        }

        public void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, UnrealArrayReadOnly<T> obj)
        {
            throw new NotImplementedException("Copying UnrealArrays from managed memory to native memory is unsupported.");
        }

        public UnrealArrayReadOnly<T> FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            if (Wrappers[arrayIndex] == null)
            {
                Wrappers[arrayIndex] = new UnrealArrayReadOnly<T>(owner, NativeProperty, nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)), null, InnerTypeFromNative);
            }
            return Wrappers[arrayIndex];
        }
    }




    public class UnrealArrayCopyMarshaler<T>
    {
        int ElementSize;
        MarshalingDelegates<T>.ToNative InnerTypeToNative;
        MarshalingDelegates<T>.FromNative InnerTypeFromNative;

        public UnrealArrayCopyMarshaler(int length, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative, int elementSize)
        {
            ElementSize = elementSize;
            InnerTypeFromNative = fromNative;
            InnerTypeToNative = toNative;
        }

        public void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, IList<T> obj)
        {
            unsafe
            {
                ScriptArray* mirror = (ScriptArray*)(nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)));
                mirror->ArrayNum = obj.Count;
                mirror->ArrayMax = obj.Count;
                mirror->Data = Marshal.AllocCoTaskMem(obj.Count * ElementSize);

                for (int i = 0; i < obj.Count; ++i)
                {
                    InnerTypeToNative(mirror->Data, i, owner, obj[i]);
                }
            }
        }

        public void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, IReadOnlyList<T> obj)
        {
            unsafe
            {
                ScriptArray* mirror = (ScriptArray*)(nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)));
                mirror->ArrayNum = obj.Count;
                mirror->ArrayMax = obj.Count;
                mirror->Data = Marshal.AllocCoTaskMem(obj.Count * ElementSize);

                for (int i = 0; i < obj.Count; ++i)
                {
                    InnerTypeToNative(mirror->Data, i, owner, obj[i]);
                }
            }
        }

        public IList<T> FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            List<T> result = new List<T>();
            unsafe
            {
                ScriptArray* Array = (ScriptArray*)nativeBuffer;
                for (int i = 0; i < Array->ArrayNum; ++i)
                {
                    result.Add(InnerTypeFromNative(Array->Data, i, owner));
                }
            }

            return result;
        }

        public static void DestructInstance (IntPtr nativeBuffer, int arrayIndex)
        {
            unsafe
            {
                ScriptArray* mirror = (ScriptArray*)(nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)));
                Marshal.FreeCoTaskMem(mirror->Data);
                mirror->Data = IntPtr.Zero;
                mirror->ArrayMax = 0;
                mirror->ArrayNum = 0;
            }
        }
    }

}
