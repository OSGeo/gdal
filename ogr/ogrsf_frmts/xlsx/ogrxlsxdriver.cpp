/******************************************************************************
 * $Id$
 *
 * Project:  XLSX Translator
 * Purpose:  Implements OGRXLSXDriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_xlsx.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

extern "C" void RegisterOGRXLSX();

// g++ -DHAVE_EXPAT -g -Wall -fPIC ogr/ogrsf_frmts/xlsx/*.cpp -shared -o ogr_XLSX.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/mem -Iogr/ogrsf_frmts/xlsx -L. -lgdal

/************************************************************************/
/*                           ~OGRXLSXDriver()                            */
/************************************************************************/

OGRXLSXDriver::~OGRXLSXDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRXLSXDriver::GetName()

{
    return "XLSX";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

#define XLSX_MIMETYPE "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"

OGRDataSource *OGRXLSXDriver::Open( const char * pszFilename, int bUpdate )

{
    if (!EQUAL(CPLGetExtension(pszFilename), "XLSX"))
        return NULL;

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return NULL;

    int bOK = FALSE;
    char szBuffer[2048];
    if (VSIFReadL(szBuffer, sizeof(szBuffer), 1, fp) == 1 &&
        memcmp(szBuffer, "PK", 2) == 0)
    {
        bOK = TRUE;
    }

    VSIFCloseL(fp);

    if (!bOK)
        return NULL;

    VSILFILE* fpContent = VSIFOpenL(CPLSPrintf("/vsizip/%s/[Content_Types].xml", pszFilename), "rb");
    if (fpContent == NULL)
        return NULL;

    int nRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fpContent);
    szBuffer[nRead] = 0;

    VSIFCloseL(fpContent);

    if (strstr(szBuffer, XLSX_MIMETYPE) == NULL)
        return NULL;

    VSILFILE* fpWorkbook = VSIFOpenL(CPLSPrintf("/vsizip/%s/xl/workbook.xml", pszFilename), "rb");
    if (fpWorkbook == NULL)
        return NULL;

    VSILFILE* fpSharedStrings = VSIFOpenL(CPLSPrintf("/vsizip/%s/xl/sharedStrings.xml", pszFilename), "rb");
    VSILFILE* fpStyles = VSIFOpenL(CPLSPrintf("/vsizip/%s/xl/styles.xml", pszFilename), "rb");

    OGRXLSXDataSource   *poDS = new OGRXLSXDataSource();

    if( !poDS->Open( pszFilename, fpWorkbook, fpSharedStrings, fpStyles, bUpdate ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRXLSXDriver::CreateDataSource( const char * pszName,
                                                char **papszOptions )

{
    if (!EQUAL(CPLGetExtension(pszName), "XLSX"))
    {
        CPLError( CE_Failure, CPLE_AppDefined, "File extension should be XLSX" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create datasource.                                       */
/* -------------------------------------------------------------------- */
    OGRXLSXDataSource     *poDS;

    poDS = new OGRXLSXDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                         DeleteDataSource()                           */
/************************************************************************/

OGRErr OGRXLSXDriver::DeleteDataSource( const char *pszName )
{
    if (VSIUnlink( pszName ) == 0)
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRXLSXDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRXLSX()                           */
/************************************************************************/

void RegisterOGRXLSX()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRXLSXDriver );
}

