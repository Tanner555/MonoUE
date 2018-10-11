// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using OpenTK;
using System;
using System.Runtime.InteropServices;
using UnrealEngine.Runtime;


namespace UnrealEngine.Core
{
    public partial struct Transform
    {
        public static readonly Transform Identity = new Transform(Quaternion.Identity, Vector3.Zero, new Vector3(1.0f, 1.0f, 1.0f));

        public Transform(Quaternion rotation)
        {
            Rotation = rotation;
            Translation = Vector3.Zero;
            Scale3D = new Vector3(1.0f, 1.0f, 1.0f);
        }

        public Transform(Rotator rotation)
        {
            Rotation = rotation.ToQuaternion();
            Translation = Vector3.Zero;
            Scale3D = new Vector3(1.0f, 1.0f, 1.0f);
        }

        public Transform(Quaternion rotation, Vector3 translation)
        {
            Rotation = rotation;
            Translation = translation;
            Scale3D = new Vector3(1.0f, 1.0f, 1.0f);
        }

        public Transform(Quaternion rotation, Vector3 translation, Vector3 scale3D)
        {
            Rotation = rotation;
            Translation = translation;
            Scale3D = scale3D;
        }

        public Transform(Rotator rotation, Vector3 translation)
        {
            Rotation = rotation.ToQuaternion();
            Translation = translation;
            Scale3D = new Vector3(1.0f, 1.0f, 1.0f);
        }

        public Transform(Rotator rotation, Vector3 translation, Vector3 scale3D)
        {
            Rotation = rotation.ToQuaternion();
            Translation = translation;
            Scale3D = scale3D;
        }

        public Matrix4 ToMatrixWithScale()
        {
            Matrix4 OutMatrix = new Matrix4();

            OutMatrix.M41 = Translation.X;
            OutMatrix.M42 = Translation.Y;
            OutMatrix.M43 = Translation.Z;

            float x2 = Rotation.X + Rotation.X;
            float y2 = Rotation.Y + Rotation.Y;
            float z2 = Rotation.Z + Rotation.Z;
            {
                float xx2 = Rotation.X * x2;
                float yy2 = Rotation.Y * y2;
                float zz2 = Rotation.Z * z2;

                OutMatrix.M11 = (1.0f - (yy2 + zz2)) * Scale3D.X;
                OutMatrix.M22 = (1.0f - (xx2 + zz2)) * Scale3D.Y;
                OutMatrix.M33 = (1.0f - (xx2 + yy2)) * Scale3D.Z;
            }
            {
                float yz2 = Rotation.Y * z2;
                float wx2 = Rotation.W * x2;

                OutMatrix.M32 = (yz2 - wx2) * Scale3D.Z;
                OutMatrix.M23 = (yz2 + wx2) * Scale3D.Y;
            }
            {
                float xy2 = Rotation.X * y2;
                float wz2 = Rotation.W * z2;

                OutMatrix.M21 = (xy2 - wz2) * Scale3D.Y;
                OutMatrix.M12 = (xy2 + wz2) * Scale3D.X;
            }
            {
                float xz2 = Rotation.X * z2;
                float wy2 = Rotation.W * y2;

                OutMatrix.M31 = (xz2 + wy2) * Scale3D.Z;
                OutMatrix.M13 = (xz2 - wy2) * Scale3D.X;
            }

            OutMatrix.M14 = 0.0f;
            OutMatrix.M24 = 0.0f;
            OutMatrix.M34 = 0.0f;
            OutMatrix.M44 = 1.0f;

            return OutMatrix;
        }

        // do backward operation when inverse, translation -> rotation
        public Vector3 InverseTransformVectorNoScale(Vector3 V)
        {
            return (Quaternion.Invert(Rotation).Scale(V));
        }
    }
}