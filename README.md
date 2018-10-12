This is a rough WIP test to get MonoUE working as a standalone plugin. See https://mono-ue.github.io/

_Current status: **not working**_

How to compile:
- Add the plugin to a C++ game project as a game plugin
- Compile the C++ game project (don't ever run this game project)
- Compile MonoUE/Managed/MonoBindings/UnrealEngine.Runtime.sln and MonoUE/Managed/UnrealEngine.MainDomain.sln and then copy the output dlls over to MonoUE/ThirdParty/mono/fx/MonoUE/v1.0
- Now copy the entire MonoUE folder over to the engine plugins folder /Engine/Plugins/MonoUE
- Open an unreal project, MonoUE should be enabled. Check the console logs for 'mono'.

- [X] Get the project to a compilable state
- [ ] Add code to manually invoke the C# code generator to produce the UnrealEngine.BuiltInModules.dll
