using System;
using UnrealEngine.Runtime;
using UnrealEngine.Engine;

namespace TP_BlankMonoBPMono
{
	public class TP_BlankMonoBPPlayerController : PlayerController
	{
		// Constructor called when creating or loading an object
		protected TP_BlankMonoBPPlayerController (ObjectInitializer initializer)
			: base (initializer)
		{
		}

		// Constructor for hot-reloading. UProperties will already be initialized, but any purely managed fields or data
		// will need to be set up
		protected TP_BlankMonoBPPlayerController (IntPtr nativeObject)
			: base (nativeObject)
		{
		}
	}
}

