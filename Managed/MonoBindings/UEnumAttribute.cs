// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;

namespace UnrealEngine.Runtime
{
    [AttributeUsage(AttributeTargets.Enum)]
    public sealed class UEnumAttribute : Attribute
    {
        public string NativeClassOwner;
        public string NativeEnumName;
        public bool BlueprintVisible;
    }
}
