// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

﻿using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace UnrealEngine.Runtime
{

    //Mirrors the native FWeakObjectPtr
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    internal struct WeakObjectData
    {
        public int ObjectIndex;
        public int ObjectSerialNumber;

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern public static UnrealObject GetObject(ref WeakObjectData data);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern public static void SetObject(ref WeakObjectData data, IntPtr nativeObject);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern public static bool IsValid(ref WeakObjectData data, bool threadsafeTest);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern public static bool IsStale(ref WeakObjectData data, bool threadsafeTest);
    }

    public class WeakObject<T>
        : IEquatable<WeakObject<T>>
          where T : UnrealObject
    {
        WeakObjectData Data;

        public WeakObject()
        {

        }

        public WeakObject(IntPtr Native)
        {
            unsafe { Data = *((WeakObjectData*)Native.ToPointer()); }
        }

        public void ToNative(IntPtr Native)
        {
            unsafe { (*((WeakObjectData*)Native.ToPointer())) = Data; }
        }

        public WeakObject(T obj)
        {
            WeakObjectData.SetObject(ref Data, obj == null ? IntPtr.Zero : obj.NativeObject);
        }

        public static implicit operator WeakObject<T>(T obj)
        {
            return new WeakObject<T>(obj);
        }

        public T Object
        {
            get
            {
                UnrealObject obj = WeakObjectData.GetObject(ref Data);
                return obj as T;
            }
        }

        public bool IsValid (bool threadsafeTest = false)
        {
            return WeakObjectData.IsValid(ref Data, threadsafeTest);
        }

        public bool IsStale(bool threadsafeTest = false)
        {
            return WeakObjectData.IsStale(ref Data, threadsafeTest);
        }

        public override string ToString()
        {
            return string.Format("ObjectIndex={0}, ObjectSerialNumber={1}", Data.ObjectIndex, Data.ObjectSerialNumber);
        }

        public override int GetHashCode()
        {
            return Data.ObjectIndex;
        }

        public override bool Equals(object obj)
        {
            WeakObject<T> other = obj as WeakObject<T>;
            if (other != null)
            {
                return Equals(other);
            }

            return false;
        }

        public bool Equals(WeakObject<T> other)
        {
            //Don't trip up on nulls
            if (other == null)
            {
                return false;
            }

            if (Data.ObjectIndex == other.Data.ObjectIndex
                && Data.ObjectSerialNumber == other.Data.ObjectSerialNumber)
            {
                return true;
            }

            T thisObject = Object;
            T otherObject = other.Object;

            if (thisObject == null && otherObject == null)
            {
                return true;
            }
            else if (thisObject == null || otherObject == null)
            {
                return false;
            }
            else
            {
                return thisObject.Equals(otherObject);
            }
        }

        internal static int NativeSize
        {
            get { return Marshal.SizeOf(typeof(WeakObjectData)); }
        }
    }

    public static class WeakObjectMarshaler<T>
          where T : UnrealObject
    {
        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, WeakObject<T> obj)
        {
            IntPtr elementPtr = nativeBuffer + arrayIndex * WeakObject<T>.NativeSize;
            if (obj != null)
            {
                obj.ToNative(elementPtr);
            }
            else
            {
                new WeakObject<T>().ToNative(elementPtr);
            }
        }

        public static WeakObject<T> FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            return new WeakObject<T>(nativeBuffer + arrayIndex * WeakObject<T>.NativeSize);
        }
    }
}