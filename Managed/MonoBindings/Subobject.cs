// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace UnrealEngine.Runtime
{
    public class Subobject<T>
        where T : UnrealObject
    {
        public T Object
        {
            get;
            private set;
        }

        public Subobject(UnrealObject obj)
        {
            Object = (T)obj;
        }

        public static implicit operator T(Subobject<T> subObj)
        {
            return subObj.Object;
        }
    }

    public static class SubobjectMarshaler<T>
        where T : UnrealObject
    {
        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, Subobject<T> obj)
        {
            IntPtr elementPtr = nativeBuffer + arrayIndex * IntPtr.Size;
            unsafe
            {
                *((IntPtr*)elementPtr) = (obj != null ? obj.Object.NativeObject : IntPtr.Zero);
            }
        }

        public static Subobject<T> FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            IntPtr elementPtr = nativeBuffer + arrayIndex * IntPtr.Size;
            unsafe
            {
                return new Subobject<T>(UnrealObject.GetUnrealObjectWrapper<T>(*((IntPtr*)elementPtr)));
            }
        }
    }
}