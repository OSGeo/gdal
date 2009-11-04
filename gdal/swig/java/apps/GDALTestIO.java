/******************************************************************************
 * $Id$
 *
 * Name:     GDALTestIO.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app to test ReadRaster_Direct / WriteRaster_Direct
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 * Adapted from a sample by Ivan Lucena
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import org.gdal.gdal.gdal;
import org.gdal.gdal.Band;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdalconst.gdalconst;

public class GDALTestIO implements Runnable
{
    String filename;
    int    nbIters;
    
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
        
        int xsize = 4;
        int ysize = 4;
        
        driver = gdal.GetDriverByName("GTiff");
            
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(4 * xsize);
        byteBuffer.order(ByteOrder.nativeOrder());
        FloatBuffer floatBuffer = byteBuffer.asFloatBuffer();
        
        for(int iter = 0; iter < nbIters; iter++)
        {
            dataset = driver.Create(filename, xsize, ysize, 1, gdalconst.GDT_Float32);
            band = dataset.GetRasterBand(1);
            
            for( int i = 0; i < ysize; i++) {
                for( int j = 0; j < xsize; j++) {
                    floatBuffer.put(j, (float) (i + j));
                }
                band.WriteRaster_Direct(0, i, xsize, 1, gdalconst.GDT_Float32, byteBuffer);
            }
            
            dataset.delete();
            
            /* Open the file to check the values */
            dataset = gdal.Open(filename);
            band = dataset.GetRasterBand(1);
            
            if (band.Checksum() != 48)
                throw new RuntimeException("Bad checksum : " + band.Checksum());
            
            for( int i = 0; i < ysize; i++) {
                band.ReadRaster_Direct(0, i, xsize, 1, xsize, 1, gdalconst.GDT_Int32, byteBuffer);
                for( int j = 0; j < xsize; j++) {
                    int val = byteBuffer.getInt(j*4);
                    if (val != (i + j))
                        throw new RuntimeException("Bad value for (" + j + "," + i + ") : " + val);
                }
            }
            dataset.delete();
            
            /* Free the memory occupied by the /vsimem file */
            gdal.Unlink(filename);  
        }
    }

    public static void main(String[] args) throws InterruptedException
    {
        gdal.AllRegister();
        
        int nbIters = 5000;
        
        Thread t1 = new Thread(new GDALTestIO("/vsimem/test1.tif", nbIters));
        Thread t2 = new Thread(new GDALTestIO("/vsimem/test2.tif", nbIters));
        t1.start();
        t2.start();
        t1.join();
        t2.join();

        System.out.println("Success !");
    }
}
