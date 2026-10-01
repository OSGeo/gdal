#!/usr/bin/python

import sys

files = sys.argv[1:]
ret = 0

for fname in files:
    f = open(fname, "rt")
    lines = f.readlines()
    for i, line in enumerate(lines):
        if line and line[len(line) - 1] == "\n":
            line = line[0:-1]
        tab = line.split("=")
        if len(tab) != 2:
            continue
        left = tab[0].strip()
        right = tab[1].strip()
        if right and right[len(right) - 1] == ";":
            right = right[0:-1]
        else:
            continue
        right = right.strip()
        if left == right:
            sys.stderr.write("%s: %d: %s\n" % (fname, i + 1, line))
            ret = 1

sys.exit(ret)
