// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Collections;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace UnrealEngine.Runtime
{
    public class FixedSizeArrayBase
    {
        public IntPtr NativeBuffer { get; private set; }
        public int Length { get; private set; }
        public int ElementSize { get; private set; }
        public WeakReference<UnrealObject> Owner { get; private set; }
        public bool HasOwner { get { return Owner != null; } }

        protected FixedSizeArrayBase(IntPtr nativeBuffer, int length, UnrealObject owner)
        {
            NativeBuffer = nativeBuffer;
            Length = length;
            if (owner != null)
            {
                Owner = new WeakReference<UnrealObject>(owner);
            }
        }
    }

    public abstract class FixedSizeArray<T> : FixedSizeArrayBase, IEnumerable<T>
    {
        protected MarshalingDelegates<T>.FromNative FromNative;

        public FixedSizeArray(IntPtr nativeBuffer, int length, MarshalingDelegates<T>.FromNative fromNative)
            : base(nativeBuffer, length, null)
        {
            FromNative = fromNative;
        }

        public FixedSizeArray(UnrealObject owner, int propertyOffset, int length, MarshalingDelegates<T>.FromNative fromNative)
            : base(owner.NativeObject + propertyOffset, length, owner)
        {
            FromNative = fromNative;
        }

        //We have to have a separate get, because the array accessors property can't be abstract and guarantee a get
        //but then have a derived class supply a set.
        public T Get(int index)
        {
            if (index < 0 || index >= Length)
            {
                throw new IndexOutOfRangeException(string.Format("{0} is out of bounds. The array is only {0} elements.", index, Length));
            }

            UnrealObject owner = null;

            if (HasOwner)
            {
                Owner.TryGetTarget(out owner);
            }
            return FromNative(NativeBuffer + index * ElementSize, index, owner);
        }

        public IEnumerator<T> GetEnumerator()
        {
            return new FixedSizeArrayEnumerator<T>(this);
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return new FixedSizeArrayEnumerator<T>(this);
        }
    }

    public class FixedSizeArrayEnumerator<T> : IEnumerator<T>
    {
        FixedSizeArray<T> Array;
        int Index;

        public FixedSizeArrayEnumerator(FixedSizeArray<T> array)
        {
            Index = -1;
            Array = array;
        }

        public T Current
        {
            get 
            {
                return Array.Get(Index);
            }
        }

        public void Dispose()
        {
            
        }

        object IEnumerator.Current
        {
            get
            {
                return Array.Get(Index);
            }
        }

        public bool MoveNext()
        {
            Index++;
            return Index < Array.Length && Index >= 0;
        }

        public void Reset()
        {
            Index = -1;
        }
    }

    public class FixedSizeArrayReadWrite<T> : FixedSizeArray<T>
    {
        MarshalingDelegates<T>.ToNative ToNative;

        public FixedSizeArrayReadWrite(IntPtr nativeBuffer, int length, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative)
            : base (nativeBuffer, length, fromNative)
        {
            ToNative = toNative;
        }

        public FixedSizeArrayReadWrite(UnrealObject owner, int propertyOffset, int length, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative)
            : base(owner, propertyOffset, length, fromNative)
        {
            ToNative = toNative;
        }

        public T this[int index]
        {
            get 
            {
                return Get(index);
            }
            set
            {
                if (index < 0 || index >= Length)
                {
                    throw new IndexOutOfRangeException(string.Format("{0} is out of bounds. The array is only {0} elements.", index, Length));
                }
                UnrealObject owner = null;
                if (HasOwner)
                {
                    Owner.TryGetTarget(out owner);
                }
                ToNative(NativeBuffer + index * ElementSize, index, owner, value);
            }
        }
    }

    public class FixedSizeArrayReadOnly<T> : FixedSizeArray<T>
    {
        public FixedSizeArrayReadOnly(IntPtr nativeBuffer, int length, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative)
            : base(nativeBuffer, length, fromNative)
        {

        }

        public FixedSizeArrayReadOnly(UnrealObject owner, int propertyOffset, int length, MarshalingDelegates<T>.ToNative toNative, MarshalingDelegates<T>.FromNative fromNative)
            : base(owner, propertyOffset, length, fromNative)
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
}
