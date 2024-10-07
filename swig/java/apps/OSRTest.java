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
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

import org.gdal.osr.SpatialReference;

public class OSRTest {
      public static void main(String[] args) throws Exception {
          SpatialReference srs = new SpatialReference(null);
          srs.ImportFromEPSGA(4326);
          if( !srs.GetAxisName(null, 0).equals("Geodetic latitude"))
              throw new Exception("srs.GetAxisName(null, 0) = " + srs.GetAxisName(null, 0));
          if( srs.GetAxisOrientation(null, 0) != org.gdal.osr.osr.OAO_North)
              throw new Exception("srs.GetAxisName(null, 0) = " + srs.GetAxisName(null, 0));
          if( srs.EPSGTreatsAsLatLong() != 1 )
              throw new Exception("srs.EPSGTreatsAsLatLong() should return 1");
          if( srs.EPSGTreatsAsNorthingEasting() != 0 )
              throw new Exception("srs.EPSGTreatsAsNorthingEasting() should return 0");
      }
}
