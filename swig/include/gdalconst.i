%module gdalconst 

// GDALDataType
#define GDT_Unknown 1;
%constant GDT_Byte = 1;
%constant GDT_UInt16 = 2;
%constant GDT_Int16 = 3;
%constant GDT_UInt32 = 4;
%constant GDT_Int32 = 5;
%constant GDT_Float32 = 6;
%constant GDT_Float64 = 7;
%constant GDT_CInt16 = 8;
%constant GDT_CInt32 = 9;
%constant GDT_CFloat32 = 10;
%constant GDT_CFloat64 = 11;
%constant GDT_TypeCount = 12;

// GDALAccess
%constant GA_ReadOnly = 0;
%constant GA_Update = 1;

// GDALRWFlag
%constant GF_Read = 0;
%constant GF_Write = 1;

// GDALColorInterp
%constant GCI_Undefined=0;
%constant GCI_GrayIndex=1;
%constant GCI_PaletteIndex=2;
%constant GCI_RedBand=3;
%constant GCI_GreenBand=4;
%constant GCI_BlueBand=5;
%constant GCI_AlphaBand=6;
%constant GCI_HueBand=7;
%constant GCI_SaturationBand=8;
%constant GCI_LightnessBand=9;
%constant GCI_CyanBand=10;
%constant GCI_MagentaBand=11;
%constant GCI_YellowBand=12;
%constant GCI_BlackBand=13;

// GDALResampleAlg

%constant GRA_NearestNeighbour = 0;
%constant GRA_Bilinear         = 1;
%constant GRA_Cubic            = 2;
%constant GRA_CubicSpline      = 3;

// GDALPaletteInterp
%constant GPI_Gray=0;
%constant GPI_RGB=1;
%constant GPI_CMYK=2;
%constant GPI_HLS=3;

%constant CXT_Element=0;
%constant CXT_Text=1;
%constant CXT_Attribute=2;
%constant CXT_Comment=3;
%constant CXT_Literal=4;

// Error classes

%constant CE_None = 0;
%constant CE_Debug = 1;
%constant CE_Warning = 2;
%constant CE_Failure = 3;
%constant CE_Fatal = 4;

// Error codes

%constant CPLE_None                       = 0;
%constant CPLE_AppDefined                 = 1;
%constant CPLE_OutOfMemory                = 2;
%constant CPLE_FileIO                     = 3;
%constant CPLE_OpenFailed                 = 4;
%constant CPLE_IllegalArg                 = 5;
%constant CPLE_NotSupported               = 6;
%constant CPLE_AssertionFailed            = 7;
%constant CPLE_NoWriteAccess              = 8;
%constant CPLE_UserInterrupt              = 9;

%constant DMD_LONGNAME = "DMD_LONGNAME";
%constant DMD_HELPTOPIC = "DMD_HELPTOPIC";
%constant DMD_MIMETYPE = "DMD_MIMETYPE";
%constant DMD_EXTENSION = "DMD_EXTENSION";
%constant DMD_CREATIONOPTIONLIST = "DMD_CREATIONOPTIONLIST" ;
%constant DMD_CREATIONDATATYPES = "DMD_CREATIONDATATYPES" ;

%constant DCAP_CREATE =    "DCAP_CREATE";
%constant DCAP_CREATECOPY = "DCAP_CREATECOPY";

%constant CPLES_BackslashQuotable = 0;
%constant CPLES_XML = 1;
%constant CPLES_URL = 2;
%constant CPLES_SQL = 3;
%constant CPLES_CSV = 4;
