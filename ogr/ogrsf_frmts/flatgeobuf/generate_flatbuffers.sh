#!/bin/sh

wget https://github.com/flatgeobuf/flatgeobuf/raw/refs/heads/master/src/fbs/header.fbs
wget https://github.com/flatgeobuf/flatgeobuf/raw/refs/heads/master/src/fbs/feature.fbs

flatc --cpp --scoped-enums ./*.fbs
