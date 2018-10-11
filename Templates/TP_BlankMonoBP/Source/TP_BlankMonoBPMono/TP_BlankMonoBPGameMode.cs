using System;
using UnrealEngine.Runtime;
using UnrealEngine.Engine;

namespace TP_BlankMonoBPMono
{
	public class TP_BlankMonoBPGameMode : GameModeBase
	{
		// Constructor called when creating or loading an object
		protected TP_BlankMonoBPGameMode (ObjectInitializer initializer)
			: base (initializer)
		{
			PlayerControllerClass = typeof(TP_BlankMonoBPPlayerController);
		}

		// Constructor for hot-reloading. UProperties will already be initialized, but any purely managed fields or data
		// will need to be set up
		protected TP_BlankMonoBPGameMode (IntPtr nativeObject)
			: base (nativeObject)
		{
		}
	}
}

