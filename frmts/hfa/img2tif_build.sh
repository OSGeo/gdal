#!/bin/sh

CC=gcc
CPPC=gcc
CFLAGS="-O -Ilibtiff -Ilibgeotiff -Iport"

LINK=gcc
XTRALIBS="-lm"

for FILE in *.c */*.c ; do
  echo $CC -c $CFLAGS  $FILE
  $CC -c $CFLAGS  $FILE
done
for FILE in *.cpp */*.cpp ; do
  echo $CPPC -c $CFLAGS  $FILE
  $CPPC -c $CFLAGS  $FILE
done

$LINK *.o $XTRALIBS -o img2tif
