/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Color table implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
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
 ****************************************************************************/

#include "gdal_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           GDALColorTable()                           */
/************************************************************************/

/**
 * \brief Construct a new color table.
 *
 * This constructor is the same as the C GDALCreateColorTable() function.
 *
 * @param eInterpIn the interpretation to be applied to GDALColorEntry
 * values. 
 */

GDALColorTable::GDALColorTable( GDALPaletteInterp eInterpIn )

{
    eInterp = eInterpIn;
}

/************************************************************************/
/*                        GDALCreateColorTable()                        */
/************************************************************************/

/**
 * \brief Construct a new color table.
 *
 * This function is the same as the C++ method GDALColorTable::GDALColorTable()
 */
GDALColorTableH CPL_STDCALL GDALCreateColorTable( GDALPaletteInterp eInterp )

{
    return (GDALColorTableH) (new GDALColorTable( eInterp ));
}


/************************************************************************/
/*                          ~GDALColorTable()                           */
/************************************************************************/

/**
 * \brief Destructor.
 *
 * This descructor is the same as the C GDALDestroyColorTable() function.
 */

GDALColorTable::~GDALColorTable()

{
}

/************************************************************************/
/*                       GDALDestroyColorTable()                        */
/************************************************************************/

/**
 * \brief Destroys a color table.
 *
 * This function is the same as the C++ method GDALColorTable::~GDALColorTable()
 */
void CPL_STDCALL GDALDestroyColorTable( GDALColorTableH hTable )

{
    delete (GDALColorTable *) hTable;
}

/************************************************************************/
/*                           GetColorEntry()                            */
/************************************************************************/

/**
 * \brief Fetch a color entry from table.
 *
 * This method is the same as the C function GDALGetColorEntry().
 *
 * @param i entry offset from zero to GetColorEntryCount()-1.
 *
 * @return pointer to internal color entry, or NULL if index is out of range.
 */

const GDALColorEntry *GDALColorTable::GetColorEntry( int i ) const

{
    if( i < 0 || i >= static_cast<int>(aoEntries.size()) )
        return NULL;
    else
        return &aoEntries[i];
}

/************************************************************************/
/*                         GDALGetColorEntry()                          */
/************************************************************************/


/**
 * \brief Fetch a color entry from table.
 *
 * This function is the same as the C++ method GDALColorTable::GetColorEntry()
 */
const GDALColorEntry * CPL_STDCALL 
GDALGetColorEntry( GDALColorTableH hTable, int i )

{
    VALIDATE_POINTER1( hTable, "GDALGetColorEntry", NULL );

    return ((GDALColorTable *) hTable)->GetColorEntry( i );
}


/************************************************************************/
/*                         GetColorEntryAsRGB()                         */
/************************************************************************/

/**
 * \brief Fetch a table entry in RGB format.
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
    if( eInterp != GPI_RGB || i < 0 || i >= static_cast<int>(aoEntries.size()) )
        return FALSE;
    
    *poEntry = aoEntries[i];
    return TRUE;
}

/************************************************************************/
/*                       GDALGetColorEntryAsRGB()                       */
/************************************************************************/

/**
 * \brief Fetch a table entry in RGB format.
 *
 * This function is the same as the C++ method GDALColorTable::GetColorEntryAsRGB()
 */
int CPL_STDCALL GDALGetColorEntryAsRGB( GDALColorTableH hTable, int i, 
                            GDALColorEntry *poEntry )

{
    VALIDATE_POINTER1( hTable, "GDALGetColorEntryAsRGB", 0 );
    VALIDATE_POINTER1( poEntry, "GDALGetColorEntryAsRGB", 0 );

    return ((GDALColorTable *) hTable)->GetColorEntryAsRGB( i, poEntry );
}

/************************************************************************/
/*                           SetColorEntry()                            */
/************************************************************************/

/**
 * \brief Set entry in color table.
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
    
    try
    {
        if( i >= static_cast<int>(aoEntries.size()) )
        {
            GDALColorEntry oBlack;
            oBlack.c1 = oBlack.c2 = oBlack.c3 = oBlack.c4 = 0;
            aoEntries.resize(i+1, oBlack);
        }
    
        aoEntries[i] = *poEntry;
    }
    catch(std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
    }
}

/************************************************************************/
/*                         GDALSetColorEntry()                          */
/************************************************************************/

/**
 * \brief Set entry in color table.
 *
 * This function is the same as the C++ method GDALColorTable::SetColorEntry()
 */
void CPL_STDCALL GDALSetColorEntry( GDALColorTableH hTable, int i, 
                        const GDALColorEntry * poEntry )

{
    VALIDATE_POINTER0( hTable, "GDALSetColorEntry" );
    VALIDATE_POINTER0( poEntry, "GDALSetColorEntry" );

    ((GDALColorTable *) hTable)->SetColorEntry( i, poEntry );
}


/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * \brief Make a copy of a color table.
 *
 * This method is the same as the C function GDALCloneColorTable().
 */

GDALColorTable *GDALColorTable::Clone() const

{
	return new GDALColorTable(*this);
}

/************************************************************************/
/*                        GDALCloneColorTable()                         */
/************************************************************************/

/**
 * \brief Make a copy of a color table.
 *
 * This function is the same as the C++ method GDALColorTable::Clone()
 */
GDALColorTableH CPL_STDCALL GDALCloneColorTable( GDALColorTableH hTable )

{
    VALIDATE_POINTER1( hTable, "GDALCloneColorTable", NULL );

    return (GDALColorTableH) ((GDALColorTable *) hTable)->Clone();
}

