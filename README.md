This is a rough test to get MonoUE working as a standalone plugin. See https://mono-ue.github.io/

_Current status: **Works** (after painfully getting the files in the right places)_

- [X] Get MonoUE to compile
- [X] Add code to manually invoke the C# code generator to produce the UnrealEngine.BuiltInModules.dll
- [X] Test one of the mono template game projects
- [ ] Create a tool which compiles the C++/C# projects and moves files into their correct places for easy install

---

How to compile:
- Add the plugin to a C++ game project as a game plugin
- Compile the C++ game project (don't ever run this game project)
- Compile MonoUE/Managed/MonoBindings/UnrealEngine.Runtime.sln and MonoUE/Managed/UnrealEngine.MainDomain.sln and then copy the output dlls over to MonoUE/ThirdParty/mono/fx/MonoUE/v1.0
- Now copy the entire MonoUE folder over to the engine plugins folder /Engine/Plugins/MonoUE
- Open an unreal project, MonoUE should be enabled. Check the console logs for 'mono'.

---

How to generate the UE4 bindings:

- In the UE4 console type "MonoGen", this will generate a bunch of .cs/.csproj files and a .sln file under "MonoUE/Intermediate/Build/Win64/Mono/". Currently this is going to the wrong location so you need to delete everything other than the "UnrealEngine.BuiltinModules" folder, then fix up the MonoUE.EngineBinding.props path in the .csproj, then open the .csproj manually (with the generated .sln deleted) and then compile. You then likely need to copy the output dll into various places.
