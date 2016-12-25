#!/bin/sh

gcc -g multistresstest.c .libs/libproj.so -lpthread -o multistresstest
./multistresstest
