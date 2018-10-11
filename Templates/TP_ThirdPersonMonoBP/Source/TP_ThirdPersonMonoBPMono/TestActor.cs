using System;
using UnrealEngine.Runtime;
using UnrealEngine.Engine;

public class TestActor : Actor
{
	[UProperty, BlueprintVisible (ReadOnly = true), EditorVisible (ReadOnly = true)]
	[Category ("TestActor")]
	[UMetaData ("ExposeFunctionCategories", "Mesh,Rendering,Physics,Components|StaticMesh")]
	public Subobject<StaticMeshComponent> MeshComponent { get; private set; }

	[UProperty, BlueprintVisible, EditorVisible]
	[Category ("Movement")]
	public float RotationRate { get; set; }

	[UProperty, BlueprintVisible, EditorVisible]
	[Category ("Movement")]
	public bool RotateClockwise { get; set; }

	[UProperty, BlueprintVisible, EditorVisible]
	[Category ("Movement")]
	public float Velocity { get; set; }

	protected TestActor (ObjectInitializer initializer)
		: base (initializer)
	{
		MeshComponent = initializer.CreateDefaultSubobject<StaticMeshComponent> (new Name ("StaticMeshComponent0"));
		SetRootComponent (MeshComponent);

		RotateClockwise = true;
		RotationRate = 20.0f;
	}

	protected TestActor (IntPtr nativeObject)
		: base (nativeObject)
	{
	}

	protected override void ReceiveTick (float DeltaSeconds)
	{
		float yawDelta = RotationRate * DeltaSeconds;
		if (!RotateClockwise) {
			yawDelta *= -1;
		}

		Rotator rotation = GetActorRotation ();
		rotation.Yaw += yawDelta;

		OpenTK.Vector3 forwardVec = GetActorForwardVector ();
		OpenTK.Vector3 location = GetActorLocation ();
		location += DeltaSeconds * Velocity * forwardVec;

		SetActorLocationAndRotation (location, rotation, true);
	}
}
