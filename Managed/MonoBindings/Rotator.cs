// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using OpenTK;
using System;
using System.Runtime.InteropServices;
using UnrealEngine.Core;

namespace UnrealEngine.Runtime
{
    [UStruct(NativeBlittable=true)]
    public struct Rotator
    {
        // Stored in degrees
        public float Pitch;
        public float Yaw;
        public float Roll;

        public readonly static Rotator ZeroRotator = new Rotator(0, 0, 0);

        public Rotator(float pitch, float yaw, float roll)
        {
            Pitch = pitch;
            Yaw = yaw;
            Roll = roll;
        }

        public Rotator(Quaternion quat)
        {
            FRotator_FromQuat(out this, quat);
        }

        public Rotator(Matrix4 rotationMatrix)
        {
            FRotator_FromMatrix(out this, ref rotationMatrix);
        }

        public Rotator(Vector3 vec)
        {
	        // Find yaw.
	        Yaw = (float)(Math.Atan2(vec.Y, vec.X) * 180.0 / Math.PI);

	        // Find pitch.
	        Pitch = (float)(Math.Atan2(vec.Z, Math.Sqrt(vec.X * vec.X + vec.Y * vec.Y)) * 180.0 / Math.PI);

	        // Find roll.
	        Roll = 0.0f;
        }

        public Quaternion ToQuaternion()
        {
            Quaternion quat;
            FQuat_FromRotator(out quat, this);
            return quat;
        }

        public Matrix4 ToMatrix()
        {
            Matrix4 rotationMatrix;
            FMatrix_FromRotator(out rotationMatrix, this);
            return rotationMatrix;
        }

        // Convert the rotator into a vector facing in its direction.
        public Vector3 ToVector()
        {
            Vector3 direction;
            FVector_FromRotator(out direction, this);
            return direction;
        }

        static public Rotator operator+(Rotator lhs, Rotator rhs)
        {
            return new Rotator 
            { 
                Pitch = lhs.Pitch + rhs.Pitch, 
                Yaw = lhs.Yaw + rhs.Yaw, 
                Roll = lhs.Roll + rhs.Roll 
            };
        }

        static public Rotator operator -(Rotator lhs, Rotator rhs)
        {
            return new Rotator
            {
                Pitch = lhs.Pitch - rhs.Pitch,
                Yaw = lhs.Yaw - rhs.Yaw,
                Roll = lhs.Roll - rhs.Roll
            };
        }

        static public Rotator operator -(Rotator rotator)
        {
            return new Rotator
            {
                Pitch = -rotator.Pitch,
                Yaw = -rotator.Yaw,
                Roll = -rotator.Roll
            };
        }

        static public Rotator operator *(Rotator rotator, float scale)
        {
            return new Rotator
            {
                Pitch = rotator.Pitch * scale,
                Yaw = rotator.Yaw * scale,
                Roll = rotator.Roll * scale
            };
        }

        static public Rotator operator *(float scale, Rotator rotator)
        {
            return rotator * scale;
        }

        public Vector3 GetForwardVector()
        {
            return ToMatrix().GetScaledAxis(Axis.X);
        }

        public Vector3 GetRightVector()
        {
            return ToMatrix().GetScaledAxis(Axis.Y);
        }

        public Vector3 GetUpVector()
        {
            return ToMatrix().GetScaledAxis(Axis.Z);
        }

        public override string ToString()
        {
            return String.Format("({0}, {1}, {2})", Pitch, Yaw, Roll);
        }

        [DllImport("__MonoRuntime")]
        extern private static void FRotator_FromQuat(out Rotator rotator, Quaternion quat);

        [DllImport("__MonoRuntime")]
        extern private static void FRotator_FromMatrix(out Rotator rotator, ref Matrix4 rotationMatrix);

        [DllImport("__MonoRuntime")]
        extern private static void FQuat_FromRotator(out Quaternion quat, Rotator rotator);

        [DllImport("__MonoRuntime")]
        extern private static void FMatrix_FromRotator(out Matrix4 rotationMatrix, Rotator rotator);

        [DllImport("__MonoRuntime")]
        extern private static void FVector_FromRotator(out Vector3 direction, Rotator rotator);
    }
}
