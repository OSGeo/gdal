#!/usr/bin/python

import sys

f = open(sys.argv[1], "rt")
lines = f.readlines()
ret = 0
for i, line in enumerate(lines):
    if line and line[len(line) - 1] == '\n':
        line = line[0:-1]
    tab = line.split('=')
    if len(tab) != 2:
        continue
    left = tab[0].strip()
    right = tab[1].strip()
    if right and right[len(right) - 1] == ';':
        right = right[0:-1]
    else:
        continue
    right = right.strip()
    if left == right:
        print("%s: %d: %s" % (sys.argv[1], i + 1, line))
        ret = 1

sys.exit(ret)
