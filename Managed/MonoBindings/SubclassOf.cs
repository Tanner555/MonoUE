// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace UnrealEngine.Runtime
{
    // Helper object for UClassProperty.
    // T specifies which UnrealObject-derived base class the Class property must conform to.
    // This struct is designed to be blittable from a native UClass* or TSubclassOf<T>.  Do not add fields!
    public struct SubclassOf<T> where T : UnrealObject
    {
        private IntPtr NativeClassPtr;
        public IntPtr NativeClass { get { return NativeClassPtr; } }

        public SubclassOf(Type klass)
        {
            if (klass == typeof(T) || klass.IsSubclassOf(typeof(T)))
            {
                NativeClassPtr = UnrealInterop.GetNativeClassFromType(klass);
            }
            else
            {
                throw new ArgumentException(String.Format("{0} is not a subclass of {1}.", klass.Name, typeof(T).Name));
            }
        }

        public SubclassOf(IntPtr nativeClass)
        {
            NativeClassPtr = nativeClass;
            if (NativeClassPtr != IntPtr.Zero && SystemType == null)
            {
                throw new InvalidOperationException(string.Format("{0} is not a valid value for SubclassOf<{1}>", UnrealObject.GetFName(NativeClassPtr), typeof(T).Name));
            }
        }

        public static implicit operator SubclassOf<T>(Type klass)
        {
            return new SubclassOf<T>(klass);
        }

        public bool Valid { get { return IsChildOf(typeof(T)); } }

        // Check if this struct represents a specific type, or one of its subclasses.
        public bool IsChildOf(Type type)
        {
            Type klass = SystemType;
            return klass != null && (klass == type || klass.IsSubclassOf(type));
        }

        // Check if this struct represents a specific type, or one of its superclasses.
        public bool IsParentOf(Type type)
        {
            Type klass = SystemType;
            // Strictly speaking, IsAssignableFrom() doesn't necessarily indicate a subclass relationship,
            // but we're operating under a generic type constraint that tells us we're in the UnrealObject
            // class hierarchy.
            return klass != null && klass.IsAssignableFrom(type);
        }

        // The nearest C# type to the configured UClass.
        // Note that it isn't safe to expose this as part of the public API, as there is not a 1:1 mapping from
        // Unreal classes to System.Types.  Blueprint-generated classes, in particular, will not have a direct
        // System.Type representation, but may be stored in a SubclassOf<T> in the editor or with a ClassFinder lookup.
        private Type SystemType
        {
            get
            {
                if (NativeClass == IntPtr.Zero)
                {
                    return null;
                }
                Type klass = UnrealInterop.GetManagedType(NativeClass);
                if (klass == typeof(T) || klass.IsSubclassOf(typeof(T)))
                {
                    return klass;
                }
                else
                {
                    //TODO: throw an exception?
                    //      For now, soft fail on access in order to tolerate previously set classes that are no longer compatible.
                    //      Ideally, this would be handled by deprecating the old property in favor of a new one and fixing things
                    //      up in PostLoad, but we don't support this in managed code yet and need to be able to recover from a
                    //      class property mismatch until we do.
                    return null;
                }
            }
        }

        // Get the default object for the UClass represented by this SubclassOf wrapper.
        public T GetDefault()
        {
            return UnrealObject.GetClassDefaultObject<T>(NativeClass);
        }

        // Get the default object for the UClass represented by this SubclassOf wrapper, cast to a specific subclass of the type parameter.
        public U GetDefault<U>() where U : T
        {
            return UnrealObject.GetClassDefaultObject<U>(NativeClass);
        }

        public override string ToString()
        {
            return (Valid ? UnrealObject.GetFName(NativeClass).ToString() : "null");
        }

        // Convert this struct to a SubclassOf wrapper of superclass type.
        public SubclassOf<U> As<U>() where U : UnrealObject
        {
            // Can't constrain the class's type parameter based on the method's, so enforce it at runtime.
            if (!IsChildOf(typeof(U)))
            {
                throw new InvalidOperationException();
            }

            return new SubclassOf<U>(NativeClassPtr);
        }

        public override bool Equals(object obj)
        {
            if (!(obj is SubclassOf<T>))
            {
                return false;
            }

            SubclassOf<T> rhs = (SubclassOf<T>)obj;
            // we can do a reference comparison here
            return NativeClass == rhs.NativeClass;
        }

        public override int GetHashCode()
        {
            return NativeClass.GetHashCode();
        }
    }

    public static class SubclassOfMarshaler<T>
          where T : UnrealObject
    {
        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, SubclassOf<T> obj)
        {
            unsafe
            {
                *((IntPtr *)(nativeBuffer + arrayIndex * IntPtr.Size)) = obj.NativeClass;
            }
        }

        public static SubclassOf<T> FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            unsafe
            {
                return new SubclassOf<T>(*((IntPtr*)(nativeBuffer + arrayIndex * IntPtr.Size)));
            }
        }
    }
}
