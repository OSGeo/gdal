/******************************************************************************
 *
 * Name:     ColorTable.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
 *
 * SPDX-License-Identifier: MIT
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
