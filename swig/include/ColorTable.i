/******************************************************************************
 * $Id$
 *
 * Name:     ColorTable.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.1  2005/02/15 21:37:43  kruland
 * Interface definition for ColorTable object.  Cut&Paste from gdal_priv.h.
 *
 *
*/

//************************************************************************
//
// Define the extensions for ColorTable (nee GDALColorTable)
//
//************************************************************************
%rename (ColorTable) GDALColorTable;

typedef int GDALPaletteInterp;

class GDALColorTable
{
public:
                GDALColorTable( GDALPaletteInterp = GPI_RGB );
                ~GDALColorTable();

    GDALColorTable *Clone() const;

    GDALPaletteInterp GetPaletteInterpretation() const;

    int           GetColorEntryCount() const;
    const GDALColorEntry *GetColorEntry( int ) const;
    int           GetColorEntryAsRGB( int, GDALColorEntry * ) const;
    void          SetColorEntry( int, const GDALColorEntry * );
};
