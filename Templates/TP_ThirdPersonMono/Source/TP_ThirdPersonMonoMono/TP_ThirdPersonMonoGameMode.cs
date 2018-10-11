using System;
using UnrealEngine.Runtime;
using UnrealEngine.Engine;
using TP_ThirdPersonMono;

namespace TP_ThirdPersonMonoMono
{
	public class TP_ThirdPersonMonoGameMode : TP_ThirdPersonMonoGameModeBase
	{
		// Constructor called when creating or loading an object
		protected TP_ThirdPersonMonoGameMode(ObjectInitializer initializer)
			: base (initializer)
		{
			if (initializer.TryFindClass<Pawn>("Class'/Game/Blueprints/MyCharacter.MyCharacter_C'", out var pawnClass))
			{
				DefaultPawnClass = pawnClass;
			}
		}

		// Constructor for hot-reloading. UProperties will already be initialized, but any purely managed fields or data
		// will need to be set up
		protected TP_ThirdPersonMonoGameMode(IntPtr nativeObject)
			: base(nativeObject)
		{
		}

	}
}

