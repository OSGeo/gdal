/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRDGNDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
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

#include "ogr_dgn.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static int OGRDGNDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return poOpenInfo->fpL != NULL &&
           poOpenInfo->nHeaderBytes >= 512 &&
           DGNTestOpen(poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDGNDriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRDGNDataSource    *poDS;
    
    if( !OGRDGNDriverIdentify(poOpenInfo) )
        return NULL;

    poDS = new OGRDGNDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, TRUE, (poOpenInfo->eAccess == GA_Update) )
        || poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRDGNDriverCreate( const char * pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions )
{
/* -------------------------------------------------------------------- */
/*      Return a new OGRDataSource()                                    */
/* -------------------------------------------------------------------- */
    OGRDGNDataSource    *poDS = NULL;

    poDS = new OGRDGNDataSource();

    if( !poDS->PreCreate( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          RegisterOGRDGN()                            */
/************************************************************************/

void RegisterOGRDGN()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "DGN" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "DGN" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Microstation DGN" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dgn" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_dgn.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='3D' type='boolean' description='whether 2D (seed_2d.dgn) or 3D (seed_3d.dgn) seed file should be used. This option is ignored if the SEED option is provided'/>"
"  <Option name='SEED' type='string' description='Filename of seed file to use'/>"
"  <Option name='COPY_WHOLE_SEED_FILE' type='boolean' description='whether the whole seed file should be copied. If not, only the first three elements (and potentially the color table) will be copied.' default='NO'/>"
"  <Option name='COPY_SEED_FILE_COLOR_TABLE' type='boolean' description='whether the color table should be copied from the seed file.' default='NO'/>"
"  <Option name='MASTER_UNIT_NAME' type='string' description='Override the master unit name from the seed file with the provided one or two character unit name.'/>"
"  <Option name='SUB_UNIT_NAME' type='string' description='Override the master unit name from the seed file with the provided one or two character unit name.'/>"
"  <Option name='MASTER_UNIT_NAME' type='string' description='Override the master unit name from the seed file with the provided one or two character unit name.'/>"
"  <Option name='SUB_UNIT_NAME' type='string' description='Override the sub unit name from the seed file with the provided one or two character unit name.'/>"
"  <Option name='SUB_UNITS_PER_MASTER_UNIT' type='int' description='Override the number of subunits per master unit. By default the seed file value is used.'/>"
"  <Option name='UOR_PER_SUB_UNIT' type='int' description='Override the number of UORs (Units of Resolution) per sub unit. By default the seed file value is used.'/>"
"  <Option name='ORIGIN' type='string' description='Value as x,y,z. Override the origin of the design plane. By default the origin from the seed file is used.'/>"
"</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST, "<LayerCreationOptionList/>" );

        poDriver->pfnOpen = OGRDGNDriverOpen;
        poDriver->pfnIdentify = OGRDGNDriverIdentify;
        poDriver->pfnCreate = OGRDGNDriverCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
