/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
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
 *****************************************************************************/

#include "libkml_headers.h"

#include "ogr_libkml.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$")

using kmldom::KmlFactory;

static CPLMutex *hMutex = nullptr;
static KmlFactory* m_poKmlFactory = nullptr;

/******************************************************************************
 OGRLIBKMLDriverUnload()
******************************************************************************/

static void OGRLIBKMLDriverUnload( GDALDriver * /* poDriver */ )
{
    if( hMutex != nullptr )
        CPLDestroyMutex(hMutex);
    hMutex = nullptr;
    m_poKmlFactory = nullptr;
}

/************************************************************************/
/*                    OGRLIBKMLDriverIdentify()                         */
/************************************************************************/

static int OGRLIBKMLDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( !poOpenInfo->bStatOK )
        return FALSE;
    if( poOpenInfo->bIsDirectory )
        return -1;

    const char* pszExt = CPLGetExtension(poOpenInfo->pszFilename);
    if( EQUAL(pszExt, "kml") ||
        EQUAL(pszExt, "kmz") )
    {
        return TRUE;
    }

    if( poOpenInfo->pabyHeader &&
        ( strstr(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                  "<kml") != nullptr ||
          strstr(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                  "<kml:kml") != nullptr) )
    {
        return TRUE;
    }

    return FALSE;
}

/******************************************************************************
 Open()
******************************************************************************/

