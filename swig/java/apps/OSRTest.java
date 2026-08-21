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

import java.util.Vector;
import java.util.Collections;

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

          /*
           * Test FindMatches binding
           */
          String wkt =
            "PROJCS[\"NAD83 / UTM zone 11N\",\n" +
            "    GEOGCS[\"NAD83\",\n" +
            "        DATUM[\"North_American_Datum_1983\",\n" +
            "            SPHEROID[\"GRS 1980\",6378137,298.257222101,\n" +
            "                AUTHORITY[\"EPSG\",\"7019\"]],\n" +
            "            AUTHORITY[\"EPSG\",\"6269\"]],\n" +
            "        PRIMEM[\"Greenwich\",0],\n" +
            "        UNIT[\"Degree\",0.0174532925199433]],\n" +
            "    PROJECTION[\"Transverse_Mercator\"],\n" +
            "    PARAMETER[\"latitude_of_origin\",0],\n" +
            "    PARAMETER[\"central_meridian\",-117],\n" +
            "    PARAMETER[\"scale_factor\",0.9996],\n" +
            "    PARAMETER[\"false_easting\",500000],\n" +
            "    PARAMETER[\"false_northing\",0],\n" +
            "    UNIT[\"metre\",1,\n" +
            "        AUTHORITY[\"EPSG\",\"9001\"]],\n" +
            "    AXIS[\"Easting\",EAST],\n" +
            "    AXIS[\"Northing\",NORTH]]";
          Vector<String> lines = new Vector<String>();
          Collections.addAll(lines, wkt.split("\n"));

          srs = new SpatialReference(null);
          int importResult = srs.ImportFromESRI(lines);
          if ( srs.IsProjected() != 1)
              throw new Exception("srs.IsProjected() should return 1");
          if ( srs.IsGeographic() != 0)
              throw new Exception("srs.IsGeographic() should return 0");

          int[][] confidence = new int[1][];
          SpatialReference[] matches = srs.FindMatches(null, confidence);
          if ( matches == null || matches.length == 0)
              throw new Exception("No match found where expected");

          if ( matches.length != 1)
              throw new Exception("Found more than one match: "+matches.length);

          SpatialReference matched = matches[0];
          if ( !"EPSG".equals(matched.GetAuthorityName(null)))
              throw new Exception("Authority name not EPSG: "+matched.GetAuthorityName(null));
          if ( !"26911".equals(matched.GetAuthorityCode(null)))
              throw new Exception("Authority code not 26911 "+matched.GetAuthorityCode(null));

          if ( confidence.length != 1)
              throw new Exception("Length of confidence array not 1: "+confidence.length);

          int[] confidenceValues = confidence[0];
          if ( confidenceValues.length != matches.length)
              throw new Exception("Length of confidenceValues array not the same as the matches array: "+confidenceValues.length+" "+ matches.length);
          if ( confidenceValues[0] != 100)
              throw new Exception("Confidence value not as expected "+confidenceValues[0]);
      }
}
