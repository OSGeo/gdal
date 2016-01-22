/******************************************************************************
 * $Id$
 *
 * Name:     GDALtest.java
 * Project:  GDAL SWIG Interface
 * Purpose:  Sample Java application showing some basic loading of raster data 
 * Author:   Benjamin Collins, The MITRE Corporation
 *
 *
 * $Log$
 * Revision 1.1  2006/02/08 19:39:03  collinsb
 * Initial version
 *
 *
*/


import java.awt.BorderLayout;
import java.awt.color.ColorSpace;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.image.BandedSampleModel;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.ComponentColorModel;
import java.awt.image.DataBuffer;
import java.awt.image.DataBufferByte;
import java.awt.image.DataBufferInt;
import java.awt.image.DataBufferShort;
import java.awt.image.Raster;
import java.awt.image.SampleModel;
import java.awt.image.WritableRaster;
import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;

import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;

import org.gdal.gdal.Band;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdal.GCP;
import org.gdal.gdal.gdal;
import org.gdal.gdalconst.gdalconst;
import org.gdal.gdalconst.gdalconstConstants;

public class GDALtest extends JFrame implements ActionListener{

	BufferedImage image = null;
	JLabel canvas = null;
	JButton load = null;

	static {
		System.out.println("GDAL init...");
		gdal.AllRegister();
		int count = gdal.GetDriverCount();
		System.out.println(count + " available Drivers");
		for (int i = 0; i < count; i++) {
			try {
				Driver driver = gdal.GetDriver(i);
				System.out.println(" " + driver.getShortName() + " : "
						+ driver.getLongName());
			} catch (Exception e) {
				System.err.println("Error loading driver " + i);
			}
		}
	}
	
	public GDALtest() {		
		load = new JButton("Load Image");
		load.addActionListener(this);

		canvas = new JLabel();
		canvas.setSize(1024, 768);

		this.getContentPane().setLayout(new BorderLayout());
		this.getContentPane().add(load, BorderLayout.NORTH);
		this.getContentPane().add(canvas, BorderLayout.SOUTH);
		this.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);

