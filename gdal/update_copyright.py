#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR
# Purpose:  Update copyright info in headers
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import os

# WARNING: Only works from a git repository !

# Please edit for a different author
author_in_file = 'rouault'
git_author = 'Even Rouault'
full_author = 'Even Rouault <even dot rouault at mines-paris dot org>'

for dirname, dirnames, filenames in os.walk('.'):

    if '.svn' in dirname:
        continue
    if '.git' in dirname:
        continue
    if 'libpng' in dirname:
        continue
    if 'libjpeg' in dirname:
        continue
    if 'libtiff' in dirname:
        continue
    if 'giflib' in dirname:
        continue
    if 'libgeotiff' in dirname:
        continue
    if 'libjson' in dirname:
        continue

    # print path to all filenames.
    for filename in filenames:
        if '.svn' in filename:
            continue
        if '.h' not in filename and '.c' not in filename and '.py' not in filename:
            continue
        if filename == 'e00read.c' or filename == 'e00compr.h':
            continue
        fullfilename = os.path.join(dirname, filename)

        # print(fullfilename)
        minyear = -1
        maxyear = -1

        # Find if we have authored something and commit numbers to ignore (commits that originate from other authors)
        ret = os.popen('git log %s' % fullfilename)
        lines = ret.readlines()
        i = 0
        nlines = len(lines)
        found_commit = False
        commits_to_ignore = []
        while i < nlines:
            line = lines[i][0:-1]
            if line.startswith('Author:') and git_author in line:
                i = i + 1
                # line = lines[i][0:-1]
                # year = int(line.split(' ')[7])
                i = i + 1
                i = i + 1
                commit_number = lines[i - 4][7:15]
                ignore_commit = False
                while i < nlines:
                    line = lines[i][0:-1]
                    if line.startswith('commit'):
                        break
                    if '(by' in line or 'contributed by' in line or \
                       'patch' in line or 'Patch' in line or \
                       'Update copyright' in line or 'From: ' in line or \
                       'Add BLX Magellan Topo driver' in line:
                        ignore_commit = True
                    i = i + 1
                if ignore_commit:
                    commits_to_ignore.append(commit_number)
                else:
                    found_commit = True
            else:
                i = i + 1

        if not found_commit:
            continue

        # Count how many lines we have authored
        ret = os.popen('git blame %s' % fullfilename)
        lines = ret.readlines()
        i = 0
        nlines = len(lines)
        count_matching_lines = 0
        while i < nlines:
            line = lines[i][0:-1]
            ignore = False
            for commit in commits_to_ignore:
                if line.startswith(commit):
                    ignore = True
            if not ignore and 'Copyright' not in line:
                idx = line.find("(" + git_author)
                if idx > 0:
                    count_matching_lines = count_matching_lines + 1
                    line = line[idx + len(git_author) + 1:]
                    idx = line.find(' 20')
                    year = int(line[idx + 1:idx + 5])
                    if minyear < 0 or year < minyear:
                        minyear = year
                    if maxyear < 0 or year > maxyear:
                        maxyear = year
            i = i + 1

        # Only claim copyright if we have authored more than 10 lines.
        if count_matching_lines < 10:
            continue

        print(fullfilename + ' %d-%d' % (minyear, maxyear))
        f = open(fullfilename, 'rb')
        f2 = open(fullfilename + '.tmp', 'wb')
        lines = f.readlines()
        i = 0
        nlines = len(lines)
        already_added = False
        if '.py' in filename:
            prefix = '# '
        else:
            prefix = ' * '
        while i < nlines:
            line = lines[i]
            if not already_added and 'Copyright' in line:
                already_added = True
                if line.startswith('#  '):
                    prefix = '#  '
                elif line.startswith('# * '):
                    prefix = '# * '
                elif line.startswith('// '):
                    prefix = '// '
                while author_in_file not in line:
                    f2.write(line)
                    i = i + 1
                    line = lines[i]
                    if ('Copyright' not in line and len(line.strip()) < 10) or line.find('Permission to use') > 0:
                        break
                if minyear < maxyear:
                    f2.write('%sCopyright (c) %d-%d, %s\n' % (prefix, minyear, maxyear, full_author))
                else:
                    f2.write('%sCopyright (c) %d, %s\n' % (prefix, minyear, full_author))
                if author_in_file not in line:
                    f2.write(line)
            else:
                f2.write(line)
            i = i + 1
        f.close()
        f2.close()
        os.rename(fullfilename + '.tmp', fullfilename)
