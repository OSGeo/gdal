/******************************************************************************
 *
 * Project:  Arc/Info Generate Translator
 * Purpose:  Implements OGRARCGENLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMARCGENS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_arcgen.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRARCGENLayer()                             */
/************************************************************************/

OGRARCGENLayer::OGRARCGENLayer( const char* pszFilename,
                                VSILFILE* fpIn, OGRwkbGeometryType eType ) :
    poFeatureDefn(NULL),
    fp(fpIn),
    bEOF(false),
    nNextFID(0)
{
    poFeatureDefn = new OGRFeatureDefn( CPLGetBasename(pszFilename) );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eType );

    OGRFieldDefn oField1( "ID", OFTInteger);
    poFeatureDefn->AddFieldDefn( &oField1 );
    SetDescription( poFeatureDefn->GetName() );
}

/************************************************************************/
/*                            ~OGRARCGENLayer()                            */
/************************************************************************/

OGRARCGENLayer::~OGRARCGENLayer()

{
    poFeatureDefn->Release();

    VSIFCloseL( fp );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRARCGENLayer::ResetReading()

{
    nNextFID = 0;
    bEOF = false;
    VSIFSeekL( fp, 0, SEEK_SET );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRARCGENLayer::GetNextFeature()
{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRARCGENLayer::GetNextRawFeature()
{
    if (bEOF)
        return NULL;

    OGRwkbGeometryType eType = poFeatureDefn->GetGeomType();

    if (wkbFlatten(eType) == wkbPoint)
    {
        while( true )
        {
            const char* pszLine = CPLReadLine2L(fp,256,NULL);
            if (pszLine == NULL || EQUAL(pszLine, "END"))
            {
                bEOF = true;
                return NULL;
            }
            char** papszTokens = CSLTokenizeString2( pszLine, " ,", 0 );
            int nTokens = CSLCount(papszTokens);
            if (nTokens == 3 || nTokens == 4)
            {
                OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
                poFeature->SetFID(nNextFID ++);
                poFeature->SetField(0, papszTokens[0]);
                if (nTokens == 3)
                    poFeature->SetGeometryDirectly(
                        new OGRPoint(CPLAtof(papszTokens[1]),
                                     CPLAtof(papszTokens[2])));
                else
                    poFeature->SetGeometryDirectly(
                        new OGRPoint(CPLAtof(papszTokens[1]),
                                     CPLAtof(papszTokens[2]),
                                     CPLAtof(papszTokens[3])));
                CSLDestroy(papszTokens);
                return poFeature;
            }

            CSLDestroy(papszTokens);
        }
    }

    CPLString osID;
    const bool bIsPoly = (wkbFlatten(eType) == wkbPolygon);
    OGRLineString* poLS = static_cast<OGRLineString*>(
        OGRGeometryFactory::createGeometry( (bIsPoly) ? wkbLinearRing : wkbLineString ));
    while( true )
    {
        const char* pszLine = CPLReadLine2L(fp,256,NULL);
        if (pszLine == NULL)
            break;

        if (EQUAL(pszLine, "END"))
        {
            if (osID.empty())
                break;

            OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
            poFeature->SetFID(nNextFID ++);
            poFeature->SetField(0, osID.c_str());
            if( bIsPoly )
            {
                OGRPolygon* poPoly = new OGRPolygon();
                poPoly->addRingDirectly((OGRLinearRing*)poLS);
                poFeature->SetGeometryDirectly(poPoly);
            }
            else
                poFeature->SetGeometryDirectly(poLS);
            return poFeature;
        }

        char** papszTokens = CSLTokenizeString2( pszLine, " ,", 0 );
        int nTokens = CSLCount(papszTokens);
        if (osID.empty())
        {
            if (nTokens >= 1)
                osID = papszTokens[0];
            else
            {
                CSLDestroy(papszTokens);
                break;
            }
        }
        else
        {
            if (nTokens == 2)
            {
                poLS->addPoint(CPLAtof(papszTokens[0]),
                               CPLAtof(papszTokens[1]));
            }
            else if (nTokens == 3)
            {
                poLS->addPoint(CPLAtof(papszTokens[0]),
                               CPLAtof(papszTokens[1]),
                               CPLAtof(papszTokens[2]));
            }
            else
            {
                CSLDestroy(papszTokens);
                break;
            }
        }
        CSLDestroy(papszTokens);
    }

    bEOF = true;
    delete poLS;
    return NULL;
}
/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRARCGENLayer::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}