static GDALDataset *OGRLIBKMLDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( OGRLIBKMLDriverIdentify(poOpenInfo) == FALSE )
        return nullptr;

    {
        CPLMutexHolderD(&hMutex);
        if( m_poKmlFactory == nullptr )
            m_poKmlFactory = KmlFactory::GetFactory();
    }

    OGRLIBKMLDataSource *poDS = new OGRLIBKMLDataSource( m_poKmlFactory );

    if( !poDS->Open( poOpenInfo->pszFilename,
                     poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;

        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRLIBKMLDriverCreate( const char * pszName,
                                           int /* nBands */,
                                           int /* nXSize */,
                                           int /* nYSize */,
                                           GDALDataType /* eDT */,
                                           char **papszOptions )
{
    CPLAssert( nullptr != pszName );
    CPLDebug( "LIBKML", "Attempt to create: %s", pszName );

    {
        CPLMutexHolderD(&hMutex);
        if( m_poKmlFactory == nullptr )
            m_poKmlFactory = KmlFactory::GetFactory();
    }

    OGRLIBKMLDataSource *poDS = new OGRLIBKMLDataSource( m_poKmlFactory );

    if( !poDS->Create( pszName, papszOptions ) ) {
        delete poDS;

        poDS = nullptr;
    }

    return poDS;
}

/******************************************************************************
 DeleteDataSource()

 Note: This method recursively deletes an entire dir if the datasource is a dir
       and all the files are kml or kmz.

******************************************************************************/

static CPLErr OGRLIBKMLDriverDelete( const char *pszName )
{
    /***** dir *****/
    VSIStatBufL sStatBuf;
    if( !VSIStatL( pszName, &sStatBuf ) && VSI_ISDIR( sStatBuf.st_mode ) )
    {
        char **papszDirList = VSIReadDir( pszName );
        for( int iFile = 0;
             papszDirList != nullptr && papszDirList[iFile] != nullptr;
             iFile++ )
        {
            if( CE_Failure == OGRLIBKMLDriverDelete( papszDirList[iFile] ) )
            {
                CSLDestroy( papszDirList );
                return CE_Failure;
            }
        }
        CSLDestroy( papszDirList );

        if( VSIRmdir( pszName ) < 0 )
        {
            return CE_Failure;
        }
    }

    /***** kml *****/
    else if( EQUAL( CPLGetExtension( pszName ), "kml" ) )
    {
        if( VSIUnlink( pszName ) < 0 )
            return CE_Failure;
    }

    /***** kmz *****/
    else if( EQUAL( CPLGetExtension( pszName ), "kmz" ) )
    {
        if( VSIUnlink( pszName ) < 0 )
            return CE_Failure;
    }

    /***** do not delete other types of files *****/
    else
    {
        return CE_Failure;
    }

    // TODO(schwehr): Isn't this redundant to the else case?
    return CE_None;
}

/******************************************************************************
 RegisterOGRLIBKML()
******************************************************************************/

void RegisterOGRLIBKML()
{
    if( GDALGetDriverByName( "LIBKML" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "LIBKML" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Keyhole Markup Language (LIBKML)" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "kml kmz" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/libkml.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='DOCUMENT_ID' type='string' description='Id of the root &lt;Document&gt; node' default='root_doc'/>"
"  <Option name='AUTHOR_NAME' type='string' description='Name in &lt;atom:Author&gt; element'/>"
"  <Option name='AUTHOR_URI' type='string' description='URI in &lt;atom:Author&gt; element'/>"
"  <Option name='AUTHOR_EMAIL' type='string' description='Email in &lt;atom:Author&gt; element'/>"
"  <Option name='LINK' type='string' description='Href of &lt;atom:link&gt; element'/>"
"  <Option name='PHONENUMBER' type='string' description='Value of &lt;phoneNumber&gt; element'/>"
"  <Option name='NAME' type='string' description='Value of &lt;name&gt; element of top container'/>"
"  <Option name='VISIBILITY' type='integer' description='Value of &lt;visibility&gt; element of top container (0/1)'/>"
"  <Option name='OPEN' type='integer' description='Value of &lt;open&gt; element of top container (0/1)'/>"
"  <Option name='SNIPPET' type='string' description='Value of &lt;snippet&gt; element of top container'/>"
"  <Option name='DESCRIPTION' type='string' description='Value of &lt;description&gt; element of top container'/>"
"  <Option name='LISTSTYLE_TYPE' type='string-select' description='Value of &lt;listItemType&gt; element of top container'>"
"    <Value>check</Value>"
"    <Value>radioFolder</Value>"
"    <Value>checkOffOnly</Value>"
"    <Value>checkHideChildren</Value>"
"  </Option>"
"  <Option name='LISTSTYLE_ICON_HREF' type='string' description='URL of the icon to display for the main folder. Sets the href element of the &lt;ItemIcon&gt; element'/>"
"  <Option name='*_BALLOONSTYLE_BGCOLOR' type='string' description='Background color of a &lt;BallonStyle&gt; element if a style X is defined'/>"
"  <Option name='*_BALLOONSTYLE_TEXT' type='string' description='Text of a &lt;BallonStyle&gt; element if a style X is defined'/>"
"  <Option name='NLC_MINREFRESHPERIOD' type='float' description='&lt;minRefreshPeriod&gt; element of a &lt;NetworkLinkControl&gt; element'/>"
"  <Option name='NLC_MAXSESSIONLENGTH' type='float' description='&lt;maxSessionLength&gt; element of a &lt;NetworkLinkControl&gt; element'/>"
"  <Option name='NLC_COOKIE' type='string' description='&lt;cookie&gt; element of a &lt;NetworkLinkControl&gt; element'/>"
"  <Option name='NLC_MESSAGE' type='string' description='&lt;message&gt; element of a &lt;NetworkLinkControl&gt; element'/>"
"  <Option name='NLC_LINKNAME' type='string' description='&lt;linkName&gt; element of a &lt;NetworkLinkControl&gt; element'/>"
"  <Option name='NLC_LINKDESCRIPTION' type='string' description='&lt;linkDescription&gt; element of a &lt;NetworkLinkControl&gt; element'/>"
"  <Option name='NLC_LINKSNIPPET' type='string' description='&lt;linkSnippet&gt; element of a &lt;NetworkLinkControl&gt; element'/>"
"  <Option name='NLC_EXPIRES' type='string' description='Date to set in &lt;expires&gt; element of a &lt;NetworkLinkControl&gt; element'/>"
"  <Option name='UPDATE_TARGETHREF' type='string' description='If set, a NetworkLinkControl KML file with an &lt;Update&gt; element will be generated'/>"
"</CreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='NAME' type='string' description='Value of &lt;name&gt; element of layer container'/>"
"  <Option name='VISIBILITY' type='integer' description='Value of &lt;visibility&gt; element of layer container (0/1)'/>"
"  <Option name='OPEN' type='integer' description='Value of &lt;open&gt; element of layer container (0/1)'/>"
"  <Option name='SNIPPET' type='string' description='Value of &lt;snippet&gt; element of layer container'/>"
"  <Option name='DESCRIPTION' type='string' description='Value of &lt;description&gt; element of layer container'/>"
"  <Option name='LOOKAT_LONGITUDE' type='float' description='&lt;longitude&gt; of a &lt;LookAt&gt; element at layer level' min='-180' max='180'/>"
"  <Option name='LOOKAT_LATITUDE' type='float' description='&lt;latitude&gt; of a &lt;LookAt&gt; element at layer level' min='-90' max='90'/>"
"  <Option name='LOOKAT_RANGE' type='float' description='&lt;range&gt; of a &lt;LookAt&gt; element at layer level' min='0'/>"
"  <Option name='LOOKAT_HEADING' type='float' description='&lt;heading&gt; of a &lt;LookAt&gt; element at layer level'/>"
"  <Option name='LOOKAT_TILT' type='float' description='&lt;tilt&gt; of a &lt;LookAt&gt; element at layer level'/>"
"  <Option name='LOOKAT_ALTITUDE' type='float' description='&lt;altitude&gt; of a &lt;LookAt&gt; element at layer level'/>"
"  <Option name='LOOKAT_ALTITUDEMODE' type='string-select' description='&lt;altitudeMode&gt; of a &lt;LookAt&gt; element at layer level'>"
"    <Value>clampToGround</Value>"
"    <Value>relativeToGround</Value>"
"    <Value>absolute</Value>"
"    <Value>clampToSeaFloor</Value>"
"    <Value>relativeToSeaFloor</Value>"
"  </Option>"
"  <Option name='CAMERA_LONGITUDE' type='float' description='&lt;longitude&gt; of a &lt;Camera&gt; element at layer level' min='-180' max='180'/>"
"  <Option name='CAMERA_LATITUDE' type='float' description='&lt;latitude&gt; of a &lt;Camera&gt; element at layer level' min='-90' max='90'/>"
"  <Option name='CAMERA_HEADING' type='float' description='&lt;heading&gt; of a &lt;Camera&gt; element at layer level'/>"
"  <Option name='CAMERA_TILT' type='float' description='&lt;tilt&gt; of a &lt;Camera&gt; element at layer level'/>"
"  <Option name='CAMERA_ROLL' type='float' description='&lt;roll&gt; of a &lt;Camera&gt; element at layer level'/>"
"  <Option name='CAMERA_ALTITUDE' type='float' description='&lt;altitude&gt; of a &lt;Camera&gt; element at layer level'/>"
"  <Option name='CAMERA_ALTITUDEMODE' type='string-select' description='&lt;altitudeMode&gt; of a &lt;Camera&gt; element at layer level'>"
"    <Value>clampToGround</Value>"
"    <Value>relativeToGround</Value>"
"    <Value>absolute</Value>"
"    <Value>clampToSeaFloor</Value>"
"    <Value>relativeToSeaFloor</Value>"
"  </Option>"
"  <Option name='ADD_REGION' type='boolean' description='Whether to generate a &lt;Region&gt; element to control when objects of the layer are visible or not' default='NO'/>"
"  <Option name='REGION_XMIN' type='float' description='West coordinate of the region' min='-180' max='180'/>"
"  <Option name='REGION_YMIN' type='float' description='South coordinate of the region' min='-90' max='90'/>"
"  <Option name='REGION_XMAX' type='float' description='East coordinate of the region' min='-180' max='180'/>"
"  <Option name='REGION_YMAX' type='float' description='North coordinate of the region' min='-90' max='90'/>"
"  <Option name='REGION_MIN_LOD_PIXELS' type='float' description='minimum size in pixels of the region so that it is displayed' default='256'/>"
"  <Option name='REGION_MAX_LOD_PIXELS' type='float' description='maximum size in pixels of the region so that it is displayed (-1=infinite)' default='-1'/>"
"  <Option name='REGION_MIN_FADE_EXTENT' type='float' description='distance over which the geometry fades, from fully opaque to fully transparent' default='0'/>"
"  <Option name='REGION_MAX_FADE_EXTENT' type='float' description='distance over which the geometry fades, from fully transparent to fully opaque' default='0'/>"
"  <Option name='SO_HREF' type='string' description='URL of the image to display in a &lt;ScreenOverlay&gt;'/>"
"  <Option name='SO_NAME' type='string' description='&lt;name&gt; of a &lt;ScreenOverlay&gt;'/>"
"  <Option name='SO_DESCRIPTION' type='string' description='&lt;description&gt; of a &lt;ScreenOverlay&gt;'/>"
"  <Option name='SO_OVERLAY_X' type='float' description='x attribute of the &lt;overlayXY&gt; of a &lt;ScreenOverlay&gt;'/>"
"  <Option name='SO_OVERLAY_Y' type='float' description='y attribute of the &lt;overlayXY&gt; of a &lt;ScreenOverlay&gt;'/>"
"  <Option name='SO_OVERLAY_XUNITS' type='string-select' description='xunits attribute of the &lt;overlayXY&gt; of a &lt;ScreenOverlay&gt;'>"
"    <Value>fraction</Value>"
"    <Value>pixels</Value>"
"    <Value>insetPixels</Value>"
"  </Option>"
"  <Option name='SO_OVERLAY_YUNITS' type='string-select' description='yunits attribute of the &lt;overlayXY&gt; of a &lt;ScreenOverlay&gt;'>"
"    <Value>fraction</Value>"
"    <Value>pixels</Value>"
"    <Value>insetPixels</Value>"
"  </Option>"
"  <Option name='SO_SCREEN_X' type='float' description='x attribute of the &lt;screenXY&gt; of a &lt;ScreenOverlay&gt;' default='0.05'/>"
"  <Option name='SO_SCREEN_Y' type='float' description='y attribute of the &lt;screenXY&gt; of a &lt;ScreenOverlay&gt;' default='0.05'/>"
"  <Option name='SO_SCREEN_XUNITS' type='string-select' description='xunits attribute of the &lt;screenXY&gt; of a &lt;ScreenOverlay&gt;' default='fraction'>"
"    <Value>fraction</Value>"
"    <Value>pixels</Value>"
"    <Value>insetPixels</Value>"
"  </Option>"
"  <Option name='SO_SCREEN_YUNITS' type='string-select' description='yunits attribute of the &lt;screenXY&gt; of a &lt;ScreenOverlay&gt;' default='fraction'>"
"    <Value>fraction</Value>"
"    <Value>pixels</Value>"
"    <Value>insetPixels</Value>"
"  </Option>"
"  <Option name='SO_SIZE_X' type='float' description='x attribute of the &lt;sizeXY&gt; of a &lt;ScreenOverlay&gt;'/>"
"  <Option name='SO_SIZE_Y' type='float' description='y attribute of the &lt;sizeXY&gt; of a &lt;ScreenOverlay&gt;'/>"
"  <Option name='SO_SIZE_XUNITS' type='string-select' description='xunits attribute of the &lt;sizeXY&gt; of a &lt;ScreenOverlay&gt;'>"
"    <Value>fraction</Value>"
"    <Value>pixels</Value>"
"    <Value>insetPixels</Value>"
"  </Option>"
"  <Option name='SO_SIZE_YUNITS' type='string-select' description='yunits attribute of the &lt;sizeXY&gt; of a &lt;ScreenOverlay&gt;'>"
"    <Value>fraction</Value>"
"    <Value>pixels</Value>"
"    <Value>insetPixels</Value>"
"  </Option>"
"  <Option name='FOLDER' type='boolean' description='Whether to generate a &lt;Folder&gt; element for layers, instead of a &lt;Document&gt;' default='NO'/>"
"  <Option name='LISTSTYLE_TYPE' type='string-select' description='Value of &lt;listItemType&gt; element of layer container'>"
"    <Value>check</Value>"
"    <Value>radioFolder</Value>"
"    <Value>checkOffOnly</Value>"
"    <Value>checkHideChildren</Value>"
"  </Option>"
"  <Option name='LISTSTYLE_ICON_HREF' type='string' description='URL of the icon to display for the layer folder. Sets the href element of the &lt;ItemIcon&gt; element'/>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Real String" );
    poDriver->SetMetadataItem( GDAL_DCAP_FEATURE_STYLES, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    poDriver->pfnOpen = OGRLIBKMLDriverOpen;
    poDriver->pfnIdentify = OGRLIBKMLDriverIdentify;
    poDriver->pfnCreate = OGRLIBKMLDriverCreate;
    poDriver->pfnDelete = OGRLIBKMLDriverDelete;
    poDriver->pfnUnloadDriver = OGRLIBKMLDriverUnload;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
