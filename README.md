This is a rough test to get MonoUE working as a standalone plugin. See https://mono-ue.github.io/

_Current status: **Functional** (after painfully getting the files in the right places)_

- [X] Get MonoUE to compile
- [X] Add code to manually invoke the C# code generator to produce the UnrealEngine.BuiltInModules.dll
- [X] Test one of the mono template game projects
- [ ] Create a tool which compiles the C++/C# projects and moves files into their correct places for easy install

## How to compile:
- Add the plugin to a C++ game project as a game plugin
- Compile the C++ game project (don't ever run this game project)
- Compile MonoUE/Managed/MonoBindings/UnrealEngine.Runtime.sln and MonoUE/Managed/UnrealEngine.MainDomain.sln and then copy the output dlls over to MonoUE/ThirdParty/mono/fx/MonoUE/v1.0
- Now copy the entire MonoUE folder over to the engine plugins folder /Engine/Plugins/MonoUE
- Open an unreal project, MonoUE should be enabled. Check the console logs for 'mono'.

## How to generate the UE4 bindings:

- In the UE4 console type "MonoGen", this will generate a bunch of .cs/.csproj files and a .sln file under "MonoUE/Intermediate/Build/Win64/Mono/". Currently this is going to the wrong location so you need to delete everything other than the "UnrealEngine.BuiltinModules" folder, then fix up the MonoUE.EngineBinding.props path in the .csproj, then open the .csproj manually (with the generated .sln deleted) and then compile. You then likely need to copy the output dll into various places.

## Issues:

- If the game sln says "Project file is incomplete. Expected imports are missing." this means that the [Sdk resolver](https://github.com/pixeltris/MonoUE-Standalone/blob/master/MSBuild/ExplainingSdksAndResolvers.md) isn't functioning properly. Ensure it is installed by running the installer MonoUE.MSBuildResolver.vsix. If it is installed and it still fails then it likely means it can't find your UE4 install (but there currently isn't any logging to tell you this). So instead you could temporarily provide the full Sdk path in your csproj to skip the resolver step. So in your game .csproj edit <Project Sdk="Mono.UE4.Sdk" to the full path of the Sdk folder (e.g. "C:/Program Files/Epic Games/UE_4.20/Engine/Plugins/MonoUE/MSBuild/Sdks/Mono.UE4.Sdk")
- If the game sln complains about "project.assets.json" try deleting the folder "C:/Program Files/dotnet/sdk/2.1.202/". I am unsure of the reasoning of this but it seems to fix the error (likely something in that sdk folder is overriding the custom MonoUE build setup). For more non-helpful information on this see https://github.com/dotnet/sdk/issues/1321#issuecomment-374706295
