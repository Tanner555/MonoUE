// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using UnrealEngine.Runtime;
using OpenTK;
using System.Runtime.InteropServices;

namespace UnrealEngine.Engine
{
    public partial class SystemLibrary
    {
        public delegate void TimerCallback();
        // Sets a timer to call the given delegate at a set interval.
        // If a timer is already set for this delegate, this will update the current timer to the new 
        // parameters and reset its elapsed time to 0.
        public static void SetTimer(Core.Object owner, TimerCallback callback, float duration, bool looping)
        {
            if (owner.IsDestroyedOrPendingKill)
            {
                throw new UnrealObjectDestroyedException("Trying to set timer on destroyed object");
            }
            if (null == callback)
            {
                throw new ArgumentNullException("callback");
            }
            //TODO: support non-UFunction timer delegates
            SystemLibrary.SetTimer(owner, callback.Method.Name, duration, looping);
        }

        public static void ClearTimer(Core.Object owner, TimerCallback callback)
        {
            if (owner.IsDestroyedOrPendingKill)
            {
                throw new UnrealObjectDestroyedException("Trying to set timer on destroyed object");
            }
            if (null == callback)
            {
                throw new ArgumentNullException("callback");
            }
            //TODO: support non-UFunction timer delegates
            SystemLibrary.ClearTimer(owner, callback.Method.Name);
        }

        [DllImport("__MonoRuntime", EntryPoint = "CollisionChannel_FromTraceType")]
        public extern static CollisionChannel ConvertToCollisionChannel(TraceTypeQuery traceType);

        [DllImport("__MonoRuntime", EntryPoint = "CollisionChannel_FromObjectType")]
        public extern static CollisionChannel ConvertToCollisionChannel(ObjectTypeQuery traceType);

        [DllImport("__MonoRuntime", EntryPoint = "TraceType_FromCollisionChannel")]
        public extern static TraceTypeQuery ConvertToTraceType(CollisionChannel collisionChannel);

        [DllImport("__MonoRuntime", EntryPoint = "ObjectType_FromCollisionChannel")]
        public extern static ObjectTypeQuery ConvertToObjectType(CollisionChannel collisionChannel);
    }

}
