/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALDriver class (and C wrappers)
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2000, Frank Warmerdam
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
 *
 * $Log$
 * Revision 1.11  2000/03/06 02:21:15  warmerda
 * Added help topic C function
 *
 * Revision 1.10  2000/01/31 16:24:01  warmerda
 * use failure, not fatal
 *
 * Revision 1.9  2000/01/31 15:00:25  warmerda
 * added some documentation
 *
 * Revision 1.8  2000/01/31 14:24:36  warmerda
 * implemented dataset delete
 *
 * Revision 1.7  2000/01/13 04:13:10  pgs
 * added initialization of pfnCreate = NULL to prevent run-time crash when format doesn't support creating a file
 *
 * Revision 1.6  1999/12/08 14:40:50  warmerda
 * Fixed error message.
 *
 * Revision 1.5  1999/10/21 13:22:10  warmerda
 * Added GDALGetDriverShort/LongName().
 *
 * Revision 1.4  1999/01/11 15:36:50  warmerda
 * Added GDALCreate()
 *
 * Revision 1.3  1998/12/31 18:54:53  warmerda
 * Flesh out create method.
 *
 * Revision 1.2  1998/12/06 22:17:32  warmerda
 * Add stub Create() method
 *
 * Revision 1.1  1998/12/03 18:32:01  warmerda
 * New
 *
 */

#include "gdal_priv.h"

/************************************************************************/
/*                             GDALDriver()                             */
/************************************************************************/

GDALDriver::GDALDriver()

{
    pszShortName = NULL;
    pszLongName = NULL;

    pfnOpen = NULL;
    pfnCreate = NULL;
    pfnDelete = NULL;
}

/************************************************************************/
/*                            ~GDALDriver()                             */
/************************************************************************/

GDALDriver::~GDALDriver()

{
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

/**
 * Create a new dataset with this driver.
 *
 * What argument values are legal for particular drivers is driver specific,
 * and there is no way to query in advance to establish legal values.
 *
 * Equivelent of the C function GDALCreate().
 * 
 * @param pszFilename the name of the dataset to create.
 * @param nXSize width of created raster in pixels.
 * @param nYSize height of created raster in pixels.
 * @param nBands number of bands.
 * @param eType type of raster.
 * @param papszParmList list of driver specific control parameters.
 *
 * @return NULL on failure, or a new GDALDataset.
 */

GDALDataset * GDALDriver::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType, char ** papszParmList )

{
    /* notdef: should add a bunch of error checking here */

    if( pfnCreate == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GDALDriver::Create() ... no create method implemented"
                  " for this format.\n" );

        return NULL;
    }
    else
    {
        return( pfnCreate( pszFilename, nXSize, nYSize, nBands, eType,
                           papszParmList ) );
    }
}

/************************************************************************/
/*                             GDALCreate()                             */
/************************************************************************/

GDALDatasetH CPL_DLL GDALCreate( GDALDriverH hDriver,
                                 const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eBandType,
                                 char ** papszOptions )

{
    return( ((GDALDriver *) hDriver)->Create( pszFilename,
                                              nXSize, nYSize, nBands,
                                              eBandType, papszOptions ) );
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

/**
 * Delete named dataset.
 *
 * The driver will attempt to delete the named dataset in a driver specific
 * fashion.  Full featured drivers will delete all associated files,
 * database objects, or whatever is appropriate.  The default behaviour when
 * no driver specific behaviour is provided is to attempt to delete the
 * passed name as a single file.
 *
 * It is unwise to have open dataset handles on this dataset when it is
 * deleted.
 *
 * Equivelent of the C function GDALDeleteDataset().
 *
 * @param pszFilename name of dataset to delete.
 *
 * @return CE_None on success, or CE_Failure if the operation fails.
 */

CPLErr GDALDriver::Delete( const char * pszFilename )

{
    if( pfnDelete != NULL )
        return pfnDelete( pszFilename );
    else
    {
        VSIStatBuf	sStat;

        if( VSIStat( pszFilename, &sStat ) == 0 && VSI_ISREG( sStat.st_mode ) )
        {
            if( VSIUnlink( pszFilename ) == 0 )
                return CE_None;
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "%s: Attempt to unlink %s failed.\n",
                          pszShortName, pszFilename );
                return CE_Failure;
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s: Unable to delete %s, not a file.\n",
                      pszShortName, pszFilename );
            return CE_Failure;
        }
    }
}

/************************************************************************/
/*                             GDALDelete()                             */
/************************************************************************/

CPLErr GDALDeleteDataset( GDALDriverH hDriver, const char * pszFilename )

{
    return ((GDALDriver *) hDriver)->Delete( pszFilename );
}

/************************************************************************/
/*                       GDALGetDriverShortName()                       */
/************************************************************************/

const char * GDALGetDriverShortName( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;
    else
        return ((GDALDriver *) hDriver)->pszShortName;
}

/************************************************************************/
/*                       GDALGetDriverLongName()                        */
/************************************************************************/

const char * GDALGetDriverLongName( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;
    else
        return ((GDALDriver *) hDriver)->pszLongName;
}

/************************************************************************/
/*                       GDALGetDriverHelpTopic()                       */
/************************************************************************/

const char * GDALGetDriverHelpTopic( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;
    else
        return ((GDALDriver *) hDriver)->pszHelpTopic;
}

