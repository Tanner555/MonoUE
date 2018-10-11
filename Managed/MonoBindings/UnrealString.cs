// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;


namespace UnrealEngine.Runtime
{
    public static class StringMarshaler
    {
        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, string obj)
        {
            unsafe
            {
                if (owner != null)
                {
                    //MarshalToUnrealString allocates memory meant to be freed by managed code
                    //SetStringValue marshals the string in place over top the existing string.
                    UnrealInterop.SetStringValue(nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)), obj);
                }
                else
                {
                    // Allocate a new buffer for the string.  This marshaler still doesn't need to do any
                    // cleanup on the managed side, but in the case where it's used for unowned memory,
                    // the memory needs to have been allocated in a way where native code can clean it up.
                    UnrealInterop.MarshalToUnrealString(obj, nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)));
                }
            }
        }

        public static string FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            unsafe
            {
                ScriptArray* ustring = (ScriptArray*)(nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)));
                return UnrealInterop.MarshalIntPtrAsString(ustring->Data);
            }
        }
    }


    public static class StringMarshalerWithCleanup
    {
        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, string obj)
        {
            unsafe
            {
                UnrealInterop.MarshalToUnrealString(obj, nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)));
            }
        }

        public static string FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            unsafe
            {
                ScriptArray* ustring = (ScriptArray*)(nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)));
                return UnrealInterop.MarshalIntPtrAsString(ustring->Data);
            }
        }

        public static void DestructInstance (IntPtr nativeBuffer, int arrayIndex)
        {
            unsafe
            {
                ScriptArray* ustring = (ScriptArray*)(nativeBuffer + arrayIndex * Marshal.SizeOf(typeof(ScriptArray)));
                Marshal.FreeCoTaskMem(ustring->Data);
                ustring->Data = IntPtr.Zero;
                ustring->ArrayNum = 0;
                ustring->ArrayMax = 0;
            }
        }
    }


}