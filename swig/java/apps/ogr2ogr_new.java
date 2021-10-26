/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for translating between formats.
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault
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
