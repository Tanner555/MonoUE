using OpenTK;
using System;
using UnrealEngine.Runtime;
using UnrealEngine.Engine;
using TP_ThirdPersonMono;
using UnrealEngine.HeadMountedDisplay;

namespace TP_ThirdPersonMonoMono
{
	class TP_ThirdPersonMonoCharacter : TP_ThirdPersonMonoCharacterBase
	{
		[UProperty, BlueprintVisible, EditorVisible]
		[Category ("Input")]
		public float BaseTurnRate { get; set; }

		[UProperty, BlueprintVisible, EditorVisible]
		[Category ("Input")]
		public float BaseLookUpRate { get; set; }

		[UProperty, BlueprintVisible (ReadOnly = true), EditorVisible (ReadOnly = true)]
		[Category ("Camera")]
		public SpringArmComponent CameraBoom { get; set; }

		[UProperty, BlueprintVisible (ReadOnly = true), EditorVisible (ReadOnly = true)]
		[Category ("Camera")]
		public CameraComponent FollowCamera { get; set; }

		// Constructor called when creating or loading an object
		protected TP_ThirdPersonMonoCharacter (ObjectInitializer initializer)
			: base (initializer)
		{
			// Set size for collision capsule
			CapsuleComponent.SetCapsuleSize (42.0f, 96.0f);

			// set our turn rates for input
			BaseTurnRate = 45.0f;
			BaseLookUpRate = 45.0f;

			// Rotate the camera with the controller, not the character.
			UseControllerRotationPitch = false;
			UseControllerRotationYaw = false;
			UseControllerRotationRoll = false;

			// Configure character movement
			CharacterMovement.OrientRotationToMovement = true; // Character moves in the direction of input...
			CharacterMovement.RotationRate = new Rotator (0.0f, 540.0f, 0.0f); // ...at this rotation rate
			CharacterMovement.JumpZVelocity = 600.0f;
			CharacterMovement.AirControl = 0.2f;

			// Create a camera boom (pulls in towards the player if there is a collision)
			CameraBoom = initializer.CreateDefaultSubobject<SpringArmComponent> ("CameraBoom");
			CameraBoom.SetupAttachment(RootComponent);
			CameraBoom.TargetArmLength = 300.0f; // The camera follows at this distance behind the character
			CameraBoom.UsePawnControlRotation = true; // Rotate the arm based on the controller

			// Create a follow camera
			FollowCamera = initializer.CreateDefaultSubobject<CameraComponent> ("FollowCamera");
			FollowCamera.SetupAttachment(CameraBoom, SpringArmComponent.SocketName);  // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
			FollowCamera.UsePawnControlRotation = false; // Camera does not rotate relative to arm

			// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
			// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C#)
		}

		// Constructor for hot-reloading. UProperties will already be initialized, but any purely managed fields or data
		// will need to be set up
		protected TP_ThirdPersonMonoCharacter (IntPtr nativeObject)
			: base (nativeObject)
		{
		}

		protected override void BindInput(InputComponent playerInputComponent)
		{
			base.BindInput(playerInputComponent);

			// Bind the jump action to Character.Jump(), which already has the appropriate delegate signature.
			playerInputComponent.BindAction(InputAction.Jump, InputEventType.Pressed, Jump);
			playerInputComponent.BindAction(InputAction.Jump, InputEventType.Released, StopJumping);

			playerInputComponent.BindAxis(InputAxis.MoveForward, MoveForward);
			playerInputComponent.BindAxis(InputAxis.MoveRight, MoveRight);

			// We have 2 versions of the rotation bindings to handle different kinds of devices differently
			// "turn" handles devices that provide an absolute delta, such as a mouse.
			// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
			playerInputComponent.BindAxis(InputAxis.Turn, AddControllerYawInput);
			playerInputComponent.BindAxis(InputAxis.TurnRate, (float axisValue) => AddControllerYawInput(axisValue * BaseTurnRate * World.GetWorldDeltaSeconds()));
			playerInputComponent.BindAxis(InputAxis.LookUp, AddControllerPitchInput);
			playerInputComponent.BindAxis(InputAxis.LookUpRate, (float axisValue) => AddControllerPitchInput(axisValue * BaseLookUpRate * World.GetWorldDeltaSeconds()));

			// handle touch devices
			playerInputComponent.BindTouch(InputEventType.Pressed, (fingerIndex, location) => Jump());
			playerInputComponent.BindTouch(InputEventType.Released, (fingerIndex, location) => StopJumping());

			// VR headset functionality
			playerInputComponent.BindAction(InputAction.ResetVR, InputEventType.Pressed, OnResetVR);
		}

		private void MoveForward (float axisValue)
		{
			if (Controller != null && axisValue != 0.0f)
			{
				// find out which way is forward
				Rotator rotation = GetControlRotation();
				Rotator yawRotation = new Rotator(0f, rotation.Yaw, 0f);
				Vector3 direction = rotation.GetForwardVector();

				// add movement in that direction
				AddMovementInput(direction, axisValue);
			}
		}

		private void MoveRight (float axisValue)
		{
			if (Controller != null && axisValue != 0.0f)
			{
				// find out which way is right
				Rotator rotation = GetControlRotation();
				Rotator yawRotation = new Rotator(0f, rotation.Yaw, 0f);
				Vector3 direction = rotation.GetRightVector();

				// add movement in that direction
				AddMovementInput(direction, axisValue);
			}
		}

		private void OnResetVR()
		{
			HeadMountedDisplayFunctionLibrary.ResetOrientationAndPosition();
		}
	}
}
