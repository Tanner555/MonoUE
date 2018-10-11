#!/bin/sh
#set -x

DIR=`dirname $0`

MONOBUILD=${DIR}/../../../../../mono
MONOINSTALL=${DIR}/../../../../../install

EMONO=${DIR}/../ThirdParty/mono

echo "Deleting existing binaries"
rm -rf ${EMONO}/lib/Mac ${EMONO}/include ${EMONO}/fx/MonoUE/v1.0/{*.dll,*.pdb,Facades}

echo "Copying build libs"
mkdir -p ${EMONO}/lib/Mac
cp -R ${MONOINSTALL}/lib/libmonosgen-2.0.1.dylib ${EMONO}/lib/Mac/libmonosgen-2.0.dylib
install_name_tool -id "@rpath/libmonosgen-2.0.dylib"  ${EMONO}/lib/Mac/libmonosgen-2.0.dylib

echo "Copying includes"
mkdir -p ${EMONO}/include/mono-2.0
cp -R ${MONOINSTALL}/include/mono-2.0/* ${EMONO}/include/mono-2.0

echo "Copying framework"
cp -R ${MONOBUILD}/mcs/class/lib/unreal/{*.dll,*.pdb,Facades} ${EMONO}/fx/MonoUE/v1.0/
