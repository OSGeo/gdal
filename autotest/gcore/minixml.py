#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Minixml services from Python.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal

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
    doc_target = '<TestDoc style="123">\n  <sub1 />\n  <sub2>abc</sub2>\n</TestDoc>\n'
    doc_got = gdal.SerializeXMLTree( tree )
    if doc_got != doc_target:
        gdaltest.post_reason( 'serialize xml tree failed.' )
        print(doc_got)
        return 'fail'
    return 'success' 

###############################################################################
# Read XML document with complex DOCTYPE element.

def minixml_3():

    fp = open( 'data/doctype.xml', 'r' )
    text = fp.read()
    tree = gdal.ParseXMLString( text )

    if tree[0] != gdal.CXT_Element:
        gdaltest.post_reason( 'wrong node type.' )
        return 'fail'

    # Check <chapter> element
    node = tree[6]

    if node[0] != gdal.CXT_Element:
        gdaltest.post_reason( 'wrong node type.' )
        return 'fail'

    if node[1] != 'chapter':
        gdaltest.post_reason( 'Wrong element name' )
        return 'fail'
    
    if len(node) != 7:
        gdaltest.post_reason( 'Wrong number of children.' )
        return 'fail'

    # Check <chapter><title> subelement
    subnode = node[2]

    if subnode[0] != gdal.CXT_Element:
        gdaltest.post_reason( 'wrong node type.' )
        return 'fail'

    if subnode[1] != 'title':
        gdaltest.post_reason( 'Wrong element name' )
        return 'fail'

    if len(subnode) != 3:
        gdaltest.post_reason( 'Wrong number of children.' )
        return 'fail'

    if subnode[2][1] != 'Chapter 1':
        gdaltest.post_reason( 'Wrong element content.' )
        return 'fail'

    # Check fist <chapter><para> subelement
    subnode = node[3]

    if subnode[0] != gdal.CXT_Element:
        gdaltest.post_reason( 'wrong node type.' )
        return 'fail'

    if subnode[1] != 'para':
        gdaltest.post_reason( 'Wrong element name' )
        return 'fail'

    if len(subnode) != 3:
        gdaltest.post_reason( 'Wrong number of children.' )
        return 'fail'

    return 'success'

###############################################################################
# Parse and serialize an XML Tree with a <?xml> prolog

def minixml_4():

    xml = """<?xml encoding="utf-8"?>\n<foo />\n"""
    got_xml = gdal.SerializeXMLTree(gdal.ParseXMLString(xml))
    if xml != got_xml:
        gdaltest.post_reason( 'serialize xml tree failed.' )
        print(got_xml)
        return 'fail'

    return 'success'

###############################################################################
# Cleanup

def minixml_cleanup():
    return 'success'

gdaltest_list = [
    minixml_1,
    minixml_2,
    minixml_3,
    minixml_4,
    minixml_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'minixml' )
    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

