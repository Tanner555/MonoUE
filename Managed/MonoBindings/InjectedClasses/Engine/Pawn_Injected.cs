using System;
using System.Runtime.InteropServices;
using UnrealEngine.Runtime;

namespace UnrealEngine.Engine
{
    public partial class Pawn
    {
        public Rotator ViewRotation
        {
            get
            {
                CheckDestroyedByUnrealGC();

                Rotator result = new Rotator();
                GetViewRotationNative(NativeObject, ref result);
                return result;
            }
        }

        [DllImport("__MonoRuntime", EntryPoint="Pawn_GetViewRotation")]
        private extern static void GetViewRotationNative(IntPtr nativePawnPointer, ref Rotator outRotator);

        public void TurnOff()
        {
            CheckDestroyedByUnrealGC();

            TurnOff(NativeObject);
        }

        [DllImport("__MonoRuntime", EntryPoint = "Pawn_TurnOff")]
        private extern static void TurnOff(IntPtr NativePawnPointer);

        protected Controller GetDamageInstigator(Controller instigatedBy, DamageType damageType)
        {
            // This logic is ported from the native APawn method of the same name.
            if ((instigatedBy != null) && (instigatedBy != Controller))
            {
                return instigatedBy;
            }
            else if (damageType.CausedByWorld && (LastHitBy != null))
            {
                return LastHitBy;
            }
            return instigatedBy;
        }
    }
}
