#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR
# Purpose:  Update copyright info in headers
# Author:   Even Rouault <even dot rouault at mines-paris dot org>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
#
# Permission is hereby granted, free of charge, to any person oxyzaining a
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
author_in_file = 'ouault'
git_author = 'Even Rouault'
full_author = 'Even Rouault <even dot rouault at mines-paris dot org>'

for dirname, dirnames, filenames in os.walk('.'):

    if dirname.find('.svn') >= 0:
        continue
    if dirname.find('.git') >= 0:
        continue
    if dirname.find('libpng') >= 0:
        continue
    if dirname.find('libjpeg') >= 0:
        continue
    if dirname.find('libtiff') >= 0:
        continue
    if dirname.find('giflib') >= 0:
        continue
    if dirname.find('libgeotiff') >= 0:
        continue
    if dirname.find('libjson') >= 0:
        continue

    # print path to all filenames.
    for filename in filenames:
        if filename.find('.svn') >= 0:
            continue
        if filename.find('.h') < 0 and filename.find('.c') < 0 and filename.find('.py') < 0:
            continue
        if filename == 'e00read.c' or filename == 'e00compr.h':
            continue
        fullfilename = os.path.join(dirname, filename)

        #print(fullfilename)
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
            if line.find('Author:') == 0 and line.find(git_author) >= 0:
                i = i + 1
                #line = lines[i][0:-1]
                #year = int(line.split(' ')[7])
                i = i + 1
                i = i + 1
                commit_number = lines[i-4][7:15]
                ignore_commit = False
                while i < nlines:
                    line = lines[i][0:-1]
                    if line.find('commit') == 0:
                        break
                    if line.find('(by') >= 0 or line.find('contributed by') >= 0 or \
                       line.find('patch') >= 0 or line.find('Patch') >= 0 or \
                       line.find('Update copyright') >= 0 or line.find('From: ') >= 0 or \
                       line.find('Add BLX Magellan Topo driver') >= 0:
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
                if line.find(commit) == 0:
                    ignore = True
            if not ignore and line.find('Copyright') < 0:
                idx = line.find("(" + git_author)
                if idx > 0:
                    count_matching_lines = count_matching_lines + 1
                    line = line[idx+len(git_author)+1:]
                    idx = line.find(' 20')
                    year = int(line[idx+1:idx+5])
                    if minyear < 0 or year < minyear:
                        minyear = year
                    if maxyear < 0 or year > maxyear:
                        maxyear = year
            i = i + 1

        # only clame copyright if we have authored more than 10 lines
        if count_matching_lines < 10:
            continue

        print(fullfilename + ' %d-%d' % (minyear, maxyear))
        f = open(fullfilename, 'rb')
        f2 = open(fullfilename + '.tmp', 'wb')
        lines = f.readlines()
        i = 0
        nlines = len(lines)
        already_added = False
        if filename.find('.py') >= 0:
            prefix = '# '
        else:
            prefix = ' * '
        while i < nlines:
            line = lines[i]
            if not already_added and line.find('Copyright') >= 0:
                already_added = True
                if line.find('#  ') == 0:
                    prefix = '#  '
                elif line.find('# * ') == 0:
                    prefix = '# * '
                elif line.find('// ') == 0:
                    prefix = '// '
                while line.find(author_in_file) < 0:
                    f2.write(line)
                    i = i + 1
                    line = lines[i]
                    if (line.find('Copyright') < 0 and len(line.strip()) < 10) or line.find('Permission to use') > 0:
                        break
                if minyear < maxyear :
                    f2.write('%sCopyright (c) %d-%d, %s\n' % (prefix, minyear, maxyear, full_author))
                else:
                    f2.write('%sCopyright (c) %d, %s\n' % (prefix, minyear, full_author))
                if line.find(author_in_file) < 0:
                    f2.write(line)
            else:
                f2.write(line)
            i = i + 1
        f.close()
        f2.close()
        os.rename(fullfilename + '.tmp', fullfilename)
