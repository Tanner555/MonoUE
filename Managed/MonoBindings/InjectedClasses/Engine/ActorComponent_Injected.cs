// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;
using UnrealEngine.Runtime;


namespace UnrealEngine.Engine
{
    public partial class ActorComponent
    {
        public bool TickEnabled
        {
            get
            {
                CheckDestroyedByUnrealGC();

                return GetComponentTickEnabled(NativeObject);
            }
            set 
            {
                CheckDestroyedByUnrealGC();

                SetComponentTickEnabled(NativeObject, value);
            }
        }
        [DllImport("__MonoRuntime", EntryPoint = "ActorComponent_GetComponentTickEnabled")]
        private extern static bool GetComponentTickEnabled(IntPtr NativeComponentPointer);
        [DllImport("__MonoRuntime", EntryPoint = "ActorComponent_SetComponentTickEnabled")]
        private extern static void SetComponentTickEnabled(IntPtr NativeComponentPointer, bool enabled);

        public TickingGroup TickGroup
        {
            get 
            {
                CheckDestroyedByUnrealGC();

                return GetTickGroup(NativeObject);
            }
            set 
            {
                CheckDestroyedByUnrealGC();

                SetTickGroup(NativeObject, value);
            }
        }
        [DllImport("__MonoRuntime", EntryPoint = "ActorComponent_GetTickGroup")]
        private extern static TickingGroup GetTickGroup(IntPtr NativeComponentPointer);
        [DllImport("__MonoRuntime", EntryPoint = "ActorComponent_SetTickGroup")]
        private extern static void SetTickGroup(IntPtr NativeComponentPointer, TickingGroup tickGroup);
    }
}