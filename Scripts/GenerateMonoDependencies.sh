#!/bin/sh

DIR=`dirname $0`
pushd "${DIR}/.."

7z u -tzip -r MonoUEDependencies.zip Templates\**\Media\*
7z u -tzip -r MonoUEDependencies.zip Templates\**\Content\*
7z u -tzip -r MonoUEDependencies.zip ThirdParty\**\* -x!FrameworkList.xml
7z u -tzip -r MonoUEDependencies.zip Binaries\DotNET\MonoUE.MSBuildResolver.vsix

popd
