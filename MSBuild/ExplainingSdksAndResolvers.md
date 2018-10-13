MonoUE uses an MSBuild feature called MSBuild `Sdks`, which were introduced very recently in MSBuild 15.0..

## Sdks

SDKs are super neat, as they allow projects to be as simple as this:

```xml
<Project Sdk="Mono.UE4.Sdk" />
```

One reason is that the project is so simple is that SDK style projects use updated targets. These targets set default values for every property, and although the project can override them it does not need to. These targets also implicitly include all C# files in the directory and its child directories using a wildcard `<Compile Include="**\*.cs" />`, though behavior can also be disabled if desired. As a result, the project file does not need any items or properties by default.

The target/properties imports are also simplified using a new Sdk attribute. Instead of this:

```xml
<Project xmnlns="something i can't ever remember">
  <Import Project="$(MSBuildSdksPath)\Mono.UE4.Sdk\Sdk\Sdk.props" />
  <!-- the rest of your project goes here -->
  <Import Project="$(MSBuildSdksPath)\Mono.UE4.Sdk\Sdk\Sdk.targets" />
</Project>
```

You can do this:

```xml
<Project Sdk="Mono.UE4.Sdk">
</Project>
```

Yes, you don't need the `xmlns` either if you're using SDKs.

You might be wondering where the Sdk is imported from. Well, there's a directory called `Sdks` in the MSBuild `bin` directory, and if they're placed there they _just work_ (tm). However, installing them there is a pain as it needs admin access.

Enter resolvers.

## Resolvers

In MSBuild 15.3 a new mechanism called SDK resolvers was added. An SDK resolver is simply a dll that subclasses the SdkResolver class. Like SDKs, SDK resolvers are installed into a subdirectory of the MSBuild `bin` - this time called `SdkResolvers` (surprise!). ("Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\SdkResolvers")

When MSBuild is attempting to resolve an SDK, it queries all the resolvers to see whether they can find it. They can return a path from anywere on the filesystem. This means that although the resolver itself still needs admin access to be installed, the SDK does not.

The MonoUE resolver resolves the "Mono.UE4.Sdk" SDK IDE directory to the UnrealEngine engine directory. Even better, it inspects the project's uproject file and looks up the matching engine instance, so if you have multiple engines installed, your project will build against the matching SDK and assemblies.

## Source

* Resolver: [..\Source\Programs\MonoUE.MSBuildSdkResolver](..\Source\Programs\MonoUE.MSBuildSdkResolver)
* SDK: [Sdks\Mono.UE4.Sdk](Sdks\Mono.UE4.Sdk).


