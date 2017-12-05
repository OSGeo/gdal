/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVFKDatasource class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2013-2016 Martin Landa <landa.martin gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include "ogr_vfk.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/*!
  \brief OGRVFKDataSource constructor
*/
OGRVFKDataSource::OGRVFKDataSource() :
    papoLayers(NULL),
    nLayers(0),
    pszName(NULL),
    poReader(NULL)
{}

/*!
  \brief OGRVFKDataSource destructor
*/
OGRVFKDataSource::~OGRVFKDataSource()
{
    CPLFree(pszName);

    if( poReader )
        delete poReader;

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree(papoLayers);
}

/*!
  \brief Open VFK datasource

  \param poOpenInfo open info

  \return TRUE on success or FALSE on failure
*/
int OGRVFKDataSource::Open(GDALOpenInfo* poOpenInfo)
{
    pszName = CPLStrdup(poOpenInfo->pszFilename);

    /* create VFK reader */
    poReader = CreateVFKReader(poOpenInfo->pszFilename);
    if (poReader == NULL || !poReader->IsValid()) {
        /*
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File %s appears to be VFK but the VFK reader can't"
                 "be instantiated",
                     pszFileName);
        */
        return FALSE;
    }

    /* read data blocks, i.e. &B */
    poReader->ReadDataBlocks();

    /* get list of layers */
    papoLayers = (OGRVFKLayer **) CPLCalloc(sizeof(OGRVFKLayer *), poReader->GetDataBlockCount());

    /* create layers from VFK blocks */
    for (int iLayer = 0; iLayer < poReader->GetDataBlockCount(); iLayer++) {
        papoLayers[iLayer] = CreateLayerFromBlock(poReader->GetDataBlock(iLayer));
        nLayers++;
    }

    if (CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_READ_ALL_BLOCKS", "YES"))) {
        /* read data records if requested */
        poReader->ReadDataRecords();

        for (int iLayer = 0; iLayer < poReader->GetDataBlockCount(); iLayer++) {
            /* load geometry */
            poReader->GetDataBlock(iLayer)->LoadGeometry();
        }
    }

    return TRUE;
}

/*!
  \brief Get VFK layer

  \param iLayer layer number

  \return pointer to OGRLayer instance or NULL on error
*/
OGRLayer *OGRVFKDataSource::GetLayer(int iLayer)
{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;

    return papoLayers[iLayer];
}

/*!
  \brief Test datasource capabilities

  \param pszCap capability

  \return TRUE if supported or FALSE if not supported
*/
int OGRVFKDataSource::TestCapability(const char * pszCap)
{
    if (EQUAL(pszCap, "IsPreProcessed") && poReader) {
        if (poReader->IsPreProcessed())
            return TRUE;
    }

    return FALSE;
}

/*!
  \brief Create OGR layer from VFKDataBlock

  \param poDataBlock pointer to VFKDataBlock instance

  \return pointer to OGRVFKLayer instance or NULL on error
*/
OGRVFKLayer *OGRVFKDataSource::CreateLayerFromBlock(const IVFKDataBlock *poDataBlock)
{
    /* create an empty layer */
    OGRVFKLayer *poLayer =
        new OGRVFKLayer(poDataBlock->GetName(), NULL,
                        poDataBlock->GetGeometryType(), this);

    /* define attributes (properties) */
    for (int iField = 0; iField < poDataBlock->GetPropertyCount(); iField++) {
        VFKPropertyDefn *poProperty = poDataBlock->GetProperty(iField);
        OGRFieldDefn oField(poProperty->GetName(), poProperty->GetType());

        if(poProperty->GetWidth() > 0)
            oField.SetWidth(poProperty->GetWidth());
        if(poProperty->GetPrecision() > 0)
            oField.SetPrecision(poProperty->GetPrecision());

        poLayer->GetLayerDefn()->AddFieldDefn(&oField);
    }

    return poLayer;
}
