// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace UnrealEngine.Runtime
{
    public struct Key
    {
        private Name KeyName;
        public Name Name
        {
            get { return Name; }
        }

        public Key(Keys key)
        {
            if (key == Keys.Invalid)
            {
                KeyName = Name.None;
            }
            else
            {
                KeyName = new Name(key.ToString());
            }
        }

        // Allow Keys.whatever to be used directly wherever a Key struct is needed.
        static public implicit operator Key(Keys key)
        {
            return new Key(key);
        }

        public static readonly int NativeDataSize;
        static Key()
        {
            IntPtr nativeStructPtr = UnrealInterop.GetNativeStructFromName("Key");
            NativeDataSize = UnrealInterop.GetNativeStructSize(nativeStructPtr);
        }

        // Marshalling constructor
        public Key(IntPtr InNativeStruct)
        {
            // Native FKey struct layout:
            //      FName KeyName;
            //      TSharedPtr<struct FKeyDetails> KeyDetails;
            //
            // We'll just disregard the shared pointer - it wouldn't be safe to propagate the refcount back to native code.
            // If we ever need to access to KeyDetails in managed code, we should do so via a custom invoke.

            unsafe { KeyName = *(Name*)InNativeStruct.ToPointer(); }
        }

        public void ToNative(IntPtr Buffer)
        {
            unsafe
            {
                // Native FKey struct layout:
                //      FName KeyName;
                //      TSharedPtr<struct FKeyDetails> KeyDetails;
                *(Name*)Buffer.ToPointer() = KeyName;

                // The shared pointer must be properly initialized, or hilarity will ensue due to the wild pointers.
                int sharedPtrOffset = sizeof(Name);
                *(UnrealInterop.SharedPtrMirror*)(IntPtr.Add(Buffer, sharedPtrOffset).ToPointer()) = default(UnrealInterop.SharedPtrMirror);
            }
        }
    }

    public static class KeyMarshaler
    {
        public static Key FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
        {
            return new Key(nativeBuffer + arrayIndex * Key.NativeDataSize);
        }

        public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, Key obj)
        {
            obj.ToNative(nativeBuffer + arrayIndex * Key.NativeDataSize);
        }
    }

    // Corresponds to the const FKeys declared in Unreal's EKeys struct.
    // Relies on all values being initialized with a string exactly matching the key's name in code.
    // Ideally, we'd omit the TouchN values and just offer a Key(ETouchIndex) constructor, but that's not 
    // visible from the bindings assembly.
    public enum Keys
    {
	    MouseX,
	    MouseY,
	    MouseScrollUp,
	    MouseScrollDown,

	    LeftMouseButton,
	    RightMouseButton,
	    MiddleMouseButton,
	    ThumbMouseButton,
	    ThumbMouseButton2,

	    BackSpace,
	    Tab,
	    Enter,
	    Pause,

	    CapsLock,
	    Escape,
	    SpaceBar,
	    PageUp,
	    PageDown,
	    End,
	    Home,

	    Left,
	    Up,
	    Right,
	    Down,

	    Insert,
	    Delete,

	    Zero,
	    One,
	    Two,
	    Three,
	    Four,
	    Five,
	    Six,
	    Seven,
	    Eight,
	    Nine,

	    A,
	    B,
	    C,
	    D,
	    E,
	    F,
	    G,
	    H,
	    I,
	    J,
	    K,
	    L,
	    M,
	    N,
	    O,
	    P,
	    Q,
	    R,
	    S,
	    T,
	    U,
	    V,
	    W,
	    X,
	    Y,
	    Z,

	    NumPadZero,
	    NumPadOne,
	    NumPadTwo,
	    NumPadThree,
	    NumPadFour,
	    NumPadFive,
	    NumPadSix,
	    NumPadSeven,
	    NumPadEight,
	    NumPadNine,

	    Multiply,
	    Add,
	    Subtract,
	    Decimal,
	    Divide,

	    F1,
	    F2,
	    F3,
	    F4,
	    F5,
	    F6,
	    F7,
	    F8,
	    F9,
	    F10,
	    F11,
	    F12,

	    NumLock,

	    ScrollLock,

	    LeftShift,
	    RightShift,
	    LeftControl,
	    RightControl,
	    LeftAlt,
	    RightAlt,
	    LeftCommand,
	    RightCommand,

	    Semicolon,
	    Equals,
	    Comma,
	    Underscore,
	    Period,
	    Slash,
	    Tilde,
	    LeftBracket,
	    Backslash,
	    RightBracket,
	    Quote,

	    // Platform Keys
	    // These keys platform specific versions of keys that go by different names.
	    // The delete key is a good example, on Windows Delete is the virtual key for Delete.
	    // On Macs, the Delete key is the virtual key for BackSpace.
        Platform_Delete,

	    // Gameplay Keys
	    Gamepad_LeftX,
	    Gamepad_LeftY,
	    Gamepad_RightX,
	    Gamepad_RightY,
	    Gamepad_LeftTriggerAxis,
	    Gamepad_RightTriggerAxis,

	    Gamepad_LeftThumbstick,
	    Gamepad_RightThumbstick,
	    Gamepad_Special_Left,
	    Gamepad_Special_Right,
	    Gamepad_FaceButton_Bottom,
	    Gamepad_FaceButton_Right,
	    Gamepad_FaceButton_Left,
	    Gamepad_FaceButton_Top,
	    Gamepad_LeftShoulder,
	    Gamepad_RightShoulder,
	    Gamepad_LeftTrigger,
	    Gamepad_RightTrigger,
	    Gamepad_DPad_Up,
	    Gamepad_DPad_Down,
	    Gamepad_DPad_Right,
	    Gamepad_DPad_Left,

	    // Virtual key codes used for input axis button press/release emulation
	    Gamepad_LeftStick_Up,
	    Gamepad_LeftStick_Down,
	    Gamepad_LeftStick_Right,
	    Gamepad_LeftStick_Left,

	    Gamepad_RightStick_Up,
	    Gamepad_RightStick_Down,
	    Gamepad_RightStick_Right,
	    Gamepad_RightStick_Left,

	    // static const FKey Vector axes (FVector; not float)
	    Tilt,
	    RotationRate,
	    Gravity,
	    Acceleration,

	    // Gestures
	    Gesture_SwipeLeftRight,
	    Gesture_SwipeUpDown,
	    Gesture_TwoFingerSwipeLeftRight,
	    Gesture_TwoFingerSwipeUpDown,
	    Gesture_Pinch,
	    Gesture_Flick,

	    // PS4-specific
	    PS4_Special,

	    // Steam Controller Specific;
	    Steam_Touch_0,
	    Steam_Touch_1,
	    Steam_Touch_2,
	    Steam_Touch_3,
	    Steam_Back_Left,
	    Steam_Back_Right,

	    // Xbox One global speech commands
	    Global_Menu,
	    Global_View,
	    Global_Pause,
	    Global_Play,
	    Global_Back,

	    Invalid,
        
        // Fingers
        Touch1,
        Touch2,
        Touch3,
        Touch4,
        Touch5,
        Touch6,
        Touch7,
        Touch8,
        Touch9,
        Touch10,
    }
}
