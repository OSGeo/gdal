/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Color table implementation.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 **********************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 ******************************************************************************
 * $Log$
 * Revision 1.3  2005/04/04 15:24:48  fwarmerdam
 * Most C entry points now CPL_STDCALL
 *
 * Revision 1.2  2001/07/18 04:04:30  warmerda
 * added CPL_CVSID
 *
 * Revision 1.1  2000/03/06 02:26:00  warmerda
 * New
 *
 */

#include "gdal_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           GDALColorTable()                           */
/************************************************************************/

/**
 * Construct a new color table.
 *
 * This constructor is the same as the C GDALCreateColorTable() function.
 *
 * @param eInterpIn the interpretation to be applied to GDALColorEntry
 * values. 
 */

GDALColorTable::GDALColorTable( GDALPaletteInterp eInterpIn )

{
    eInterp = eInterpIn;

    nEntryCount = 0;
    paoEntries = NULL;
}

/************************************************************************/
/*                        GDALCreateColorTable()                        */
/************************************************************************/

GDALColorTableH CPL_STDCALL GDALCreateColorTable( GDALPaletteInterp eInterp )

{
    return (GDALColorTableH) (new GDALColorTable( eInterp ));
}


/************************************************************************/
/*                          ~GDALColorTable()                           */
/************************************************************************/

/**
 * Destructor.
 *
 * This descructor is the same as the C GDALDestroyColorTable() function.
 */

GDALColorTable::~GDALColorTable()

{
    CPLFree( paoEntries );
    paoEntries = NULL;
}

/************************************************************************/
/*                       GDALDestroyColorTable()                        */
/************************************************************************/

void CPL_STDCALL GDALDestroyColorTable( GDALColorTableH hTable )

{
    delete (GDALColorTable *) hTable;
}

/************************************************************************/
/*                           GetColorEntry()                            */
/************************************************************************/

/**
 * Fetch a color entry from table.
 *
 * This method is the same as the C function GDALGetColorEntry().
 *
 * @param i entry offset from zero to GetColorEntryCount()-1.
 *
 * @return pointer to internal color entry, or NULL if index is out of range.
 */

const GDALColorEntry *GDALColorTable::GetColorEntry( int i ) const

{
    if( i < 0 || i >= nEntryCount )
        return NULL;
    else
        return paoEntries + i;
}

/************************************************************************/
/*                         GDALGetColorEntry()                          */
/************************************************************************/

const GDALColorEntry * CPL_STDCALL 
GDALGetColorEntry( GDALColorTableH hTable, int i )

{
    return ((GDALColorTable *) hTable)->GetColorEntry( i );
}


/************************************************************************/
/*                         GetColorEntryAsRGB()                         */
/************************************************************************/

/**
 * Fetch a table entry in RGB format.
 *
 * In theory this method should support translation of color palettes in
 * non-RGB color spaces into RGB on the fly, but currently it only works
 * on RGB color tables.
 *
 * This method is the same as the C function GDALGetColorEntryAsRGB().
 *
 * @param i entry offset from zero to GetColorEntryCount()-1.
 *
 * @param poEntry the existing GDALColorEntry to be overrwritten with the RGB
 * values.
 *
 * @return TRUE on success, or FALSE if the conversion isn't supported.
 */

int GDALColorTable::GetColorEntryAsRGB( int i, GDALColorEntry *poEntry ) const

{
    if( eInterp != GPI_RGB || i < 0 || i >= nEntryCount )
        return FALSE;
    
    *poEntry = paoEntries[i];
    return TRUE;
}

/************************************************************************/
/*                       GDALGetColorEntryAsRGB()                       */
/************************************************************************/

int CPL_STDCALL GDALGetColorEntryAsRGB( GDALColorTableH hTable, int i, 
                            GDALColorEntry *poEntry )

{
    return ((GDALColorTable *) hTable)->GetColorEntryAsRGB( i, poEntry );
}

/************************************************************************/
/*                           SetColorEntry()                            */
/************************************************************************/

/**
 * Set entry in color table.
 *
 * Note that the passed in color entry is copied, and no internal reference
 * to it is maintained.  Also, the passed in entry must match the color
 * interpretation of the table to which it is being assigned.
 *
 * The table is grown as needed to hold the supplied offset.  
 *
 * This function is the same as the C function GDALSetColorEntry().
 *
 * @param i entry offset from zero to GetColorEntryCount()-1.
 * @param poEntry value to assign to table.
 */

void GDALColorTable::SetColorEntry( int i, const GDALColorEntry * poEntry )

{
    if( i < 0 )
        return;
    
    if( i >= nEntryCount )
    {
        paoEntries = (GDALColorEntry *) 
            CPLRealloc(paoEntries, sizeof(GDALColorEntry) * (i+1));
        memset( paoEntries + nEntryCount, 0, 
                sizeof(GDALColorEntry) * (i + 1 - nEntryCount) );
        
        nEntryCount = i+1;
    }

    paoEntries[i] = *poEntry;
}

/************************************************************************/
/*                         GDALSetColorEntry()                          */
/************************************************************************/

void CPL_STDCALL GDALSetColorEntry( GDALColorTableH hTable, int i, 
                        const GDALColorEntry * poEntry )

{
    ((GDALColorTable *) hTable)->SetColorEntry( i, poEntry );
}


/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * Make a copy of a color table.
 *
 * This method is the same as the C function GDALCloneColorTable().
 */

GDALColorTable *GDALColorTable::Clone() const

{
    GDALColorTable *poNew;

    poNew = new GDALColorTable(eInterp);
    poNew->nEntryCount = nEntryCount;
    poNew->paoEntries = (GDALColorEntry *) 
        CPLMalloc(sizeof(GDALColorEntry)*nEntryCount);
    memcpy( poNew->paoEntries, paoEntries, sizeof(GDALColorEntry)*nEntryCount);

    return poNew;
}

/************************************************************************/
/*                        GDALCloneColorTable()                         */
/************************************************************************/

GDALColorTableH CPL_STDCALL GDALCloneColorTable( GDALColorTableH hTable )

{
    return (GDALColorTableH) ((GDALColorTable *) hTable)->Clone();
}

/************************************************************************/
/*                         GetColorEntryCount()                         */
/************************************************************************/

/**
 * Get number of color entries in table.
 *
 * This method is the same as the function GDALGetColorEntryCount().
 *
 * @return the number of color entries.
 */

int GDALColorTable::GetColorEntryCount() const

{
    return nEntryCount;
}

/************************************************************************/
/*                       GDALGetColorEntryCount()                       */
/************************************************************************/

int CPL_STDCALL GDALGetColorEntryCount( GDALColorTableH hTable )

{
    return ((GDALColorTable *) hTable)->GetColorEntryCount();
}

/************************************************************************/
/*                      GetPaletteInterpretation()                      */
/************************************************************************/

/**
 * Fetch palette interpretation.
 *
 * The returned value is used to interprete the values in the GDALColorEntry.
 *
 * This method is the same as the C function GDALGetPaletteInterpretation().
 *
 * @return palette interpretation enumeration value, usually GPI_RGB.
 */

GDALPaletteInterp GDALColorTable::GetPaletteInterpretation() const

{
    return eInterp;
}

/************************************************************************/
/*                    GDALGetPaltteInterpretation()                     */
/************************************************************************/

GDALPaletteInterp CPL_STDCALL 
GDALGetPaletteInterpretation( GDALColorTableH hTable )

{
    return ((GDALColorTable *) hTable)->GetPaletteInterpretation();
}
