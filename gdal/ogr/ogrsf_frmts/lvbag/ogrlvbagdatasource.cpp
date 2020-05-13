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
    papoLayers{ OGRLVBAG::LayerVector{} }
{}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRLVBAGDataSource::Open( const char* pszFilename )
{
    papoLayers.emplace_back(new OGRLVBAGLayer(pszFilename));

    return TRUE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRLVBAGDataSource::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= static_cast<int>(papoLayers.size()) )
        return nullptr;
    
    return papoLayers[iLayer].get();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRLVBAGDataSource::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}
