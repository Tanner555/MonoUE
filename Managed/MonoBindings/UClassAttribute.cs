// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Reflection;

namespace UnrealEngine.Runtime
{
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Class)]
    class ClassFlagsMapAttribute : Attribute
    {
        public ClassFlags Flags;

        public ClassFlagsMapAttribute(ClassFlags flags = ClassFlags.None)
        {
            Flags = flags;
        }
    }

    [Flags]
    public enum UserClassFlags
    {
        [ClassFlagsMap(ClassFlags.None)]
        None = 0x0,

        [ClassFlagsMap(ClassFlags.PerObjectConfig)]
        PerObjectConfig = 0x10,

        [ClassFlagsMap(ClassFlags.DefaultToInstanced)]
        DefaultToInstanced = 0x20,

        // The following flag is for internal use only and will be flagged (ha) in MonoAssemblyProcess on user-written classes
        [ClassFlagsMap(ClassFlags.Native)]
        NativeBindingsClass = 0x1000,
    }

    [AttributeUsage(AttributeTargets.Class)]
    [ClassFlagsMap(ClassFlags.None)]
    public sealed class UClassAttribute : Attribute
    {
        public UserClassFlags Flags { get; private set; }
        public string ConfigFile { get; set; }
        public string Group { get; set; }
        public UClassAttribute(UserClassFlags Flags = UserClassFlags.None)
        {
            this.Flags = Flags;
        }
    }

    //If this enum changes AssemblyMetadata.cs and MonoUnrealClass.cpp need updating.
    /// <summary>
    /// Indicates on what level a blueprint can interact with a UClass
    /// </summary>
    public enum BlueprintAccess
    {
        None,
        Accessible,
        Derivable
    }

    [AttributeUsage(AttributeTargets.Class)]
    public sealed class BlueprintUseAttribute : Attribute
    {
        public BlueprintAccess Usage { get; private set; }

        public BlueprintUseAttribute(BlueprintAccess usage = BlueprintAccess.Derivable)
        {
            Usage = usage;
        }
    }

    [AttributeUsage(AttributeTargets.Class)]
    public sealed class TransientAttribute : Attribute
    {
        public bool Enabled { get; private set; }
    }

    [AttributeUsage(AttributeTargets.Class)]
    public sealed class PlaceableAttribute : Attribute
    {
        public bool Enabled { get; private set; }
    }
}
