// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using UnrealEngine.Runtime;

namespace UnrealEngine.Engine
{
    // Mirrors native, non-UEnum ENetMode
    public enum NetMode
    {
        Standalone,
        DedicatedServer,
        ListenServer,
        Client,	// note that everything below this value is a kind of server
    }

    public partial class Actor
    {
        public bool IsRootComponentStatic
        {
            get
            {
                return null != RootComponent && RootComponent.Mobility == ComponentMobility.Static;
            }
        }

        public bool IsRootComponentStationary
        {
            get
            {
                return null != RootComponent && RootComponent.Mobility == ComponentMobility.Stationary;
            }
        }

        public Core.Box GetComponentsBoundingBox(bool includeNonCollidingComponents=false)
        {
            CheckDestroyedByUnrealGC();

            Core.Box result = new Core.Box();
            GetComponentsBoundingBoxNative(NativeObject, ref result, includeNonCollidingComponents);
            return result;
        }

        [DllImport("__MonoRuntime", EntryPoint = "Actor_GetComponentsBoundingBoxNative")]
        private extern static void GetComponentsBoundingBoxNative(IntPtr NativeActor, ref Core.Box outBox, bool includeNonCollidingComponents);

        public bool SetRootComponent(SceneComponent NewRootComponent)
        {
            unsafe { return SetRootNodeOnActor(NativeObject.ToPointer(), NewRootComponent.NativeObject.ToPointer()); }
        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        unsafe private extern static bool SetRootNodeOnActor(void* NativeActorPointer, void* NativeComponentPointer);

        public NetRole Role
        {
            get { unsafe { return GetNetRole(NativeObject.ToPointer()); } }
        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        unsafe private extern static NetRole GetNetRole(void* NativeActor);

        public NetMode NetMode
        {
            get { unsafe { return GetNetMode(NativeObject.ToPointer()); } }
        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        unsafe private extern static NetMode GetNetMode(void* NativeActor);

        // Used for replication (bNetUseOwnerRelevancy & bOnlyRelevantToOwner) and visibility (PrimitiveComponent bOwnerNoSee and bOnlyOwnerSee)
        public Actor Owner
        {
            get { return GetOwner(); }
            set 
            {
                IntPtr nativeValue = value == null ? IntPtr.Zero : value.NativeObject;
                unsafe { SetOwner(this.NativeObject.ToPointer(), nativeValue.ToPointer()); }
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static Actor GetOwner();

        [MethodImpl(MethodImplOptions.InternalCall)]
        unsafe private extern static void SetOwner(void* thisActor, void* newOwner);

        #pragma warning disable 1998
        protected virtual async Task Update(CancellationToken token)
        {
        }
        #pragma warning restore 1998

        protected virtual void BindInput(InputComponent inputComponent)
        {
        }

        public bool TickEnabled
        {
            get
            {
                CheckDestroyedByUnrealGC();

                return GetActorTickEnabled(NativeObject);
            }
            set
            {
                CheckDestroyedByUnrealGC();

                SetActorTickEnabled(NativeObject, value);
            }
        }
        [DllImport("__MonoRuntime", EntryPoint = "Actor_GetActorTickEnabled")]
        private extern static bool GetActorTickEnabled(IntPtr NativeComponentPointer);
        [DllImport("__MonoRuntime", EntryPoint = "Actor_SetActorTickEnabled")]
        private extern static void SetActorTickEnabled(IntPtr NativeComponentPointer, bool enabled);

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
        [DllImport("__MonoRuntime", EntryPoint = "Actor_GetTickGroup")]
        private extern static TickingGroup GetTickGroup(IntPtr NativeComponentPointer);
        [DllImport("__MonoRuntime", EntryPoint = "Actor_SetTickGroup")]
        private extern static void SetTickGroup(IntPtr NativeComponentPointer, TickingGroup tickGroup);

		// cleaner overloads of GetOverlappingActors. should really make the generator generate these

		public IList<Actor> GetOverlappingActors()
		{
			return GetOverlappingActorsInternal<Actor>(IntPtr.Zero);
		}

		public IList<T> GetOverlappingActors<T>() where T : Actor
		{
			var nativeFilter = UnrealInterop.GetNativeClassFromType(typeof(T));
			return GetOverlappingActorsInternal<T>(nativeFilter);
		}

		IList<T> GetOverlappingActorsInternal<T>(IntPtr nativeFilter) where T : Actor
		{
			unsafe
			{
				byte* ParamsBufferAllocation = stackalloc byte[GetOverlappingActors_ParamsSize];
				IntPtr ParamsBuffer = new IntPtr(ParamsBufferAllocation);
				*((IntPtr*)(IntPtr.Add(ParamsBuffer, GetOverlappingActors_ClassFilter_Offset))) = nativeFilter;

				InvokeFunction(NativeObject, GetOverlappingActors_NativeFunction, ParamsBuffer, GetOverlappingActors_ParamsSize);

				IntPtr OverlappingActors_NativeBuffer = IntPtr.Add(ParamsBuffer, GetOverlappingActors_OverlappingActors_Offset);
				var OverlappingActors_Marshaler = new UnrealArrayCopyMarshaler<T>(1, UnrealObjectMarshaler<T>.ToNative, UnrealObjectMarshaler<T>.FromNative, GetOverlappingActors_OverlappingActors_ElementSize);
				var overlappingActors = OverlappingActors_Marshaler.FromNative(OverlappingActors_NativeBuffer, 0, null);
				UnrealArrayCopyMarshaler<T>.DestructInstance(OverlappingActors_NativeBuffer, 0);
				return overlappingActors;
			}
		}

        /// <summary>
        /// If true, this actor is no longer replicated to new clients, and is "torn off" (becomes a ROLE_Authority) on clients to which it was being replicated.
        /// @see TornOff()
        /// </summary>
        public bool TearOff
        {
            get
            {
                CheckDestroyedByUnrealGC();
                return UnrealInterop.GetBitfieldValueFromProperty(NativeObject, bTearOff_NativeProperty, bTearOff_Offset);
            }
            set
            {
                CheckDestroyedByUnrealGC();
                if (value)
                {
                    Actor_TearOff(NativeObject);
                }
                else
                {
                    UnrealInterop.SetBitfieldValueForProperty(NativeObject, bTearOff_NativeProperty, bTearOff_Offset, value);
                }
            }
        }

        [DllImport("__MonoRuntime", EntryPoint = "Actor_TearOff")]
        private extern static void Actor_TearOff(IntPtr nativeThis);

        /// <summary>
        /// Gets or set the Actor's root SceneComponent.  It should be owned by this Actor.
        /// </summary>
        public SceneComponent RootComponent
        {
            get
            {
                CheckDestroyedByUnrealGC();
                return GetRootComponent (this.NativeObject);
            }
            set
            {
                CheckDestroyedByUnrealGC();
#if !CONFIG_SHIPPING
                if (value == null)
                {
                    throw new ArgumentNullException();
                }
#endif
                bool success = SetRootComponent(NativeObject, value.NativeObject);
#if !CONFIG_SHIPPING
                if (!success)
                {
                    throw new ArgumentException("The RootComponent must be owned by this Actor");
                }
#endif
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static SceneComponent GetRootComponent(IntPtr nativeThis);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static bool SetRootComponent(IntPtr nativeThis, IntPtr newRootComponent);

        //TODO: generate these overloads automatically
        //it's a bit odd that the teleport overloads omit parameters from the middle of the list
        //but that's an unfortunate consequence of the out parameter not being at the end

        public void SetActorRelativeTransform(UnrealEngine.Core.Transform newRelativeTransform)
        {
            HitResult sweepResult;
            SetActorRelativeTransform(newRelativeTransform, false, out sweepResult, false);
        }

        public void SetActorRelativeRotation(UnrealEngine.Runtime.Rotator newRelativeRotation)
        {
            HitResult sweepResult;
            SetActorRelativeRotation(newRelativeRotation, false, out sweepResult, false);
        }

        public void SetActorRelativeLocation(OpenTK.Vector3 newRelativeLocation)
        {
            HitResult sweepResult;
            SetActorRelativeLocation(newRelativeLocation, false, out sweepResult, false);
        }

        public void AddActorLocalTransform(UnrealEngine.Core.Transform newTransform)
        {
            HitResult sweepResult;
            AddActorLocalTransform(newTransform, false, out sweepResult, false);
        }

        public void AddActorLocalRotation(UnrealEngine.Runtime.Rotator deltaRotation)
        {
            HitResult sweepResult;
            AddActorLocalRotation(deltaRotation, false, out sweepResult, false);
        }

        public void AddActorLocalOffset(OpenTK.Vector3 deltaLocation)
        {
            HitResult sweepResult;
            AddActorLocalOffset(deltaLocation, false, out sweepResult, false);
        }

        public bool SetActorTransform(UnrealEngine.Core.Transform newTransform)
        {
            HitResult sweepResult;
            return SetActorTransform(newTransform, false, out sweepResult, false);
        }

        public void AddActorWorldTransform(UnrealEngine.Core.Transform deltaTransform)
        {
            HitResult sweepResult;
            AddActorWorldTransform(deltaTransform, false, out sweepResult, false);
        }

        public void AddActorWorldRotation(UnrealEngine.Runtime.Rotator deltaRotation)
        {
            HitResult sweepResult;
            AddActorWorldRotation(deltaRotation, false, out sweepResult, false);
        }

        public void AddActorWorldOffset(OpenTK.Vector3 deltaLocation)
        {
            HitResult sweepResult;
            AddActorWorldOffset(deltaLocation, false, out sweepResult, false);
        }

        public bool SetActorLocationAndRotation(OpenTK.Vector3 newLocation, UnrealEngine.Runtime.Rotator newRotation)
        {
            HitResult sweepResult;
            return SetActorLocationAndRotation(newLocation, newRotation, false, out sweepResult, false);
        }

        public bool SetActorLocation(OpenTK.Vector3 newLocation)
        {
            HitResult sweepResult;
            return SetActorLocation(newLocation, false, out sweepResult, false);
        }

        //teleport overloads

        public void SetActorRelativeTransform(UnrealEngine.Core.Transform newRelativeTransform, bool teleport)
        {
            HitResult sweepResult;
            SetActorRelativeTransform(newRelativeTransform, false, out sweepResult, teleport);
        }

        public void SetActorRelativeRotation(UnrealEngine.Runtime.Rotator newRelativeRotation, bool teleport)
        {
            HitResult sweepResult;
            SetActorRelativeRotation(newRelativeRotation, false, out sweepResult, teleport);
        }

        public void SetActorRelativeLocation(OpenTK.Vector3 newRelativeLocation, bool teleport)
        {
            HitResult sweepResult;
            SetActorRelativeLocation(newRelativeLocation, false, out sweepResult, teleport);
        }

        public void AddActorLocalTransform(UnrealEngine.Core.Transform newTransform, bool teleport)
        {
            HitResult sweepResult;
            AddActorLocalTransform(newTransform, false, out sweepResult, teleport);
        }

        public void AddActorLocalRotation(UnrealEngine.Runtime.Rotator deltaRotation, bool teleport)
        {
            HitResult sweepResult;
            AddActorLocalRotation(deltaRotation, false, out sweepResult, teleport);
        }

        public void AddActorLocalOffset(OpenTK.Vector3 deltaLocation, bool teleport)
        {
            HitResult sweepResult;
            AddActorLocalOffset(deltaLocation, false, out sweepResult, teleport);
        }

        public bool SetActorTransform(UnrealEngine.Core.Transform newTransform, bool teleport)
        {
            HitResult sweepResult;
            return SetActorTransform(newTransform, false, out sweepResult, teleport);
        }

        public void AddActorWorldTransform(UnrealEngine.Core.Transform deltaTransform, bool teleport)
        {
            HitResult sweepResult;
            AddActorWorldTransform(deltaTransform, false, out sweepResult, teleport);
        }

        public void AddActorWorldRotation(UnrealEngine.Runtime.Rotator deltaRotation, bool teleport)
        {
            HitResult sweepResult;
            AddActorWorldRotation(deltaRotation, false, out sweepResult, teleport);
        }

        public void AddActorWorldOffset(OpenTK.Vector3 deltaLocation, bool teleport)
        {
            HitResult sweepResult;
            AddActorWorldOffset(deltaLocation, false, out sweepResult, teleport);
        }

        public bool SetActorLocationAndRotation(OpenTK.Vector3 newLocation, UnrealEngine.Runtime.Rotator newRotation, bool teleport)
        {
            HitResult sweepResult;
            return SetActorLocationAndRotation(newLocation, newRotation, false, out sweepResult, teleport);
        }

        public bool SetActorLocation(OpenTK.Vector3 newLocation, bool teleport)
        {
            HitResult sweepResult;
            return SetActorLocation(newLocation, false, out sweepResult, false);
        }
    }
}
