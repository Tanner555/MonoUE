// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;

namespace UnrealEngine.Runtime
{
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Property)]
    class StructFlagsMapAttribute : Attribute
    {
        public StructFlags Flags;

        public StructFlagsMapAttribute(StructFlags flags = StructFlags.None)
        {
            Flags = flags;
        }
 
    }

    [AttributeUsage(AttributeTargets.Struct)]
    [StructFlagsMap(StructFlags.Native)]
    public sealed class UStructAttribute : Attribute
    {
        public string NativeClassOwner;
        public bool NativeBlittable = false;

        [StructFlagsMap(StructFlags.Atomic)]
        public bool Atomic { get; set; }
    }
}
