from osgeo import gdal, gdal_array
import numpy


def GDALTypeCodeToNumericTypeCodeWithDefault(buf_type, signed_byte, default=None):
    typecode = gdal_array.GDALTypeCodeToNumericTypeCode(buf_type)
    if typecode is None:
        typecode = default

    if buf_type == gdal.GDT_Byte and signed_byte:
        typecode = numpy.int8
    return typecode


def GDALTypeCodeAndNumericTypeCodeFromDataSet(ds):
    buf_type = ds.GetRasterBand(1).DataType
    signed_byte = ds.GetRasterBand(1).GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE') == 'SIGNEDBYTE'
    np_typecode = GDALTypeCodeToNumericTypeCodeWithDefault(buf_type, signed_byte=signed_byte, default=numpy.float32)
    return buf_type, np_typecode
