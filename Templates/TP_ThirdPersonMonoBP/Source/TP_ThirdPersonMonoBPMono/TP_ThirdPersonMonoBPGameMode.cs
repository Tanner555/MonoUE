using System;
using UnrealEngine.Runtime;
using UnrealEngine.Engine;

namespace TP_ThirdPersonMonoBPMono
{
	public class TP_ThirdPersonMonoBPGameMode : GameMode
	{
		// Constructor called when creating or loading an object
		protected TP_ThirdPersonMonoBPGameMode(ObjectInitializer initializer)
			: base(initializer)
		{
			if (initializer.TryFindClass<Pawn>("Class'/Game/Blueprints/MyCharacter.MyCharacter_C'", out var pawnClass))
			{
				DefaultPawnClass = pawnClass;
			}
		}

		// Constructor for hot-reloading. UProperties will already be initialized, but any purely managed fields or data
		// will need to be set up
		protected TP_ThirdPersonMonoBPGameMode(IntPtr nativeObject)
			: base(nativeObject)
		{
		}

	}
}

