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

		ExtendedDataType dt = ExtendedDataType.Create(gdalconst.GDT_UInt16);

		long[] dims = new long[]{10,6,2,7};

		BigInteger[] dimsB = new BigInteger[dims.length];
		
		for (int i = 0; i < dims.length; i++) {
			
			dimsB[i] = BigInteger.valueOf(dims[i]);
		}
		
		Vector<Dimension> inDims = new Vector<>();
		
		for (int i = 0; i < dims.length; i++) {
			
			Dimension d =
				
				rg.CreateDimension("name"+i, "fieldtype"+i, "direction"+i, dimsB[i]);
			
			inDims.add(d);
		}
		
		MDArray mdarray = null; // rg.CreateMDArray("my_data", inDims, dt);
		
		Vector<Dimension> outDims = null; // (Vector<Dimension>) mdarray.GetDimensions();
		
		if (outDims.size() != dims.length) {
			
			throw new RuntimeException("resulting dimension count "+outDims.size()+" does not equal input dim len "+dims.length);
		}
		
		for (int i = 0; i < dims.length; i++) {
			
			Dimension d = outDims.get(i);
			
			if (d.GetSize().compareTo(dimsB[i]) != 0) {

				throw new RuntimeException("resulting dimension "+i+" has size"+ d.GetSize()+" but should equal "+dims[i]);
			}
			
			if (!d.GetName().equals("name"+i)) {

				throw new RuntimeException("resulting dimension name "+d.GetName()+"does not match name"+i);
			}
			
			if (!d.GetFieldType().equals("fieldtype"+i)) {

				throw new RuntimeException("resulting dimension type "+d.GetFieldType()+"does not match fieldtype"+i);
			}
			
			if (!d.GetDirection().equals("direction"+i)) {

				throw new RuntimeException("resulting dimension direction "+d.GetDirection()+"does not match direction"+i);
			}
		}
	}
	
/*  python test code from gdal source that I can try and replicate

    drv = gdal.GetDriverByName("MEM")
    ds = drv.CreateMultiDimensional("")
    rg = ds.GetRootGroup()
    subg = rg.CreateGroup("subgroup")
    subg.CreateGroup("subsubgroup")

    dim0 = rg.CreateDimension("dim0", "my_type", "my_direction", 2)
    comp0 = gdal.EDTComponent.Create(
        "x", 0, gdal.ExtendedDataType.Create(gdal.GDT_Int16)
    )
    comp1 = gdal.EDTComponent.Create(
        "y", 4, gdal.ExtendedDataType.Create(gdal.GDT_Int32)
    )
    dt = gdal.ExtendedDataType.CreateCompound("mytype", 8, [comp0, comp1])
    ar = rg.CreateMDArray("ar_compound", [dim0], dt)
    assert (
        ar.Write(struct.pack("hi" * 2, 32767, 1000000, -32768, -1000000))
        == gdal.CE_None
    )
    assert ar.SetNoDataValueRaw(struct.pack("hi", 32767, 1000000)) == gdal.CE_None

    dim1 = rg.CreateDimension("dim1", None, None, 3)
    ar = rg.CreateMDArray(
        "ar_2d", [dim0, dim1], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    )
    ar.SetOffset(1)
    ar.SetScale(2)
    ar.SetUnit("foo")
    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=utm +zone=31 +datum=WGS84")
    srs.SetDataAxisToSRSAxisMapping([2, 1])
    ar.SetSpatialRef(srs)
    attr = ar.CreateAttribute("myattr", [], gdal.ExtendedDataType.CreateString())
    attr.WriteString("bar")
    ret = gdal.MultiDimInfo(ds, detailed=True, as_text=True)
*/
}
