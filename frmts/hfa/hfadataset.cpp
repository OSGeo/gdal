/******************************************************************************
 * $Id$
 *
 * Name:     hfadataset.cpp
 * Project:  Erdas Imagine Driver
 * Purpose:  Main driver for Erdas Imagine format.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.7  2000/08/25 21:31:34  warmerda
 * added overview support
 *
 * Revision 1.6  2000/08/18 16:24:06  warmerda
 * Added color table support
 *
 * Revision 1.5  2000/08/15 19:28:26  warmerda
 * added help topic
 *
 * Revision 1.4  2000/02/28 16:32:20  warmerda
 * use SetBand method
 *
 * Revision 1.3  1999/01/27 18:48:12  warmerda
 * Declare constructor and destructor for HFADataset.
 *
 * Revision 1.2  1999/01/27 18:32:46  warmerda
 * compiles OK
 *
 * Revision 1.1  1999/01/22 17:40:43  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "hfa.h"

CPL_C_START
void	GDALRegister_HFA(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				HFADataset				*/
/* ==================================================================== */
/************************************************************************/

class HFARasterBand;

class CPL_DLL HFADataset : public GDALDataset
{
    friend	HFARasterBand;
    
    HFAHandle	hHFA;

  public:
                HFADataset();
                ~HFADataset();

    static GDALDataset *Open( GDALOpenInfo * );

#ifdef notdef    
    virtual const char *GetProjectionRef(void);
#endif    
    virtual CPLErr GetGeoTransform( double * );
};

/************************************************************************/
/* ==================================================================== */
/*                            HFARasterBand                             */
/* ==================================================================== */
/************************************************************************/

class HFARasterBand : public GDALRasterBand
{
    friend	HFADataset;

    GDALColorTable *poCT;

  public:

                   HFARasterBand( HFADataset *, int );
    virtual        ~HFARasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};

static GDALDriver	*poHFADriver = NULL;

/************************************************************************/
/*                           HFARasterBand()                            */
/************************************************************************/

HFARasterBand::HFARasterBand( HFADataset *poDS, int nBand )

{
    int		nHFADataType;
    
    this->poDS = poDS;
    this->nBand = nBand;
    this->poCT = NULL;

    HFAGetBandInfo( poDS->hHFA, nBand, &nHFADataType,
                    &nBlockXSize, &nBlockYSize );

    switch( nHFADataType )
    {
      case EPT_u8:
      case EPT_s8:
        eDataType = GDT_Byte;
        break;

      case EPT_u16:
        eDataType = GDT_UInt16;
        break;

      case EPT_s16:
        eDataType = GDT_Int16;
        break;

      case EPT_f32:
        eDataType = GDT_Float32;
        break;

      default:
        eDataType = GDT_Byte;
        /* notdef: this should really report an error, but this isn't
           so easy from within constructors. */
        break;
    }

/* -------------------------------------------------------------------- */
/*      Collect color table if present.                                 */
/* -------------------------------------------------------------------- */
    double    *padfRed, *padfGreen, *padfBlue;
    int       nColors;

    if( HFAGetPCT( poDS->hHFA, nBand, &nColors, 
                   &padfRed, &padfGreen, &padfBlue ) == CE_None
        && nColors > 0 )
    {
        poCT = new GDALColorTable();
        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            GDALColorEntry   sEntry;

            sEntry.c1 = (int) (padfRed[iColor]   * 255);
            sEntry.c2 = (int) (padfGreen[iColor] * 255);
            sEntry.c3 = (int) (padfBlue[iColor]  * 255);
            sEntry.c4 = 255;
            poCT->SetColorEntry( iColor, &sEntry );
        }
    }
}

/************************************************************************/
/*                           ~HFARasterBand()                           */
/************************************************************************/

HFARasterBand::~HFARasterBand()

{
    FlushCache();
    if( poCT != NULL )
        delete poCT;
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr HFARasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    HFADataset	*poODS = (HFADataset *) poDS;

    return( HFAGetRasterBlock( poODS->hHFA, nBand, nBlockXOff, nBlockYOff,
                               pImage ) );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp HFARasterBand::GetColorInterpretation()

{
    if( poCT != NULL )
        return GCI_PaletteIndex;
    else
        return GCI_Undefined;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *HFARasterBand::GetColorTable()

{
    return poCT;
}

/************************************************************************/
/* ==================================================================== */
/*                            HFADataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            HFADataset()                            */
/************************************************************************/

HFADataset::HFADataset()

{
    hHFA = NULL;
}

/************************************************************************/
/*                           ~HFADataset()                            */
/************************************************************************/

HFADataset::~HFADataset()

{
    if( hHFA != NULL )
        HFAClose( hHFA );
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HFADataset::Open( GDALOpenInfo * poOpenInfo )

{
    HFAHandle	hHFA;
    int		i;
    
/* -------------------------------------------------------------------- */
/*      Verify that this is a HFA file.                                 */
/* -------------------------------------------------------------------- */
    if( !poOpenInfo->bStatOK || poOpenInfo->nHeaderBytes < 15
        || !EQUALN((char *) poOpenInfo->pabyHeader,"EHFA_HEADER_TAG",15) )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    hHFA = HFAOpen( poOpenInfo->pszFilename, "r" );
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HFADataset 	*poDS;

    poDS = new HFADataset();

    poDS->hHFA = hHFA;
    poDS->poDriver = poHFADriver;

/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    HFAGetRasterInfo( hHFA, &poDS->nRasterXSize, &poDS->nRasterYSize,
                      &poDS->nBands );
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poDS->nBands; i++ )
        poDS->SetBand( i+1, new HFARasterBand( poDS, i+1 ) );

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HFADataset::GetGeoTransform( double * padfTransform )

{
#ifdef notdef    
    padfTransform[0] = sGlobalBounds.west;
    padfTransform[1] = sGlobalBounds.ew_res;
    padfTransform[2] = 0.0;

    padfTransform[3] = sGlobalBounds.north;
    padfTransform[4] = 0.0;
    padfTransform[5] = -sGlobalBounds.ns_res;
#endif
    
    return( CE_Failure );
}

/************************************************************************/
/*                          GDALRegister_HFA()                        */
/************************************************************************/

void GDALRegister_HFA()

{
    GDALDriver	*poDriver;

    if( poHFADriver == NULL )
    {
        poHFADriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "HFA";
        poDriver->pszLongName = "Erdas Imagine Images (.img)";
        poDriver->pszHelpTopic = "frmt_various.html#HFA";
        
        poDriver->pfnOpen = HFADataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