		this.setSize(1024, 768);
		this.show();
	}
	
	public void setImage(BufferedImage image) {
		ImageIcon icon = new ImageIcon(image);
		
		if(this.canvas != null) {
			canvas.setIcon(icon);
		}
	}
	
	public BufferedImage openFile(File f) {
		Dataset poDataset = null;
		try {
			poDataset = (Dataset) gdal.Open(f.getAbsolutePath(),
					gdalconst.GA_ReadOnly);
			if (poDataset == null) {
				System.out.println("The image could not be read.");
				printLastError();
				return null;
			}
		} catch(Exception e) {
			System.err.println("Exception caught.");
			System.err.println(e.getMessage());
			e.printStackTrace();
			return null;
		}
		double[] adfGeoTransform = new double[6];

		System.out.println("Driver: " + poDataset.GetDriver().GetDescription());

		System.out.println("Size is: " + poDataset.getRasterXSize() + "x"
				+ poDataset.getRasterYSize() + "  bands:"
				+ poDataset.getRasterCount());

		if (poDataset.GetProjectionRef() != null)
			System.out.println("Projection is `" + poDataset.GetProjectionRef()
					+ "'");
		
		Hashtable dict = poDataset.GetMetadata_Dict("");
		Enumeration keys = dict.keys();
		System.out.println(dict.size() + " items of metadata found (via Hashtable dict):");
		while(keys.hasMoreElements()) {
			String key = (String)keys.nextElement();
			System.out.println(" :" + key + ":==:" + dict.get(key) + ":");
		}

		Vector list = poDataset.GetMetadata_List("");
		Enumeration enumerate = list.elements();
		System.out.println(list.size() + " items of metadata found (via Vector list):");
		while(enumerate.hasMoreElements()) {
			String s = (String)enumerate.nextElement();
			System.out.println(" " + s);
		}
		
		Vector GCPs = new Vector();
		poDataset.GetGCPs(GCPs);
		System.out.println("Got " + GCPs.size() + " GCPs");
		Enumeration e = GCPs.elements();
		while(e.hasMoreElements()) {
			GCP gcp = (GCP)e.nextElement();
			System.out.println(" x:" + gcp.getGCPX() +
					" y:" + gcp.getGCPY() +
					" z:" + gcp.getGCPZ() +
					" pixel:" + gcp.getGCPPixel() +
					" line:" + gcp.getGCPLine() +
					" line:" + gcp.getInfo());
		}
		

		poDataset.GetGeoTransform(adfGeoTransform);
		{
			System.out.println("Origin = (" + adfGeoTransform[0] + ", "
					+ adfGeoTransform[3] + ")");

			System.out.println("Pixel Size = (" + adfGeoTransform[1] + ", "
					+ adfGeoTransform[5] + ")");
		}

		Band poBand = null;
		double[] adfMinMax = new double[2];
		Double[] max = new Double[1];
		Double[] min = new Double[1];
		
		int bandCount = poDataset.getRasterCount();
		ByteBuffer[] bands = new ByteBuffer[bandCount];
		int[] banks = new int[bandCount];
		int[] offsets = new int[bandCount];
		
		int xsize = 1024;//poDataset.getRasterXSize();
		int ysize = 1024;//poDataset.getRasterYSize();
		int pixels = xsize * ysize;
		int buf_type = 0, buf_size = 0;

		for(int band = 0; band < bandCount; band++) {
			/* Bands are not 0-base indexed, so we must add 1 */
			poBand = poDataset.GetRasterBand(band+1);
			
			buf_type = poBand.getDataType();
			buf_size = pixels * gdal.GetDataTypeSize(buf_type) / 8;

			System.out.println(" Data Type = "
					+ gdal.GetDataTypeName(poBand.getDataType()));
			System.out.println(" ColorInterp = "
					+ gdal.GetColorInterpretationName(poBand
							.GetRasterColorInterpretation()));
			
			System.out.println("Band size is: " + poBand.getXSize() + "x"
					+ poBand.getYSize());
	
			poBand.GetMinimum(min);
			poBand.GetMaximum(max);
			if(min[0] != null || max[0] != null) {
				System.out.println("  Min=" + min[0] + " Max="
						+ max[0]);
			} else {
				System.out.println("  No Min/Max values stored in raster.");
			}
	
			if (poBand.GetOverviewCount() > 0) {
				System.out.println("Band has " + poBand.GetOverviewCount()
						+ " overviews.");
			}
	
			if (poBand.GetRasterColorTable() != null) {
				System.out.println("Band has a color table with "
						+ poBand.GetRasterColorTable().GetCount() + " entries.");
				for(int i = 0; i < poBand.GetRasterColorTable().GetCount(); i++) {
					System.out.println(" " + i + ": " + 
							poBand.GetRasterColorTable().GetColorEntry(i));
				}
			}
			
			System.out.println("Allocating ByteBuffer of size: " + buf_size);

			ByteBuffer data = ByteBuffer.allocateDirect(buf_size);
			data.order(ByteOrder.nativeOrder());

			int returnVal = 0;
			try {
				returnVal = poBand.ReadRaster_Direct(0, 0, poBand.getXSize(), 
						poBand.getYSize(), xsize, ysize,
						buf_type, data);
			} catch(Exception ex) {
				System.err.println("Could not read raster data.");
				System.err.println(ex.getMessage());
				ex.printStackTrace();
				return null;
			}
			if(returnVal == gdalconstConstants.CE_None) {
				bands[band] = data;
			} else {
				printLastError();
			}
			banks[band] = band;
			offsets[band] = 0;
		}

		DataBuffer imgBuffer = null;
		SampleModel sampleModel = null;
		int data_type = 0, buffer_type = 0;
		
		if(buf_type == gdalconstConstants.GDT_Byte) {
			byte[][] bytes = new byte[bandCount][];
			for(int i = 0; i < bandCount; i++) {				
				bytes[i] = new byte[pixels];
				bands[i].get(bytes[i]);
			}
			imgBuffer = new DataBufferByte(bytes, pixels);
			buffer_type = DataBuffer.TYPE_BYTE;
			sampleModel = new BandedSampleModel(buffer_type, 
					xsize, ysize, xsize, banks, offsets);
			data_type = (poBand.GetRasterColorInterpretation() == 
				gdalconstConstants.GCI_PaletteIndex)? 
				BufferedImage.TYPE_BYTE_INDEXED : BufferedImage.TYPE_BYTE_GRAY;
		} else if(buf_type == gdalconstConstants.GDT_Int16) {
			short[][] shorts = new short[bandCount][];
			for(int i = 0; i < bandCount; i++) {				
				shorts[i] = new short[pixels];
				bands[i].asShortBuffer().get(shorts[i]);
			}
			imgBuffer = new DataBufferShort(shorts, pixels);
			buffer_type = DataBuffer.TYPE_USHORT;
			sampleModel = new BandedSampleModel(buffer_type, 
					xsize, ysize, xsize, banks, offsets);
			data_type = BufferedImage.TYPE_USHORT_GRAY;
		} else if(buf_type == gdalconstConstants.GDT_Int32) {
			int[][] ints = new int[bandCount][];
			for(int i = 0; i < bandCount; i++) {				
				ints[i] = new int[pixels];
				bands[i].asIntBuffer().get(ints[i]);
			}
			imgBuffer = new DataBufferInt(ints, pixels);
			buffer_type = DataBuffer.TYPE_INT;
			sampleModel = new BandedSampleModel(buffer_type, 
					xsize, ysize, xsize, banks, offsets);
			data_type = BufferedImage.TYPE_CUSTOM;
		}

		WritableRaster raster = Raster.createWritableRaster(sampleModel, imgBuffer, null);
		BufferedImage img = null;
		ColorModel cm = null;

		if(poBand.GetRasterColorInterpretation() == 
			gdalconstConstants.GCI_PaletteIndex) {
			data_type = BufferedImage.TYPE_BYTE_INDEXED;
			cm = poBand.GetRasterColorTable().getIndexColorModel(
								gdal.GetDataTypeSize(buf_type));
			img = new BufferedImage(cm, raster, false, null);
		} else {
			ColorSpace cs = null;
			if(bandCount > 2){
				cs = ColorSpace.getInstance(ColorSpace.CS_sRGB);
				cm = new ComponentColorModel(cs, false, false, 
						ColorModel.OPAQUE, buffer_type);
				img = new BufferedImage(cm, raster, true, null);
			} else {
				img = new BufferedImage(xsize, ysize,
						data_type);
				img.setData(raster);
			}
		}
		return img;
	}

	public void printLastError() {
		System.out.println("Last error: " + gdal.GetLastErrorMsg());
		System.out.println("Last error no: " + gdal.GetLastErrorNo());
		System.out.println("Last error type: " + gdal.GetLastErrorType());
	}

	public void actionPerformed(ActionEvent arg0) {
		System.out.println("Loading file chooser...");
		JFileChooser chooser = new JFileChooser();
		int result = chooser.showOpenDialog(this);
		if(result == JFileChooser.APPROVE_OPTION) {
			/* open the image! */
			BufferedImage tmpImage = openFile(chooser.getSelectedFile());
			setImage(tmpImage);
		}
	}

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		GDALtest test = new GDALtest();
                if (args.length >= 1)
                {
                    BufferedImage tmpImage = test.openFile(new File(args[0]));
                    test.setImage(tmpImage);
                }
	}

}
