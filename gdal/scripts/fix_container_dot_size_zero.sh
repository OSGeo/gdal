#!/bin/sh

find port alg gcore gnm ogr frmts apps -name "*.c*" -exec python scripts/fix_container_dot_size_zero.py {} \;
