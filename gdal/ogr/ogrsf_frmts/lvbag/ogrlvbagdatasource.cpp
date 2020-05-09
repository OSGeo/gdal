/******************************************************************************
 *
 * Project:  LV BAG Translator
 * Purpose:  Implements OGRLVBAGDataSource.
 * Author:   Laixer B.V., info at laixer dot com
 *
 ******************************************************************************
 * Copyright (c) 2020, Laixer B.V. <info at laixer dot com>
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

#include "ogr_lvbag.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                          OGRLVBAGDataSource()                          */
/************************************************************************/

OGRLVBAGDataSource::OGRLVBAGDataSource() :
    poLayer{ nullptr },
    fp{ nullptr }
{}

/************************************************************************/
/*                         ~OGRLVBAGDataSource()                          */
/************************************************************************/

OGRLVBAGDataSource::~OGRLVBAGDataSource()
{
    if ( fp != nullptr )
    {
        VSIFCloseL(fp);
        fp = nullptr;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRLVBAGDataSource::Open( const char* pszFilename, int bUpdate,
    VSILFILE* fpIn)
{
    /** FUTURE: For now we're just read-only */
    if( bUpdate )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                "OGR/LVBAG driver does not support opening a file in "
                "update mode");
        return FALSE;
    }

    fp = fpIn;

    SetDescription(pszFilename);
    poLayer = std::unique_ptr<OGRLVBAGLayer>(new OGRLVBAGLayer(pszFilename, fp));

    return TRUE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRLVBAGDataSource::GetLayer( int iLayer )
{
    if( iLayer != 0 )
        return nullptr;

    return poLayer.get();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRLVBAGDataSource::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}
