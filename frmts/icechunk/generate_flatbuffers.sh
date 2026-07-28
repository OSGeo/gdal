#!/bin/sh

wget https://github.com/earth-mover/icechunk/raw/refs/tags/v2.0.5/icechunk-format/flatbuffers/common.fbs
wget https://github.com/earth-mover/icechunk/raw/refs/tags/v2.0.5/icechunk-format/flatbuffers/manifest.fbs
wget https://github.com/earth-mover/icechunk/raw/refs/tags/v2.0.5/icechunk-format/flatbuffers/repo.fbs
wget https://github.com/earth-mover/icechunk/raw/refs/tags/v2.0.5/icechunk-format/flatbuffers/snapshot.fbs

# Unused
# wget https://github.com/earth-mover/icechunk/raw/refs/tags/v2.0.5/icechunk-format/flatbuffers/transaction_log.fbs

flatc --cpp --cpp-std c++17 -o generated --scoped-enums ./*.fbs
