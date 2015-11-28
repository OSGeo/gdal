                         MITAB Library overview
                         ======================

Note: until there is good complete documentation for the MITAB library,
this README file is the only source of information for integrating the
MITAB library in your application.

Please also visit the following URLs:

  - The library's web page:
    http://mitab.maptools.org/

  - The OGR architecture documentation:
    http://ogr.maptools.org/ogr_arch.html


COPYRIGHT AND LICENSE TERMS:
----------------------------

The most part of the MITAB library is 
   Copyright (c) 1998-2005, Daniel Morissette (morissette@dmsolutions.ca)
it also contains parts and uses support libraries that are
   Copyright (c) 1998-2005, Frank Warmerdam (warmerdam@pobox.com)
and 
   Copyright (c) 1999, 2000, Stephane Villeneuve (stephane.v@videotron.ca)

The MITAB library, and its supporting libraries (OGR and CPL) are freely
available under the following OpenSource license terms:

 **********************************************************************
 * Copyright (c) 1998-2005, Daniel Morissette
 * Copyright (c) 1998-2005, Frank Warmerdam
 * Copyright (c) 1999,2000, Stephane Villeneuve
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************


USING THE TAB2TAB CONVERSION PROGRAM:
-------------------------------------

MITAB comes with a conversion program called TAB2TAB that can do tab-to-mif 
and mif-to-tab translations:

Usage: tab2tab <src_filename> <dst_filename>
    Converts TAB or MIF file <src_filename> to TAB or MIF format.
    The extension of <dst_filename> (.tab or .mif) defines the output format.


COMPILING THE LIBRARY:
----------------------

When you extract the ZIP (or .tar.gz) file, you will get 3 directories 
and the makefiles to compile the library using VC++ 6 under Windows or 
using GNU make and GCC on Unix.  

The MITAB directory contains the core of the library and the TAB2TAB 
conversion program.
The OGR and CPL directories are support libraries used by MITAB and probably
won't be of much interest to you at the beginning.

To compile the lib on Windows: 
  In a DOS prompt, setup the VC++ environment variables by executing
  VCVARS32.BAT (somewhere in your VC++ install) and start the build 
  using:
        nmake -f makefile.vc

  This should automagically compile the 3 sub-directories and the test
  program (tab2tab.exe, tabdump.exe, mitabc_test.exe) in the MITAB directory.

  If you plan to use MITAB from Visual Basic or non-C environments, then
  have a look at contrib/README_VB.TXT

To compile the library on Unix:

  The main directory contains a GNUmakefile whose default target will 
  compile the contents of the 3 sub-directories and the test programs
  in the mitab directory.

  Note about byte ordering: by default, the library is built for systems
  with LSB first (Intel) byte ordering.  To build the library on systems 
  that use MSB first byte ordering (such as SUN systems), you should add
  the "-DCPL_MSB" flag to the compile flags in the "GNUmake.opt" file.


USING THE LIBRARY IN YOUR PROGRAMS:
-----------------------------------

There are 2 interfaces to access the library:

1- The C API.  The C API is a simplified interface which allows you to
   build simple applications quickly but may not give you access to every
   property of every object type.
   See the C API documentation on http://mitab.maptools.org/
   There are also interface definition files for various environments
   in the contrib directory (mainly VB, Pascal and MapBasic... thanks to
   Bo Thomsen)

2- The C++ API will give you full access to every feature of the MapInfo
   data model.  Unfortunately there is no complete documentation for the 
   library's C++ classes yet, but tab2tab.cpp is a good example that 
   shows how to open TAB files for read and write.  Also, the main 
   classes you will need to deal with live in the header MITAB.H.


The rest of this file covers mostly the C++ API features but may also apply
to the C API indirectly.


TO READ FILES:
--------------

To open a .TAB or .MIF file for read, you can use the static method:

  IMapInfoFile *IMapInfoFile::SmartOpen(const char *pszFname,
                                        GBool bTestOpenNoError);

This function returns NULL if the file cannot be opened.  If the open
was successful, then it returns a new object of the type corresponding
to the type of file that was opened:

  class TABFile:  Class to handle .TAB datasets for read/write access.
                  Note that you cannot use it to modify existing datasets
                  (not yet!).

  class TABView:  Class to handle views on pairs of .TAB files linked 
                  through an indexed field of type integer.

  class MIFFile:  Class to handle MIF files for read/write.


The method IMapInfoFile::GetFileClass() can be used to establish the 
type of object that has been returned by SmartOpen().


TO CREATE NEW FILES:
--------------------

For write access, you create an instance of TABFile or MIFFile and use
it to open the new file and write to it.  Note that only sequential write is 
supported.


FEATURE CLASSES:
----------------

On read access, GetFeatureRef() returns object of classes derived from
class TABFeature.  You can tell the type of an object using the method
TABFeature::GetFeatureClass().

The following table lists the various feature types and the geometry 
types that can be returned and that are accepted (for writing) by each
of them:

    Feature Type     Returns (read mode)    Accepts (write mode)
    ------------     -------------------    --------------------

    TABPoint         OGRPoint               OGRPoint

    TABFontPoint     OGRPoint               OGRPoint

    TABCustomPoint   OGRPoint               OGRPoint

    TABPolyline      OGRLineString or       OGRLineString or
                     OGRMultilineString     OGRMultilineString

    TABRegion        OGRPolygon (with a     OGRPolygon (with 1 or more
                     single ring) or        rings) or OGRMultiPolygon
                     OGRMultiPolygon (for 
                     multiple rings)

    TABRectangle     OGRPolygon             OGRPolygon

    TABEllipse       OGRPolygon             OGRPolygon or 
                                            OGRPoint corresponding to the
                                            ellipse center

    TABArc           OGRLineString          OGRLineString or 
                                            OGRPoint corresponding to the
                                            arc's defining ellipse center

    TABText          OGRPoint (the lower-   OGRPoint
                     left corner of the
                     text)

    TABMultiPoint    OGRMultiPoint          OGRMultiPoint

    TABCollection    OGRCollection with     In write mode, the geometry
                     3 optional components: cannot be set directly as an
                     1- 0 or 1 OGRPolygon   OGRGeometry. The following methods
                        or OGRMultiPolygon  must be used instead:
                     2- 0 or 1 OGRLineString     SetRegionDirectly()
                        or OGRMultiLineString    SetPolylineDirectly()
                     3- 0 or 1 OGRMultiPoint     SetMultiPointDirectly()


NOTE ABOUT "\n" IN STRINGS ON TABText OBJECTS:
----------------------------------------------

The text strings on text object may contain embedded "\n" characters in
MapInfo.  

In those cases, the library returns strings in which the "\n" character
is escaped on two characters: "\" + "n"

The library will also automatically convert any "\"+"n" sequence in
strings into a binary "\n" when it writes text objects to TAB files.

This also implies that a single "\" character also has to be converted
to "\"+"\".

Also note that this conversion does not apply to table fields of type 
"Char" since there appears to be no special treatment in MapInfo for 
the "\n" sequence in char attribute field values.


------------
$Id: README.TXT,v 1.12 2005/10/07 14:00:07 dmorissette Exp $
------------
