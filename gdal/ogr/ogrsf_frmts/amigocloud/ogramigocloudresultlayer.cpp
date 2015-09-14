/******************************************************************************
 * $Id
 *
 * Project:  AmigoCloud Translator
 * Purpose:  Implements OGRAMIGOCLOUDResultLayer class.
 * Author:   Victor Chernetsky, <victor at amigocloud dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Victor Chernetsky, <victor at amigocloud dot com>
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

#include "ogr_amigocloud.h"

CPL_CVSID("$Id");

/************************************************************************/
/*                          OGRAMIGOCLOUDResultLayer()                     */
/************************************************************************/

OGRAMIGOCLOUDResultLayer::OGRAMIGOCLOUDResultLayer( OGRAMIGOCLOUDDataSource* poDS,
                                              const char * pszRawQueryIn ) :
                                              OGRAMIGOCLOUDLayer(poDS)
{
    osBaseSQL = pszRawQueryIn;
    SetDescription( "result" );
    poFirstFeature = NULL;
}

/************************************************************************/
/*                       ~OGRAMIGOCLOUDResultLayer()                       */
/************************************************************************/

OGRAMIGOCLOUDResultLayer::~OGRAMIGOCLOUDResultLayer()

{
    delete poFirstFeature;
}

/************************************************************************/
/*                          GetLayerDefnInternal()                      */
/************************************************************************/

OGRFeatureDefn * OGRAMIGOCLOUDResultLayer::GetLayerDefnInternal(json_object* poObjIn)
{
    if( poFeatureDefn != NULL )
        return poFeatureDefn;

    EstablishLayerDefn("result", poObjIn);

    return poFeatureDefn;
}

/************************************************************************/
/*                           GetNextRawFeature()                        */
/************************************************************************/

OGRFeature  *OGRAMIGOCLOUDResultLayer::GetNextRawFeature()
{
    if( poFirstFeature )
    {
        OGRFeature* poRet = poFirstFeature;
        poFirstFeature = NULL;
        return poRet;
    }
    else
        return OGRAMIGOCLOUDLayer::GetNextRawFeature();
}

/************************************************************************/
/*                                IsOK()                                */
/************************************************************************/

int  OGRAMIGOCLOUDResultLayer::IsOK()
{
    CPLErrorReset();
    poFirstFeature = GetNextFeature();
    return CPLGetLastErrorType() == 0;
}

/************************************************************************/
/*                             GetSRS_SQL()                             */
/************************************************************************/

CPLString OGRAMIGOCLOUDResultLayer::GetSRS_SQL(const char* pszGeomCol)
{
    CPLString osSQL;
    CPLString osLimitedSQL;

    size_t nPos = osBaseSQL.ifind(" LIMIT ");
    if( nPos != std::string::npos )
    {
        osLimitedSQL = osBaseSQL;
        size_t nSize = osLimitedSQL.size();
        for(size_t i = nPos + strlen(" LIMIT "); i < nSize; i++)
        {
            if( osLimitedSQL[i] == ' ' && osLimitedSQL[i-1] == '0')
            {
                osLimitedSQL[i-1] = '1';
                break;
            }
            osLimitedSQL[i] = '0';
        }
    }
    else
        osLimitedSQL.Printf("%s LIMIT 1", osBaseSQL.c_str());

    /* Assuming that the SRID of the first non-NULL geometry applies */
    /* to geometries of all rows. */
    osSQL.Printf("SELECT srid, srtext FROM spatial_ref_sys WHERE srid IN "
                "(SELECT ST_SRID(%s) FROM (%s) ogr_subselect)",
                OGRAMIGOCLOUDEscapeIdentifier(pszGeomCol).c_str(),
                osLimitedSQL.c_str());

    return osSQL;
}
