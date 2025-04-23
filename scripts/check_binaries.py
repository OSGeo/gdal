#!/usr/bin/env python3

"""This file checks that the root directory does not contain an unexpected binary file"""

import csv
import os
import subprocess
import sys

this_dir = os.path.dirname(__file__)
if not os.path.isabs(this_dir):
    this_dir = os.path.join(os.getcwd(), this_dir)
this_dir = os.path.abspath(this_dir)
root_dir = os.path.dirname(this_dir)

binaries_allow_list = {}
with open(
    os.path.join(this_dir, "binaries_allow_list.csv"), newline="", encoding="utf-8"
) as f:
    reader = csv.DictReader(f)
    assert reader.fieldnames == ["filename", "sha256sum"]
    for row in reader:
        binaries_allow_list[row["filename"]] = row["sha256sum"]

error_code = 0

for dirname in os.listdir(root_dir):
    # We skip doc and autotest as they are not included in the GDAL source tarball
    if dirname not in ("doc", "autotest", ".git"):
        p = subprocess.Popen(
            [
                "find",
                os.path.join(root_dir, dirname),
                "-type",
                "f",
                "-exec",
                "file",
                "{}",
                ";",
            ],
            stdout=subprocess.PIPE,
        )
        out, _ = p.communicate()
        for line in out.decode("utf-8").split("\n"):
            if not line:
                continue
            tokens = line.split(":")
            filename = tokens[0]
            kind = ":".join(tokens[1:]).strip()
            if (
                "text" not in kind
                and "AutoCAD" not in kind
                and kind not in ("empty", "JSON data")
            ):
                p = subprocess.Popen(["sha256sum", filename], stdout=subprocess.PIPE)
                sha256sum, _ = p.communicate()
                sha256sum = sha256sum.decode("utf-8").split("  ")[0]

                rel_filename = filename[len(root_dir) + 1 :]
                if rel_filename not in binaries_allow_list:
                    error_code = 1
                    print(
                        f'Found unknown binary file {rel_filename} of kind "{kind}". If it is legit, add the following line in scripts/binaries_allow_list.csv:\n"{rel_filename}",{sha256sum}'
                    )
                elif binaries_allow_list[rel_filename] != sha256sum:
                    error_code = 1
                    print(
                        f'Binary file {rel_filename} has a different sha256sum than expected. If it is legit, update the following line in scripts/binaries_allow_list.csv:\n"{rel_filename}",{sha256sum}'
                    )

sys.exit(error_code)
