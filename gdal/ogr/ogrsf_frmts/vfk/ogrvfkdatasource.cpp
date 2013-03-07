/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVFKDatasource class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2013 Martin Landa <landa.martin gmail.com>
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

CPL_CVSID("$Id$");

/*!
  \brief OGRVFKDataSource constructor
*/
OGRVFKDataSource::OGRVFKDataSource()
{
    pszName    = NULL;
    
    poReader   = NULL;
    
    papoLayers = NULL;
    nLayers    = 0;
}

/*!
  \brief OGRVFKDataSource destructor
*/
OGRVFKDataSource::~OGRVFKDataSource()
{
    CPLFree(pszName);
    
    if (poReader)
        delete poReader;
    
    for(int i = 0; i < nLayers; i++)
        delete papoLayers[i];

    CPLFree(papoLayers);
}

/*!
  \brief Open VFK datasource

  \param pszNewName datasource name
  \param bTestOpen True to test if datasource is possible to open

  \return TRUE on success or FALSE on failure
*/
int OGRVFKDataSource::Open(const char *pszNewName, int bTestOpen)
{
    FILE * fp;
    char   szHeader[1000];
    
    /* open the source file */
    fp = VSIFOpen(pszNewName, "r");
    if (fp == NULL) {
        if (!bTestOpen)
            CPLError(CE_Failure, CPLE_OpenFailed, 
                     "Failed to open VFK file `%s'",
                     pszNewName);
        
        return FALSE;
    }

   /* If we aren't sure it is VFK, load a header chunk and check    
      for signs it is VFK */
    if (bTestOpen) {
        size_t nRead = VSIFRead(szHeader, 1, sizeof(szHeader), fp);
        if (nRead <= 0) {
            VSIFClose(fp);
            return FALSE;
        }
        szHeader[MIN(nRead, sizeof(szHeader))-1] = '\0';
        
        // TODO: improve check
        if (strncmp(szHeader, "&HVERZE;", 8) != 0) {
            VSIFClose(fp);
            return FALSE;
        } 
    }

    /* We assume now that it is VFK. Close and instantiate a
       VFKReader on it. */
    VSIFClose(fp);

    pszName = CPLStrdup(pszNewName);
    
    poReader = CreateVFKReader(pszNewName);
    if (poReader == NULL) {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "File %s appears to be VFK but the VFK reader can't"
                 "be instantiated",
                 pszNewName);
        return FALSE;
    }

#ifndef HAVE_SQLITE
    CPLError(CE_Warning, CPLE_AppDefined, 
             "GDAL is not compiled with SQLite support. "
             "VFK driver may not work properly.");
#endif
    
    /* read data blocks, i.e. &B */
    poReader->ReadDataBlocks();
    
    /* get list of layers */
    papoLayers = (OGRVFKLayer **) CPLCalloc(sizeof(OGRVFKLayer *), poReader->GetDataBlockCount());
    
    for (int iLayer = 0; iLayer < poReader->GetDataBlockCount(); iLayer++) {
        papoLayers[iLayer] = CreateLayerFromBlock(poReader->GetDataBlock(iLayer));
        nLayers++;
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
  \brief Test datasource capabilies

  \param pszCap capability

  \return TRUE if supported or FALSE if not supported
*/
int OGRVFKDataSource::TestCapability(const char * pszCap)
{
    return FALSE;
}

/*!
  \brief Create OGR layer from VFKDataBlock

  \param poDataBlock pointer to VFKDataBlock instance

  \return pointer to OGRVFKLayer instance or NULL on error
*/
OGRVFKLayer *OGRVFKDataSource::CreateLayerFromBlock(const IVFKDataBlock *poDataBlock)
{
    OGRVFKLayer *poLayer;

    poLayer = NULL;

    /* create an empty layer */
    poLayer = new OGRVFKLayer(poDataBlock->GetName(), NULL,
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
