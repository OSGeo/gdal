#ifndef __FIT_H__
#define __FIT_H__

#include "gdal.h"

struct FITinfo {
    unsigned short magic;	// file ident
    unsigned short version;	// file version
    unsigned int xSize;		// image size
    unsigned int ySize;
    unsigned int zSize;
    unsigned int cSize;
    int dtype;			// data type
    int order;			// RGBRGB.. or RR..GG..BB..
    int space;			// coordinate space
    int cm;			// color model
    unsigned int xPageSize;	// page size
    unsigned int yPageSize;
    unsigned int zPageSize;
    unsigned int cPageSize;
				// NOTE: a word of padding is inserted here
				//       due to struct alignment rules
    double minValue;		// min/max pixel values
    double maxValue;
    unsigned int dataOffset;	// offset to first page of data

    // non-header values
    unsigned int userOffset;	// offset to area of user data
};

struct FIThead02 {		// file header for version 02
    unsigned short magic;	// file ident
    unsigned short version;	// file version
    unsigned int xSize;		// image size
    unsigned int ySize;
    unsigned int zSize;
    unsigned int cSize;
    int dtype;			// data type
    int order;			// RGBRGB.. or RR..GG..BB..
    int space;			// coordinate space
    int cm;			// color model
    unsigned int xPageSize;	// page size
    unsigned int yPageSize;
    unsigned int zPageSize;
    unsigned int cPageSize;
    short _padding;		// NOTE: a word of padding is inserted here
				//       due to struct alignment rules
    double minValue;		// min/max pixel values
    double maxValue;
    unsigned int dataOffset;	// offset to first page of data
    // user extensible area...
};


struct FIThead01 {		// file header for version 01
    unsigned short magic;	// file ident
    unsigned short version;	// file version
    unsigned int xSize;		// image size
    unsigned int ySize;
    unsigned int zSize;
    unsigned int cSize;
    int dtype;			// data type
    int order;			// RGBRGB.. or RR..GG..BB..
    int space;			// coordinate space
    int cm;			// color model
    unsigned int xPageSize;	// page size
    unsigned int yPageSize;
    unsigned int zPageSize;
    unsigned int cPageSize;
    unsigned int dataOffset;	// offset to first page of data
    // user extensible area...
};

#ifdef __cplusplus
extern "C" {
#endif

GDALDataType fitDataType(int dtype);
int fitGetDataType(GDALDataType eDataType);
int fitGetColorModel(GDALColorInterp colorInterp, int nBands);

#ifdef __cplusplus
}
#endif

#endif // __FIT_H__
