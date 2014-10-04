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
 ****************************************************************************/

#include "ogrogdi.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGROGDIDataSource()                          */
/************************************************************************/

OGROGDIDataSource::OGROGDIDataSource()

{
    m_pszFullName = NULL;
    m_papoLayers = NULL;
    m_nLayers = 0;
    m_nClientID = -1;
    m_poSpatialRef = NULL;
    m_poCurrentLayer = NULL;
    m_bLaunderLayerNames =
            CSLTestBoolean(CPLGetConfigOption("OGR_OGDI_LAUNDER_LAYER_NAMES", "NO"));
}

/************************************************************************/
/*                         ~OGROGDIDataSource()                         */
/************************************************************************/

OGROGDIDataSource::~OGROGDIDataSource()

{
    CPLFree(m_pszFullName );

    for( int i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];
    CPLFree( m_papoLayers );

    if (m_nClientID != -1)
    {
        ecs_Result *psResult;

        psResult = cln_DestroyClient( m_nClientID );
        ecs_CleanUp( psResult );
    }

    if (m_poSpatialRef)
        m_poSpatialRef->Release();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROGDIDataSource::Open( const char * pszNewName, int bTestOpen )

{
    ecs_Result *psResult;
    char *pszFamily=NULL, *pszLyrName=NULL;
    char *pszWorkingName;

    CPLAssert( m_nLayers == 0 );
    
/* -------------------------------------------------------------------- */
/*      Parse the dataset name.                                         */
/*      i.e.                                                            */
/* gltp://<hostname>/<format>/<path_to_dataset>[:<layer_name>:<Family>] */
/*                                                                      */
/*      Where <Family> is one of: Line, Area, Point, and Text           */
/* -------------------------------------------------------------------- */
        if( !EQUALN(pszNewName,"gltp:",5) )
            return FALSE;

        pszWorkingName = CPLStrdup( pszNewName );

        pszFamily = strrchr(pszWorkingName, ':');

        // Don't treat drive name colon as family separator.  It is assumed
        // that drive names are on character long, and preceeded by a 
        // forward or backward slash.
        if( pszFamily < pszWorkingName+2 
            || pszFamily[-2] == '/'
            || pszFamily[-2] == '\\' )
            pszFamily = NULL;
        
        if (pszFamily && pszFamily != pszWorkingName + 4)
        {
            *pszFamily = '\0';
            pszFamily++;

            pszLyrName = strrchr(pszWorkingName, ':');
            if (pszLyrName == pszWorkingName + 4)
                pszLyrName = NULL;

            if( pszLyrName != NULL )
            {
                *pszLyrName = '\0';
                pszLyrName++;
            }
        }

/* -------------------------------------------------------------------- */
/*      Open the client interface.                                      */
/* -------------------------------------------------------------------- */
        psResult = cln_CreateClient(&m_nClientID, pszWorkingName);
        CPLFree( pszWorkingName );

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

        m_pszFullName = CPLStrdup(pszNewName);

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
/*      If an explicit layer was selected, just create that layer.      */
/* -------------------------------------------------------------------- */
        m_poCurrentLayer = NULL;

        if( pszLyrName != NULL )
        {
            ecs_Family  eFamily;

            if (EQUAL(pszFamily, "Line"))
                eFamily = Line;
            else if (EQUAL(pszFamily, "Area"))
                eFamily = Area;
            else if (EQUAL(pszFamily, "Point"))
                eFamily = Point;
            else if (EQUAL(pszFamily, "Text"))
                eFamily = Text;
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

            IAddLayer( pszLyrName, eFamily );
        }

/* -------------------------------------------------------------------- */
/*      Otherwise create a layer for every layer in the capabilities.   */
/* -------------------------------------------------------------------- */
        else
        {
            int         i;
            const ecs_LayerCapabilities *psLayerCap;

            for( i = 0; 
                (psLayerCap = cln_GetLayerCapabilities(m_nClientID,i)) != NULL;
                 i++ )
            {
                if( psLayerCap->families[Point] )
                    IAddLayer( psLayerCap->name, Point );
                if( psLayerCap->families[Line] )
                    IAddLayer( psLayerCap->name, Line );
                if( psLayerCap->families[Area] )
                    IAddLayer( psLayerCap->name, Area );
                if( psLayerCap->families[Text] )
                    IAddLayer( psLayerCap->name, Text );
            }
        }

        return TRUE;
}

/************************************************************************/
/*                             IAddLayer()                              */
/*                                                                      */
/*      Internal helper function for adding one existing layer to       */
/*      the datasource.                                                 */
/************************************************************************/

void OGROGDIDataSource::IAddLayer( const char *pszLayerName, 
                                   ecs_Family eFamily )

{
    m_papoLayers = (OGROGDILayer**)
        CPLRealloc( m_papoLayers, (m_nLayers+1) * sizeof(OGROGDILayer*));
    
    m_papoLayers[m_nLayers++] = new OGROGDILayer(this, pszLayerName, eFamily);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROGDIDataSource::TestCapability( CPL_UNUSED const char * pszCap )

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
