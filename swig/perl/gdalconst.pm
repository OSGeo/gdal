# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
package gdalconst;
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
package gdalconstc;
bootstrap gdalconst;
package gdalconst;
@EXPORT = qw( );

# ---------- BASE METHODS -------------

package gdalconst;

sub TIEHASH {
    my ($classname,$obj) = @_;
    return bless $obj, $classname;
}

sub CLEAR { }

sub FIRSTKEY { }

sub NEXTKEY { }

sub FETCH {
    my ($self,$field) = @_;
    my $member_func = "swig_${field}_get";
    $self->$member_func();
}

sub STORE {
    my ($self,$field,$newval) = @_;
    my $member_func = "swig_${field}_set";
    $self->$member_func($newval);
}

sub this {
    my $ptr = shift;
    return tied(%$ptr);
}


# ------- FUNCTION WRAPPERS --------

package gdalconst;


# ------- VARIABLE STUBS --------

package gdalconst;

*GDT_Unknown = *gdalconstc::GDT_Unknown;
*GDT_Byte = *gdalconstc::GDT_Byte;
*GDT_UInt16 = *gdalconstc::GDT_UInt16;
*GDT_Int16 = *gdalconstc::GDT_Int16;
*GDT_UInt32 = *gdalconstc::GDT_UInt32;
*GDT_Int32 = *gdalconstc::GDT_Int32;
*GDT_Float32 = *gdalconstc::GDT_Float32;
*GDT_Float64 = *gdalconstc::GDT_Float64;
*GDT_CInt16 = *gdalconstc::GDT_CInt16;
*GDT_CInt32 = *gdalconstc::GDT_CInt32;
*GDT_CFloat32 = *gdalconstc::GDT_CFloat32;
*GDT_CFloat64 = *gdalconstc::GDT_CFloat64;
*GDT_TypeCount = *gdalconstc::GDT_TypeCount;
*GA_ReadOnly = *gdalconstc::GA_ReadOnly;
*GA_Update = *gdalconstc::GA_Update;
*GF_Read = *gdalconstc::GF_Read;
*GF_Write = *gdalconstc::GF_Write;
*GCI_Undefined = *gdalconstc::GCI_Undefined;
*GCI_GrayIndex = *gdalconstc::GCI_GrayIndex;
*GCI_PaletteIndex = *gdalconstc::GCI_PaletteIndex;
*GCI_RedBand = *gdalconstc::GCI_RedBand;
*GCI_GreenBand = *gdalconstc::GCI_GreenBand;
*GCI_BlueBand = *gdalconstc::GCI_BlueBand;
*GCI_AlphaBand = *gdalconstc::GCI_AlphaBand;
*GCI_HueBand = *gdalconstc::GCI_HueBand;
*GCI_SaturationBand = *gdalconstc::GCI_SaturationBand;
*GCI_LightnessBand = *gdalconstc::GCI_LightnessBand;
*GCI_CyanBand = *gdalconstc::GCI_CyanBand;
*GCI_MagentaBand = *gdalconstc::GCI_MagentaBand;
*GCI_YellowBand = *gdalconstc::GCI_YellowBand;
*GCI_BlackBand = *gdalconstc::GCI_BlackBand;
*GRA_NearestNeighbour = *gdalconstc::GRA_NearestNeighbour;
*GRA_Bilinear = *gdalconstc::GRA_Bilinear;
*GRA_Cubic = *gdalconstc::GRA_Cubic;
*GRA_CubicSpline = *gdalconstc::GRA_CubicSpline;
*GPI_Gray = *gdalconstc::GPI_Gray;
*GPI_RGB = *gdalconstc::GPI_RGB;
*GPI_CMYK = *gdalconstc::GPI_CMYK;
*GPI_HLS = *gdalconstc::GPI_HLS;
*CXT_Element = *gdalconstc::CXT_Element;
*CXT_Text = *gdalconstc::CXT_Text;
*CXT_Attribute = *gdalconstc::CXT_Attribute;
*CXT_Comment = *gdalconstc::CXT_Comment;
*CXT_Literal = *gdalconstc::CXT_Literal;
*CE_None = *gdalconstc::CE_None;
*CE_Debug = *gdalconstc::CE_Debug;
*CE_Warning = *gdalconstc::CE_Warning;
*CE_Failure = *gdalconstc::CE_Failure;
*CE_Fatal = *gdalconstc::CE_Fatal;
*CPLE_None = *gdalconstc::CPLE_None;
*CPLE_AppDefined = *gdalconstc::CPLE_AppDefined;
*CPLE_OutOfMemory = *gdalconstc::CPLE_OutOfMemory;
*CPLE_FileIO = *gdalconstc::CPLE_FileIO;
*CPLE_OpenFailed = *gdalconstc::CPLE_OpenFailed;
*CPLE_IllegalArg = *gdalconstc::CPLE_IllegalArg;
*CPLE_NotSupported = *gdalconstc::CPLE_NotSupported;
*CPLE_AssertionFailed = *gdalconstc::CPLE_AssertionFailed;
*CPLE_NoWriteAccess = *gdalconstc::CPLE_NoWriteAccess;
*CPLE_UserInterrupt = *gdalconstc::CPLE_UserInterrupt;
*DMD_LONGNAME = *gdalconstc::DMD_LONGNAME;
*DMD_HELPTOPIC = *gdalconstc::DMD_HELPTOPIC;
*DMD_MIMETYPE = *gdalconstc::DMD_MIMETYPE;
*DMD_EXTENSION = *gdalconstc::DMD_EXTENSION;
*DMD_CREATIONOPTIONLIST = *gdalconstc::DMD_CREATIONOPTIONLIST;
*DMD_CREATIONDATATYPES = *gdalconstc::DMD_CREATIONDATATYPES;
*DCAP_CREATE = *gdalconstc::DCAP_CREATE;
*DCAP_CREATECOPY = *gdalconstc::DCAP_CREATECOPY;
*CPLES_BackslashQuotable = *gdalconstc::CPLES_BackslashQuotable;
*CPLES_XML = *gdalconstc::CPLES_XML;
*CPLES_URL = *gdalconstc::CPLES_URL;
*CPLES_SQL = *gdalconstc::CPLES_SQL;
*CPLES_CSV = *gdalconstc::CPLES_CSV;
1;
