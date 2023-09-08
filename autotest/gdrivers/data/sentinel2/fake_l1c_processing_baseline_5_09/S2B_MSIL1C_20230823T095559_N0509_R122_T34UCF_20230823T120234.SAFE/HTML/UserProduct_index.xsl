<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">


<xsl:template match="/">
  <html>
  <head>
	<style type="text/css">
		body{
  
		  color: #003388;
		}
		.box-table-a {
		    border-collapse: collapse;
		    font-family: "Lucida Sans Unicode","Lucida Grande",Sans-Serif;
		    font-size: 12px;
		    margin: 10px;
		    text-align: left;
		    width: 90%;
		}
		
		
		.box-table-a th{
		    background: none repeat scroll 0 0 #B9C9FE;
		    border-bottom: 1px solid #FFFFFF;
		    border-top: 4px solid #AABCFE;
		    color: #003399;
		    font-size: 13px;
		    font-weight: normal;
		    padding: 8px;
		}
		
		
		.box-table-a td {
		    background: none repeat scroll 0 0 #E8EDFF;
		    border-bottom: 1px solid #FFFFFF;
		    border-top: 1px solid rgba(0, 0, 0, 0);
		    color: #666699;
		    padding: 8px;
		}
		
		
		pre {
		    border-bottom: 1px solid #CCCCCC;
		    color: #666699;
		    padding: 6px 8px;
		}
	</style>
  </head>
  <TABLE cellSpacing="0" cellPadding="0" border="0" width="90%" ID="page_header">
	 <TR>
		<tr style="background-image:url(./star_bg.jpg)">
    		<td><img src="banner_1.png" width="100%" height="132" align="left"/></td>
    		<td><img src="banner_2.png" width="100%" height="132"/></td>
    		<td><img src="banner_3.png" width="100%" height="132" align="right"/></td>
  		</tr>
	 </TR>
