/**********************************************************************
 * $Id: mitab_ogr_datasource.cpp,v 1.6 2003/03/21 14:20:49 warmerda Exp $
 *
 * Name:     mitab_ogr_datasource.cpp
 * Project:  MapInfo Mid/Mif, Tab ogr support
 * Language: C++
 * Purpose:  Implementation of OGRTABDataSource.
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *           and Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Stephane Villeneuve
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log: mitab_ogr_datasource.cpp,v $
 * Revision 1.6  2003/03/21 14:20:49  warmerda
 * fixed email
 *
 * Revision 1.5  2002/02/08 16:52:16  warmerda
 * added support for FORMAT=MIF option for creating layers
 *
 * Revision 1.4  2001/02/06 22:13:54  warmerda
 * fixed memory leak in OGRTABDataSource::CreateLayer()
 *
 * Revision 1.3  2001/01/22 16:03:58  warmerda
 * expanded tabs
 *
 * Revision 1.2  2000/07/04 01:46:23  warmerda
 * Avoid warnings on unused arguments.
 *
 * Revision 1.1  2000/01/26 18:17:09  warmerda
 * New
 *
 **********************************************************************/

#include "mitab_ogr_driver.h"


/*=======================================================================
 *                 OGRTABDataSource
 *
 * We need one single OGRDataSource/Driver set of classes to handle all
 * the MapInfo file types.  They all deal with the IMapInfoFile abstract
 * class.
 *=====================================================================*/

/************************************************************************/
/*                         OGRTABDataSource()                           */
/************************************************************************/

OGRTABDataSource::OGRTABDataSource()

{
    m_pszName = NULL;
    m_pszDirectory = NULL;
    m_nLayerCount = 0;
    m_papoLayers = NULL;
    m_papszOptions = NULL;
    m_bCreateMIF = FALSE;
}

/************************************************************************/
/*                         ~OGRTABDataSource()                          */
/************************************************************************/

OGRTABDataSource::~OGRTABDataSource()

{
    CPLFree( m_pszName ); 
    CPLFree( m_pszDirectory );

    for( int i = 0; i < m_nLayerCount; i++ )
        delete m_papoLayers[i];

    CPLFree( m_papoLayers );
    CSLDestroy( m_papszOptions );
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new dataset (directory or file).                       */
/************************************************************************/

int OGRTABDataSource::Create( const char * pszName, char **papszOptions )

{
    VSIStatBuf  sStat;

    CPLAssert( m_pszName == NULL );
    
    m_pszName = CPLStrdup( pszName );
    m_papszOptions = CSLDuplicate( papszOptions );

    if( CSLFetchNameValue(papszOptions,"FORMAT") != NULL 
        && EQUAL(CSLFetchNameValue(papszOptions,"FORMAT"),"MIF") )
        m_bCreateMIF = TRUE;
    else if( EQUAL(CPLGetExtension(pszName),"mif")
             || EQUAL(CPLGetExtension(pszName),"mid") )
        m_bCreateMIF = TRUE;

/* -------------------------------------------------------------------- */
/*      Create a new empty directory.                                   */
/* -------------------------------------------------------------------- */
    if( strlen(CPLGetExtension(pszName)) == 0 )
    {
        if( VSIStat( pszName, &sStat ) == 0 )
        {
            if( !VSI_ISDIR(sStat.st_mode) )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Attempt to create dataset named %s,\n"
                          "but that is an existing file.\n",
                          pszName );
                return FALSE;
            }
        }
        else
        {
            if( VSIMkdir( pszName, 0755 ) != 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to create directory %s.\n",
                          pszName );
                return FALSE;
            }
        }
        
        m_pszDirectory = CPLStrdup(pszName);
    }

