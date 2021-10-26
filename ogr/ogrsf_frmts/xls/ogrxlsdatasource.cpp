/******************************************************************************
 *
 * Project:  XLS Translator
 * Purpose:  Implements OGRXLSDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

#include <freexl.h>

#ifdef _WIN32
#  include <windows.h>
#endif

#include "ogr_xls.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRXLSDataSource()                          */
/************************************************************************/

OGRXLSDataSource::OGRXLSDataSource() :
    pszName(nullptr),
    papoLayers(nullptr),
    nLayers(0),
    xlshandle(nullptr)
{}

/************************************************************************/
/*                         ~OGRXLSDataSource()                          */
/************************************************************************/

OGRXLSDataSource::~OGRXLSDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );

    if( xlshandle )
        freexl_close(xlshandle);
#ifdef WIN32
    if( m_osTempFilename.empty() )
    {
        VSIUnlink(m_osTempFilename);
    }
#endif
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRXLSDataSource::TestCapability( CPL_UNUSED const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRXLSDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRXLSDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = CPLStrdup(pszFilename);
    m_osANSIFilename = pszFilename;
#ifdef WIN32
    if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        char* pszTmpName = CPLRecode( pszFilename, CPL_ENC_UTF8, CPLString().Printf( "CP%d", GetACP() ) );
        CPLPopErrorHandler();
        m_osANSIFilename = pszTmpName;
        CPLFree(pszTmpName);
        
        // In case recoding to the ANSI code page failed, then create a temporary file
        // in a "safe" location
        if( CPLGetLastErrorType() != CE_None )
        {
            CPLErrorReset();

            // FIXME: CPLGenerateTempFilename() would normally be expected to return a UTF-8 filename
            // but I doubt it does in all cases.
            m_osTempFilename = CPLGenerateTempFilename("temp_xls");
            m_osANSIFilename = m_osTempFilename;
            CPLCopyFile( m_osANSIFilename, pszFilename );
            CPLDebug("XLS", "Create temporary file: %s", m_osTempFilename.c_str());
        }
    }
#endif

// --------------------------------------------------------------------
//      Does this appear to be a .xls file?
// --------------------------------------------------------------------

    /* Open only for getting info. To get cell values, we have to use freexl_open */
    if ( !GetXLSHandle() )
        return FALSE;

    unsigned int nSheets = 0;
    if (freexl_get_info (xlshandle, FREEXL_BIFF_SHEET_COUNT, &nSheets) != FREEXL_OK)
        return FALSE;

    for(unsigned short i=0; i<(unsigned short)nSheets; i++)
    {
        freexl_select_active_worksheet(xlshandle, i);

        const char* pszSheetname = nullptr;
        if (freexl_get_worksheet_name(xlshandle, i, &pszSheetname) != FREEXL_OK)
            return FALSE;

        unsigned int nRows = 0;
        unsigned short nCols = 0;
        if (freexl_worksheet_dimensions(xlshandle, &nRows, &nCols) != FREEXL_OK)
            return FALSE;

        /* Skip empty sheets */
        if (nRows == 0)
            continue;

        papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
        papoLayers[nLayers ++] = new OGRXLSLayer(this, pszSheetname, i, (int)nRows, nCols);
    }

    freexl_close(xlshandle);
    xlshandle = nullptr;

    return TRUE;
}

/************************************************************************/
/*                           GetXLSHandle()                             */
/************************************************************************/

const void* OGRXLSDataSource::GetXLSHandle()
{
    if (xlshandle)
        return xlshandle;

    if (freexl_open (m_osANSIFilename, &xlshandle) != FREEXL_OK)
        return nullptr;

    return xlshandle;
}
