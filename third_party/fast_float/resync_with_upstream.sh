#!/bin/sh

set -eu

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac
cd "${SCRIPT_DIR}"

rm -rf tmp_fast_float
git clone --depth 1 https://github.com/fastfloat/fast_float tmp_fast_float
cp tmp_fast_float/include/fast_float/* "${SCRIPT_DIR}/"

cat > "${SCRIPT_DIR}/PROVENANCE.TXT" << EOF
https://github.com/fastfloat/fast_float
Retrieved at commit https://github.com/fastfloat/fast_float/commit/$(git rev-parse HEAD)

Using the MIT license choice.
EOF

rm -rf tmp_fast_float
