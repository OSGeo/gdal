/*
 * $Id$
 */

/*
 * $Log$
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
	CPLErr ReadRasterInternal(int xOff, int yOff, int xSize, int ySize, void* buffer,
                          int buf_xSize, int buf_ySize, GDALDataType buf_type, 
                          int pixelSpace, int lineSpace) {
       return GDALRasterIO( self, GF_Read, xOff, yOff, xSize, ySize, 
		        buffer, buf_xSize, buf_ySize, buf_type, pixelSpace, lineSpace );
    }
    CPLErr WriteRasterInternal(int xOff, int yOff, int xSize, int ySize, void* buffer,
                          int buf_xSize, int buf_ySize, GDALDataType buf_type, 
                          int pixelSpace, int lineSpace) {
       return GDALRasterIO( self, GF_Write, xOff, yOff, xSize, ySize, 
		        buffer, buf_xSize, buf_ySize, buf_type, pixelSpace, lineSpace );
    }
    %clear void *buffer;
}
