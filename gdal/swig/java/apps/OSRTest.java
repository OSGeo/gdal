/******************************************************************************
 * $Id $
 *
 * Name:     OSRTest.java
 * Project:  GDAL Java Interface
 * Purpose:  OSR Test
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault
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

import org.gdal.osr.SpatialReference;

public class OSRTest {
      public static void main(String[] args) throws Exception {
          SpatialReference srs = new SpatialReference(null);
          srs.ImportFromEPSGA(4326);
          if( !srs.GetAxisName(null, 0).equals("Latitude"))
              throw new Exception("srs.GetAxisName(null, 0) = " + srs.GetAxisName(null, 0));
          if( srs.GetAxisOrientation(null, 0) != org.gdal.osr.osr.OAO_North)
              throw new Exception("srs.GetAxisName(null, 0) = " + srs.GetAxisName(null, 0));
      }
}
