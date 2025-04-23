/******************************************************************************
 *
 * Name:     multireadtest.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app to stress-test thread-safety
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

import org.gdal.gdal.gdal;
import org.gdal.gdal.Dataset;

public class multireadtest implements Runnable
{
    private String _filename;

    public multireadtest(String filename)
    {
        _filename = filename;
    }

    public void run()
    {
        for(int i=0;i<100;i++)
        {
            Dataset ds = gdal.Open(_filename);
            ds.GetRasterBand(1).Checksum();
            //ds.delete();
        }
    }

    public static void main(String[] args) throws InterruptedException
    {
        String filename = args[0];

        gdal.AllRegister();

        Thread t[] = new Thread[4];
        for(int i=0;i<4;i++)
        {
            t[i] = new Thread(new multireadtest(filename));
            t[i].start();
        }
        for(int i=0;i<4;i++)
        {
            t[i].join();
        }
        //gdal.GDALDestroyDriverManager();
    }
}
