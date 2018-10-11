// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace UnrealEngine.Runtime
{
    public class Text : IEquatable<Text>, IComparable<Text>
    {
#if !CONFIG_SHIPPING
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern private static void CheckSizeof(int size);

        static unsafe Text()
        {
            CheckSizeof(sizeof(FText));
        }
#endif

        [StructLayout(LayoutKind.Sequential)]
        struct FText
        {
            public IntPtr ObjectPtr;
            public IntPtr SharedReferenceCount;
            public uint Flags;
        }

        internal Text(IntPtr nativeInstance)
        {
            NativeInstance = nativeInstance;
            OwnerObject = null;
            Data = new SharedPtrTheadSafe(nativeInstance);
        }

        internal Text(UnrealObject owner, IntPtr nativeBuffer)
        {
            OwnerObject = owner;
            CheckOwnerObject();
            NativeInstance = nativeBuffer;
            //Don't own a reference we're referring to a property
            if (owner != null)
            {
                Data = SharedPtrTheadSafe.NonReferenceOwningSharedPtr(NativeInstance);
            }
            else
            {
                Data = new SharedPtrTheadSafe(NativeInstance);
            }
        }

        internal Text(UnrealObject owner, int propertyOffset)
        {
            OwnerObject = owner;
            CheckOwnerObject();
            NativeInstance = IntPtr.Add(owner.NativeObject, propertyOffset);
            //Don't own a reference we're referring to a property
            Data = SharedPtrTheadSafe.NonReferenceOwningSharedPtr(NativeInstance);
        }

        private Text ()
        {
            NativeInstance = Marshal.AllocHGlobal(NativeSize);

            Data = SharedPtrTheadSafe.NewNulledSharedPtr(NativeInstance);
        }

        ~Text()
        {
            if (OwnerObject == null && NativeInstance != IntPtr.Zero)
            {
                //Destruct the native instance
                Data.Dispose();

                Marshal.FreeHGlobal(NativeInstance);
            }
        }

        public static Text Empty()
        {
            Text result = new Text();
            FText_CreateEmptyText(result.NativeInstance);
            return result;
        }

        public static Text Localized (string key, string nameSpace, string literal)
        {
            Text result = new Text();

            FText_CreateText(result.NativeInstance, key,nameSpace,literal);

            return result;
        }

        public void SetLocalized (string key, string nameSpace, string literal)
        {
            CheckOwnerObject();
            FText_CreateText(NativeInstance, key, nameSpace, literal);
        }

        public static Text FromString (string text)
        {
            Text result = new Text();

            FText_FromString(result.NativeInstance,text);

            return result;
        }

        public void SetFromString(string text)
        {
            CheckOwnerObject();
            FText_FromString(NativeInstance, text);
        }

        public void SetEmpty()
        {
            CheckOwnerObject();
            FText_CreateEmptyText(NativeInstance);
        }

        public static Text FromName(Name name)
        {
            Text result = new Text();

            FText_FromName(result.NativeInstance, name);

            return result;
        }

        public override string ToString() 
        {
            CheckOwnerObject();
            return FText_ToString(NativeInstance);
        }

        public bool Equals(Text other)
        {
            return CompareTo(other) == 0;
        }

        public int CompareTo(Text other)
        {
            CheckOwnerObject();
            return FText_Compare(NativeInstance, other.NativeInstance);
        }

        public bool IsEmpty
        {
            get 
            {
                CheckOwnerObject();
                return FText_IsEmpty(NativeInstance);
            }
        }

        UnrealObject OwnerObject;
        IntPtr NativeInstance;
        SharedPtrTheadSafe Data;

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static string FText_ToString(IntPtr thisText);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static void FText_FromString(IntPtr thisText, string text);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static void FText_CreateText(IntPtr thisText, string key, string nameSpace, string literal);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static void FText_CreateEmptyText(IntPtr thisText);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static void FText_FromName(IntPtr thisText, Name name);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static int FText_Compare(IntPtr a, IntPtr b);
        
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static bool FText_IsEmpty(IntPtr thisText);

        #region MarshalingHelpers

        internal static unsafe int NativeSize
        {
            get { return sizeof(FText); }
        }

        private void CheckOwnerObject ()
        {
            if (OwnerObject != null && OwnerObject._NativeObject == IntPtr.Zero)
            { 
                throw new UnrealObjectDestroyedException("Trying to access Text UProperty on destroyed object of type " + OwnerObject.GetType().ToString());
            }
        }

        public unsafe void CopyFrom (Text other)
        {
            other.CheckOwnerObject();
            Data.CopyFrom(other.Data);

            ((FText*)NativeInstance)->Flags = ((FText*)other.NativeInstance)->Flags;
        }

        public Text Clone ()
        {
            Text result = new Text();
            result.CopyFrom(this);
            return result;
        }

        #endregion
    }

    public class TextMarshaler
    {
        Text[] Wrapper;
        public TextMarshaler(int length)
        {
            Wrapper = new Text[length];
        }

        public void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, Text obj)
        {
            if (Wrapper[arrayIndex] == null)
            {
                Wrapper[arrayIndex] = new Text(owner, nativeBuffer + arrayIndex * Text.NativeSize);
            }
            Wrapper[arrayIndex].CopyFrom(obj);
        }

        public Text FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            if (Wrapper[arrayIndex] == null)
            {
                Wrapper[arrayIndex] = new Text(owner, nativeBuffer + arrayIndex * Text.NativeSize);
            }
            return Wrapper[arrayIndex];
        }

        public void Dispose()
        {
            
        }
    }
}
