#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
#  Project:  GDAL
#  Purpose:  Check validity of <a href="XXXX"> links
#  Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
###############################################################################

import os
import sys
import requests

ok_set = dict()
broken_set = {}


def check(filename):
    f = open(filename, 'r')
    lines = f.readlines()

    for i, line in enumerate(lines):
        line = line.strip('\n')
        pos = line.find('<a href="')
        if pos >= 0:
            pos += len('<a href="')
            end_pos = line.find('"', pos)
            while end_pos < 0:
                i += 1
                line += lines[i].strip('\n')
                end_pos = line.find('"', pos)
            url = line[pos:end_pos]
            sharp = url.find('#')
            if sharp == 0:
                continue
            elif sharp > 0:
                url = url[0:sharp]
            if url in ok_set:
                continue
            if url in broken_set:
                print('ERROR: Broken link %s in %s' % (url, filename))
            print('Checking %s...' % url)
            if url.startswith('http'):
                r = requests.get(url, verify=False)
                if r.status_code == requests.codes.ok:
                    ok_set[url] = True
                else:
                    print('ERROR: Broken link %s in %s' % (url, filename))
                    broken_set[url] = True
            else:
                checked_filename = os.path.join(os.path.dirname(filename), url)
                # print(checked_filename)
                if not os.path.exists(checked_filename):
                    print('ERROR: Broken link %s in %s' % (url, filename))
                    broken_set[url] = True
                else:
                    ok_set[url] = True


for filename in sys.argv[1:]:
    check(filename)
