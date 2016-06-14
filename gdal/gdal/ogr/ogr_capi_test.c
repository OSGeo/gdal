/**********************************************************************
 * $Id$
 **********************************************************************
 * Copyright (c) 2003, Daniel Morissette
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
 *
 * Test the OGR C API (ogr_api.h)
 *
 * Compile using:
 *
 * gcc -g ogr_capi_test.c `gdal-config --libs` `gdal-config --cflags` \
 *     -o ogr_capi_test
 *
 ****************************************************************************/

#include "ogr_api.h"

int OGRCDump(const char *pszFname);
int OGRCCreate(const char *pszFname);

/**********************************************************************
 *                      main()
 **********************************************************************/
int main(int argc, char *argv[])
{

    if (argc == 3 && EQUAL(argv[1], "dump"))
    {
        return OGRCDump(argv[2]);
    }
    else if (argc == 3 && EQUAL(argv[1], "create"))
    {
        return OGRCCreate(argv[2]);
    }
    else
    {
        printf("Usage: ogr_capi_test <command> <filename>\n");
    }

    return 0;
}

/**********************************************************************
 *                      OGRCDump()
 *
 * Open a dataset using OGR and dump all its layers.
 *
 **********************************************************************/

int OGRCDump(const char *pszFname)
{
    OGRDataSourceH datasource;
    int i, numLayers;

    /* Register all OGR drivers */
    OGRRegisterAll();

    /* Open data source */
    datasource = OGROpen(pszFname, 0 /* bUpdate */, NULL);

    if (datasource == NULL)
    {
        printf("Unable to open %s\n", pszFname);
        return -1;
    }

    /* Loop through layers and dump their contents */

    numLayers = OGR_DS_GetLayerCount(datasource);
    for(i=0; i<numLayers; i++)
    {
        OGRLayerH layer;
        int j, numFields;
        OGRFeatureH feature;
        OGRFeatureDefnH layerDefn;

        layer = OGR_DS_GetLayer( datasource, i );

        /* Dump info about this layer */
        layerDefn = OGR_L_GetLayerDefn( layer );
        numFields = OGR_FD_GetFieldCount( layerDefn );

        printf("\n===================\n");
        printf("Layer %d: '%s'\n\n", i, OGR_FD_GetName(layerDefn));

        for(j=0; j<numFields; j++)
        {
            OGRFieldDefnH fieldDefn;

            fieldDefn = OGR_FD_GetFieldDefn( layerDefn, j );
            printf(" Field %d: %s (%s)\n",
                   j, OGR_Fld_GetNameRef(fieldDefn),
                   OGR_GetFieldTypeName(OGR_Fld_GetType(fieldDefn)) );
        }
        printf("\n");

        /* And dump each feature individually */
        while( (feature = OGR_L_GetNextFeature( layer )) != NULL )
        {
            OGR_F_DumpReadable( feature, stdout );
            OGR_F_Destroy( feature );
        }

        /* No need to free layer handle, it belongs to the datasource */
    }

    /* Close data source */
    OGR_DS_Destroy( datasource );

    return 0;
}

/**********************************************************************
 *                      OGRCCreate()
 *
 * Create a new dataset using OGR C API with a few features in it.
 *
 **********************************************************************/

int OGRCCreate(const char *pszFname)
{
    OGRSFDriverH    driver;
    int             i, numDrivers;
    OGRDataSourceH  datasource;
    OGRLayerH       layer;
    OGRFeatureDefnH layerDefn;
    OGRFieldDefnH   fieldDefn;
    OGRFeatureH     feature;
    OGRGeometryH    geometry, ring;

    /* Register all OGR drivers */
    OGRRegisterAll();

    /* Fetch MITAB driver - we want to create a TAB file */
    numDrivers = OGRGetDriverCount();
    for(i=0; i<numDrivers; i++)
    {
        driver = OGRGetDriver(i);
        if (EQUAL("MapInfo File", OGR_Dr_GetName(driver)))
            break;  /* Found it! */
        driver = NULL;
    }

    if (!driver)
    {
        printf("Driver not found!\n");
        return -1;
    }

    /* Create new file using this driver */
    datasource = OGR_Dr_CreateDataSource(driver, pszFname, NULL);

    if (datasource == NULL)
    {
        printf("Unable to create %s\n", pszFname);
        return -1;
    }

    /* MapInfo data sources are created with one empty layer.
       Fetch the layer handle */
    layer = OGR_DS_GetLayer(datasource, 0);

    if (layer == NULL)
    {
        printf("Unable to create new layer in %s\n", pszFname);
        return -1;
    }

    /* Add a few fields to the layer defn */
    fieldDefn = OGR_Fld_Create( "id", OFTInteger );
    OGR_L_CreateField(layer, fieldDefn, 0);

    fieldDefn = OGR_Fld_Create( "area", OFTReal );
    OGR_L_CreateField(layer, fieldDefn, 0);

    fieldDefn = OGR_Fld_Create( "name", OFTString );
    OGR_L_CreateField(layer, fieldDefn, 0);

    /* We'll need the layerDefn handle to create new features in this layer */
    layerDefn = OGR_L_GetLayerDefn( layer );

    /* Create a new point */
    feature = OGR_F_Create( layerDefn );
    OGR_F_SetFieldInteger( feature, 0, 1);
    OGR_F_SetFieldDouble( feature, 1, 123.45);
    OGR_F_SetFieldString( feature, 2, "Feature #1");

    geometry = OGR_G_CreateGeometry( wkbPoint );
    OGR_G_SetPoint(geometry, 0, 123.45, 456.78, 0);

    OGR_F_SetGeometryDirectly(feature, geometry);

    OGR_L_CreateFeature( layer, feature );

    /* Create a new line */
    feature = OGR_F_Create( layerDefn );
    OGR_F_SetFieldInteger( feature, 0, 2);
    OGR_F_SetFieldDouble( feature, 1, 42.45);
    OGR_F_SetFieldString( feature, 2, "Feature #2");

    geometry = OGR_G_CreateGeometry( wkbLineString );
    OGR_G_AddPoint(geometry, 123.45, 456.78, 0);
    OGR_G_AddPoint(geometry, 12.34,  45.67, 0);

    OGR_F_SetGeometryDirectly(feature, geometry);

    OGR_L_CreateFeature( layer, feature );

    /* Create a new polygon (square) */
    feature = OGR_F_Create( layerDefn );
    OGR_F_SetFieldInteger( feature, 0, 3);
    OGR_F_SetFieldDouble( feature, 1, 49.71);
    OGR_F_SetFieldString( feature, 2, "Feature #3");

    geometry = OGR_G_CreateGeometry( wkbPolygon );
    ring = OGR_G_CreateGeometry( wkbLinearRing );
    OGR_G_AddPoint(ring, 123.45, 456.78, 0);
    OGR_G_AddPoint(ring, 12.34,  456.78, 0);
    OGR_G_AddPoint(ring, 12.34,  45.67, 0);
    OGR_G_AddPoint(ring, 123.45, 45.67, 0);
    OGR_G_AddPoint(ring, 123.45, 456.78, 0);
    OGR_G_AddGeometryDirectly(geometry, ring);

    OGR_F_SetGeometryDirectly(feature, geometry);

    OGR_L_CreateFeature( layer, feature );

    /* Close data source */
    OGR_DS_Destroy( datasource );

    return 0;
}
