// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using UnrealEngine.Runtime;

namespace UnrealEngine.Engine
{
    public partial class CharacterMovementComponent
    {
        public void ForceReplicationUpdate()
        {
            CheckDestroyedByUnrealGC();

            ForceReplicationUpdate(NativeObject);
        }

        [DllImport("__MonoRuntime", EntryPoint = "CharacterMovementComponent_ForceReplicationUpdate")]
        private extern static void ForceReplicationUpdate(IntPtr NativePawnPointer);
    }
}
