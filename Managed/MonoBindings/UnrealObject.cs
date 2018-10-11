// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Collections.Generic;
using System.Reflection;

namespace UnrealEngine.Runtime
{
    [Serializable]
    public class UnrealObjectDestroyedException : InvalidOperationException
    {
        public UnrealObjectDestroyedException()
        {

        }

        public UnrealObjectDestroyedException(string message)
            : base(message)
        {

        }

        public UnrealObjectDestroyedException(string message, Exception innerException)
            : base(message, innerException)
        {

        }
    }

    abstract public partial class UnrealObject
    {
        public Name ObjectName
        {
            get
            {
                if (IsDestroyed)
                {
                    return Name.None;
                }
                return GetFName(_NativeObject);
            }
        }

        // Has this object been destroyed
        public bool IsDestroyed
        {
            get
            {
                return _NativeObject == IntPtr.Zero;
            }
        }

        // Is this object scheduled for destruction
        public bool IsPendingKill
        {
            get
            {
                return IsPendingKillNative(NativeObject);
            }
        }

        // Has this object been destroyed, or scheduled for destruction
        public bool IsDestroyedOrPendingKill
        {
            get
            {
                return IsDestroyed || IsPendingKill;
            }
        }

        internal IntPtr _NativeObject;
        public IntPtr NativeObject { get { return _NativeObject; } }

        // This is an infrastructure class that provides core functionality for UObjects but is not a fully
        // functional base type. User types should subclass UnrealEngine.Core.Object instead.

        // wrapping constructor
        internal UnrealObject(IntPtr nativeObject)
        {
            _NativeObject = nativeObject;
        }

        // derivation constructor
        internal UnrealObject(ObjectInitializer initializer)
        {
            _NativeObject = initializer.NativeObject;
        }

