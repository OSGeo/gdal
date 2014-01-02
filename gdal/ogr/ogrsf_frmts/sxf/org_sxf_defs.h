/******************************************************************************
 * $Id: org_sxf_defs.h  $
 *
 * Project:  SXF Translator
 * Purpose:  Include file defining Records Structures for file reading and 
 *           basic constants.
 * Author:   Ben Ahmed Daho Ali, bidandou(at)yahoo(dot)fr
 *           Dmitry Baryshnikov, polimax@mail.ru
 *           Alexandr Lisovenko, alexander.lisovenko@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Ben Ahmed Daho Ali
 * Copyright (c) 2013, NextGIS
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
 *
 ******************************************************************************
 * Structure of the SXF file : 
 * ----------------------
 *    - Header 
 *    - Passport
 *    - Descriptor of data
 *    - Records 
 *         - Title of the record 
 *         - The certificate of the object (the geomety)
 *             - sub-objects
 *             - The graphic description of object
 *             - The description of the vector of the tying of the 3d- model of object
 *         - Semantics of object
 *
 * Notes  : 
 * -------
 * Note 1.  Flag of the state of data (2 bits):
 * xxxxxx11- given in the state e (size of the exchange of data).
 *
 * Note 2.  Flag of the correspondence to projection (1 bit):
 * xxxxx0xx - do not correspond to the projection (i.e. map it can have turning 
 *                 relative to true position and certain deformation);
 * xxxxx1xx - correspond to projection.
 *
 * Note 3.  Flag of the presence of real coordinates (2 bits):
 * xxx00xxx - entire certificate of objects is represented in the conditional 
 *                                 system of coordinates (in the samples);
 * xxx11xxx - entire certificate of objects is represented in the real coordinates 
 *            in the locality in accordance with the specifications of sheet 
 *            (projection, the coordinate system, unit of measurement), 
 *            the data about the scale and the discretion of digitization bear 
 *            reference nature.
 *
 * Note 4. Flag of the method of coding (2 bits):
 * x00xxxxx - the classification codes of objects and semantic characteristics 
 *          are represented by the decimal numbers, recorded in the binary 
 *          form (for example: the code of the object “32100000” will be written 
 *          down in the form 0x01E9CEA0, the code of semantics “253” - in the form 0x00FD).
 *
 * Note 5. Table of generalization (1 bit):
 * 0xxxxxxx - the level of generalization is assigned according to the table of the 
 *           general maps (it is described in Table 2.4);
 * 1xxxxxxx - noload condition the level of generalization is assigned according to 
 *           the table of the large-scale maps (it is described in Table 2.5).
 *
 * Note 6.  Flag of coding the texts of the Texts objects (1 bytes):
 * 0- in the coding ASCIIZ (Dos);
 * 1- in the coding ANSI (Windows);
 * 2- in the coding KOI-8 (Unix).
 *
 * Note 7.  Flag of the accuracy of coordinates (1 bytes):
 * 0 – are not established;
 * 1 – the increased accuracy of storage of coordinates (meters, radians or degrees);
 * 2 – of coordinate are recorded with an accuracy to centimeter (meters, 2 signs after comma);
 * 3 – coordinates are recorded with an accuracy to millimeter (meters, 3 sign after comma).
 *
 * Note 8. Form of the framework (1 byte):
 * -1- it is not established;
 *  0- map is unconfined by the framework;
 *  1- trapeziform without the salient points;
 *  2- trapeziform with the salient points;
 *  3- rectangular;
 *  4- circular;
 *  5- arbitrary.
 *
 * Note 9. Sign of output to the framework (4 bits):
 * 0000xxxx - there are no outputs to the framework;
 * 1000xxxx - northern framework;
 * 0100xxxx - eastern framework;
 * 0010xxxx - southern framework;
 * 0001xxxx - western framework.
 *
 * Note 10. Size of the element of certificate (1 bit):
 * xxxxx0xx - 2 bytes (for the integer value); 
 *            4 bytes (for the floating point); 
 * xxxxx1xx - 4 bytes (for the integer value); 
 *            8 bytes (for the floating point).
 *
 * Note 11. Sign of certificate with the text (1 bit): 
 * xxxx0xxx - certificate contains only the coordinates of points; 
 * xxxx1xxx - no-load condition certificate contains the text of signature, 
 *         is allowed only for the objects of the type "signature" or 
 *         "the template of signature".
 *
 * Note 12. [Masshtabiruemost] of drawing (sign) (1 bit):
 * xx0xxxxx - arbitrary symbol of object not scaled;
 * xx1xxxxx - the arbitrary symbol of object is scaled during the mapping.
 * 
 * Note 13. Sign of the construction of spline on the certificate (2 bits):
 * 00xxxxxx – the construction of spline with the visualization is not carried out;
 * 01xxxxxx – smoothing out spline (cutting angles);
 * 10xxxxxx – enveloping spline (it penetrates all points of certificate).
 ****************************************************************************/