/* -------------------------------------------------------------------- */
/*      Create a new single file.                                       */
/* -------------------------------------------------------------------- */
    else
    {
        IMapInfoFile    *poFile;

        if( m_bCreateMIF )
            poFile = new MIFFile;
        else
            poFile = new TABFile;

        if( poFile->Open( pszName, "wb", FALSE ) != 0 )
        {
            delete poFile;
            return FALSE;
        }
        
        poFile->SetBounds( -30000000, -15000000, 30000000, 15000000 );
        
        m_nLayerCount = 1;
        m_papoLayers = (IMapInfoFile **) CPLMalloc(sizeof(void*));
        m_papoLayers[0] = poFile;
        
        m_pszDirectory = CPLStrdup( CPLGetPath(pszName) );
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open an existing file, or directory of files.                   */
/************************************************************************/

int OGRTABDataSource::Open( const char * pszName, int bTestOpen )

{
    VSIStatBuf  stat;

    CPLAssert( m_pszName == NULL );
    
    m_pszName = CPLStrdup( pszName );

/* -------------------------------------------------------------------- */
/*      Is this a file or directory?                                    */
/* -------------------------------------------------------------------- */
    if( VSIStat( pszName, &stat ) != 0 
        || (!VSI_ISDIR(stat.st_mode) && !VSI_ISREG(stat.st_mode)) )
    {
        if( !bTestOpen )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "%s is not a file or directory.\n"
                      "Unable to open as a Mapinfo dataset.\n",
                      pszName );
        }

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If it is a file, try to open as a Mapinfo file.                 */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(stat.st_mode) )
    {
        IMapInfoFile    *poFile;

        poFile = IMapInfoFile::SmartOpen( pszName, bTestOpen );
        if( poFile == NULL )
            return FALSE;

        m_nLayerCount = 1;
        m_papoLayers = (IMapInfoFile **) CPLMalloc(sizeof(void*));
        m_papoLayers[0] = poFile;

        m_pszDirectory = CPLStrdup( CPLGetPath(pszName) );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, we need to scan the whole directory for files        */
/*      ending in .tab or .mif.                                         */
/* -------------------------------------------------------------------- */
    else
    {
        char    **papszFileList = CPLReadDir( pszName );
        
        m_pszDirectory = CPLStrdup( pszName );

        for( int iFile = 0;
             papszFileList != NULL && papszFileList[iFile] != NULL;
             iFile++ )
        {
            IMapInfoFile *poFile;
            const char  *pszExtension = CPLGetExtension(papszFileList[iFile]);
            char        *pszSubFilename;

            if( !EQUAL(pszExtension,"tab") && !EQUAL(pszExtension,"mif") )
                continue;

            pszSubFilename = CPLStrdup(
                CPLFormFilename( m_pszDirectory, papszFileList[iFile], NULL ));

            poFile = IMapInfoFile::SmartOpen( pszSubFilename, bTestOpen );
            CPLFree( pszSubFilename );
            
            if( poFile == NULL )
                return FALSE;

            m_nLayerCount++;
            m_papoLayers = (IMapInfoFile **)
                CPLRealloc(m_papoLayers,sizeof(void*)*m_nLayerCount);
            m_papoLayers[m_nLayerCount-1] = poFile;
        }

        if( m_nLayerCount == 0 )
        {
            if( !bTestOpen )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "No mapinfo files found in directory %s.\n",
                          m_pszDirectory );
            
            return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRTABDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= m_nLayerCount )
        return NULL;
    else
        return m_papoLayers[iLayer];
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRTABDataSource::CreateLayer( const char * pszLayerName,
                               OGRSpatialReference *poSRSIn,
                               OGRwkbGeometryType /* eGeomTypeIn */,
                               char ** /* papszOptions */ )

{
    IMapInfoFile        *poFile;
    char                *pszFullFilename;

    if( m_bCreateMIF )
    {
        pszFullFilename = CPLStrdup( CPLFormFilename( m_pszDirectory,
                                                      pszLayerName, "mif" ) );

        poFile = new MIFFile;
    }
    else
    {
        pszFullFilename = CPLStrdup( CPLFormFilename( m_pszDirectory,
                                                      pszLayerName, "tab" ) );

        poFile = new TABFile;
    }

    if( poFile->Open( pszFullFilename, "wb", FALSE ) != 0 )
    {
        CPLFree( pszFullFilename );
        delete poFile;
        return FALSE;
    }

    if( poSRSIn != NULL )
        poFile->SetSpatialRef( poSRSIn );

    if( poSRSIn != NULL && poSRSIn->GetRoot() != NULL
        && EQUAL(poSRSIn->GetRoot()->GetValue(),"GEOGCS") )
    {
        poFile->SetBounds( -180, -90, 180, 90 );
    }
    else
    {
        poFile->SetBounds( -30000000, -15000000, 30000000, 15000000 );
    }

    m_nLayerCount++;
    m_papoLayers = (IMapInfoFile **)
        CPLRealloc(m_papoLayers,sizeof(void*)*m_nLayerCount);
    m_papoLayers[m_nLayerCount-1] = poFile;

    CPLFree( pszFullFilename );

    return poFile;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRTABDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

