/******************************************************************************
 *
 * Project:  SOSI Translator
 * Purpose:  Implements OGRSOSIDriver.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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

// Must be included before FYBA headers that mess with min() / max()
#include <mutex>

#include "ogr_sosi.h"

CPL_CVSID("$Id$")

static int bFYBAInit = FALSE;
static std::mutex oMutex;

/************************************************************************/
/*                          OGRSOSIInit()                               */
/************************************************************************/

static void OGRSOSIInit()
{
    std::lock_guard<std::mutex> oLock(oMutex);
    if ( !bFYBAInit )
    {
        LC_Init();  /* Init FYBA */
        SOSIInitTypes();
        bFYBAInit = TRUE;
    }
}

/************************************************************************/
/*                        OGRSOSIDriverUnload()                         */
/************************************************************************/

static void OGRSOSIDriverUnload(CPL_UNUSED GDALDriver* poDriver) {

    if ( bFYBAInit )
    {
        LC_Close(); /* Close FYBA */
        SOSICleanupTypes();
        bFYBAInit = FALSE;
    }
}

/************************************************************************/
/*                           Identify()                                 */
/************************************************************************/

static int OGRSOSIDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    if( poOpenInfo->fpL == nullptr ||
        strstr((const char*)poOpenInfo->pabyHeader, ".HODE") == nullptr )
        return FALSE;

    // TODO: add better identification
    return -1;
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

static GDALDataset *OGRSOSIDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( OGRSOSIDriverIdentify(poOpenInfo) == FALSE )
        return nullptr;

    OGRSOSIInit();

    OGRSOSIDataSource   *poDS = new OGRSOSIDataSource();
    if ( !poDS->Open( poOpenInfo->pszFilename, 0 ) ) {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

#ifdef WRITE_SUPPORT
/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRSOSIDriverCreate( const char * pszName,
                                         CPL_UNUSED int nBands, CPL_UNUSED int nXSize,
                                         CPL_UNUSED int nYSize, CPL_UNUSED GDALDataType eDT,
                                         CPL_UNUSED char **papszOptions )
{
    OGRSOSIInit();
    OGRSOSIDataSource   *poDS = new OGRSOSIDataSource();
    if ( !poDS->Create( pszName ) ) {
        delete poDS;
        return NULL;
    }
    return poDS;
}
#endif

/************************************************************************/
/*                         RegisterOGRSOSI()                            */
/************************************************************************/

void RegisterOGRSOSI() {
    if( GDALGetDriverByName( "SOSI" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SOSI" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Norwegian SOSI Standard" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/sosi.html" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,  "<OpenOptionList>"
    "<Option name='appendFieldsMap' type='string' description='Default is that all rows for equal field names will be appended in a feature, but with this parameter you select what field this should be valid for. With appendFieldsMap=f1&amp;f2, Append will be done for field f1 and f2 using a comma as delimiter.'/>"
    "</OpenOptionList>");


    poDriver->pfnOpen = OGRSOSIDriverOpen;
    poDriver->pfnIdentify = OGRSOSIDriverIdentify;
#ifdef WRITE_SUPPORT
    poDriver->pfnCreate = OGRSOSIDriverCreate;
#endif
    poDriver->pfnUnloadDriver = OGRSOSIDriverUnload;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
