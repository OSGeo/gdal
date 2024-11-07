#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Minixml services from Python.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import gdal

###############################################################################
# Parse a simple document into a tree of lists.


def test_minixml_1():

    tree = gdal.ParseXMLString('<TestDoc style="123"><sub1/><sub2>abc</sub2></TestDoc>')

    assert tree[0] == gdal.CXT_Element, "wrong node type."

    assert tree[1] == "TestDoc", "Wrong element name"

    assert len(tree) == 5, "Wrong number of children."

    # Check style attribute
    node = tree[2]

    assert node[0] == gdal.CXT_Attribute, "wrong node type."

    assert node[1] == "style", "Wrong element name"

    assert len(node) == 3, "Wrong number of children."

    assert node[2][1] == "123", "Wrong element content."

    # Check <sub1> element
    node = tree[3]

    assert node[0] == gdal.CXT_Element, "wrong node type."

    assert node[1] == "sub1", "Wrong element name"

    assert len(node) == 2, "Wrong number of children."

    # Check <sub2> element
    node = tree[4]

    assert node[0] == gdal.CXT_Element, "wrong node type."

    assert node[1] == "sub2", "Wrong element name"

    assert len(node) == 3, "Wrong number of children."

    assert node[2][1] == "abc", "Wrong element content."


###############################################################################
# Serialize an XML Tree


def test_minixml_2():

    tree = [
        0,
        "TestDoc",
        [2, "style", [1, "123"]],
        [0, "sub1"],
        [0, "sub2", [1, "abc"]],
    ]
    doc_target = '<TestDoc style="123">\n  <sub1 />\n  <sub2>abc</sub2>\n</TestDoc>\n'
    doc_got = gdal.SerializeXMLTree(tree)
    assert doc_got == doc_target, "serialize xml tree failed."


###############################################################################
# Read XML document with complex DOCTYPE element.


def test_minixml_3():

    fp = open("data/doctype.xml", "r")
    text = fp.read()
    tree = gdal.ParseXMLString(text)

    assert tree[0] == gdal.CXT_Element, "wrong node type."

    # Check <chapter> element
    node = tree[6]

    assert node[0] == gdal.CXT_Element, "wrong node type."

    assert node[1] == "chapter", "Wrong element name"

    assert len(node) == 7, "Wrong number of children."

    # Check <chapter><title> subelement
    subnode = node[2]

    assert subnode[0] == gdal.CXT_Element, "wrong node type."

    assert subnode[1] == "title", "Wrong element name"

    assert len(subnode) == 3, "Wrong number of children."

    assert subnode[2][1] == "Chapter 1", "Wrong element content."

    # Check fist <chapter><para> subelement
    subnode = node[3]

    assert subnode[0] == gdal.CXT_Element, "wrong node type."

    assert subnode[1] == "para", "Wrong element name"

    assert len(subnode) == 3, "Wrong number of children."


###############################################################################
# Parse and serialize an XML Tree with a <?xml> prolog


def test_minixml_4():

    xml = """<?xml encoding="utf-8"?>\n<foo />\n"""
    got_xml = gdal.SerializeXMLTree(gdal.ParseXMLString(xml))
    assert xml == got_xml, "serialize xml tree failed."


###############################################################################
# Parse malformed XML.  Complains, but still makes a tree.


def test_minixml_5():

    test_pairs = (
        ("<a></A>", "case"),
        ("<a b=c></a>", "quoted"),
    )

    for xml_str, expect in test_pairs:
        with gdal.quiet_errors():
            tree = gdal.ParseXMLString(xml_str)

        found = gdal.GetLastErrorMsg()
        assert expect in found, (
            'Did not find expected error message: "%s"  '
            'Found: "%s"  '
            'For test string: "%s""' % (expect, found, xml_str)
        )

        assert tree is not None, 'Tree is None: "%s"' % tree


###############################################################################
# Parse malformed XML.


def test_minixml_6():

    test_pairs = (
        ("<", "element token after open angle bracket"),
        ("<a>", "not all elements have been closed"),
        ("<a><b>", "not all elements have been closed"),
        ("<a><b></a></b>", "have matching"),
        ("<a foo=></a>", "attribute value"),
        ("<></>", "element token"),
        ("<&></&>", "matching"),
        ("<a></a", "Missing close angle"),
        ("<a foo=2'> foo=2'>", "unexpected token"),
        ("<a?>", "without matching"),
        ("<a/.", "for value of attribute "),
        ("<a'>", "reached EOF before closing quote"),
    )

    for xml_str, expect in test_pairs:
        with pytest.raises(Exception):
            gdal.ParseXMLString(xml_str)


###############################################################################
# Parse malformed XML.  Pass without warning, but should not pass.


def test_minixml_7():

    test_strings = (
        "<1></1>",
        "<-></->",
        "<.></.>",
        "<![CDATA[",
    )

    for xml_str in test_strings:
        gdal.ErrorReset()
        tree = gdal.ParseXMLString(xml_str)

        found = gdal.GetLastErrorMsg()
        assert found == "", 'Unexpected msg "%s"' % found

        assert tree is not None, 'Tree is None: "%s"' % tree


###############################################################################
# Parse XML with too many nesting


def test_minixml_8():

    xml_str = "<a>" * 10001
    xml_str += "</a>" * 10001

    gdal.ErrorReset()
    with pytest.raises(Exception):
        gdal.ParseXMLString(xml_str)


###############################################################################
# Parse and serialize an XML Tree with a <?a b c d ?> processing instruction


def test_minixml_processing_instruction():

    xml = """<?a b c d?>\n<foo />\n"""
    got_xml = gdal.SerializeXMLTree(gdal.ParseXMLString(xml))
    assert xml == got_xml, "serialize xml tree failed."


###############################################################################
# Cleanup


def test_minixml_cleanup():
    pass
