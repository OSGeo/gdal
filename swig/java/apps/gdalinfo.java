/******************************************************************************
 * $Id$
 *
 * Name:     gdalinfo.java
 * Project:  GDAL SWIG Interface
 * Purpose:  Java port of gdalinfo application
 * Author:   Benjamin Collins, The MITRE Corporation
 *
 * ****************************************************************************
 * Copyright (c) 2009, Even Rouault
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2006, The MITRE Corporation
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


import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;

import org.gdal.gdal.Band;
import org.gdal.gdal.ColorTable;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdal.GCP;
import org.gdal.gdal.gdal;
import org.gdal.gdal.TermProgressCallback;
import org.gdal.gdal.RasterAttributeTable;
import org.gdal.gdalconst.gdalconstConstants;
import org.gdal.osr.CoordinateTransformation;
import org.gdal.osr.SpatialReference;

public class gdalinfo {

	/************************************************************************/
	/*                               Usage()                                */
	/************************************************************************/

	public static void Usage()

	{
		System.out
				.println("Usage: gdalinfo [--help-general] [-mm] [-stats] [-hist] [-nogcp] [-nomd]\n"
				        + "               [-norat] [-noct] [-mdd domain]* [-checksum] datasetname");
		System.exit(1);
	}

	/************************************************************************/
	/*                                main()                                */
	/************************************************************************/

	public static void main(String[] args) {
		{
			Dataset hDataset;
			Band hBand;
			int i, iBand;
			double[] adfGeoTransform = new double[6];
			Driver hDriver;
			Vector papszMetadata;
			boolean bComputeMinMax = false, bSample = false;
			boolean bShowGCPs = true, bShowMetadata = true;
			boolean bStats = false, bApproxStats = true;
                        boolean bShowColorTable = true, bComputeChecksum = false;
                        boolean bReportHistograms = false;
                        boolean bShowRAT = true;
			String pszFilename = null;
                        Vector papszFileList;
                        Vector papszExtraMDDomains = new Vector();

			gdal.AllRegister();

                        args = gdal.GeneralCmdLineProcessor(args);

			if (args.length < 1) {
				Usage();
				System.exit(0);
			}
			/* -------------------------------------------------------------------- */
			/*      Parse arguments.                                                */
			/* -------------------------------------------------------------------- */
			for (i = 0; i < args.length; i++) {
				if (args[i].equals("-mm"))
					bComputeMinMax = true;
                                else if (args[i].equals("-hist"))
					bReportHistograms = true;
				else if (args[i].equals("-stats"))
                                {
					bStats = true;
                                        bApproxStats = false;
                                }
				else if (args[i].equals("-approx_stats"))
				{
					bStats = true;
                                        bApproxStats = true;
                                }
				else if (args[i].equals("-nogcp"))
					bShowGCPs = false;
                                else if( args[i].equals("-noct"))
                                        bShowColorTable = false;
				else if (args[i].equals("-nomd"))
					bShowMetadata = false;
                                else if (args[i].equals("-norat"))
					bShowRAT = false;
                                else if (args[i].equals("-checksum"))
					bComputeChecksum = true;
                                else if (args[i].equals("-mdd") && i + 1 < args.length)
					papszExtraMDDomains.addElement(args[++i]);
				else if (args[i].startsWith("-"))
					Usage();
				else if (pszFilename == null)
					pszFilename = args[i];
				else
					Usage();
			}
			if (pszFilename == null)
				Usage();

			/* -------------------------------------------------------------------- */
			/*      Open dataset.                                                   */
			/* -------------------------------------------------------------------- */
			hDataset = gdal.Open(pszFilename, gdalconstConstants.GA_ReadOnly);

			if (hDataset == null) {
				System.err
						.println("GDALOpen failed - " + gdal.GetLastErrorNo());
				System.err.println(gdal.GetLastErrorMsg());

				//gdal.DumpOpenDatasets( stderr );

				//gdal.DestroyDriverManager();

				//gdal.DumpSharedList( null );

				System.exit(1);
			}

			/* -------------------------------------------------------------------- */
			/*      Report general info.                                            */
			/* -------------------------------------------------------------------- */
			hDriver = hDataset.GetDriver();
			System.out.println("Driver: " + hDriver.getShortName() + "/"
					+ hDriver.getLongName());

                        papszFileList = hDataset.GetFileList( );
                        if( papszFileList.size() == 0 )
                        {
                            System.out.println( "Files: none associated" );
                        }
                        else
                        {
                            Enumeration e = papszFileList.elements();
                            System.out.println( "Files: " + (String)e.nextElement() );
                            while(e.hasMoreElements())
                                System.out.println( "       " +  (String)e.nextElement() );
                        }

			System.out.println("Size is " + hDataset.getRasterXSize() + ", "
					+ hDataset.getRasterYSize());

			/* -------------------------------------------------------------------- */
			/*      Report projection.                                              */
			/* -------------------------------------------------------------------- */
			if (hDataset.GetProjectionRef() != null) {
				SpatialReference hSRS;
				String pszProjection;

				pszProjection = hDataset.GetProjectionRef();

				hSRS = new SpatialReference(pszProjection);
				if (hSRS != null && pszProjection.length() != 0) {
					String[] pszPrettyWkt = new String[1];

					hSRS.ExportToPrettyWkt(pszPrettyWkt, 0);
					System.out.println("Coordinate System is:");
					System.out.println(pszPrettyWkt[0]);
					//gdal.CPLFree( pszPrettyWkt );
				} else
					System.out.println("Coordinate System is `"
							+ hDataset.GetProjectionRef() + "'");

				hSRS.delete();
			}

			/* -------------------------------------------------------------------- */
			/*      Report Geotransform.                                            */
			/* -------------------------------------------------------------------- */
			hDataset.GetGeoTransform(adfGeoTransform);
			{
				if (adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0) {
					System.out.println("Origin = (" + adfGeoTransform[0] + ","
							+ adfGeoTransform[3] + ")");

					System.out.println("Pixel Size = (" + adfGeoTransform[1]
							+ "," + adfGeoTransform[5] + ")");
				} else {
					System.out.println("GeoTransform =");
                                        System.out.println("  " + adfGeoTransform[0] + ", "
                                                        + adfGeoTransform[1] + ", " + adfGeoTransform[2]);
                                        System.out.println("  " + adfGeoTransform[3] + ", "
                                                        + adfGeoTransform[4] + ", " + adfGeoTransform[5]);
                                }
			}

			/* -------------------------------------------------------------------- */
			/*      Report GCPs.                                                    */
			/* -------------------------------------------------------------------- */
			if (bShowGCPs && hDataset.GetGCPCount() > 0) {
				System.out.println("GCP Projection = "
						+ hDataset.GetGCPProjection());

				int count = 0;
				Vector GCPs = new Vector();
				hDataset.GetGCPs(GCPs);

				Enumeration e = GCPs.elements();
				while (e.hasMoreElements()) {
					GCP gcp = (GCP) e.nextElement();
					System.out.println("GCP[" + (count++) + "]: Id="
							+ gcp.getId() + ", Info=" + gcp.getInfo());
					System.out.println("    (" + gcp.getGCPPixel() + ","
							+ gcp.getGCPLine() + ") (" + gcp.getGCPX() + ","
							+ gcp.getGCPY() + "," + gcp.getGCPZ() + ")");
				}

			}

			/* -------------------------------------------------------------------- */
			/*      Report metadata.                                                */
			/* -------------------------------------------------------------------- */
			papszMetadata = hDataset.GetMetadata_List("");
			if (bShowMetadata && papszMetadata.size() > 0) {
				Enumeration keys = papszMetadata.elements();
				System.out.println("Metadata:");
				while (keys.hasMoreElements()) {
					System.out.println("  " + (String) keys.nextElement());
				}
			}
                        
                        Enumeration eExtraMDDDomains = papszExtraMDDomains.elements();
                        while(eExtraMDDDomains.hasMoreElements())
                        {
                            String pszDomain = (String)eExtraMDDDomains.nextElement();
                            papszMetadata = hDataset.GetMetadata_List(pszDomain);
                            if( bShowMetadata && papszMetadata.size() > 0 )
                            {
                                Enumeration keys = papszMetadata.elements();
                                System.out.println("Metadata (" + pszDomain + "):");
                                while (keys.hasMoreElements()) {
					System.out.println("  " + (String) keys.nextElement());
				}
                            }
                        }
                        /* -------------------------------------------------------------------- */
                        /*      Report "IMAGE_STRUCTURE" metadata.                              */
                        /* -------------------------------------------------------------------- */
                        papszMetadata = hDataset.GetMetadata_List("IMAGE_STRUCTURE" );
                        if( bShowMetadata && papszMetadata.size() > 0) {
				Enumeration keys = papszMetadata.elements();
				System.out.println("Image Structure Metadata:");
				while (keys.hasMoreElements()) {
					System.out.println("  " + (String) keys.nextElement());
				}
			}
			/* -------------------------------------------------------------------- */
			/*      Report subdatasets.                                             */
			/* -------------------------------------------------------------------- */
			papszMetadata = hDataset.GetMetadata_List("SUBDATASETS");
			if (papszMetadata.size() > 0) {
				System.out.println("Subdatasets:");
				Enumeration keys = papszMetadata.elements();
				while (keys.hasMoreElements()) {
					System.out.println("  " + (String) keys.nextElement());
				}
			}
                    
                    /* -------------------------------------------------------------------- */
                    /*      Report geolocation.                                             */
                    /* -------------------------------------------------------------------- */
                        papszMetadata = hDataset.GetMetadata_List("GEOLOCATION" );
                        if (papszMetadata.size() > 0) {
                            System.out.println( "Geolocation:" );
                            Enumeration keys = papszMetadata.elements();
                            while (keys.hasMoreElements()) {
                                    System.out.println("  " + (String) keys.nextElement());
                            }
                        }
                    
                    /* -------------------------------------------------------------------- */
                    /*      Report RPCs                                                     */
                    /* -------------------------------------------------------------------- */
                        papszMetadata = hDataset.GetMetadata_List("RPC" );
                        if (papszMetadata.size() > 0) {
                            System.out.println( "RPC Metadata:" );
                            Enumeration keys = papszMetadata.elements();
                            while (keys.hasMoreElements()) {
                                    System.out.println("  " + (String) keys.nextElement());
                            }
                        }

			/* -------------------------------------------------------------------- */
			/*      Report corners.                                                 */
			/* -------------------------------------------------------------------- */
			System.out.println("Corner Coordinates:");
			GDALInfoReportCorner(hDataset, "Upper Left ", 0.0, 0.0);
			GDALInfoReportCorner(hDataset, "Lower Left ", 0.0, hDataset
					.getRasterYSize());
			GDALInfoReportCorner(hDataset, "Upper Right", hDataset
					.getRasterXSize(), 0.0);
			GDALInfoReportCorner(hDataset, "Lower Right", hDataset
					.getRasterXSize(), hDataset.getRasterYSize());
			GDALInfoReportCorner(hDataset, "Center     ",
					hDataset.getRasterXSize() / 2.0,
					hDataset.getRasterYSize() / 2.0);

			/* ==================================================================== */
			/*      Loop over bands.                                                */
			/* ==================================================================== */
			for (iBand = 0; iBand < hDataset.getRasterCount(); iBand++) {
				Double[] pass1 = new Double[1], pass2 = new Double[1];
				double[] adfCMinMax = new double[2];
				ColorTable hTable;

				hBand = hDataset.GetRasterBand(iBand + 1);

				/*if( bSample )
				 {
				 float[] afSample = new float[10000];
				 int   nCount;

				 nCount = hBand.GetRandomRasterSample( 10000, afSample );
				 System.out.println( "Got " + nCount + " samples." );
				 }*/

                                int[] blockXSize = new int[1];
                                int[] blockYSize = new int[1];
                                hBand.GetBlockSize(blockXSize, blockYSize);
				System.out.println("Band "
						+ (iBand+1)
                                                + " Block="
                                                + blockXSize[0] + "x" + blockYSize[0]
						+ " Type="
						+ gdal.GetDataTypeName(hBand.getDataType())
						+ ", ColorInterp="
						+ gdal.GetColorInterpretationName(hBand
								.GetRasterColorInterpretation()));

				String hBandDesc = hBand.GetDescription();
				if (hBandDesc != null && hBandDesc.length() > 0)
					System.out.println("  Description = " + hBandDesc);

				hBand.GetMinimum(pass1);
				hBand.GetMaximum(pass2);
				if(pass1[0] != null || pass2[0] != null || bComputeMinMax) {
                                    System.out.print( "  " );
                                    if( pass1[0] != null )
                                        System.out.print( "Min=" + pass1[0] + " ");
                                    if( pass2[0] != null )
                                        System.out.print( "Max=" + pass2[0] + " ");
                                
                                    if( bComputeMinMax )
                                    {
                                        hBand.ComputeRasterMinMax(adfCMinMax, 0);
                                        System.out.print( "  Computed Min/Max=" + adfCMinMax[0]
							+ "," + adfCMinMax[1]);
                                    }
                        
                                    System.out.print( "\n" );
				}

                                double dfMin[] = new double[1];
                                double dfMax[] = new double[1];
                                double dfMean[] = new double[1];
                                double dfStdDev[] = new double[1];
				if( hBand.GetStatistics( bApproxStats, bStats,
                                                         dfMin, dfMax, dfMean, dfStdDev ) == gdalconstConstants.CE_None )
				{
				    System.out.println( "  Minimum=" + dfMin[0] + ", Maximum=" + dfMax[0] +
                                                        ", Mean=" + dfMean[0] + ", StdDev=" + dfStdDev[0] );
				}

                                if( bReportHistograms )
                                {
                                    int[][] panHistogram = new int[1][];
                                    int eErr = hBand.GetDefaultHistogram(dfMin, dfMax, panHistogram, true, new TermProgressCallback());
                                    if( eErr == gdalconstConstants.CE_None )
                                    {
                                        int iBucket;
                                        int nBucketCount = panHistogram[0].length;
                                        System.out.print( "  " + nBucketCount + " buckets from " +
                                                           dfMin[0] + " to " + dfMax[0] + ":\n  " );
                                        for( iBucket = 0; iBucket < nBucketCount; iBucket++ )
                                            System.out.print( panHistogram[0][iBucket] + " ");
                                        System.out.print( "\n" );
                                    }
                                }

                                if ( bComputeChecksum)
                                {
                                    System.out.println( "  Checksum=" + hBand.Checksum());
                                }

				hBand.GetNoDataValue(pass1);
				if(pass1[0] != null)
				{
					System.out.println("  NoData Value=" + pass1[0]);
				}

				if (hBand.GetOverviewCount() > 0) {
					int iOverview;

					System.out.print("  Overviews: ");
					for (iOverview = 0; iOverview < hBand.GetOverviewCount(); iOverview++) {
						Band hOverview;

						if (iOverview != 0)
							System.out.print(", ");

						hOverview = hBand.GetOverview(iOverview);
						System.out.print(hOverview.getXSize() + "x"
								+ hOverview.getYSize());
					}
					System.out.print("\n");

                                        if ( bComputeChecksum)
                                        {
                                            System.out.print( "  Overviews checksum: " );
                                            for( iOverview = 0; 
                                                iOverview < hBand.GetOverviewCount();
                                                iOverview++ )
                                            {
                                                Band	hOverview;
                            
                                                if( iOverview != 0 )
                                                    System.out.print( ", " );
                            
                                                hOverview = hBand.GetOverview(iOverview);
                                                System.out.print( hOverview.Checksum());
                                            }
                                            System.out.print( "\n" );
                                        }
				}

				if( hBand.HasArbitraryOverviews() )
				{
				    System.out.println( "  Overviews: arbitrary" );
				}


                                int nMaskFlags = hBand.GetMaskFlags(  );
                                if( (nMaskFlags & (gdalconstConstants.GMF_NODATA|gdalconstConstants.GMF_ALL_VALID)) == 0 )
                                {
                                    Band hMaskBand = hBand.GetMaskBand() ;
                        
                                    System.out.print( "  Mask Flags: " );
                                    if( (nMaskFlags & gdalconstConstants.GMF_PER_DATASET) != 0 )
                                        System.out.print( "PER_DATASET " );
                                    if( (nMaskFlags & gdalconstConstants.GMF_ALPHA) != 0 )
                                        System.out.print( "ALPHA " );
                                    if( (nMaskFlags & gdalconstConstants.GMF_NODATA) != 0 )
                                        System.out.print( "NODATA " );
                                    if( (nMaskFlags & gdalconstConstants.GMF_ALL_VALID) != 0 )
                                        System.out.print( "ALL_VALID " );
                                    System.out.print( "\n" );
                        
                                    if( hMaskBand != null &&
                                        hMaskBand.GetOverviewCount() > 0 )
                                    {
                                        int		iOverview;
                        
                                        System.out.print( "  Overviews of mask band: " );
                                        for( iOverview = 0; 
                                            iOverview < hMaskBand.GetOverviewCount();
                                            iOverview++ )
                                        {
                                            Band	hOverview;
                        
                                            if( iOverview != 0 )
                                                System.out.print( ", " );
                        
                                            hOverview = hMaskBand.GetOverview( iOverview );
                                            System.out.print( 
                                                    hOverview.getXSize() + "x" +
                                                    hOverview.getYSize() );
                                        }
                                        System.out.print( "\n" );
                                    }
                                }
                                
				if( hBand.GetUnitType() != null && hBand.GetUnitType().length() > 0)
				{
				     System.out.println( "  Unit Type: " + hBand.GetUnitType() );
				}

                                Vector papszCategories = hBand.GetRasterCategoryNames();
                                if (papszCategories.size() > 0)
                                {
                                    System.out.println( "  Categories:" );
                                    Enumeration eCategories = papszCategories.elements();
                                    i = 0;
				    while (eCategories.hasMoreElements()) {
                                            System.out.println("    " + i + ": " + (String) eCategories.nextElement());
                                            i ++;
                                    }
                                }

				hBand.GetOffset(pass1);
				if(pass1[0] != null && pass1[0].doubleValue() != 0) {
					System.out.print("  Offset: " + pass1[0]);
				}
				hBand.GetScale(pass1);
				if(pass1[0] != null && pass1[0].doubleValue() != 1) {
					System.out.println(",   Scale:" + pass1[0]);
				}

				papszMetadata = hBand.GetMetadata_List("");
				 if( bShowMetadata && papszMetadata.size() > 0 ) {
						Enumeration keys = papszMetadata.elements();
						System.out.println("  Metadata:");
						while (keys.hasMoreElements()) {
							System.out.println("    " + (String) keys.nextElement());
						}
				 }
				if (hBand.GetRasterColorInterpretation() == gdalconstConstants.GCI_PaletteIndex
						&& (hTable = hBand.GetRasterColorTable()) != null) {
					int count;

					System.out.println("  Color Table ("
							+ gdal.GetPaletteInterpretationName(hTable
									.GetPaletteInterpretation()) + " with "
							+ hTable.GetCount() + " entries)");

                                        if (bShowColorTable)
                                        {
                                            for (count = 0; count < hTable.GetCount(); count++) {
                                                    System.out.println(" " + count + ": "
                                                                    + hTable.GetColorEntry(count));
                                            }
                                        }
				}

                                RasterAttributeTable rat = hBand.GetDefaultRAT();
                                if( bShowRAT && rat != null )
                                {
                                    System.out.print("<GDALRasterAttributeTable ");
                                    double[] pdfRow0Min = new double[1];
                                    double[] pdfBinSize = new double[1];
                                    if (rat.GetLinearBinning(pdfRow0Min, pdfBinSize))
                                    {
                                        System.out.print("Row0Min=\"" + pdfRow0Min[0] + "\" BinSize=\"" + pdfBinSize[0] + "\">");
                                    }
                                    System.out.print("\n");
                                    int colCount = rat.GetColumnCount();
                                    for(int col=0;col<colCount;col++)
                                    {
                                        System.out.println("  <FieldDefn index=\"" + col + "\">");
                                        System.out.println("    <Name>" + rat.GetNameOfCol(col) + "</Name>");
                                        System.out.println("    <Type>" + rat.GetTypeOfCol(col) + "</Type>");
                                        System.out.println("    <Usage>" + rat.GetUsageOfCol(col) + "</Usage>");
                                        System.out.println("  </FieldDefn>");
                                    }
                                    int rowCount = rat.GetRowCount();
                                    for(int row=0;row<rowCount;row++)
                                    {
                                        System.out.println("  <Row index=\"" + row + "\">");
                                        for(int col=0;col<colCount;col++)
                                        {
                                            System.out.println("    <F>" + rat.GetValueAsString(row, col)+ "</F>");
                                        }
                                        System.out.println("  </Row>");
                                    }
                                    System.out.println("</GDALRasterAttributeTable>");
                                }
			}

			hDataset.delete();

                        /* Optional */
			gdal.GDALDestroyDriverManager();

			System.exit(0);
		}
	}

	/************************************************************************/
	/*                        GDALInfoReportCorner()                        */
	/************************************************************************/

	static boolean GDALInfoReportCorner(Dataset hDataset, String corner_name,
			double x, double y)

	{
		double dfGeoX, dfGeoY;
		String pszProjection;
		double[] adfGeoTransform = new double[6];
		CoordinateTransformation hTransform = null;

		System.out.print(corner_name + " ");

		/* -------------------------------------------------------------------- */
		/*      Transform the point into georeferenced coordinates.             */
		/* -------------------------------------------------------------------- */
		hDataset.GetGeoTransform(adfGeoTransform);
		{
			pszProjection = hDataset.GetProjectionRef();

			dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x
					+ adfGeoTransform[2] * y;
			dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x
					+ adfGeoTransform[5] * y;
		}

		if (adfGeoTransform[0] == 0 && adfGeoTransform[1] == 0
				&& adfGeoTransform[2] == 0 && adfGeoTransform[3] == 0
				&& adfGeoTransform[4] == 0 && adfGeoTransform[5] == 0) {
			System.out.println("(" + x + "," + y + ")");
			return false;
		}

		/* -------------------------------------------------------------------- */
		/*      Report the georeferenced coordinates.                           */
		/* -------------------------------------------------------------------- */
		System.out.print("(" + dfGeoX + "," + dfGeoY + ") ");

		/* -------------------------------------------------------------------- */
		/*      Setup transformation to lat/long.                               */
		/* -------------------------------------------------------------------- */
		if (pszProjection != null && pszProjection.length() > 0) {
			SpatialReference hProj, hLatLong = null;

			hProj = new SpatialReference(pszProjection);
			if (hProj != null)
				hLatLong = hProj.CloneGeogCS();

			if (hLatLong != null) {
				/* New in GDAL 2.0. Before was "new CoordinateTransformation(srs,dst)". */
				hTransform = CoordinateTransformation.CreateCoordinateTransformation(hProj, hLatLong);
            }

			if (hProj != null)
				hProj.delete();
		}

		/* -------------------------------------------------------------------- */
		/*      Transform to latlong and report.                                */
		/* -------------------------------------------------------------------- */
		if (hTransform != null) {
			double[] transPoint = new double[3];
			hTransform.TransformPoint(transPoint, dfGeoX, dfGeoY, 0);
			System.out.print("(" + gdal.DecToDMS(transPoint[0], "Long", 2));
			System.out
					.print("," + gdal.DecToDMS(transPoint[1], "Lat", 2) + ")");
		}

		if (hTransform != null)
			hTransform.delete();

		System.out.println("");

		return true;
	}
}
