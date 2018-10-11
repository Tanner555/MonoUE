// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace UnrealEngine.Runtime
{
    public static class UnrealInterop
    {
        // Mirror struct for TSharedPtr
        internal struct SharedPtrMirror
        {
#pragma warning disable 169
            IntPtr Object;
            IntPtr SharedReferenceCount;
#pragma warning restore 169
        }

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern public static string MarshalIntPtrAsString(IntPtr str);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern public static void MarshalToUnrealString(string str, IntPtr scriptArray);

        // Return a native class pointer from a class name
        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_GetNativeClassFromName")]
        extern public static IntPtr GetNativeClassFromName([MarshalAs(UnmanagedType.LPWStr)] string className);

        // Return a native struct pointer from a class name
        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_GetNativeStructFromName")]
        extern public static IntPtr GetNativeStructFromName([MarshalAs(UnmanagedType.LPWStr)] string structName);

        // Return the size of a UStruct, including any non-script exposed data.
        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_GetNativeStructSize")]
        extern public static int GetNativeStructSize(IntPtr nativeStruct);

        // Return a property offset from a native class and property name
        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_GetPropertyOffsetFromName"), CLSCompliant(false)]
        extern public static int GetPropertyOffsetFromName(IntPtr nativeClass,
                                                        [MarshalAs(UnmanagedType.LPWStr)]
                                                        string propertyName);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_GetNativePropertyFromName"), CLSCompliant(false)]
        extern public static IntPtr GetNativePropertyFromName(IntPtr nativeClass,
                                                        [MarshalAs(UnmanagedType.LPWStr)]
                                                        string propertyName);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_GetPropertyRepIndexFromName"), CLSCompliant(false)]
        extern public static ushort GetPropertyRepIndexFromName(IntPtr nativeClass,
                                                                [MarshalAs(UnmanagedType.LPWStr)]
                                                                string propertyName);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_GetArrayElementSize")]
        public extern static int GetArrayElementSize(IntPtr nativeClass,
                                                     [MarshalAs(UnmanagedType.LPWStr)]
                                                     string propertyName);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_GetPropertyArrayDimFromName")]
        public extern static int GetPropertyArrayDimFromName(IntPtr nativeClass,
                                                    [MarshalAs(UnmanagedType.LPWStr)]
                                                    string propertyName);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_GetBitfieldValueFromProperty"), CLSCompliant(false)]
        extern public static bool GetBitfieldValueFromProperty(IntPtr nativeObject, IntPtr nativeProperty, int offset);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_SetBitfieldValueForProperty"), CLSCompliant(false)]
        extern public static void SetBitfieldValueForProperty(IntPtr nativeObject, IntPtr nativeProperty, int offset, bool value);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_SetStringValueForProperty"), CLSCompliant(false)]
        extern public static void SetStringValueForProperty(IntPtr nativeObject,
                                                        IntPtr nativeProperty,
                                                        int offset,
                                                        [MarshalAs(UnmanagedType.LPWStr)] 
                                                        string value);


        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_SetStringValue"), CLSCompliant(false)]
        extern public static void SetStringValue(IntPtr nativeString,
                                                       [MarshalAs(UnmanagedType.LPWStr)] 
                                                        string value);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern public static Type GetManagedType(IntPtr nativeClass);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern public static IntPtr GetNativeClassFromType(Type managedType);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_RPC_ResetLastFailedReason")]
        extern public static void RPCResetLastFailedReason();

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_RPC_ValidateFailed"), CLSCompliant(false)]
        extern public static void RPCValidateFailed([MarshalAs(UnmanagedType.LPWStr)] string reason);
        
        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_RPC_GetLastFailedReason")]
        extern private static IntPtr RPCGetLastFailedReason_Native();

        public static string RPCGetLastFailedReason()
        {
            return MarshalIntPtrAsString(RPCGetLastFailedReason_Native());
        }
    }
}
