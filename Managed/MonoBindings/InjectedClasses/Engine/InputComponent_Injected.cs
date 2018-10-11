// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.CompilerServices;
using UnrealEngine.Runtime;
using UnrealEngine.InputCore;
using UnrealEngine.Slate;
using OpenTK;

namespace UnrealEngine.Engine
{
    public partial class InputComponent
    {
        public delegate void ActionInputCallback();
        // Bind a delegate to a named input action.
        // Action names are defined in the Editor, in the Input section of Edit->Project Settings...
        public void BindAction(string actionName, InputEventType inputEventType, ActionInputCallback callback)
        {
            if (IsDestroyedOrPendingKill)
            {
                throw new UnrealObjectDestroyedException("Trying to bind input action on destroyed input component");
            }
            if(null == callback)
            {
                throw new ArgumentNullException("callback");
            }
            UnrealObject targetObj = callback.Target as UnrealObject;
            RegisterActionInputCallback(NativeObject, targetObj != null ? targetObj.NativeObject : IntPtr.Zero, actionName, inputEventType, callback);
        }

        public delegate void KeyInputCallback();
        // Bind a delegate directly to a specific key, gamepad button, axis, or touch input.
        public void BindKey(InputChord inputChord, InputEventType inputEventType, KeyInputCallback callback)
        {
            if (IsDestroyedOrPendingKill)
            {
                throw new UnrealObjectDestroyedException("Trying to bind input key on destroyed input component");
            }
            if (null == callback)
            {
                throw new ArgumentNullException("callback");
            }
            UnrealObject targetObj = callback.Target as UnrealObject;
            unsafe
            {
                byte* nativeInputChordBuffer = stackalloc byte[InputChord.NativeDataSize];
                inputChord.ToNative(new IntPtr(nativeInputChordBuffer));
                RegisterKeyInputCallback(NativeObject, targetObj != null ? targetObj.NativeObject : IntPtr.Zero , nativeInputChordBuffer, inputEventType, callback); 
            }
        }

        public delegate void TouchInputCallback(TouchIndex fingerIndex, Vector3 location);
        // Bind a delegate to a touch input.
        public void BindTouch(InputEventType inputEventType, TouchInputCallback callback)
        {
            if (IsDestroyedOrPendingKill)
            {
                throw new UnrealObjectDestroyedException("Trying to bind touch input on destroyed input component");
            }
            if (null == callback)
            {
                throw new ArgumentNullException("callback");
            }
            UnrealObject targetObj = callback.Target as UnrealObject;
            RegisterTouchInputCallback(NativeObject, targetObj.NativeObject, inputEventType, callback);
        }

        public delegate void AxisInputCallback(float AxisValue);
        // Bind a delegate to a named input axis.
        // Axis names are defined in the Editor, in the Input section of Edit->Project Settings...
        public void BindAxis(string axisName, AxisInputCallback callback)
        {
            if (IsDestroyedOrPendingKill)
            {
                throw new UnrealObjectDestroyedException("Trying to bind input axis on destroyed input component");
            }
            if (null == callback)
            {
                throw new ArgumentNullException("callback");
            }
            UnrealObject targetObj = callback.Target as UnrealObject;
            RegisterAxisInputCallback(NativeObject, targetObj != null ? targetObj.NativeObject : IntPtr.Zero, axisName, callback); 
        }

        // Bind a delegate directly to a specific float axis key, i.e. Keys.MouseX or Keys.Gamepad_LeftStick_Up
        public void BindAxisKey(Key axisKey, AxisInputCallback callback)
        {
            if (IsDestroyedOrPendingKill)
            {
                throw new UnrealObjectDestroyedException("Trying to bind input axis key on destroyed input component");
            }
            if (null == callback)
            {
                throw new ArgumentNullException("callback");
            }
            UnrealObject targetObj = callback.Target as UnrealObject;
            unsafe
            {
                byte* nativeKeyBuffer = stackalloc byte[InputChord.NativeDataSize];
                axisKey.ToNative(new IntPtr(nativeKeyBuffer));
                RegisterAxisKeyInputCallback(NativeObject, targetObj != null ? targetObj.NativeObject : IntPtr.Zero, nativeKeyBuffer, callback); 
            }
        }

        public delegate void VectorAxisInputCallback(OpenTK.Vector3 AxisValue);
        // Bind a delegate directly to a specific vector axis key, i.e. Keys.Tilt
        public void BindVectorAxis(Key vectorAxisKey, VectorAxisInputCallback callback)
        {
            if (IsDestroyedOrPendingKill)
            {
                throw new UnrealObjectDestroyedException("Trying to bind input axis vector on destroyed input component");
            }
            if (null == callback)
            {
                throw new ArgumentNullException("callback");
            }
            UnrealObject targetObj = callback.Target as UnrealObject;
            unsafe
            {
                byte* nativeKeyBuffer = stackalloc byte[InputChord.NativeDataSize];
                vectorAxisKey.ToNative(new IntPtr(nativeKeyBuffer));
                RegisterVectorAxisInputCallback(NativeObject, targetObj != null ? targetObj.NativeObject : IntPtr.Zero, nativeKeyBuffer, callback);
            }
        }

        public delegate void GestureInputCallback(float value);
        // Bind a delegate directly to a specific type of gesture input, i.e. Keys.Gesture_Pinch
        public void BindGesture(Key gestureKey, GestureInputCallback callback)
        {
            if (IsDestroyedOrPendingKill)
            {
                throw new UnrealObjectDestroyedException("Trying to bind input gesture on destroyed input component");
            }
            if (null == callback)
            {
                throw new ArgumentNullException("callback");
            }
            UnrealObject targetObj = callback.Target as UnrealObject;
            unsafe
            {
                byte* nativeKeyBuffer = stackalloc byte[InputChord.NativeDataSize];
                gestureKey.ToNative(new IntPtr(nativeKeyBuffer));
                RegisterGestureInputCallback(NativeObject, targetObj != null ? targetObj.NativeObject : IntPtr.Zero, nativeKeyBuffer, callback);
            }
        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private extern static void RegisterActionInputCallback(IntPtr nativeComponentPointer, IntPtr nativeTargetObjectPointer, string actionName, InputEventType inputEventType, ActionInputCallback callback);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        unsafe private extern static void RegisterKeyInputCallback(IntPtr nativeComponentPointer, IntPtr nativeTargetObjectPointer, void* nativeInputChord, InputEventType inputEventType, KeyInputCallback callback);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private extern static void RegisterTouchInputCallback(IntPtr nativeComponentPointer, IntPtr nativeTargetObjectPointer, InputEventType inputEventType, TouchInputCallback callback);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private extern static void RegisterAxisInputCallback(IntPtr nativeComponentPointer, IntPtr nativeTargetObjectPointer, string axisName, AxisInputCallback callback);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        unsafe private extern static void RegisterAxisKeyInputCallback(IntPtr nativeComponentPointer, IntPtr nativeTargetObjectPointer, void* nativeAxisKey, AxisInputCallback callback);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        unsafe private extern static void RegisterVectorAxisInputCallback(IntPtr nativeComponentPointer, IntPtr nativeTargetObjectPointer, void* nativeVectorAxisKey, VectorAxisInputCallback callback);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        unsafe private extern static void RegisterGestureInputCallback(IntPtr nativeComponentPointer, IntPtr nativeTargetObjectPointer, void* nativeGestureKey, GestureInputCallback callback);
    }
}
