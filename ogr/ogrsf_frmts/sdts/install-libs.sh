#!/bin/sh

SRCLIB=$1
DSTLIB=$2

OBJ=`ar t $SRCLIB`

ar x $SRCLIB $OBJ
ar r $DSTLIB $OBJ
rm $OBJ

