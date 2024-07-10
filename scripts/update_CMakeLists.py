#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright 2024 Even Rouault

"""This scripts updates the cmake_minimum_required() content of CMakeLists.txt
   files in subdirectories (typically drivers with a standalone mode) with the
   content of the toplevel one.
"""

import os

root_dir = os.path.dirname(os.path.dirname(__file__))

main_CMakeLists = os.path.join(root_dir, "CMakeLists.txt")
cmake_minimum_required = None
for line in open(main_CMakeLists, "rt").readlines():
    line = line[:-1]
    if line.startswith("cmake_minimum_required"):
        cmake_minimum_required = line
        break
assert cmake_minimum_required


def update_file(filename):
    new_lines = []
    changed = False
    for line in open(filename, "rt").readlines():
        line = line[:-1]
        if line.startswith("cmake_minimum_required") and line != cmake_minimum_required:
            changed = True
            new_lines.append(cmake_minimum_required)
        else:
            new_lines.append(line)
    if changed:
        print("Updating " + filename)
        with open(filename, "wt") as f:
            for line in new_lines:
                f.write(line)
                f.write("\n")


def update_files(dirname):
    for filename in os.listdir(dirname):
        full_filename = os.path.join(dirname, filename)
        if filename == "CMakeLists.txt":
            update_file(full_filename)
        elif os.path.isdir(full_filename):
            update_files(full_filename)


for subdirname in ["port", "alg", "gcore", "frmts", "ogr"]:
    full_filename = os.path.join(root_dir, subdirname)
    update_files(full_filename)
