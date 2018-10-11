// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.CompilerServices;
using UnrealEngine.Runtime;


namespace UnrealEngine.Engine
{
    public partial class SkinnedMeshComponent
    {
        public PhysicsAsset GetPhysicsAsset()
        {
            CheckDestroyedByUnrealGC();

            return GetPhysicsAssetNative(NativeObject);
        }
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static PhysicsAsset GetPhysicsAssetNative(IntPtr NativeComponentPointer);
    }
}