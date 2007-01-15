#!/usr/bin/env python
###############################################################################
# $Id: minixml.py,v 1.2 2006/10/27 04:31:51 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Minixml services from Python.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
#  $Log: minixml.py,v $
#  Revision 1.2  2006/10/27 04:31:51  fwarmerdam
#  corrected licenses
#
#  Revision 1.1  2005/08/08 15:55:17  fwarmerdam
#  New
#
#

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import gdal

###############################################################################
# Parse a simple document into a tree of lists.

def minixml_1():

    tree = gdal.ParseXMLString( '<TestDoc style="123"><sub1/><sub2>abc</sub2></TestDoc>' )

    if tree[0] != gdal.CXT_Element:
        gdaltest.post_reason( 'wrong node type.' )
        return 'fail'

    if tree[1] != 'TestDoc':
        gdaltest.post_reason( 'Wrong element name' )
        return 'fail'

    if len(tree) != 5:
        gdaltest.post_reason( 'Wrong number of children.' )
        return 'fail'

    # Check style attribute
    node = tree[2]
    
    if node[0] != gdal.CXT_Attribute:
        gdaltest.post_reason( 'wrong node type.' )
        return 'fail'

    if node[1] != 'style':
        gdaltest.post_reason( 'Wrong element name' )
        return 'fail'

    if len(node) != 3:
        gdaltest.post_reason( 'Wrong number of children.' )
        return 'fail'

    if node[2][1] != '123':
        gdaltest.post_reason( 'Wrong element content.' )
        return 'fail'

    # Check <sub1> element
    node = tree[3]
    
    if node[0] != gdal.CXT_Element:
        gdaltest.post_reason( 'wrong node type.' )
        return 'fail'

    if node[1] != 'sub1':
        gdaltest.post_reason( 'Wrong element name' )
        return 'fail'

    if len(node) != 2:
        gdaltest.post_reason( 'Wrong number of children.' )
        return 'fail'

    # Check <sub2> element
    node = tree[4]
    
    if node[0] != gdal.CXT_Element:
        gdaltest.post_reason( 'wrong node type.' )
        return 'fail'

    if node[1] != 'sub2':
        gdaltest.post_reason( 'Wrong element name' )
        return 'fail'

    if len(node) != 3:
        gdaltest.post_reason( 'Wrong number of children.' )
        return 'fail'

    if node[2][1] != 'abc':
        gdaltest.post_reason( 'Wrong element content.' )
        return 'fail'

    return 'success' 

###############################################################################
# Serialize an XML Tree

def minixml_2():

    tree = [0,'TestDoc',[2,'style',[1,'123']],[0,'sub1'],[0,'sub2',[1,'abc']]]
    doc_target = '<TestDoc style="123">\n  <sub1/>\n  <sub2>abc</sub2>\n</TestDoc>\n'
    doc_got = gdal.SerializeXMLTree( tree )
    if doc_got != doc_target:
        gdaltest.post_reason( 'serialize xml tree failed.' )
        print doc_got
        return 'fail'
    return 'success' 

###############################################################################
# Cleanup

def minixml_cleanup():
    return 'success'

gdaltest_list = [
    minixml_1,
    minixml_2,
    minixml_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'minixml' )
    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

