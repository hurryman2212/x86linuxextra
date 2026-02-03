#!/bin/bash
PACKAGE_NAME=$1
kernelver=$2
rm -f /usr/lib/modules/"$kernelver"/updates/dkms/"$PACKAGE_NAME".a
