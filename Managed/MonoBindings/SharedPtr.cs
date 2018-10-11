// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Diagnostics;

namespace UnrealEngine.Runtime
{
    class SharedPtrTheadSafe : IDisposable
    {
#if !CONFIG_SHIPPING
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern private static void CheckSizeof(int size);

        static unsafe SharedPtrTheadSafe()
        {
            CheckSizeof(sizeof(FMarshaledSharedPtr));
        }
#endif

        [StructLayout(LayoutKind.Sequential)]
        struct FMarshaledSharedPtr
        {
            public IntPtr ObjectPtr;
            public IntPtr ReferenceController;
        };

        public bool OwnsAReference
        { get; private set; }

        public IntPtr NativeInstance { get; private set; }

        public unsafe IntPtr ObjectPtr
        {
            get
            {
                Debug.Assert(NativeInstance != IntPtr.Zero, "Can't access a null SharedPtr.");
                return ((FMarshaledSharedPtr*)NativeInstance)->ObjectPtr;
            }
            private set
            {
                Debug.Assert(NativeInstance != IntPtr.Zero, "Can't access a null SharedPtr.");
                ((FMarshaledSharedPtr*)NativeInstance)->ObjectPtr = value;
            }
        }
        
        public unsafe IntPtr ReferenceController 
        { 
            get
            {
                Debug.Assert(NativeInstance != IntPtr.Zero, "Can't access a null SharedPtr.");
                return ((FMarshaledSharedPtr*)NativeInstance)->ReferenceController;
            }
            private set
            {
                Debug.Assert(NativeInstance != IntPtr.Zero, "Can't access a null SharedPtr.");
                ((FMarshaledSharedPtr*)NativeInstance)->ReferenceController = value;
            }
        }

        public int ReferenceCount
        {
            get
            {
                Debug.Assert(NativeInstance != IntPtr.Zero, "Can't access a null SharedPtr.");
                return Marshal.ReadInt32(ReferenceController);
            }
        }

        private SharedPtrTheadSafe()
        {
            OwnsAReference = true;
        }

        ~SharedPtrTheadSafe()
        {
            Dispose();
        }

        public SharedPtrTheadSafe(IntPtr nativeInstance)
        {
            OwnsAReference = true;
            NativeInstance = nativeInstance;
            ForceIncRef();
        }

        public static SharedPtrTheadSafe NewNulledSharedPtr (IntPtr nativeInstance)
        {
            SharedPtrTheadSafe result = new SharedPtrTheadSafe();
            result.NativeInstance = nativeInstance;
            result.ObjectPtr = IntPtr.Zero;
            result.ReferenceController = IntPtr.Zero;
            return result;
        }

        public static SharedPtrTheadSafe NonReferenceOwningSharedPtr (IntPtr nativeInstance)
        {
            SharedPtrTheadSafe result = new SharedPtrTheadSafe();
            result.OwnsAReference = false;
            result.NativeInstance = nativeInstance;
            return result;
        }


        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static string FSharedRef_IncRefThreadSafe(IntPtr reference);

        private void ForceIncRef()
        {
            if (NativeInstance == IntPtr.Zero || ReferenceController == IntPtr.Zero)
            {
                return;
            }
            FSharedRef_IncRefThreadSafe(ReferenceController);
        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static string FSharedRef_DecRefThreadSafe(IntPtr reference);

        private void ForceDecRef()
        {
            if (NativeInstance == IntPtr.Zero || ReferenceController == IntPtr.Zero)
            {
                return;
            }
            FSharedRef_DecRefThreadSafe(ReferenceController);
        }

        public void CopyFrom(SharedPtrTheadSafe other)
        {
            ForceDecRef();
            ObjectPtr = other.ObjectPtr;
            ReferenceController = other.ReferenceController;
            ForceIncRef();
        }

        public override string ToString()
        {
            return "ShrPtr {" + ObjectPtr + ", " + ReferenceController + ":" + ReferenceCount + "}";
        }

        public void Dispose()
        {
            if (OwnsAReference)
            {
                ForceDecRef();
            }
            NativeInstance = IntPtr.Zero;
        }
    }
}
