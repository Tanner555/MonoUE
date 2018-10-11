// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.CompilerServices;
using UnrealEngine.Runtime;

namespace UnrealEngine.Core
{
    public partial class Object
    {
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        protected extern static UnrealEngine.Engine.World GetWorldFromContextObjectNative(IntPtr nativeContextObject);
    }
}