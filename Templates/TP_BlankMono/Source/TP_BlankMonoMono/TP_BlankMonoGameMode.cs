using System;
using UnrealEngine.Runtime;
using UnrealEngine.Engine;
using TP_BlankMono;

namespace TP_BlankMonoMono
{
	public class TP_BlankMonoGameMode : TP_BlankMonoGameModeBase
	{
		// Constructor called when creating or loading an object
		protected TP_BlankMonoGameMode (ObjectInitializer initializer)
			: base (initializer)
		{
			PlayerControllerClass = typeof(TP_BlankMonoPlayerController);
		}

		// Constructor for hot-reloading. UProperties will already be initialized, but any purely managed fields or data
		// will need to be set up
		protected TP_BlankMonoGameMode (IntPtr nativeObject)
			: base (nativeObject)
		{
		}
	}
}

