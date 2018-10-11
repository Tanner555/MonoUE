using System;
using UnrealEngine.Runtime;
using UnrealEngine.Engine;
using TP_BlankMono;

namespace TP_BlankMonoMono
{
	public class TP_BlankMonoPlayerController : TP_BlankMonoPlayerControllerBase
	{
		// Constructor called when creating or loading an object
		protected TP_BlankMonoPlayerController (ObjectInitializer initializer)
			: base (initializer)
		{
		}

		// Constructor for hot-reloading. UProperties will already be initialized, but any purely managed fields or data
		// will need to be set up
		protected TP_BlankMonoPlayerController (IntPtr nativeObject)
			: base (nativeObject)
		{
		}
	}
}

