// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using OpenTK;
using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using UnrealEngine.Runtime;

namespace UnrealEngine.Engine
{
    public partial class Controller
    {
        public void GetPlayerViewPoint(out Vector3 location, out Rotator rotation)
        {
            GetPlayerViewPoint(NativeObject, out location, out rotation);
        }

        [DllImport("__MonoRuntime", EntryPoint = "Controller_GetPlayerViewPoint")]
        private extern static void GetPlayerViewPoint(IntPtr NativePawnPointer, out Vector3 location, out Rotator rotation);
    }
}
