
import _gdal

GDT_Byte = _gdal.GDT_Byte

def Open(file,access=_gdal.GA_ReadOnly):
    _gdal.GDALAllRegister()
    _obj = _gdal.GDALOpen(file,access)
    if _obj:
        _gdal.GDALDereferenceDataset( _obj )
        return Dataset(_obj)
    else:
        return None

class Dataset:

    def __init__(self, _obj):
        self._o = _obj
        _gdal.GDALReferenceDataset( _obj )
        self.RasterXSize = _gdal.GDALGetRasterXSize(self._o)
        self.RasterYSize = _gdal.GDALGetRasterYSize(self._o)
        self.RasterCount = _gdal.GDALGetRasterCount(self._o)

        self._band = []
        for i in range(self.RasterCount):
            self._band.append(Band(_gdal.GDALGetRasterBand(self._o,i+1)))

    def __del__(self):
        if self._o:
            if _gdal.GDALDereferenceDataset(self._o) <= 0:
                _gdal.GDALClose(self._o)

    def GetRasterBand(self, i):
        if i > 0 & i <= self.RasterCount:
            return self._band[i-1]
        else:
            return None

    def GetGeoTransform(self):
        return _gdal.GDALGetGeoTransform(self._o)

    def SetGeoTransform(self,transform):
        _gdal.GDALSetGeoTransform(self._o,transform)

    def GetProjection(self):
        return _gdal.GDALGetProjectionRef(self._o)

    def GetProjectionRef(self):
        return _gdal.GDALGetProjectionRef(self._o)

class Band:            
    def __init__(self, _obj):
        self._o = _obj

    def ReadRaster(self, xoff, yoff, xsize, ysize,
                   buf_xsize, buf_ysize, buf_type ):
        return _gdal.GDALReadRaster(self._o, xoff, yoff, xsize, ysize,
                                    buf_xsize, buf_ysize, buf_type )
    
            
    
