// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;
using OpenTK;

namespace UnrealEngine.Runtime
{
    // C# mirror of C++ FRandomStream
    [UStruct(NativeBlittable = true)]
	public struct RandomStream
    {
        int InitialSeed;
        int Seed;

        ///<summary>Current seed</summary>
        public int CurrentSeed
        {
			get { return Seed; }
        }

        /// <summary>
        /// Creates and initializes a new random stream from the specified seed value.
        /// </summary>
        /// <param name="initialSeed">The seed value.</param>
        public RandomStream(int initialSeed)
        {
            InitialSeed = initialSeed;
            Seed = initialSeed;
        }

        /// <summary>
        /// Initializes this random stream with the specified seed value.
        /// </summary>
        /// <param name="initialSeed">The seed value.</param>
        public void Initialize(int initialSeed)
        {
            InitialSeed = initialSeed;
            Seed = initialSeed;
        }

        /// <summary>
        /// Resets this random stream to the initial seed value.
        /// </summary>
        public void Reset()
        {
            Seed = InitialSeed;
        }

        /// <summary>
        /// Generates a new random seed.
        /// </summary>
        public void GenerateNewSeed()
        {
            Initialize(new Random().Next());
        }

		/// <summary>
		/// Returns a random number between 0 and 1.
		/// </summary>
        /// <returns>Random number.</returns>
		public float GetFraction( )
        {
			return FRandomStream_GetFraction(ref this);
		}

        /// <summary>
        /// Returns a random number between 0 and MAXUINT.
        /// </summary>
        /// <returns>Random number.</returns>
        [CLSCompliant(false)]
		public uint GetUnsignedInt( )
		{
			return FRandomStream_GetUnsignedInt(ref this);
		}

        /// <summary>
        /// Returns a random vector of unit size.
        /// </summary>
        /// <returns>Random unit vector.</returns>
        public Vector3 GetUnitVector( )
        {
            Vector3 result = new Vector3();
			FRandomStream_GetUnitVector(ref this, out result);
            return result;
		}


        /// <summary>
        /// Helper function for rand implementations.
        /// </summary>
        /// <returns>A random number >= Min and &lt;= Max</returns>
        public int RandRange(int min, int max) 
		{
			return FRandomStream_RandRange(ref this, min, max);
        }

        /// <summary>
		/// Returns a random unit vector, uniformly distributed, within the specified cone.
		/// </summary>
		/// <param name="dir"> The center direction of the cone</param>
		/// <param name="coneHalfAngleRad"> Half-angle of cone, in radians.</param>
        /// <returns>Normalized vector within the specified cone.</returns>
        public Vector3 GetUnitVectorInCone(Vector3 dir, float coneHalfAngleRad)
        {
            Vector3 result = new Vector3();
            FRandomStream_VRandCone(ref this, out result, dir, coneHalfAngleRad);
            return result;
        }

        /// <summary>
        /// Returns a random unit vector, uniformly distributed, within the specified cone.
        /// </summary>
        /// <param name="dir">The center direction of the cone</param>
        /// <param name="horizontalConeHalfAngleRad">Horizontal half-angle of cone, in radians.</param>
        /// <param name="verticalConeHalfAngleRad">Vertical half-angle of cone, in radians.</param>
        /// <returns>Normalized vector within the specified cone.</returns>
        public Vector3 GetUnitVectorInCone(Vector3 dir, float horizontalConeHalfAngleRad, float verticalConeHalfAngleRad)
        {
            Vector3 result = new Vector3();
            FRandomStream_VRandCone2(ref this, out result, dir, horizontalConeHalfAngleRad, verticalConeHalfAngleRad);
            return result;
        }


		[DllImport("__MonoRuntime")]
        static extern float FRandomStream_GetFraction(ref RandomStream selfParameter);

        [DllImport("__MonoRuntime")]
        static extern uint FRandomStream_GetUnsignedInt(ref RandomStream selfParameter);

        [DllImport("__MonoRuntime")]
        static extern void FRandomStream_GetUnitVector(ref RandomStream selfParameter, out Vector3 result);

        [DllImport("__MonoRuntime")]
        static extern int FRandomStream_RandRange(ref RandomStream selfParameter, int min, int max);

        [DllImport("__MonoRuntime")]
        static extern void FRandomStream_VRandCone(ref RandomStream selfParameter, out Vector3 result, Vector3 dir, float coneHalfAngleRad);

        [DllImport("__MonoRuntime")]
        static extern void FRandomStream_VRandCone2(ref RandomStream selfParameter, out Vector3 result, Vector3 dir, float horizontalConeHalfAngleRad, float verticalConeHalfAngleRad);

    }
}
 