</TABLE>
  <body>
  <h1>User product metadata</h1>
  <h2>Product Info</h2>
  <table class="box-table-a">
    <tr>
      <th>Product uri</th>
      <th>Processing level</th>
      <th>Product type</th>
      <th>Processing baseline</th>
      <th>Generation time</th>
      <th>Preview image url</th>
    </tr>
    <xsl:for-each select="//Product_Info">
    <tr>
      <td><xsl:value-of select="PRODUCT_URI"/></td>
      <td><xsl:value-of select="PROCESSING_LEVEL"/></td>
      <td><xsl:value-of select="PRODUCT_TYPE"/></td>
      <td><xsl:value-of select="PROCESSING_BASELINE"/></td>
      <td><xsl:value-of select="GENERATION_TIME"/></td>
      <td>
      	<xsl:element name="a">
            <xsl:attribute name="href">
                    <xsl:value-of select="PREVIEW_IMAGE_URL"/>
                        </xsl:attribute>
                            <xsl:value-of select="PREVIEW_IMAGE_URL"/>
       	</xsl:element>
        </td>
    </tr>
    </xsl:for-each>
  </table>
  
  <h2>Datatake <xsl:value-of select="//Datatake/@datatakeIdentifier"/></h2>
  <table class="box-table-a">
    <tr>
      <th>Spacecraft name</th>
      <th>Type</th>
      <th>Sensing start</th>
      <th>Sensing stop</th>
      <th>Sensing orbit number</th>
      <th>Sensing orbit direction</th>
    </tr>
    <xsl:for-each select="//Datatake">
    <tr>
      <td><xsl:value-of select="SPACECRAFT_NAME"/></td>
      <td><xsl:value-of select="DATATAKE_TYPE"/></td>
      <td><xsl:value-of select="DATATAKE_SENSING_START"/></td>
      <td><xsl:value-of select="DATATAKE_SENSING_STOP"/></td>
      <td><xsl:value-of select="SENSING_ORBIT_NUMBER"/></td>
      <td><xsl:value-of select="SENSING_ORBIT_DIRECTION"/></td>
    </tr>
    </xsl:for-each>
  </table>


	<h2>Query Options</h2>
  	<table class="box-table-a">
	    <tr>
	      <th>Full swath datatake</th>
	      <th>Preview image</th>
	      <th>Metadata level</th>
	      <th>Auxiliary data</th>
	      <th>Product format</th>
	      <th>Aggregation flag</th>
	    </tr>
	    <xsl:for-each select="//Product_Info/Query_Options">
	    <tr>
	      <td><xsl:value-of select="FULL_SWATH_DATATAKE"/></td>
	      <td><xsl:value-of select="PREVIEW_IMAGE"/></td>
	      <td><xsl:value-of select="METADATA_LEVEL"/></td>
	      <td><xsl:value-of select="Aux_List/aux/GIPP"/></td>
	      <td><xsl:value-of select="PRODUCT_FORMAT"/></td>
	      <td><xsl:value-of select="AGGREGATION_FLAG"/></td>
	    </tr>
	    </xsl:for-each>
  </table>
  <h3>Selected bands</h3>
  	<UL>
  	  	<xsl:for-each select="//Product_Info/Query_Options/Band_List/BAND_NAME">
			<LI>Band <xsl:value-of select="."/></LI> 
		</xsl:for-each>
	</UL>
	
	<h2>Granules list</h2>
    <xsl:for-each select="//Product_Info/Product_Organisation/Granule_List">
    	Granule: <xsl:value-of select = "Granule/@granuleIdentifier"/> Image format: <xsl:value-of select = "./Granule/@imageFormat"/>
    	<xsl:for-each select="Granule/IMAGE_ID">
			<Pre>	Image id: <xsl:value-of select = "."/></Pre>
    	</xsl:for-each>
    	<xsl:for-each select="Granule/IMAGE_FILE">
			<Pre>	Image file: <xsl:value-of select = "."/></Pre>
    	</xsl:for-each>
		<br></br>
    </xsl:for-each>

	<h2>Product Image Characteristics</h2>
	    <table class="box-table-a">
		    <tr>
		      <th>Physical gains</th>
		      <th>Reference band</th>
		      <th>On board compression mode</th>
		    </tr>
		    <tr>
		      <td><xsl:value-of select="//Product_Image_Characteristics/PHYSICAL_GAINS"/></td>
		      <td><xsl:value-of select="//Product_Image_Characteristics/REFERENCE_BAND"/></td>
		      <td><xsl:value-of select="//Product_Image_Characteristics/ON_BOARD_COMPRESSION_MODE"/></td>
		    </tr>
	  	</table>
	  <h2>Geometric info</h2>
	  <Pre>Raster CS type: <xsl:value-of select = "//RASTER_CS_TYPE"/>   Pixel origin: <xsl:value-of select = "//PIXEL_ORIGIN"/></Pre>
	  <h1>Global Footprint</h1>
	  	<xsl:value-of select="//EXT_POS_LIST"/>
	  <h1>Coordinate reference system</h1>
	  	<table class="box-table-a">
		    <tr>
		      <th>GEO tables</th>
		      <th>Horizontal CS type</th>
		      <th>Horizontal CS name</th>
		      <th>Horizontal CS code</th>
		    </tr>
		    <tr>
		      <td><xsl:value-of select="//Coordinate_Reference_System/GEO_TABLES"/></td>
		      <td><xsl:value-of select="//Coordinate_Reference_System/Horizontal_CS/HORIZONTAL_CS_TYPE"/></td>
		      <td><xsl:value-of select="//Coordinate_Reference_System/Horizontal_CS/HORIZONTAL_CS_NAME"/></td>
		      <td><xsl:value-of select="//Coordinate_Reference_System/Horizontal_CS/HORIZONTAL_CS_CODE"/></td>
		    </tr>
	  	</table>
	  	<h2>Geometric header list</h2>
	    <xsl:for-each select="//Geometric_Header">
	    	<h3> Geometry Header (Geometry: <xsl:value-of select = "./@geometry"/> GPS time: <xsl:value-of select = "./GPS_TIME"/> Line index: <xsl:value-of select = "./LINE_INDEX"/>)</h3>  
			<h3>Pointing Angles</h3>	    	
				<h4>Satellite Reference</h4>	
				<table class="box-table-a">
				    <tr>
				      <th>Roll</th>
				      <th>Pitch</th>
				      <th>Yaw</th>
				    </tr>
				    <tr>
				      <td><xsl:value-of select="./Pointing_Angles/Satellite_Reference/ROLL"/></td>
				      <td><xsl:value-of select="./Pointing_Angles/Satellite_Reference/PITCH"/></td>
				      <td><xsl:value-of select="./Pointing_Angles/Satellite_Reference/YAW"/></td>
				    </tr>
			  	</table>
			  	
	  			<h4>Image Reference</h4>
				<table class="box-table-a">
				    <tr>
				      <th>PSI X</th>
				      <th>PSI Y</th>
				    </tr>
				    <tr>
				      <td><xsl:value-of select="./Pointing_Angles/Image_Reference/PSI_X"/></td>
				      <td><xsl:value-of select="./Pointing_Angles/Image_Reference/PSI_Y"/></td>
				    </tr>
		  		</table>
	  	<xsl:for-each select="./Located_Geometric_Header">
	  		<h3>Located Geometric Header (Position: <xsl:value-of select="./@pos"/> Orientation: <xsl:value-of select="./ORIENTATION"/>)</h3>
	  			<h3>Incidence Angles</h3>
				<table class="box-table-a">
				    <tr>
				      <th>Zenith angle</th>
				      <th>Azimuth angle</th>
				    </tr>
				    <tr>
				      <td><xsl:value-of select="./Incidence_Angles/ZENITH_ANGLE"/></td>
				      <td><xsl:value-of select="./Incidence_Angles/AZIMUTH_ANGLE"/></td>
				    </tr>
		  		</table>
		  		
		  		<h3>Solar Angles</h3>
				<table class="box-table-a">
				    <tr>
				      <th>Zenith angle</th>
				      <th>Azimuth angle</th>
				    </tr>
				    <tr>
				      <td><xsl:value-of select="./Solar_Angles/ZENITH_ANGLE"/></td>
				      <td><xsl:value-of select="./Solar_Angles/AZIMUTH_ANGLE"/></td>
				    </tr>
		  		</table>
		  		
				<h3>Pixel Size</h3>
				<table class="box-table-a">
				    <tr>
				      <th>Along track</th>
				      <th>Across tracke</th>
				    </tr>
				    <tr>
				      <td><xsl:value-of select="./Pixel_Size/ALONG_TRACK"/></td>
				      <td><xsl:value-of select="./Pixel_Size/ACROSS_TRACK"/></td>
				    </tr>
		  		</table>

			</xsl:for-each>
		</xsl:for-each>
		<h2>Auxiliary_Data</h2>
	    	<h3>GIPP Files</h3>
	    	<xsl:for-each select="//GIPP_List_Ref/GIPP_FILENAME">
				<Pre>	File name: <xsl:value-of select = "."/></Pre>
	    	</xsl:for-each>
	    	<h3>PRODUCTION DEM TYPE</h3>
	    		<xsl:value-of select = "//PRODUCTION_DEM_TYPE"/>
	    	<h3>IERS BULLETIN FILENAME</h3>
	    		<xsl:value-of select = "//IERS_BULLETIN_FILENAME"/>
	    
		<h2>Quality Indicators</h2>
		<h3>Cloud Coverage Assessment: <xsl:value-of select = "//Cloud_Coverage_Assessment"/></h3>
		<h3>Technical Quality Assessment</h3>
		<table class="box-table-a">
		    <tr>
		      <th>Degraded anc data</th>
		      <th>Degraded msi data</th>
		    </tr>
		    <tr>
		      <td><xsl:value-of select="//Technical_Quality_Assessment/DEGRADED_ANC_DATA_PERCENTAGE"/></td>
		      <td><xsl:value-of select="//Technical_Quality_Assessment/DEGRADED_MSI_DATA_PERCENTAGE"/></td>
		    </tr>
  		</table>
  		<h3>Quality Inspection</h3>
		<table class="box-table-a">
			<xsl:for-each select="//Quality_Inspections//quality_check">
				<tr>
					<td>
			    		<xsl:value-of select="@checkType"/>
		     		</td>
		     		<td>
		      			<xsl:value-of select="."/>
		      		</td>
		      	</tr>
		    </xsl:for-each>
  		</table>
		<h3>Failed Inspection</h3>
		<table class="box-table-a">
		    <tr>
		      <th>Data strip report name</th>
		      <th>Geometric quality flag</th>
		    </tr>
		    <tr>
		      <td><xsl:value-of select="//Failed_Inspections/Datastrip_Report/REPORT_FILENAME"/></td>
		      <td><xsl:value-of select="//Failed_Inspections/Granule_Report/REPORT_FILENAME"/></td>
		    </tr>
  		</table>
   </body>
  </html>
</xsl:template>

</xsl:stylesheet> 