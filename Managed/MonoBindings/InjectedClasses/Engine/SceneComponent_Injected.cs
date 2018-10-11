using OpenTK;
using System;
using System.Runtime.InteropServices;
using UnrealEngine.Runtime;


namespace UnrealEngine.Engine
{
    public partial class SceneComponent
    {
        public Vector3 RelativeLocation
        {
            get
            {
                CheckDestroyedByUnrealGC();
                return BlittableTypeMarshaler<Vector3>.FromNative(IntPtr.Add(NativeObject, RelativeLocation_Offset), 0, this);
            }
            set
            {
                HitResult sweepResult;
                SetRelativeLocation(value, false, out sweepResult, false);
            }
        }

        public Rotator RelativeRotation
        {
            get
            {
                CheckDestroyedByUnrealGC();
                return BlittableTypeMarshaler<Rotator>.FromNative(IntPtr.Add(NativeObject, RelativeRotation_Offset), 0, this);
            }
            set
            {
                HitResult sweepResult;
                SetRelativeRotation(value, false, out sweepResult, false);
            }
        }

        public void SetupAttachment(SceneComponent parent, Name socketName=default(Name))
        {
#if !CONFIG_SHIPPING
            if (parent == null)
            {
                throw new ArgumentNullException(nameof(parent));
            }
#endif

            CheckDestroyedByUnrealGC();
            SceneComponent_SetupAttachment(NativeObject, parent.NativeObject, socketName);
        }

        [DllImport("__MonoRuntime", EntryPoint = "SceneComponent_SetupAttachment")]
        private extern static void SceneComponent_SetupAttachment(IntPtr Self, IntPtr Parent, Name SocketName);
    }

    public partial class SpringArmComponent
    {
        public static readonly Name SocketName = new Name("SpringEndpoint");
    }
}