/************************************************************************/
/*                         GetColorEntryCount()                         */
/************************************************************************/

/**
 * \brief Get number of color entries in table.
 *
 * This method is the same as the function GDALGetColorEntryCount().
 *
 * @return the number of color entries.
 */

int GDALColorTable::GetColorEntryCount() const

{
    return aoEntries.size();
}

/************************************************************************/
/*                       GDALGetColorEntryCount()                       */
/************************************************************************/

/**
 * \brief Get number of color entries in table.
 *
 * This function is the same as the C++ method GDALColorTable::GetColorEntryCount()
 */
int CPL_STDCALL GDALGetColorEntryCount( GDALColorTableH hTable )

{
    VALIDATE_POINTER1( hTable, "GDALGetColorEntryCount", 0 );

    return ((GDALColorTable *) hTable)->GetColorEntryCount();
}

/************************************************************************/
/*                      GetPaletteInterpretation()                      */
/************************************************************************/

/**
 * \brief Fetch palette interpretation.
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

/**
 * \brief Fetch palette interpretation.
 *
 * This function is the same as the C++ method GDALColorTable::GetPaletteInterpretation()
 */
GDALPaletteInterp CPL_STDCALL 
GDALGetPaletteInterpretation( GDALColorTableH hTable )

{
    VALIDATE_POINTER1( hTable, "GDALGetPaletteInterpretation", GPI_Gray );

    return ((GDALColorTable *) hTable)->GetPaletteInterpretation();
}

/**
 * \brief Create color ramp
 *
 * Automatically creates a color ramp from one color entry to
 * another. It can be called several times to create multiples ramps
 * in the same color table.
 *
 * This function is the same as the C function GDALCreateColorRamp().
 *
 * @param nStartIndex index to start the ramp on the color table [0..255]
 * @param psStartColor a color entry value to start the ramp
 * @param nEndIndex index to end the ramp on the color table [0..255]
 * @param psEndColor a color entry value to end the ramp
 * @return total number of entries, -1 to report error
 */

int GDALColorTable::CreateColorRamp(
            int nStartIndex, const GDALColorEntry *psStartColor,
            int nEndIndex, const GDALColorEntry *psEndColor )
{
    /* validate indexes */

    if( nStartIndex < 0 || nStartIndex > 255 ||
        nEndIndex < 0 || nEndIndex > 255 ||
        nStartIndex > nEndIndex )
    {
        return -1;
    }

    /* validate color entries */

    if( psStartColor == NULL || psEndColor == NULL )
    {
        return -1;
    }

    /* calculate number of colors in-between */

    int nColors = nEndIndex - nStartIndex;

    /* set starting color */

    SetColorEntry( nStartIndex, psStartColor );

    if( nColors == 0 )
    {
        return GetColorEntryCount(); /* it should not proceed */
    }

    /* set ending color */

    SetColorEntry( nEndIndex, psEndColor );

    /* calculate the slope of the linear transformation */

    double dfSlope1, dfSlope2, dfSlope3, dfSlope4;

    dfSlope1 = ( psEndColor->c1 - psStartColor->c1 ) / (double) nColors;
    dfSlope2 = ( psEndColor->c2 - psStartColor->c2 ) / (double) nColors;
    dfSlope3 = ( psEndColor->c3 - psStartColor->c3 ) / (double) nColors;
    dfSlope4 = ( psEndColor->c4 - psStartColor->c4 ) / (double) nColors;

    /* loop through the new colors */

    GDALColorEntry sColor = *psStartColor;

    int i;

    for( i = 1; i < nColors; i++ )
    {
        sColor.c1 = (short) ( i * dfSlope1 + (double) psStartColor->c1 );
        sColor.c2 = (short) ( i * dfSlope2 + (double) psStartColor->c2 );
        sColor.c3 = (short) ( i * dfSlope3 + (double) psStartColor->c3 );
        sColor.c4 = (short) ( i * dfSlope4 + (double) psStartColor->c4 );

        SetColorEntry( nStartIndex + i, &sColor );
    }

    /* return the total number of colors */

    return GetColorEntryCount();
}

/************************************************************************/
/*                         GDALCreateColorRamp()                        */
/************************************************************************/

/**
 * \brief Create color ramp
 *
 * This function is the same as the C++ method GDALColorTable::CreateColorRamp()
 */
void CPL_STDCALL 
GDALCreateColorRamp( GDALColorTableH hTable, 
            int nStartIndex, const GDALColorEntry *psStartColor,
            int nEndIndex, const GDALColorEntry *psEndColor )
{
    VALIDATE_POINTER0( hTable, "GDALCreateColorRamp" );

    ((GDALColorTable *) hTable)->CreateColorRamp( nStartIndex, psStartColor, 
                                                  nEndIndex, psEndColor );
}

/************************************************************************/
/*                           IsSame()                                   */
/************************************************************************/

/**
 * \brief Returns if the current color table is the same as another one.
 *
 * @param poOtherCT other color table to be compared to.
 * @return TRUE if both color tables are identical.
 * @since GDAL 2.0
 */

int GDALColorTable::IsSame(const GDALColorTable* poOtherCT) const
{
    return aoEntries.size() == poOtherCT->aoEntries.size() &&
           (aoEntries.size() == 0 ||
            memcmp(&aoEntries[0], &poOtherCT->aoEntries[0], aoEntries.size() * sizeof(GDALColorEntry)) == 0);
}
