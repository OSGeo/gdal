/******************************************************************************
 * $Id$
 *
 * Name:     ColorTable.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

//************************************************************************
//
// Define the extensions for ColorTable (nee GDALColorTable)
//
//************************************************************************
%rename (ColorTable) GDALColorTableShadow;

class GDALColorTableShadow {
private:
  GDALColorTableShadow();

public:

%extend {

#ifndef SWIGJAVA
    %feature("kwargs") GDALColorTableShadow;
#endif
    GDALColorTableShadow(GDALPaletteInterp palette = GPI_RGB ) {
        return (GDALColorTableShadow*) GDALCreateColorTable(palette);
    }

    ~GDALColorTableShadow() {
        GDALDestroyColorTable(self);
    }

    %newobject Clone();
    GDALColorTableShadow* Clone() {
        return (GDALColorTableShadow*) GDALCloneColorTable (self);
    }

    GDALPaletteInterp GetPaletteInterpretation() {
        return GDALGetPaletteInterpretation(self);
    }

%rename (GetCount) GetColorEntryCount;

    int GetColorEntryCount() {
        return GDALGetColorEntryCount(self);
    }

    GDALColorEntry* GetColorEntry (int entry) {
        return (GDALColorEntry*) GDALGetColorEntry(self, entry);
    }

#if !defined(SWIGJAVA)
    int GetColorEntryAsRGB(int entry, GDALColorEntry* centry) {
        return GDALGetColorEntryAsRGB(self, entry, centry);
    }
#endif

    void SetColorEntry( int entry, const GDALColorEntry* centry) {
        GDALSetColorEntry(self, entry, centry);
    }

    void CreateColorRamp(   int nStartIndex, const GDALColorEntry* startcolor,
                            int nEndIndex, const GDALColorEntry* endcolor) {
        GDALCreateColorRamp(self, nStartIndex, startcolor, nEndIndex, endcolor);
    }

}

};
