// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace UnrealEngine.Runtime
{

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public struct ObjectInitializer 
    {
        internal IntPtr NativeObject;
        IntPtr NativeObjectInitializer;

        public Subobject<T> CreateDefaultSubobject<T>(Name subobjectName, bool transient = false)
            where T : UnrealObject
        {
            Type type = typeof(T);
            return new Subobject<T>(CreateDefaultSubobject_Name(NativeObjectInitializer, type, subobjectName, true, false, transient)); 
        }

        public Subobject<T> CreateOptionalDefaultSubobject<T>(Name subobjectName, bool transient = false)
            where T : UnrealObject
        {
            Type type = typeof(T);
            return new Subobject<T>(CreateDefaultSubobject_Name(NativeObjectInitializer, type, subobjectName, false, false, transient)); 
        }

        public Subobject<T> CreateAbstractDefaultSubobject<T>(Name subobjectName, bool transient = false)
            where T : UnrealObject
        {
            Type type = typeof(T);
            return new Subobject<T>(CreateDefaultSubobject_Name(NativeObjectInitializer, type, subobjectName, true, true, transient)); 
        }

        public Subobject<ReturnType> CreateDefaultSubobject<ReturnType, CreateType>(Name subobjectName, bool transient = false)
            where ReturnType : UnrealObject
            where CreateType : ReturnType
        {
            Type createType = typeof(CreateType);
            return new Subobject<ReturnType>(CreateDefaultSubobject_Name(NativeObjectInitializer, createType, subobjectName, true, true, transient)); 
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static UnrealObject CreateDefaultSubobject_Name(IntPtr ObjectInitializer, Type unrealType, Name objectName, bool isRequired, bool isAbstract, bool isTransient);

        public bool TryFindObject<T>(string searchString, out T @object) where T : UnrealObject
        {
            @object = (T)ObjectFinder_FindNativeObject(typeof(T), searchString);
            return @object != null;
        }

        public T FindObject<T>(string searchString) where T : UnrealObject
        {
            if (TryFindObject(searchString, out T @object))
            {
                return @object;
            }
            throw new ObjectNotFoundException("Could not find required object: " + searchString);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        extern internal static UnrealObject ObjectFinder_FindNativeObject(Type objectType, string searchString);

        public bool TryFindClass<T>(string searchString, out SubclassOf<T> @class) where T : UnrealObject
        {
            @class = new SubclassOf<T>(ClassFinder_FindNativeClass(searchString));
            return @class.Valid;
        }

        public SubclassOf<T> FindClass<T>(string searchString) where T : UnrealObject
        {
            if (TryFindClass(searchString, out SubclassOf<T> @class))
            {
                return @class;
            }
            throw new ObjectNotFoundException("Could not find required class: " + searchString);
        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern internal static IntPtr ClassFinder_FindNativeClass(string searchString);
    }
}
