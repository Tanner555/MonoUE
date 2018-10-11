// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Reflection;

namespace UnrealEngine.Runtime
{
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Class)]
    class PropertyFlagsMapAttribute : Attribute
    {
        public PropertyFlags Flags;

        public PropertyFlagsMapAttribute(PropertyFlags flags=PropertyFlags.None)
        {
            Flags = flags;
        }
    }

    [Flags]
    public enum UserPropertyFlags
    {
        [PropertyFlagsMap(PropertyFlags.None)]
        None = 0,
        [PropertyFlagsMap(PropertyFlags.Config)]
        Config = 0x1,
        [PropertyFlagsMap(PropertyFlags.Config| PropertyFlags.GlobalConfig)]
        GlobalConfig = 0x2,
        [PropertyFlagsMap(PropertyFlags.Transient)]
        Transient = 0x4,
        [PropertyFlagsMap(PropertyFlags.DuplicateTransient)]
        DuplicateTransient = 0x8,
        [PropertyFlagsMap(PropertyFlags.TextExportTransient)]
        TextExportTransient = 0x10,
        [PropertyFlagsMap(PropertyFlags.NonPIEDuplicateTransient)]
        NonPIETransient = 0x20,
        [PropertyFlagsMap(PropertyFlags.ExportObject)]
        Export = 0x40,
        [PropertyFlagsMap(PropertyFlags.NoClear)]
        NoClear = 0x100,
        [PropertyFlagsMap(PropertyFlags.EditFixedSize)]
        EditFixedSize = 0x200,
        [PropertyFlagsMap(PropertyFlags.Edit|PropertyFlags.BlueprintVisible|PropertyFlags.Interp)]
        Interp = 0x400,
        [PropertyFlagsMap(PropertyFlags.NonTransactional)]
        NonTransactional = 0x800,
        [PropertyFlagsMap(PropertyFlags.PersistentInstance | PropertyFlags.ExportObject | PropertyFlags.InstancedReference)]
        Instanced = 0x1000,
        // TODO: delegates,
        [PropertyFlagsMap(PropertyFlags.AssetRegistrySearchable)]
        AssetRegistrySearchable = 0x2000,
        [PropertyFlagsMap(PropertyFlags.SimpleDisplay)]
        SimpleDisplay = 0x4000,
        [PropertyFlagsMap(PropertyFlags.AdvancedDisplay)]
        AdvancedDisplay = 0x8000,
        [PropertyFlagsMap(PropertyFlags.SaveGame)]
        SaveGame = 0x10000
    }

    [AttributeUsage(AttributeTargets.Property | AttributeTargets.Field)]
    [PropertyFlagsMap(PropertyFlags.None)] // No default flags, but make that explicit so we can post-process all attributes with this attribute.
    public sealed class UPropertyAttribute : Attribute
    {
        public UPropertyAttribute(UserPropertyFlags flags = UserPropertyFlags.None)
        {
            Flags = flags;
            ArrayDim = 1;
        }

        public UserPropertyFlags Flags
        {
            get;
            private set;
        }

        // Specifies the desired length for a fixed-size property array.
        public int ArrayDim;
    }

    [Flags]
    public enum ObjectRestriction
    {
        // Allow editing on any object
        [PropertyFlagsMap(PropertyFlags.None)]
        None,
        // Only allow editing on instances.  Equivalent to native Edit/VisibleInstanceOnly
        [PropertyFlagsMap(PropertyFlags.DisableEditOnTemplate)]
        Instance,
        // Only allow editing on templates.  Equivalent to native Edit/VisibleDefaultsOnly
        [PropertyFlagsMap(PropertyFlags.DisableEditOnInstance)]
        Defaults,
    }

    // When present, this attribute specifies that a UProperty should appear in the editor's property windows.
    // An unmodified [EditorVisible] attribute corresponds to the Unreal's EditAnywhere keyword.
    [AttributeUsage(AttributeTargets.Property | AttributeTargets.Field)]
    [PropertyFlagsMap(PropertyFlags.Edit)]
    public sealed class EditorVisibleAttribute : Attribute
    {
        public EditorVisibleAttribute(ObjectRestriction restriction = ObjectRestriction.None)
        {
            ObjectRestriction = restriction;
        }

        // Setting ReadOnly to true is equivalent to making the property VisibleAnywhere.
        [PropertyFlagsMap(PropertyFlags.EditConst)]
        public bool ReadOnly { get; set; }

        public ObjectRestriction ObjectRestriction { get; private set; }
    }

    // When present, this attribute specifies that a UProperty should be available in Blueprint.
    // An unmodified [BlueprintVisible] attribute corresponds to the Unreal's BlueprintReadWrite keyword.
    [AttributeUsage(AttributeTargets.Property | AttributeTargets.Field)]
    [PropertyFlagsMap(PropertyFlags.BlueprintVisible)]
    public sealed class BlueprintVisibleAttribute : Attribute
    {
        [PropertyFlagsMap(PropertyFlags.BlueprintReadOnly)]
        public bool ReadOnly { get; set; }
    }

    public delegate void ReplicationNotificationCallback();

    // When present, specifies that a UProperty is relevant to network replication.
    [AttributeUsage(AttributeTargets.Property)]
    [PropertyFlagsMap(PropertyFlags.Net)]
    public sealed class ReplicatedAttribute : Attribute
    {
        public ReplicatedAttribute()
        {
            // We could do this with a default parameter, but that would appear as a constructor argument in the IL.
            // To support the CustomConditionMethod shorthand for LifetimeCondition.Custom, we need to be able to
            // distinguish between the user explicitly specifying no condition and not specifying a condition at all.
            Condition = LifetimeCondition.None;
        }

        public ReplicatedAttribute(LifetimeCondition condition)
        {
            Condition = condition;
        }

        // Secondary condition to check before replicating a UProperty.
        // Note that, although this is specified as part of the [Replicated] attribute, it adds no property flags.
        // It is used only to automate property replication setup at runtime.
        public LifetimeCondition Condition { get; private set; }

        private string InternalCustomConditionMethod;
        // When using LifetimeCondition.Custom, this property must be set to the name of a method
        // within the same UClass, which returns true when the UProperty should be replicated.
        public string CustomConditionMethod
        {
            get { return InternalCustomConditionMethod; }
            set
            {
                InternalCustomConditionMethod = value;

                // We allow "no condition, with a custom condition method" as a shorthand for LifetimeCondition.Custom.
                // This is enforced in MonoAssemblyProcess by throwing invalid UProperty exceptions if this property is
                // ever used as a named argument with any other lifetime condition.  Setting the condition here ensures
                // that the correct condition is set at runtime, during replication init.
                Condition = LifetimeCondition.Custom;
            }
        }

        private delegate bool CustomReplicationConditionCallback();
        private CustomReplicationConditionCallback CustomConditionCallback;
        // Returns true if the UProperty associated with this attribute should replicated,
        // based on the configured custom activation callback.
        public bool ShouldReplicate(UnrealObject owner)
        {
            if (Condition != LifetimeCondition.Custom)
            {
                throw new InvalidOperationException("ShouldReplicate() checks are only valid when using LifetimeCondition.Custom.");
            }

            if (CustomConditionCallback == null)
            {
                MethodInfo method = owner.GetType().GetMethod(CustomConditionMethod, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
                CustomConditionCallback = (CustomReplicationConditionCallback)Delegate.CreateDelegate(typeof(CustomReplicationConditionCallback), owner, method);
            }

            return CustomConditionCallback();
        }

        // Optional name of a method to be called when the associated UProperty is replicated.
        // The named method must be a UFunction and a member of the same class, with void return type and no parameters.
        [PropertyFlagsMap(PropertyFlags.RepNotify)]
        public string NotificationMethod { get; set; }
    }
}