        public override bool Equals(object obj)
        {
            UnrealObject other = obj as UnrealObject;
            if (other != null)
            {
                return _NativeObject.Equals(other._NativeObject);
            }

            return false;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        internal protected void CheckDestroyedByUnrealGC()
        {
            if (IsDestroyed)
            {
                throw new UnrealObjectDestroyedException("Attempting to access a destroyed unreal object of type " + GetType().ToString());
            }
        }

        public override int GetHashCode()
        {
            return _NativeObject.GetHashCode();
        }

        // Native callbacks
        void ClearNativePointer()
        {
            // Certain types of unreal objects can be deleted when references still exist to them, such as Actor and Component
            // clear out our native pointer when this happens
            _NativeObject = IntPtr.Zero;
        }

        public static T GetUnrealObjectWrapper<T>(IntPtr nativePointer) where T : UnrealObject
        {
            return (T)GetUnrealObjectWrapperNative(nativePointer);
        }

        [Obsolete("Use NewObject<T>")]
        public static T ConstructUnrealObject<T>(UnrealObject outer = null, Name objectName = default(Name)) where T : UnrealObject
        {
            return NewObject<T>(outer, objectName);
        }

#if CLSCOMPLIANT
        [CLSCompliant(false)]
#endif
        public static T NewObject<T>(
            UnrealObject outer, SubclassOf<T> klass,
            Name objectName = default(Name), ObjectFlags flags = ObjectFlags.NoFlags,
            UnrealObject template = null, bool copyTransientsFromClassDefaults = false, bool assumeTemplateIsArchetype = false
            ) where T : UnrealObject
        {
            IntPtr nativeOuter = outer?.NativeObject ?? IntPtr.Zero;
            IntPtr nativeTemplate = template?.NativeObject ?? IntPtr.Zero;
            return (T)ConstructUnrealObjectNative(typeof(T), klass.NativeClass, nativeOuter, objectName, flags, nativeTemplate, copyTransientsFromClassDefaults, IntPtr.Zero, assumeTemplateIsArchetype);
        }

        public static T NewObject<T>(UnrealObject outer) where T : UnrealObject
        {
            IntPtr nativeOuter = outer?.NativeObject ?? IntPtr.Zero;
            return (T)ConstructUnrealObjectNative(typeof(T), IntPtr.Zero, nativeOuter, Name.None, ObjectFlags.NoFlags, IntPtr.Zero, false, IntPtr.Zero, false);
        }

#if CLSCOMPLIANT
        [CLSCompliant(false)]
#endif
        public static T NewObject<T>(
            UnrealObject outer,
            Name objectName, ObjectFlags flags = ObjectFlags.NoFlags,
            UnrealObject template = null, bool copyTransientsFromClassDefaults = false, bool assumeTemplateIsArchetype = false
            ) where T : UnrealObject
        {
            Type type = typeof(T);
            IntPtr nativeOuter = outer?.NativeObject ?? IntPtr.Zero;
            IntPtr nativeTemplate = template?.NativeObject ?? IntPtr.Zero;
            return (T)ConstructUnrealObjectNative(typeof(T), IntPtr.Zero, nativeOuter, objectName, flags, nativeTemplate, copyTransientsFromClassDefaults, IntPtr.Zero, assumeTemplateIsArchetype);
        }

        public static T NewObject<T>(Name objectName = default (Name)) where T : UnrealObject
        {
            Type type = typeof(T);
            return (T)ConstructUnrealObjectNative(typeof(T), IntPtr.Zero, IntPtr.Zero, objectName, ObjectFlags.NoFlags, IntPtr.Zero, false, IntPtr.Zero, false);
        }

        // Get the CDO for a native UClass pointer, cast to T.
        public static T GetClassDefaultObject<T>(IntPtr unrealClass) where T : UnrealObject
        {
            return (T)GetDefaultObjectFromUnrealClass(unrealClass);
        }

        // Get the CDO for a specific subclass of the UnrealObject subclass represented by the type parameter.
        public static T GetClassDefaultObject<T>(Type type) where T : UnrealObject
        {
            return (T)GetDefaultObjectFromMonoClass(type);
        }

        // Get the CDO for the UnrealObject subclass represented by the type parameter.
        public static T GetClassDefaultObject<T>() where T : UnrealObject
        {
            return (T)GetDefaultObjectFromMonoClass(typeof(T));
        }

        // Get the CDO for an UnrealObject's actual UClass.  Note that this may not correspond to its managed Type,
        // for example if this wrapper represents an instance of a derived Blueprint class.
        public T GetDefaultObject<T>() where T : UnrealObject
        {
            return (T)GetDefaultObjectFromUnrealObject(NativeObject);
        }

        public T GetDefaultSubobjectByName<T>(string subobjectName) where T : UnrealObject
        {
            return (T)GetDefaultSubobjectFromName(NativeObject, subobjectName);
        }

        private ushort GetRepIndex(PropertyInfo property)
        {
            var fieldInfo = property.DeclaringType.GetField(property.Name + "_RepIndex", BindingFlags.NonPublic | BindingFlags.Static);
            return (ushort)fieldInfo.GetValue(this);
        }

        private LifetimeReplicatedProperty[] GetLifetimeReplicationList()
        {
            var lifetimeProperties = new List<LifetimeReplicatedProperty>();
            foreach (var propInfo in GetType().GetProperties(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance))
            {
                ReplicatedAttribute attr = (ReplicatedAttribute)Attribute.GetCustomAttribute(propInfo, typeof(ReplicatedAttribute));
                if (attr != null)
                {
                    lifetimeProperties.Add(new LifetimeReplicatedProperty(GetRepIndex(propInfo), attr.Condition));
                }
            }

            return lifetimeProperties.ToArray();
        }

        CustomReplicatedPropertyActivation[] GetCustomReplicationList()
        {
            var activationInfo = new List<CustomReplicatedPropertyActivation>();
            foreach (var propInfo in GetType().GetProperties(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance))
            {
                ReplicatedAttribute attr = (ReplicatedAttribute)Attribute.GetCustomAttribute(propInfo, typeof(ReplicatedAttribute));
                if (attr != null && attr.Condition == LifetimeCondition.Custom)
                {
                    activationInfo.Add(new CustomReplicatedPropertyActivation(GetRepIndex(propInfo), attr.ShouldReplicate(this)));
                }
            }

            return activationInfo.ToArray();
        }

        protected static Text ConstructPropertyText(UnrealObject owner, int offset)
        {
            return new Text(owner, offset);
        }

        // Get object name
        [DllImport("__MonoRuntime", EntryPoint = "UnrealObject_GetFName")]
        internal extern static Name GetFName(IntPtr nativeObject);

        // Is this object pending kill?
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private extern static bool IsPendingKillNative(IntPtr nativeObject);

        // Return an object wrapper corresponding to a UObject
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private extern static UnrealObject GetUnrealObjectWrapperNative(IntPtr nativePointer);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private extern static UnrealObject ConstructUnrealObjectNative(Type unrealType, IntPtr nativeClass, IntPtr nativeOuter, Name objectName, ObjectFlags flags, IntPtr nativeTemplate, bool copyTransientsFromClassDefaults, IntPtr instanceGraph, bool assumeTemplateIsArchetype);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private extern static UnrealObject GetDefaultObjectFromUnrealClass(IntPtr unrealType);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private extern static UnrealObject GetDefaultObjectFromMonoClass(Type monoClass);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static UnrealObject GetDefaultObjectFromUnrealObject(IntPtr nativeObject);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private extern static UnrealObject GetDefaultSubobjectFromName(IntPtr nativeObject, string subobjectName);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealObject_GetNativeFunctionFromClassAndName")]
        extern protected static IntPtr GetNativeFunctionFromClassAndName(IntPtr nativeClass,
                                                        [MarshalAs(UnmanagedType.LPWStr)]
                                                        string functionName);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealObject_GetNativeFunctionFromInstanceAndName")]
        extern protected static IntPtr GetNativeFunctionFromInstanceAndName(IntPtr nativeObject,
                                                         [MarshalAs(UnmanagedType.LPWStr)]
                                                         string functionName);

        [DllImport("__MonoRuntime", EntryPoint = "UnrealObject_GetNativeFunctionParamsSize")]
        extern protected static short GetNativeFunctionParamsSize(IntPtr nativeFunction);

        // Invoke a UFunction using a buffer to hold the parameters and return value.
        [DllImport("__MonoRuntime", EntryPoint = "UnrealObject_InvokeFunction")]
        extern protected static void InvokeFunction(IntPtr nativeObject, IntPtr nativeFunction, IntPtr arguments, int argumentsSize);

        // Invoke a static UFunction using a buffer to hold the parameters and return value.
        [DllImport("__MonoRuntime", EntryPoint = "UnrealObject_InvokeStaticFunction")]
        extern protected static void InvokeStaticFunction(IntPtr nativeClass, IntPtr nativeFunction, IntPtr arguments, int argumentsSize);

    }

    // Managed mirror of FLifetimeProperty.
    // Not exposed in the bindings since it's not needed in blueprint, nor in managed code,
    // other than in GetLifetimeReplicationList()
    internal struct LifetimeReplicatedProperty
    {
        public LifetimeReplicatedProperty(ushort repIndex, LifetimeCondition condition)
        {
            RepIndex = repIndex;
            Condition = condition;
        }

        public ushort RepIndex;
        public LifetimeCondition Condition;
    }

    internal struct CustomReplicatedPropertyActivation
    {
        public CustomReplicatedPropertyActivation(ushort repIndex, bool active)
        {
            RepIndex = repIndex;
            Active = active;
        }

        public ushort RepIndex;
        public bool Active;
    }
}
