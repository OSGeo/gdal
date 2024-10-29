#!/usr/bin/env python
# ******************************************************************************
#  $Id$
#
#  Project:  S-57 OGR Translator
#  Purpose:  Script to translate s57 .csv files into C code "data" statements.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2001, Frank Warmerdam
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import os
import sys

# -----------------------------------------------------------------------------
# EscapeLine - escape anything C-problematic in a line.
# -----------------------------------------------------------------------------


def EscapeLine(ln):
    return ln.replace('"', '\\"')


# -----------------------------------------------------------------------------
#


if __name__ != "__main__":
    print("This module should only be used as a mainline.")
    sys.exit(1)

if len(sys.argv) < 2:
    directory = os.environ["S57_CSV"]
else:
    directory = sys.argv[1]


print("char *gpapszS57Classes[] = {")
classes = open(directory + "/s57objectclasses.csv").readlines()

for line in classes:
    print('"%s",' % EscapeLine(line.strip()))

print("NULL };")

print("char *gpapszS57attributes[] = {")
classes = open(directory + "/s57attributes.csv").readlines()

for line in classes:
    print('"%s",' % EscapeLine(line.strip()))

print("NULL };")
