pushd ..
"%ProgramFiles%\7-zip\7z.exe" u -tzip -r MonoUEDependencies.zip Templates\**\Media\*
"%ProgramFiles%\7-zip\7z.exe" u -tzip -r MonoUEDependencies.zip Templates\**\Content\*
"%ProgramFiles%\7-zip\7z.exe" u -tzip -r MonoUEDependencies.zip ThirdParty\**\* -x!FrameworkList.xml
"%ProgramFiles%\7-zip\7z.exe" u -tzip -r MonoUEDependencies.zip Binaries\DotNET\MonoUE.MSBuildResolver.vsix

popd

