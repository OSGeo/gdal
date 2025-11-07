/******************************************************************************
 *
 * Name:     gdal_colortable.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALColorTable class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALCOLORTABLE_H_INCLUDED
#define GDALCOLORTABLE_H_INCLUDED

#include "cpl_port.h"

#include "gdal.h"

#include <memory>
#include <vector>

/* ******************************************************************** */
/*                             GDALColorTable                           */
/* ******************************************************************** */

/** A color table / palette. */

class CPL_DLL GDALColorTable
{
    GDALPaletteInterp eInterp;

    std::vector<GDALColorEntry> aoEntries{};

  public:
    explicit GDALColorTable(GDALPaletteInterp = GPI_RGB);

    /** Copy constructor */
    GDALColorTable(const GDALColorTable &) = default;

    /** Copy assignment operator */
    GDALColorTable &operator=(const GDALColorTable &) = default;

    /** Move constructor */
    GDALColorTable(GDALColorTable &&) = default;

    /** Move assignment operator */
    GDALColorTable &operator=(GDALColorTable &&) = default;

    ~GDALColorTable();

    GDALColorTable *Clone() const;
    int IsSame(const GDALColorTable *poOtherCT) const;

    GDALPaletteInterp GetPaletteInterpretation() const;

    int GetColorEntryCount() const;
    const GDALColorEntry *GetColorEntry(int i) const;
    int GetColorEntryAsRGB(int i, GDALColorEntry *poEntry) const;
    void SetColorEntry(int i, const GDALColorEntry *poEntry);
    int CreateColorRamp(int nStartIndex, const GDALColorEntry *psStartColor,
                        int nEndIndex, const GDALColorEntry *psEndColor);
    bool IsIdentity() const;

    static std::unique_ptr<GDALColorTable>
    LoadFromFile(const char *pszFilename);

    /** Convert a GDALColorTable* to a GDALRasterBandH.
     */
    static inline GDALColorTableH ToHandle(GDALColorTable *poCT)
    {
        return static_cast<GDALColorTableH>(poCT);
    }

    /** Convert a GDALColorTableH to a GDALColorTable*.
     */
    static inline GDALColorTable *FromHandle(GDALColorTableH hCT)
    {
        return static_cast<GDALColorTable *>(hCT);
    }
};

#endif