#ifndef SXF_DEFS_H
#define SXF_DEFS_H

#define IDSXF          0x00465853     /* SXF  */

#define IDSXFDATA      0x00544144     /* DAT  */
#define IDSXFOBJ       0X7FFF7FFF     /* Object */
#define IDSXFGRAPH     0X7FFF7FFE     /* graphics section */
#define IDSXFVECT3D    0X7FFF7FFD     /* 3D vector section */

#include <map>

#include "cpl_port.h"

enum SXFDataState /* Flag of the state of the data (Note 1) */
{
    SXF_DS_UNKNOWN = 0,
    SXF_DS_EXCHANGE = 8
};

enum SXFCodingType /* Flag of the semantics coding type (Note 4) */
{
    SXF_SEM_DEC = 0,
    SXF_SEM_HEX = 1,
    SXF_SEM_TXT = 2
};

enum SXFGeneralizationType /* Flag of the source for generalization data (Note 5) */
{
    SXF_GT_SMALL_SCALE = 0,
    SXF_GT_LARGE_SCALE = 1
};

enum SXFTextEncoding /* Flag of text encoding (Note 6) */
{
    SXF_ENC_DOS = 0,
    SXF_ENC_WIN = 1,
    SXF_ENC_KOI_8 = 2
};

enum SXFCoordinatesAccuracy /* Flag of coordinate storing accuracy (Note 7) */
{
    SXF_COORD_ACC_UNDEFINED = 0,
    SXF_COORD_ACC_HIGH = 1, //meters, radians or degree
    SXF_COORD_ACC_CM = 2,   //cantimeters
    SXF_COORD_ACC_MM = 3,   //millimeters
    SXF_COORD_ACC_DM = 4    //decimeters
};

typedef struct
{
//    SXFDataState   stDataState;         /* Flag of the state of the data (Note 1) may be will be needed in future*/
    bool bProjectionDataCompliance;     /* Flag of the correspondence to the projection (Note 2) */
    bool bRealCoordinatesCompliance;    /* Flag of the presence of the real coordinates (Note 3) */
    SXFCodingType stCodingType;         /* Flag of the semantics coding type (Note 4) */
    SXFGeneralizationType stGenType;    /* Flag of the source for generalization data (Note 5) */
    SXFTextEncoding stEnc;              /* Flag of text encoding (Note 6) */
    SXFCoordinatesAccuracy stCoordAcc;  /* Flag of coordinate storing accuracy (Note 7) */
    bool bSort;
} SXFInformationFlags;

enum SXFCoordinateMeasUnit
{
    SXF_COORD_MU_METRE = 1,
    SXF_COORD_MU_DECIMETRE,
    SXF_COORD_MU_CENTIMETRE,
    SXF_COORD_MU_MILLIMETRE,
    SXF_COORD_MU_DEGREE,
    SXF_COORD_MU_RADIAN
} ;

typedef struct
{
    long double stProjCoords[8]; //X(0) & Y(1) South West, X(2) & Y(3) North West, X(4) & Y(5) North East, X(6) & Y(7) South East
    long double stGeoCoords[8];
    long double stFrameCoords[8];
    OGREnvelope Env;
    OGRSpatialReference *pSpatRef;
    SXFCoordinateMeasUnit eUnitInPlan;
    double dfXOr;
    double dfYOr;
    double dfFalseNorthing;
    double dfFalseEasting;
    GUInt32 nResolution;
    long double dfScale;
    bool bIsRealCoordinates;
    SXFCoordinatesAccuracy stCoordAcc;

} SXFMapDescription;


enum SXFCoordinateType
{
    SXF_CT_RECTANGULAR = 0,
    SXF_CT_GEODETIC
};


/*
 * List of SXF file format geometry types.
 */
enum SXFGeometryType
{
    SXF_GT_Line    = 0,     /* MultiLineString geometric object                  */
    SXF_GT_Polygon = 1,     /* Polygon geometric object                          */
    SXF_GT_Point = 2,       /* MultiPoint geometric object                       */
    SXF_GT_Text = 3,        /* LineString geometric object with associated label */
    SXF_GT_Vector = 4,      /* Vector geometric object with associated label */
    SXF_GT_TextTemplate = 5 /* Text template */
};

