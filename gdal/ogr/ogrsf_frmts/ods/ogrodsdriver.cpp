/******************************************************************************
 * $Id$
 *
 * Project:  ODS Translator
 * Purpose:  Implements OGRODSDriver.
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

#include "ogr_ods.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

extern "C" void RegisterOGRODS();

// g++ -DHAVE_EXPAT -g -Wall -fPIC ogr/ogrsf_frmts/ods/*.cpp -shared -o ogr_ODS.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/mem -Iogr/ogrsf_frmts/ods -L. -lgdal

/************************************************************************/
/*                           ~OGRODSDriver()                            */
/************************************************************************/

OGRODSDriver::~OGRODSDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRODSDriver::GetName()

{
    return "ODS";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

#define ODS_MIMETYPE "application/vnd.oasis.opendocument.spreadsheet"

OGRDataSource *OGRODSDriver::Open( const char * pszFilename, int bUpdate )

{
    if (bUpdate)
    {
        return NULL;
    }

    CPLString osContentFilename;
    const char* pszContentFilename = pszFilename;

    VSILFILE* fpContent = NULL;

    if (EQUAL(CPLGetExtension(pszFilename), "ODS"))
    {
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (fp == NULL)
            return NULL;

        int bOK = FALSE;
        char szBuffer[1024];
        if (VSIFReadL(szBuffer, sizeof(szBuffer), 1, fp) == 1 &&
            memcmp(szBuffer, "PK", 2) == 0)
        {
            for(size_t i=0;i< sizeof(szBuffer) - strlen(ODS_MIMETYPE); i++)
            {
                if (memcmp(szBuffer + i, ODS_MIMETYPE, strlen(ODS_MIMETYPE)) == 0)
                {
                    bOK = TRUE;
                    break;
                }
            }
        }

        VSIFCloseL(fp);

        if (!bOK)
            return NULL;

        osContentFilename.Printf("/vsizip/%s/content.xml", pszFilename);
        pszContentFilename = osContentFilename.c_str();
    }

    if (EQUAL(CPLGetFilename(pszContentFilename), "content.xml"))
    {
        fpContent = VSIFOpenL(pszContentFilename, "rb");
        if (fpContent == NULL)
            return NULL;

        char szBuffer[1024];
        int nRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fpContent);
        szBuffer[nRead] = 0;

        if (strstr(szBuffer, "<office:document-content") == NULL)
        {
            VSIFCloseL(fpContent);
            return NULL;
        }

        /* We could also check that there's a <office:spreadsheet>, but it might be further */
        /* in the XML due to styles, etc... */
    }
    else
    {
        return NULL;
    }

    OGRODSDataSource   *poDS = new OGRODSDataSource();

    if( !poDS->Open( pszFilename, fpContent, bUpdate ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODSDriver::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRODS()                           */
/************************************************************************/

void RegisterOGRODS()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRODSDriver );
}

