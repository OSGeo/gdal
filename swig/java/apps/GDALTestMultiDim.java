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

import java.math.BigInteger;
import java.util.Arrays;
import java.util.Vector;

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

		long[] dims = new long[]{10,6,2,7};

		Vector<Dimension> inDims = new Vector<>();
		
		for (int i = 0; i < dims.length; i++) {
			
			Dimension d =
				
				rg.CreateDimension("name"+i, "type"+i, "direction"+i, dims[i]);
			
			inDims.add(d);
		}
		
		MDArray mdarray = rg.CreateMDArray("my_data", inDims, dt);
		
		Vector<Dimension> outDims = (Vector<Dimension>) mdarray.GetDimensions();
		
		if (outDims.size() != dims.length) {
			
			throw new RuntimeException("resulting dimension count "+outDims.size()+" does not equal input dim len "+dims.length);
		}
		
		for (int i = 0; i < dims.length; i++) {
			
			Dimension d = outDims.get(i);
			
			if (d.GetSize().longValue() != dims[i]) {

				throw new RuntimeException("resulting dimension "+i+" has size "+ d.GetSize()+" but should equal "+dims[i]);
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

		long xSize = dims[0];
		
		long ySize = dims[1];
		
		long zSize = dims[2];
		
		long timePoints = dims[3];
		
		int planeSize = (int) (xSize * ySize);
		
		short[] planeZeros = new short[planeSize];

		short[] planeWritten = new short[planeSize];

		short[] planeRead = new short[planeSize];

		long[] starts = new long[dims.length];

		long[] counts = new long[dims.length];

		long[] steps = new long[dims.length];

		long[] strides = new long[dims.length];

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
						
						planeWritten[pos++] = val;
					}
				}

				if (Arrays.equals(planeWritten, planeZeros)) {
					
					throw new RuntimeException("write plane is zero and shouldn't be");
				}
				
				if (!Arrays.equals(planeRead, planeZeros)) {
					
					throw new RuntimeException("read plane is not zero and should be");
				}

				if (!mdarray.Write(starts, counts, steps, strides, planeWritten)) {

					throw new RuntimeException("could not write a plane for some reason");
				}
				
				if (!mdarray.Read(starts, counts, steps, strides, planeRead)) {

					throw new RuntimeException("could not read a plane for some reason");
				}
				
				if (Arrays.equals(planeRead, planeZeros)) {
					
					throw new RuntimeException("read plane is zero and shouldn't be");
				}
				
				if (!Arrays.equals(planeRead, planeWritten)) {
					
					throw new RuntimeException("plane read does not match plane written");
				}
				
				Arrays.fill(planeWritten, (short)0);
				
				Arrays.fill(planeRead, (short)0);
			}			
		}
	}
}