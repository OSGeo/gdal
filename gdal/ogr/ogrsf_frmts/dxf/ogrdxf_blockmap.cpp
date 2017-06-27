/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements BlockMap reading and management portion of
 *           OGRDXFDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          ReadBlockSection()                          */
/************************************************************************/

bool OGRDXFDataSource::ReadBlocksSection()

{
    OGRDXFLayer *poReaderLayer = static_cast<OGRDXFLayer *>(
        GetLayerByName( "Entities" ));
    const bool bMergeBlockGeometries = CPLTestBool(
        CPLGetConfigOption( "DXF_MERGE_BLOCK_GEOMETRIES", "TRUE" ) );

    iEntitiesSectionOffset = oReader.iSrcBufferFileOffset + oReader.iSrcBufferOffset;

    char szLineBuf[257];
    int nCode = 0;
    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1
           && !EQUAL(szLineBuf,"ENDSEC") )
    {
        // We are only interested in extracting blocks.
        if( nCode != 0 || !EQUAL(szLineBuf,"BLOCK") )
            continue;

        // Process contents of BLOCK definition till we find the
        // first entity.
        CPLString osBlockName;

        while( (nCode = ReadValue( szLineBuf,sizeof(szLineBuf) )) > 0 )
        {
            if( nCode == 2 )
                osBlockName = szLineBuf;

            // anything else we want?
        }
        if( nCode < 0 )
        {
            DXF_READER_ERROR();
            return false;
        }

        if( EQUAL(szLineBuf,"ENDBLK") )
            continue;

        if (nCode >= 0)
            UnreadValue();

        if( oBlockMap.find(osBlockName) != oBlockMap.end() )
        {
            DXF_READER_ERROR();
            return false;
        }

        // Now we will process entities till we run out at the ENDBLK code.
        // we aggregate the geometries of the features into a multi-geometry,
        // but throw away other stuff attached to the features.

        OGRFeature *poFeature = NULL;
        OGRGeometryCollection *poColl = new OGRGeometryCollection();
        std::vector<OGRFeature*> apoFeatures;

        while( (poFeature = poReaderLayer->GetNextUnfilteredFeature()) != NULL )
        {
            if( (poFeature->GetStyleString() != NULL
                 && strstr(poFeature->GetStyleString(),"LABEL") != NULL)
                || !bMergeBlockGeometries )
            {
                apoFeatures.push_back( poFeature );
            }
            else
            {
                OGRGeometry* poSubGeom = poFeature->StealGeometry();
                if( poSubGeom )
                    poColl->addGeometryDirectly( poSubGeom );
                delete poFeature;
            }
        }

        if( poColl->getNumGeometries() == 0 )
            delete poColl;
        else
            oBlockMap[osBlockName].poGeometry = SimplifyBlockGeometry(poColl);

        if( !apoFeatures.empty() )
            oBlockMap[osBlockName].apoFeatures = apoFeatures;
    }
    if( nCode < 0 )
    {
        DXF_READER_ERROR();
        return false;
    }

    CPLDebug( "DXF", "Read %d blocks with meaningful geometry.",
              (int) oBlockMap.size() );
    return true;
}

/************************************************************************/
/*                       SimplifyBlockGeometry()                        */
/************************************************************************/

OGRGeometry *OGRDXFDataSource::SimplifyBlockGeometry(
    OGRGeometryCollection *poCollection )

{
/* -------------------------------------------------------------------- */
/*      If there is only one geometry in the collection, just return    */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    if( poCollection->getNumGeometries() == 1 )
    {
        OGRGeometry *poReturn = poCollection->getGeometryRef(0);
        poCollection->removeGeometry(0, FALSE);
        delete poCollection;
        return poReturn;
    }

/* -------------------------------------------------------------------- */
/*      Convert to polygon, multipolygon, multilinestring or multipoint */
/* -------------------------------------------------------------------- */

    OGRwkbGeometryType eType =
                wkbFlatten(poCollection->getGeometryRef(0)->getGeometryType());
    int i;
    for(i=1;i<poCollection->getNumGeometries();i++)
    {
        if (wkbFlatten(poCollection->getGeometryRef(i)->getGeometryType())
            != eType)
        {
            eType = wkbUnknown;
            break;
        }
    }
    if (eType == wkbPoint || eType == wkbLineString)
    {
        OGRGeometryCollection* poNewColl;
        if (eType == wkbPoint)
            poNewColl = new OGRMultiPoint();
        else
            poNewColl = new OGRMultiLineString();
        while(poCollection->getNumGeometries() > 0)
        {
            OGRGeometry *poGeom = poCollection->getGeometryRef(0);
            poCollection->removeGeometry(0,FALSE);
            poNewColl->addGeometryDirectly(poGeom);
        }
        delete poCollection;
        return poNewColl;
    }
    else if (eType == wkbPolygon)
    {
        std::vector<OGRGeometry*> aosPolygons;
        while(poCollection->getNumGeometries() > 0)
        {
            OGRGeometry *poGeom = poCollection->getGeometryRef(0);
            poCollection->removeGeometry(0,FALSE);
            aosPolygons.push_back(poGeom);
        }
        delete poCollection;
        int bIsValidGeometry;
        return OGRGeometryFactory::organizePolygons(
            &aosPolygons[0], (int)aosPolygons.size(),
            &bIsValidGeometry, NULL);
    }

    return poCollection;
}

/************************************************************************/
/*                            LookupBlock()                             */
/*                                                                      */
/*      Find the geometry collection corresponding to a name if it      */
/*      exists.  Note that the returned geometry pointer is to a        */
/*      geometry that continues to be owned by the datasource.  It      */
/*      should be cloned for use.                                       */
/************************************************************************/

DXFBlockDefinition *OGRDXFDataSource::LookupBlock( const char *pszName )

{
    CPLString l_osName = pszName;

    if( oBlockMap.count( l_osName ) == 0 )
        return NULL;
    else
        return &(oBlockMap[l_osName]);
}

/************************************************************************/
/*                        ~DXFBlockDefinition()                         */
/*                                                                      */
/*      Safe cleanup of a block definition.                             */
/************************************************************************/

DXFBlockDefinition::~DXFBlockDefinition()

{
    delete poGeometry;

    while( !apoFeatures.empty() )
    {
        delete apoFeatures.back();
        apoFeatures.pop_back();
    }
}
