/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for translating between formats.
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

import org.gdal.gdal.gdal;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.VectorTranslateOptions;
import org.gdal.gdalconst.*;
import java.util.Vector;

public class ogr2ogr_new
{
    public static void main(String[] args)
    {
        gdal.AllRegister();

        String src = null, dest = null;
        Vector newArgs = new Vector();
        boolean tryUpdate = false;
        for(int i=0;i<args.length;i++)
        {
            if( args[i].charAt(0) == '-')
            {
                if( args[i].equals("-update") ||
                    args[i].equals("-append") ||
                    args[i].equals("-overwrite") )
                {
                    tryUpdate = true;
                }
                newArgs.add(args[i]);
            }
            else if( dest == null )
                dest = args[i];
            else if( src == null )
                src = args[i];
            else
                newArgs.add(args[i]);
        }

        Dataset srcDS = gdal.OpenEx(src, gdalconst.OF_VECTOR | gdalconst.OF_VERBOSE_ERROR);
        Dataset outDS = null;
        if( tryUpdate )
        {
            outDS = gdal.OpenEx(dest, gdalconst.OF_VECTOR | gdalconst.OF_UPDATE);
        }
        if( outDS != null )
        {
            int ret = gdal.VectorTranslate(outDS, srcDS, new VectorTranslateOptions(newArgs));
            if( ret != 1 )
                System.exit(1);
        }
        else
        {
            outDS = gdal.VectorTranslate(dest, srcDS, new VectorTranslateOptions(newArgs));
            if( outDS == null )
                System.exit(1);
        }
    }
}
