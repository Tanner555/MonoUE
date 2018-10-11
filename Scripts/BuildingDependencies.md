# Building Dependencies

## What and Why

The dependencies zip contains Mono runtime and framework libraries and miscellaneous other other binary files. The MonoUE bootstrapper automatically downloads and unpacks the dependencies zip.

Ideally MonoUE could just use the Mono release binaries, but the need to build a custom framework complicates that. The MonoUE custom framework is basically a Windows/Mac version of the Mono "mobile" framework profile that's used on iOS and Android. Compared to the full "Desktop" .NET Framework, it's a little smaller, removes some irrelevant (e.g. ASP.NET) and problematic (System.Configuration) features, and is optimized for linking.

The zip is a mixture of several kinds of things:

* Binaries for project templates. This just comes from the previous dependencies zip.
* Framework assemblies. Can be built from source on a Mac or Linux. Difficult to build on Windows.
* Mac Mono runtime libraries. Can be built from source on a Mac.
* Windows Mono runtime libraries. Can be built from source on Windows.

Note that the runtime and framework versions MUST match or you will get weird runtime errors.

## Building Mono

To refresh the dependencies zip, you will need to build Mono on Mac and Windows.

First you should check out https://github.com/mono/mono in a `mono` directory adjacent to UnrealEngine. On Mac, you should also configure it to install into an `install` directory adjacent to this.

```
ParentDir
\- MonoUE
\- mono
```

You will need a Mac to build the framework assemblies; only the runtime assemblies can be built on Windows.

### Mac

You will need to configure the Mono to include the Unreal profile and install to an adjacent `install` directory, then build and install it.

```sh
./autogen.sh --prefix="`pwd`/../install" --with-unreal
make \
make install
```

Then run [CopyMonoRuntime.sh](CopyMonoRuntime.sh) to copy the runtime, headers, and framework libraries into various subdirectories of `MonoUE\ThirdParty\mono`.


### Windows

Open [mono\msvc\mono.sln](https://github.com/mono/mono/blob/master/msvc/mono.sln) in Visual Studio and build the `Debug|Win32` and `Debug|Win64` configurations.

Then run [CopyMonoRuntime.bat](CopyMonoRuntime.bat) to copy the runtime into various subdirectories of `MonoUE\ThirdParty\mono`.

## Refreshing the zip

To fully refresh the zip file, you will need to perform the following steps:

On Mac:

1. Unpack old dependencies
2. Build new Mac Mono runtime and (portable) framework assemblies
3. Run script to copy Mac runtime and framework to UE directory
4. Run script to create Mac zip

On Windows:
1. Unpack the new Mac zip
2. Copy this new Mac dependencies zip to Windows and unpack
3. Build new Windows Mono runtime from the same commit you used on Mac
4. Run script to copy Windows runtime to UE directory
5. Run script to create zip, which now contains Mac+Windows assemblies
6. Rename zip to match Mono commit hash

Now you have your combined zip. Rename it to match the Mono commit you used and upload to a CDN.

Finally update the URL and SHA1 of the zip [in the bootstrapper](../Source/Programs/MonoUEBootstrapper/Program.cs#L14)..