# Mono for Unreal Engine

## Summary

The Mono plugin for Unreal Engine 4 allows writing gameplay code with the C# language.

Microsoft has developed this plugin and has released the source code on GitHub as a fork
of Epic Games' UE4 repository. This code is now available to all UE4 licensees under the terms
of the UE4 license, which provide for source code redistribution and use.

## License

**This code is provided by Microsoft "as is" with no warranty.**

For details, see [LICENSE.md](LICENSE.md) in the root of the plugin directory.

## Setup

MonoUE requires several engine patches, so you should build from git, from the branch in which you see this file.

If building on Windows, you will need **Visual Studio 2017**. On a Mac you will need **Visual Studio for Mac**.

On Windows, the following Visual Studio workloads are required:

* Game Development with C++
* .NET Core cross-platform development

After this, build UE as you would normally.

For example, on Windows, run these commands in the root UnrealEngine directory:

```
Setup.bat
GenerateProjectFiles.bat
Engine\Build\BatchFiles\Build.bat ShaderCompileWorker Win64 Development
Engine\Build\BatchFiles\Build.bat UE4Editor Win64 Development
```

Or on Mac, run these commands:

```
./Setup.command
./GenerateProjectFiles.command
./Engine/Build/BatchFiles/Mac/Build.sh ShaderCompileWorker Mac Development
./Engine/Build/BatchFiles/Mac/Build.sh UE4Editor Mac Development
```

## Directory Layout

### Plugin directory layout

* [Build](Build)
  * [MonoAutomation](Build/MonoAutomation) - UAT extensions for staging/packaging
* [Managed](Managed)
  * [MonoBindings](Managed/MonoBindings) - Base hand-rolled assembly for UE4 bindings - contains internal bindings functionality and some fundamental UE4 types which required hand-rolled bindings.
  * [MonoMainDomain](Managed/MonoMainDomain) - Editor agent that handles hot-reloading and IDE connection..
  * [MonoManagedExtensions](Managed/MonoManagedExtensions) - UE4 extension classes which are part of runtime, built with same process as game assemblies. Currently just contains unit tests.
* [MSBuild](MSBuild) - MSBuild targets and props files for game assembly processing (running MonoAssemblyProcess)
* [Source](Source)
  * [MonoEditor](Source/MonoEditor) - Plugin module for UE4 Editor
  * [MonoRuntime](Source/MonoRuntime) - Plugin module for UE4 Runtime
  * [MonoScriptGenerator](Source/MonoScriptGenerator) - Plugin module for UBT to generate C# bindings
  * [Programs](Source/Programs)
    * [MonoAssemblyProcess](Source/Programs/MonoAssemblyProcess) - Rewrites IL of games assemblies and extracts type metadata for registration.
    * [MonoUEBuildTool](Source/Programs/MonoUEBuildTool) - Generates/refreshes managed solutions for game projects,a nd builds them. Invoked from UE4 editor and UnrealBuildTool.
    * [MonoUE.Tasks](Source/Programs/MonoUE.Tasks) - Tasks assembly for the [MSBuild](MSBuild) targets.
* [Templates](Templates) - UE4 game project templates used by [MonoEditor](Source/MonoEditor)

### Generated directories

In UE4 engine directory

* Engine/Binaries/[PLATFORM]/Mono[TARGET][-[CONFIG]] - Contains compiled UE4 bindings assemblies after building UE4
* Engine/Intermediate/Build/[PLATFORM]/Mono - Contains generated source, projects, and solutions for UE4 bindings assemblies.