enum SXFValueType
{
    SXF_VT_SHORT = 0,     /* 2 byte integer */
    SXF_VT_FLOAT = 1,   /* 2 byte float */
    SXF_VT_INT = 2,    /* 4 byte integer*/
    SXF_VT_DOUBLE = 3  /* 8 byte float */
};

typedef struct
{
    SXFGeometryType eGeomType;  // Geometry type (Note 1)
    SXFValueType eValType;      // size of values (Note 3)
    int bFormat;                 // Has 3D vector (Note 4) /* Format of the certificate (0- linear size, 1-vector format ) */
    GByte bDim;                 // Dimensionality of the idea (0- 2D, 1- 3D) (Note 6)
    int bHasTextSign;           // Sign of certificate with the text (Note 8)
    GUInt32 nPointCount;        // Point count
    GUInt16 nSubObjectCount;    // The sub object count

} SXFRecordDescription;

typedef struct{
    GUInt32 nID;                /* Identifier of the beginning of record (0x7FFF7FFF) */
    GUInt32 nFullLength;        /* The overall length of record (with the title) */
    GUInt32 nGeometryLength;    /* Length of certificate (in bytes) */
    GUInt32 nClassifyCode;      /* Classification code */
    GUInt16 anGroup[2];         /* 0 - group no, 1 - no in group */
    GByte   nRef[3];            /* Reference data */
    GByte   byPadding;
    GUInt32 nPointCount;        /* Point count */
    GUInt16 nSubObjectCount;    /* The sub object count */
    GUInt16 nPointCountSmall;   /* Point count in small geometries */
} SXFRecordHeader;

typedef struct
{
    GUInt16 nCode;       //type
    char   nType;
    char   nScale;
} SXFRecordAttributeInfo;

enum SXFRecordAttributeType
{
    SXF_RAT_ASCIIZ_DOS = 0, //text in DOS encoding
    SXF_RAT_ONEBYTE = 1,    //number 1 byte
    SXF_RAT_TWOBYTE = 2,    //number 2 byte
    SXF_RAT_FOURBYTE = 4,   //number 4 byte
    SXF_RAT_EIGHTBYTE = 8,  //float point number 8 byte
    SXF_RAT_ANSI_WIN = 126, //text in Win encoding
    SXF_RAT_UNICODE = 127,  //text in unicode
    SXF_RAT_BIGTEXT = 128   //text more than 255 chars
};

/************************************************************************/
/*                         SXFPassport                                  */
/************************************************************************/

typedef struct{
    GUInt16 nYear, nMonth, nDay;
} SXFDate;

struct SXFPassport
{
    GUInt32 version;
    SXFDate dtCrateDate;
    CPLString sMapSheet;   
    GUInt32 nScale;
    CPLString sMapSheetName;
    SXFInformationFlags informationFlags;
    SXFMapDescription stMapDescription;
};

typedef struct
{
    char szID[4]; //the file ID should be "SXF"
    GUInt32 nHeaderLength; //the Header length
    GByte nFormatVersion[4]; //the format version (e.g. 4)
    GUInt32 nCheckSum; //check sum
}  SXFHeader;


/************************************************************************/
/*                         RSCInfo                                      */
/************************************************************************/

/*
    RSC File record
*/
typedef struct  {
    GUInt32 nOffset;      //RSC Section offset in bytes from the beginning of the RSC file
    GUInt32 nLenght;      //RSC Section record length
    GUInt32 nRecordCount; //count of records in the section
} RSCSection;

/*
    RSC File header
*/
typedef struct{
    char szID[4];
    GUInt32 nFileLength;
    GUInt32 nVersion;
    GUInt32 nEncoding;
    GUInt32 nFileState;
    GUInt32 nFileModState;
    GUInt32 nLang;                  //1 - en, 2 - rus
    GUInt32 nNextID;
    GByte date[8];
    char szMapType[32];
    char szClassifyName[32];
    char szClassifyCode[8];
    GUInt32 nScale;
    char nScales[4];
    RSCSection Objects;
    RSCSection Semantic;
    RSCSection ClassifySemantic;
    RSCSection Defaults;
    RSCSection Semantics;
    RSCSection Layers;
    RSCSection Limits;
    RSCSection Parameters;
    RSCSection Print;
    RSCSection Palettes;
    RSCSection Fonts;
    RSCSection Libs;
    RSCSection ImageParams;
    RSCSection Tables;
    GByte nFlagKeysAsCodes;
    GByte nFlagPalleteMods;
    GByte Reserved[30];
    GByte szFontEnc[4];
    GUInt32 nColorsInPalette;
} RSCHeader;

#endif  /* SXF_DEFS_H */
