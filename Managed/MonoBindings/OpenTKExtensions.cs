// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using OpenTK;
using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using UnrealEngine.Core;

namespace UnrealEngine.Runtime
{
    public static class OpenTKExtensions
    {
        const float SmallNumber = 1e-8f;
        const float KindaSmallNumber = 1e-4f;

        // Vector3
        public static Vector3 SafeNormal(this Vector3 vec, float tolerance = SmallNumber)
        {
            Vector3 result;
            NativeSafeNormal(out result, vec, tolerance);
            return result;
        }

        public static Vector3 SafeNormal2D(this Vector3 vec, float tolerance = SmallNumber)
        {
            Vector3 result;
            NativeSafeNormal2D(out result, vec, tolerance);
            return result;
        }

        public static bool IsNearlyZero(this Vector3 vec, float tolerance)
        {
	        return
		        Math.Abs(vec.X) < tolerance
		        &&	Math.Abs(vec.Y) < tolerance
		        &&	Math.Abs(vec.Z) < tolerance;
        }

        public static Rotator ToRotator(this Vector3 vec)
        {
            Rotator result;
            NativeToRotator(out result, vec);
            return result;
        }

        // Matrix4
        public static Vector3 GetOrigin(this Matrix4 matrix)
        {
            return new Vector3(matrix.M41, matrix.M42, matrix.M43);
        }

        public static Vector3 GetScaledAxis(this Matrix4 matrix, Axis axis)
        {
            switch(axis)
            {
                case Core.Axis.X:
                    return matrix.Row0.Xyz;
                case Core.Axis.Y:
                    return matrix.Row1.Xyz;
                case Core.Axis.Z:
                    return matrix.Row2.Xyz;
                default:
                    throw new ArgumentException("axis");
            }
        }

        public static Matrix4 InvertSafe(this Matrix4 matrix)
        {
	        // Check for zero scale matrix to invert
	        if(	matrix.GetScaledAxis( Axis.X ).IsNearlyZero(SmallNumber) && 
		        matrix.GetScaledAxis( Axis.Y ).IsNearlyZero(SmallNumber) && 
		        matrix.GetScaledAxis( Axis.Z ).IsNearlyZero(SmallNumber) ) 
	        {
		        // just set to zero - avoids unsafe inverse of zero and duplicates what QNANs were resulting in before (scaling away all children)
                return new Matrix4();
	        }
	        else
	        {
                matrix.Invert();
                return matrix;
	        }
        }

        public static Matrix4 CreateRotationTranslationMatrix(Rotator rot, Vector3 origin)
        {
            const float piOver180 = (float)Math.PI / 180.0f;
	        float SR = (float)Math.Sin(rot.Roll * piOver180);
            float SP = (float)Math.Sin(rot.Pitch * piOver180);
            float SY = (float)Math.Sin(rot.Yaw * piOver180);
            float CR = (float)Math.Cos(rot.Roll * piOver180);
            float CP = (float)Math.Cos(rot.Pitch * piOver180);
            float CY = (float)Math.Cos(rot.Yaw * piOver180);

            return new Matrix4(
	            CP * CY,
	            CP * SY,
	            SP,
	            0.0f,

	            SR * SP * CY - CR * SY,
	            SR * SP * SY + CR * CY,
	            - SR * CP,
	            0.0f,

	            -( CR * SP * CY + SR * SY ),
	            CY * SR - CR * SP * SY,
	            CR * CP,
	            0.0f,

	            origin.X,
	            origin.Y,
	            origin.Z,
	            1.0f);
        }

        // Quaternion
        public static Vector3 Scale(this Quaternion quat, Vector3 vec)
        {
            Vector3 result;
            NativeQuatScaleVector(out result, quat, vec);
            return result;
        }
        [DllImport("__MonoRuntime", EntryPoint = "FQuat_ScaleVector")]
        extern private static void NativeQuatScaleVector(out Vector3 result, Quaternion quat, Vector3 vec);

        [DllImport("__MonoRuntime", EntryPoint = "FVector_SafeNormal")]
        extern private static void NativeSafeNormal(out Vector3 result, Vector3 vec, float tolerance);
        [DllImport("__MonoRuntime", EntryPoint = "FVector_SafeNormal2D")]
        extern private static void NativeSafeNormal2D(out Vector3 result, Vector3 vec, float tolerance);
        [DllImport("__MonoRuntime", EntryPoint = "FVector_ToRotator")]
        extern private static void NativeToRotator(out Rotator result, Vector3 vec);
    }
}
