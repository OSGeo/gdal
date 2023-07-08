/******************************************************************************
 * $Id$
 *
 * Name:     GDALTestMultiDim.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app to test MDArray kinds of things in the java api
 * Author:   Barry DeZonia, <bdezonia at gmail.com>
 *
 * Adapted from the GDALTestIO code
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

import java.util.Arrays;

import org.gdal.gdal.gdal;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Dimension;
import org.gdal.gdal.Driver;
import org.gdal.gdal.ExtendedDataType;
import org.gdal.gdal.Group;
import org.gdal.gdal.MDArray;
import org.gdal.gdalconst.gdalconst;

public class GDALTestMultiDim
{
    public GDALTestMultiDim() { }

    public static void main(String[] args)
    {
        gdal.AllRegister();

        testMDArrayStuff();

        System.out.println("Success !");
    }
    
    private static void testMDArrayStuff() {

        Driver driver = gdal.GetDriverByName("MEM");

        Dataset dataset = driver.CreateMultiDimensional("mdstuff");
        
        Group rg = dataset.GetRootGroup();

        ExtendedDataType dt = ExtendedDataType.Create(gdalconst.GDT_Int16);

        long[] sizes = new long[]{10,6,2,7};

        Dimension[] inDims = new Dimension[sizes.length];
        
        for (int i = 0; i < sizes.length; i++) {

            Dimension d =
                
                rg.CreateDimension("name"+i, "type"+i, "direction"+i, sizes[i]);

            if (d == null) {
                
                throw new RuntimeException("dimension create returned null!");
            }
            
            inDims[i] = d;
        }
        
        MDArray mdarray = rg.CreateMDArray("my_data", inDims, dt);
        
        long cnt = mdarray.GetDimensionCount();
        
        for (long i = 0; i < cnt; i++) {

            Dimension d = mdarray.GetDimension(i);

            if (d == null) {
                
                throw new RuntimeException("returned dimension was null!");
            }
        }
        
        Dimension[] outDims = mdarray.GetDimensions();
        
        if (outDims.length != sizes.length) {
            
            throw new RuntimeException("resulting dimension count "+outDims.length+" does not equal input dim len "+sizes.length);
        }
        
        for (int i = 0; i < sizes.length; i++) {
            
            Dimension d = outDims[i];

            if (d.GetSize() != sizes[i]) {

                throw new RuntimeException("resulting dimension "+i+" has size "+ d.GetSize()+" but should equal "+sizes[i]);
            }

            if (!d.GetName().equals("name"+i)) {

                throw new RuntimeException("resulting dimension name "+d.GetName()+" does not match name"+i);
            }

            if (!d.GetType().equals("type"+i)) {

                throw new RuntimeException("resulting dimension type "+d.GetType()+" does not match type"+i);
            }

            if (!d.GetDirection().equals("direction"+i)) {

                throw new RuntimeException("resulting dimension direction "+d.GetDirection()+" does not match direction"+i);
            }
        }
        
        long xSize = sizes[0];
        
        long ySize = sizes[1];
        
        long zSize = sizes[2];
        
        long timePoints = sizes[3];
        
        int planeSize = (int) (xSize * ySize);
        
        short[] zeros = new short[planeSize];

        short[] writeData = new short[planeSize];

        short[] readData = new short[planeSize];

        long[] starts = new long[sizes.length];

        long[] counts = new long[sizes.length];

        long[] steps = new long[sizes.length];

        long[] strides = new long[sizes.length];

        // read/write XY planes one at a time through whole mdarray
        
        for (int t = 0; t < timePoints; t++) {
            
            starts[3] = t;
            counts[3] = 1;
            steps[3] = 1;
            strides[3] = 1;

            for (int z = 0; z < zSize; z++) {
            
                starts[2] = z;
                counts[2] = 1;
                steps[2] = 1;
                strides[2] = 1;

                starts[1] = 0;
                counts[1] = ySize;
                steps[1] = 1;
                strides[1] = 1;

                starts[0] = 0;
                counts[0] = xSize;
                steps[0] = 1;
                strides[0] = 1;

                int pos = 0;
                
                for (int y = 0; y < ySize; y++) {
                
                    for (int x = 0; x < xSize; x++) {
                    
                        short val = (short) ((t+1)*(z+1)*(y+1)*(x+1));
                        
                        writeData[pos++] = val;
                    }
                }

                if (Arrays.equals(writeData, zeros)) {
                    
                    throw new RuntimeException("data to be written is zero and shouldn't be");
                }
                
                if (!Arrays.equals(readData, zeros)) {
                    
                    throw new RuntimeException("data read buffer is not zero and should be");
                }

                if (!mdarray.Write(starts, counts, steps, strides, writeData)) {

                    throw new RuntimeException("could not write a plane for some reason");
                }
                
                if (!mdarray.Read(starts, counts, steps, strides, readData)) {

                    throw new RuntimeException("could not read a plane for some reason");
                }
                
                if (Arrays.equals(readData, zeros)) {
                    
                    throw new RuntimeException("data read is zero and shouldn't be");
                }
                
                if (!Arrays.equals(readData, writeData)) {
                    
                    throw new RuntimeException("data read does not match data written");
                }
                
                Arrays.fill(writeData, (short) 0);
                
                Arrays.fill(readData, (short) 0);
            }            
        }
    }
}
