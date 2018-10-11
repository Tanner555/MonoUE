The purpose of these files is as follows:

MonoUE.CSharp.targets

  Performs property fixup and imports the UE common
  targets and the MS C# targets.

  Should be imported into C# game project files INSTEAD
  of importing Microsoft.CSharp.targets.

MonoUE.Common.targets

  Common targets to be imported into game projects
  by the C# targets, or other languages' targets.

  Should not be imported directly.

MonoUE.Core.props

  Common logic for computing various values from the
  project configuration and platform. Also imports the
  MS Common props.

  Should not be imported directly.

MonoUE.EngineBinding.props

  Computes property values for engine binding projects.

  Imported automatically in the generated binding projects.

MonoUE.GameBinding.props

  Computes property values for app/game binding projects.

  Imported automatically in the generated binding projects.

MonoUE.EngineAssembly.props

  Computes property values for engine assemblies that reference bindings.

Sdks:

  SDK props and targets to be imported by game projects.
