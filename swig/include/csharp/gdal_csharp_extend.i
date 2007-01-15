/******************************************************************************
 * $Id$
 *
 * Name:     gdal_csharp_extensions.i
 * Project:  GDAL SWIG Interface
 * Purpose:  C# specific GDAL extensions 
 * Author:   Tamas Szekeres (szekerest@gmail.com)
 *
 
 * $Log$
 * Revision 1.3  2006/11/23 22:50:53  tamas
 * C# ExportToWkb support
 *
 * Revision 1.2  2006/11/05 22:13:27  tamas
 * Adding the C# specific ReadRaster/WriteRaster
 *
 * Revision 1.1  2006/11/04 22:10:49  tamas
 * gdal csharp specific extensions
 *
 */
 
/******************************************************************************
 * GDAL raster R/W support                                                    *
 *****************************************************************************/

%extend GDALRasterBandShadow 
{
	%apply (void *buffer_ptr) {void *buffer};
	CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, void* buffer,
                          int buf_xSize, int buf_ySize, GDALDataType buf_type, 
                          int pixelSpace, int lineSpace) {
       return GDALRasterIO( self, GF_Read, xOff, yOff, xSize, ySize, 
		        buffer, buf_xSize, buf_ySize, buf_type, pixelSpace, lineSpace );
    }
    CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, void* buffer,
                          int buf_xSize, int buf_ySize, GDALDataType buf_type, 
                          int pixelSpace, int lineSpace) {
       return GDALRasterIO( self, GF_Write, xOff, yOff, xSize, ySize, 
		        buffer, buf_xSize, buf_ySize, buf_type, pixelSpace, lineSpace );
    }
    %clear void *buffer;
}
