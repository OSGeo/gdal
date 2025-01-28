/******************************************************************************
 *
 * Name:     GDALTestIO.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app to test ReadRaster_Direct / WriteRaster_Direct
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 * Adapted from a sample by Ivan Lucena
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import org.gdal.gdal.gdal;
import org.gdal.gdal.Band;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdalconst.gdalconst;
import org.gdal.gdal.*;

public class GDALTestIO implements Runnable
{
    String filename;
    int    nbIters;
    static final int METHOD_DBB = 1;
    static final int METHOD_JAVA_ARRAYS = 2;
    static int    method;

    static volatile boolean bWait = true;
    static volatile int nReady = 0;
    static Object waiter = new Object();
    static Object notifier = new Object();

    public GDALTestIO(String filename, int nbIters)
    {
        this.filename = filename;
        this.nbIters = nbIters;
    }

    public void run()
    {
        Dataset dataset = null;
        Driver driver = null;
        Band band = null;

        int xsize = 4000;
        int ysize = 400;

        synchronized(notifier)
        {
            nReady ++;
            notifier.notify();
        }

        synchronized(waiter)
        {
            while( bWait )
            {
                try
                {
                    waiter.wait();
                }
                catch(InterruptedException ie)
                {
                }
            }
        }

        driver = gdal.GetDriverByName("GTiff");

        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(4 * xsize);
        byteBuffer.order(ByteOrder.nativeOrder());
        FloatBuffer floatBuffer = byteBuffer.asFloatBuffer();
        int[] intArray = new int[xsize];
        float[] floatArray = new float[xsize];

        dataset = driver.Create(filename, xsize, ysize, 1, gdalconst.GDT_Float32);
        band = dataset.GetRasterBand(1);

        for(int iter = 0; iter < nbIters; iter++)
        {
            if (method == METHOD_DBB)
            {
                for( int i = 0; i < ysize; i++) {
                    for( int j = 0; j < xsize; j++) {
                        floatBuffer.put(j, (float) (i + j));
                    }
                    band.WriteRaster_Direct(0, i, xsize, 1, gdalconst.GDT_Float32, byteBuffer);
                }
            }
            else
            {
                for( int i = 0; i < ysize; i++) {
                    for( int j = 0; j < xsize; j++) {
                        floatArray[j] = (float) (i + j);
                    }
                    band.WriteRaster(0, i, xsize, 1, floatArray);
                }
            }
        }

        dataset.delete();

        /* Open the file to check the values */
        dataset = gdal.Open(filename);
        band = dataset.GetRasterBand(1);

        for(int iter = 0; iter < nbIters; iter++)
        {
            if (method == METHOD_DBB)
            {
                for( int i = 0; i < ysize; i++) {
                    band.ReadRaster_Direct(0, i, xsize, 1, xsize, 1, gdalconst.GDT_Int32, byteBuffer);
                    for( int j = 0; j < xsize; j++) {
                        int val = byteBuffer.getInt(j*4);
                        if (val != (i + j))
                            throw new RuntimeException("Bad value for (" + j + "," + i + ") : " + val);
                    }
                }
            }
            else
            {
                for( int i = 0; i < ysize; i++) {
                    band.ReadRaster(0, i, xsize, 1, intArray);
                    for( int j = 0; j < xsize; j++) {
                        int val = intArray[j];
                        if (val != (i + j))
                            throw new RuntimeException("Bad value for (" + j + "," + i + ") : " + val);
                    }
                }
            }
        }

        dataset.delete();

        /* Free the memory occupied by the /vsimem file */
        gdal.Unlink(filename);
    }

    public static void main(String[] args) throws InterruptedException
    {
        gdal.AllRegister();

        testInt64();

        testGetMemFileBuffer();

        int nbIters = 50;

        method = METHOD_JAVA_ARRAYS;
        if (args.length >= 1 && args[0].equalsIgnoreCase("-dbb"))
            method = METHOD_DBB;

        Thread t1 = new Thread(new GDALTestIO("/vsimem/test1.tif", nbIters));
        Thread t2 = new Thread(new GDALTestIO("/vsimem/test2.tif", nbIters));
        t1.start();
        t2.start();

        synchronized(notifier)
        {
            while( nReady != 2 )
            {
                try
                {
                    notifier.wait();
                }
                catch(InterruptedException ie)
                {
                }
            }
        }


        synchronized(waiter)
        {
            bWait = false;
            waiter.notifyAll();
        }

        t1.join();
        t2.join();

        System.out.println("Success !");
    }

    private static void testInt64() {

        long[] data1;
        long[] data2;

        int xSz = 5;
        int ySz = 2;
        int nBands = 1;
        Driver driver = gdal.GetDriverByName("MEM");
        Dataset dataset = driver.Create("fred", xSz, ySz, nBands, gdalconst.GDT_Int64);
        data1 = new long[]{1,43L*Integer.MAX_VALUE,3,4,5,6,7,8,9,10};
        data2 = new long[data1.length];
        dataset.WriteRaster(0, 0, xSz, ySz, xSz, ySz, gdalconst.GDT_Int64, data1, new int[]{1});
        dataset.ReadRaster(0, 0, xSz, ySz, xSz, ySz, gdalconst.GDT_Int64, data2, new int[]{1});
        for (int i = 0; i < data1.length; i++) {
            if (data1[i] != data2[i])
                throw new RuntimeException("int64 write and read values are not the same "+data1[i]+" "+data2[i]);
        }
        data1 = new long[data2.length];
        data2 = new long[]{10,9,8,7,6,5,4,3,2,1};
        dataset.GetRasterBand(1).WriteRaster(0, 0, xSz, ySz, xSz, ySz, gdalconst.GDT_Int64, data1);
        dataset.GetRasterBand(1).ReadRaster(0, 0, xSz, ySz, xSz, ySz, gdalconst.GDT_Int64, data2);
        for (int i = 0; i < data1.length; i++) {
            if (data1[i] != data2[i])
                throw new RuntimeException("int64 write and read values are not the same "+data1[i]+" "+data2[i]);
        }

        dataset.Close();
        dataset.Close();
    }

    private static void testGetMemFileBuffer()
    {
        gdal.FileFromMemBuffer("/vsimem/test", new byte[] {1, 2, 3});
        for(int iter = 0; iter < 2; iter++)
        {
            byte[] res = gdal.GetMemFileBuffer("/vsimem/test");
            if (res.length != 3 )
                throw new RuntimeException("res.length != 3");
            if (res[0] != 1 || res[1] != 2 || res[2] != 3)
                throw new RuntimeException("res != {1, 2, 3}");
        }
    }
}
