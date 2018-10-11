#!/bin/sh

MONO_PREFIX=/Library/Frameworks/Mono.framework/Versions/Current

# UBT sets PATH and MONO_PATH to use Unreal's bundled Mono, which breaks calling the system Mono
unset MONO_PATH
export PATH="$MONO_PREFIX/bin:$PATH"

exec $MONO_PREFIX/bin/mono "$@"
