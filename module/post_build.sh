#!/bin/bash
PACKAGE_NAME=$1
PACKAGE_VERSION=$2
kernelver=$3
cp /var/lib/dkms/"$PACKAGE_NAME"/"$PACKAGE_VERSION"/build/{"$PACKAGE_NAME".a,Module.symvers} /var/lib/dkms/"$PACKAGE_NAME"/"$PACKAGE_VERSION"/"$kernelver"/"$(uname -m)"/module/
