/******************************************************************************
 * $Id$
 *
 * Project:  OGDI Bridge
 * Purpose:  Implements OGROGDIDataSource class.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *           (Based on some code contributed by Frank Warmerdam :)
 *
 ******************************************************************************
 * Copyright (c) 2000, Daniel Morissette
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2000/08/30 01:36:57  danmo
 * Added GetSpatialRef() support
 *
 * Revision 1.1  2000/08/24 04:16:19  danmo
 * Initial revision
 *
 */

#include "ogrogdi.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                         OGROGDIDataSource()                          */
/************************************************************************/

OGROGDIDataSource::OGROGDIDataSource()

{
    m_pszFullName = NULL;
    m_pszURL = NULL;
    m_pszOGDILayerName = NULL;
    m_papoLayers = NULL;
    m_nLayers = 0;
    m_nClientID = -1;
    m_poSpatialRef = NULL;
}

/************************************************************************/
/*                         ~OGROGDIDataSource()                         */
/************************************************************************/

OGROGDIDataSource::~OGROGDIDataSource()

{
    CPLFree(m_pszFullName );
    CPLFree(m_pszURL);
    CPLFree(m_pszOGDILayerName);

    for( int i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];
    CPLFree( m_papoLayers );

    if (m_nClientID != -1)
        cln_DestroyClient( m_nClientID );

    if (m_poSpatialRef)
        delete m_poSpatialRef;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROGDIDataSource::Open( const char * pszNewName, int bTestOpen )

{
    ecs_Result *psResult;
    char *pszFamily=NULL, *pszLyrName=NULL;

    CPLAssert( m_nLayers == 0 );
    
/* -------------------------------------------------------------------- */
/*      Parse the dataset name.                                         */
/*                                                                      */
/*      Since I don't know of any way to parse the dictionary to        */
/*      retrieve the list of layers, the current implementation         */
/*      expects the layer of interest to be included at the end of      */
/*      the gltp URL,                                                   */
/*      i.e.                                                            */
/*      gltp://<hostname>/<format>/<path_to_dataset>:<layer_name>:<Family>*/
/*                                                                      */
/*      Where <Family> is one of: Line, Area, Point, and Text           */
/* -------------------------------------------------------------------- */

    if( !EQUALN(pszNewName,"gltp:",5) )
        return FALSE;

    m_pszFullName = CPLStrdup( pszNewName );
    pszFamily = strrchr(m_pszFullName, ':');
    if (pszFamily)
    {
        *pszFamily = '\0';
        pszLyrName = strrchr(m_pszFullName, ':');
    }

    if (pszLyrName == NULL || pszLyrName == m_pszFullName + 4)
    {
        if (!bTestOpen)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Incomplete OGDI Dataset Name (%s). "
                      "OGR needs layer name and feature family at end of URL: "
    "gltp://<hostname>/<format>/<path_to_dataset>:<layer_name>:<Family>\n",
                      m_pszFullName);
        }
        return FALSE;
    }

    *pszLyrName = '\0';
    m_pszURL = CPLStrdup(m_pszFullName);
    *pszLyrName = ':';
    m_pszOGDILayerName = CPLStrdup(pszLyrName+1);
    *pszFamily = ':';
    pszFamily++;

    if (EQUAL(pszFamily, "Line"))
        m_eFamily = Line;
    else if (EQUAL(pszFamily, "Area"))
        m_eFamily = Area;
    else if (EQUAL(pszFamily, "Point"))
        m_eFamily = Point;
    else if (EQUAL(pszFamily, "Text"))
        m_eFamily = Text;
    else
    {
        if (!bTestOpen)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid or unsupported family name (%s) in URL %s\n",
                      pszFamily, m_pszFullName);
        }
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Open the client interface.                                      */
/* -------------------------------------------------------------------- */
    psResult = cln_CreateClient(&m_nClientID, m_pszURL);
    if( ECSERROR( psResult ) )
    {
        if (!bTestOpen)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "OGDI DataSource Open Failed: %s\n",
                      psResult->message );
        }
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file.                         */
/* -------------------------------------------------------------------- */
    psResult = cln_GetGlobalBound( m_nClientID );
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return FALSE;
    }

    m_sGlobalBounds = ECSREGION(psResult);

    psResult = cln_GetServerProjection(m_nClientID);
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return FALSE;
    }

    m_poSpatialRef = new OGRSpatialReference;

    if( m_poSpatialRef->importFromProj4( ECSTEXT(psResult) ) != OGRERR_NONE )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "untranslatable PROJ.4 projection: %s\n", 
                  ECSTEXT(psResult) );
        delete m_poSpatialRef;
        m_poSpatialRef = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Select the global region.                                       */
/* -------------------------------------------------------------------- */
    psResult = cln_SelectRegion( m_nClientID, &m_sGlobalBounds );
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Create layer.                                                   */
/*                                                                      */
/*      Since OGDI forces us to set a global active layer and           */
/*      family, the current implementation will support only one        */
/*      layer at a time.                                                */
/* -------------------------------------------------------------------- */
    m_papoLayers = (OGROGDILayer**)CPLCalloc(1, sizeof(OGROGDILayer*));

    m_papoLayers[m_nLayers++] = new OGROGDILayer(m_nClientID, 
                                                 m_pszOGDILayerName,
                                                 m_eFamily, &m_sGlobalBounds,
                                                 m_poSpatialRef);

    return TRUE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROGDIDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGROGDIDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= m_nLayers )
        return NULL;
    else
        return m_papoLayers[iLayer];
}
