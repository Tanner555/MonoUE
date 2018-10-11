// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using OpenTK;
using System.Runtime.InteropServices;

namespace UnrealEngine.Engine
{
    public static class UnrealMath
    {
        // TODO: could probably generate these or rename KismetMathLibrary to something better?
        /// <summary>Returns a random vector with length of 1</summary>
        public static Vector3 RandomUnitVector()
        {
            return MathLibrary.RandomUnitVector();
        }


        /// <summary>
        /// * Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
        /// * @param ConeDir       The base "center" direction of the cone.
        /// * @param ConeHalfAngle         The half-angle of the cone (from ConeDir to edge), in degrees.
        /// </summary>
        public static Vector3 RandomUnitVectorInCone(Vector3 coneDir, float coneHalfAngle)
        {
            return MathLibrary.RandomUnitVectorInConeInRadians(coneDir, coneHalfAngle);
        }

        [DllImport("__MonoRuntime", EntryPoint = "UnrealInterop_RandHelper")]
        public extern static int RandHelper(int max);
    }
}