#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  OGR Python samples
# Purpose:  This little script is basically curl or wget, with the added 
#           benefit of dumping the contents of the url right out to the 
#           filesystem. This is useful if you want to roll curl-like 
#           functionality your python script using the ZipURL class. 
# Author:   Howard Butler <hobu@hobu.net>
#
###############################################################################
# Copyright (c) 2005, Howard Butler <hobu@hobu.net>
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
# 
#  $Log$
#  Revision 1.1  2005/12/26 00:15:01  hobu
#  New.  Little utility tool to add some curl-like functionality to
#  get a zip file from a URL and extract it to a given directory.
#  Useful for the ZipURL class which can be added to your own Python
#  scripts to do the same thing.
#


# =============================================================================
def Usage():
    print 'Usage: get_world_zip url [directory]'
    print 'Get a zipped shapefile from a URL across the internet'
    print
    print '  url          URL to retrieve'
    print '  [directory]  optional directory (defaults to cwd)'
    print
    sys.exit(1)
# =============================================================================


import urllib2
import StringIO
import zipfile
import os


class ZipURL(object):
    def __init__(self, url):
        self.url = url
    def get(self):
       url = urllib2.urlopen(self.url)
       data = url.read()
       data = StringIO.StringIO(data)
       self.zip = zipfile.ZipFile(data)
    def write(self, directory):
        for i in self.zip.filelist:
            filename = os.path.join(directory, i.filename)
            f = open(filename,'wb')
            f.write(self.zip.read(i.filename))
            f.close()
        return True

   
if __name__ == '__main__':
    import sys
    try:
        url = sys.argv[1]
    except:
        print "====================="
        print "No URL was given"
        print "====================="
        Usage()
    try:
        directory = sys.argv[2]
    except:
        directory = '.'
        

    z = ZipURL(url)
    z.get()
    z.write(directory)