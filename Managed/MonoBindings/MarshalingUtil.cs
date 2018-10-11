// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace UnrealEngine.Runtime
{
    public static class MarshalingDelegates<T>
    {
        public delegate void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, T obj);
        public delegate T FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner);
        public delegate void DestructInstance(IntPtr nativeBuffer, int arrayIndex);
    }

    public static class UnrealObjectMarshaler<T>
        where T : UnrealObject
    {
        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, T obj)
        {
            IntPtr elementPtr = nativeBuffer + arrayIndex * IntPtr.Size;
            unsafe
            {
                *((IntPtr*)elementPtr) = (obj != null ? obj.NativeObject : IntPtr.Zero);
            }
        }

        public static T FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            IntPtr elementPtr = nativeBuffer + arrayIndex * IntPtr.Size;
            unsafe
            {
                return UnrealObject.GetUnrealObjectWrapper<T>(*((IntPtr*)elementPtr));
            }
        }
    }

    public static class BlittableTypeMarshaler<T>
        where T : struct
    {
        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, T obj)
        {
            Marshal.StructureToPtr<T>(obj, nativeBuffer + arrayIndex * Marshal.SizeOf<T>(), false);
        }

        public static T FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            return Marshal.PtrToStructure<T>(nativeBuffer + arrayIndex * Marshal.SizeOf<T>());
        }
    }

    public static class EnumMarshaler<T>
        where T : struct
    {
        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, T obj)
        {
            Marshal.StructureToPtr<T>(obj, nativeBuffer + arrayIndex * Marshal.SizeOf(Enum.GetUnderlyingType(typeof(T))), false);
        }

        public static T FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            return Marshal.PtrToStructure<T>(nativeBuffer + arrayIndex * Marshal.SizeOf(Enum.GetUnderlyingType(typeof(T))));
        }
    }

    public static class BoolMarshaler
    {
        //sizeof(bool) in C++ is not well defined.
        private static readonly int NativeBooleanSize;

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static int GetBooleanSize();

        static BoolMarshaler()
        {
            NativeBooleanSize = GetBooleanSize();
        }

        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, bool obj)
        {
            IntPtr elementPtr = nativeBuffer + arrayIndex * NativeBooleanSize;
            unsafe
            {
                byte boolval = (byte)(obj ? 1 : 0);
                for (int i = 0; i < NativeBooleanSize; ++i)
                {
                    *((byte*)(elementPtr + i)) = boolval;
                }
            }
        }

        public static bool FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            IntPtr elementPtr = nativeBuffer + arrayIndex * NativeBooleanSize;
            unsafe
            {
                for (int i = 0; i < NativeBooleanSize; ++i)
                {
                    if (*((byte*)(elementPtr + i)) != 0)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    }

}