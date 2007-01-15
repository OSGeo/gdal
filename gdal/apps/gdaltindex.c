/******************************************************************************
 * $Id$
 *
 * Project:  MapServer
 * Purpose:  Commandline App to build tile index for raster files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam, DM Solutions Group Inc
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
 ****************************************************************************/

#include "ogrsf_frmts/shape/shapefil.h"
#include "gdal.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    fprintf(stdout, "%s", 
            "\n"
            "Usage: gdaltindex [-tileindex field_name] index_file [gdal_file]*\n"
            "\n"
            "eg.\n"
            "  % gdaltindex doq_index.shp doq/*.tif\n" 
            "\n"
            "NOTES:\n"
            "  o The shapefile (index_file) will be created if it doesn't already exist.\n" 
            "  o The default tile index field is 'location'.\n"
            "  o Raster filenames will be put in the file exactly as they are specified\n"
            "    on the commandline.\n"
            "  o Simple rectangular polygons are generated in the same\n"
            "    coordinate system as the rasters.\n" );
    exit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main(int argc, char *argv[])
{
    const char *index_filename = NULL;
    const char *tile_index = "location";
    int		i_arg, ti_field;
    SHPHandle   hSHP;
    DBFHandle	hDBF;

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Get commandline arguments other than the GDAL raster filenames. */
/* -------------------------------------------------------------------- */
    for( i_arg = 1; i_arg < argc; i_arg++ )
    {
        if( strcmp(argv[i_arg],"-tileindex") == 0 )
        {
            tile_index = argv[++i_arg];
        }
        else if( argv[i_arg][0] == '-' )
            Usage();
        else if( index_filename == NULL )
        {
            index_filename = argv[i_arg];
            i_arg++;
            break;
        }
    }

    if( index_filename == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open or create the target shapefile and DBF file.               */
/* -------------------------------------------------------------------- */
    hSHP = SHPOpen( index_filename, "r+" );
    if( hSHP == NULL )
    {
        printf( "Creating new index file...\n" );
        hSHP = SHPCreate( index_filename, SHPT_POLYGON );
    }

    if( hSHP == NULL )
    {
        fprintf( stderr, "Unable to open/create shapefile `%s'.\n", 
                 index_filename );
        exit(2);
    }

    hDBF = DBFOpen( index_filename, "r+" );
    if( hDBF == NULL )
    {
        hDBF = DBFCreate( index_filename );
        if( hDBF == NULL )
        {
            fprintf( stderr, "Unable to open/create DBF file `%s'.\n", 
                     index_filename );
            exit(2);
        }
            
        DBFAddField( hDBF, tile_index, FTString, 255, 0 );
    }

    for( ti_field = 0; ti_field < DBFGetFieldCount(hDBF); ti_field++ )
    {
        char	field_name[16];

        DBFGetFieldInfo( hDBF, ti_field, field_name, NULL, NULL );
        if( strcmp(field_name, tile_index) == 0 )
            break;
    }

    if( ti_field == DBFGetFieldCount(hDBF) )
    {
        fprintf( stderr, "Unable to find field `%s' in DBF file `%s'.\n", 
                 tile_index, index_filename );
        exit(2);
    }

/* -------------------------------------------------------------------- */
/*      loop over GDAL files, processing.                               */
/* -------------------------------------------------------------------- */
    for( ; i_arg < argc; i_arg++ )
    {
        GDALDatasetH	hDS;
        double	        adfGeoTransform[6];
        double		adfX[5], adfY[5];
        int		nXSize, nYSize, iShape;
        SHPObject	*psOutline;

        hDS = GDALOpen( argv[i_arg], GA_ReadOnly );
        if( hDS == NULL )
        {
            fprintf( stderr, "Unable to open %s, skipping.\n", 
                     argv[i_arg] );
            continue;
        }

        GDALGetGeoTransform( hDS, adfGeoTransform );
        if( adfGeoTransform[0] == 0.0 
            && adfGeoTransform[1] == 1.0
            && adfGeoTransform[3] == 0.0
            && ABS(adfGeoTransform[5]) == 1.0 )
        {
            fprintf( stderr, 
                     "It appears no georeferencing is available for\n"
                     "`%s', skipping.\n", 
                     argv[i_arg] );
            GDALClose( hDS );
            continue;
        }

        nXSize = GDALGetRasterXSize( hDS );
        nYSize = GDALGetRasterYSize( hDS );
        
        adfX[0] = adfGeoTransform[0] 
            + 0 * adfGeoTransform[1] 
            + 0 * adfGeoTransform[2];
        adfY[0] = adfGeoTransform[3] 
            + 0 * adfGeoTransform[4] 
            + 0 * adfGeoTransform[5];
        
        adfX[1] = adfGeoTransform[0] 
            + nXSize * adfGeoTransform[1] 
            + 0 * adfGeoTransform[2];
        adfY[1] = adfGeoTransform[3] 
            + nXSize * adfGeoTransform[4] 
            + 0 * adfGeoTransform[5];
        
        adfX[2] = adfGeoTransform[0] 
            + nXSize * adfGeoTransform[1] 
            + nYSize * adfGeoTransform[2];
        adfY[2] = adfGeoTransform[3] 
            + nXSize * adfGeoTransform[4] 
            + nYSize * adfGeoTransform[5];
        
        adfX[3] = adfGeoTransform[0] 
            + 0 * adfGeoTransform[1] 
            + nYSize * adfGeoTransform[2];
        adfY[3] = adfGeoTransform[3] 
            + 0 * adfGeoTransform[4] 
            + nYSize * adfGeoTransform[5];
        
        adfX[4] = adfGeoTransform[0] 
            + 0 * adfGeoTransform[1] 
            + 0 * adfGeoTransform[2];
        adfY[4] = adfGeoTransform[3] 
            + 0 * adfGeoTransform[4] 
            + 0 * adfGeoTransform[5];

        psOutline = SHPCreateSimpleObject( SHPT_POLYGON, 5, adfX, adfY, NULL );
        iShape = SHPWriteObject(hSHP, -1, psOutline );
        SHPDestroyObject( psOutline );
        
        DBFWriteStringAttribute( hDBF, iShape, ti_field, argv[i_arg] );
        GDALClose( hDS );
    }

    DBFClose( hDBF );
    SHPClose( hSHP );
    
    exit( 0 );
} 
