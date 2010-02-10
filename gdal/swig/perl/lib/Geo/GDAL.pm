# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
package Geo::GDAL;
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
require Geo::OGR;
require Geo::OSR;
package Geo::GDALc;
bootstrap Geo::GDAL;
package Geo::GDAL;
@EXPORT = qw( );

# ---------- BASE METHODS -------------

package Geo::GDAL;

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

package Geo::GDAL;

*callback_d_cp_vp = *Geo::GDALc::callback_d_cp_vp;
*UseExceptions = *Geo::GDALc::UseExceptions;
*DontUseExceptions = *Geo::GDALc::DontUseExceptions;
*Debug = *Geo::GDALc::Debug;
*Error = *Geo::GDALc::Error;
*PushErrorHandler = *Geo::GDALc::PushErrorHandler;
*PopErrorHandler = *Geo::GDALc::PopErrorHandler;
*ErrorReset = *Geo::GDALc::ErrorReset;
*EscapeString = *Geo::GDALc::EscapeString;
*GetLastErrorNo = *Geo::GDALc::GetLastErrorNo;
*GetLastErrorType = *Geo::GDALc::GetLastErrorType;
*GetLastErrorMsg = *Geo::GDALc::GetLastErrorMsg;
*PushFinderLocation = *Geo::GDALc::PushFinderLocation;
*PopFinderLocation = *Geo::GDALc::PopFinderLocation;
*FinderClean = *Geo::GDALc::FinderClean;
*FindFile = *Geo::GDALc::FindFile;
*ReadDir = *Geo::GDALc::ReadDir;
*SetConfigOption = *Geo::GDALc::SetConfigOption;
*GetConfigOption = *Geo::GDALc::GetConfigOption;
*CPLBinaryToHex = *Geo::GDALc::CPLBinaryToHex;
*CPLHexToBinary = *Geo::GDALc::CPLHexToBinary;
*FileFromMemBuffer = *Geo::GDALc::FileFromMemBuffer;
*Unlink = *Geo::GDALc::Unlink;
*HasThreadSupport = *Geo::GDALc::HasThreadSupport;
*GDAL_GCP_GCPX_get = *Geo::GDALc::GDAL_GCP_GCPX_get;
*GDAL_GCP_GCPX_set = *Geo::GDALc::GDAL_GCP_GCPX_set;
*GDAL_GCP_GCPY_get = *Geo::GDALc::GDAL_GCP_GCPY_get;
*GDAL_GCP_GCPY_set = *Geo::GDALc::GDAL_GCP_GCPY_set;
*GDAL_GCP_GCPZ_get = *Geo::GDALc::GDAL_GCP_GCPZ_get;
*GDAL_GCP_GCPZ_set = *Geo::GDALc::GDAL_GCP_GCPZ_set;
*GDAL_GCP_GCPPixel_get = *Geo::GDALc::GDAL_GCP_GCPPixel_get;
*GDAL_GCP_GCPPixel_set = *Geo::GDALc::GDAL_GCP_GCPPixel_set;
*GDAL_GCP_GCPLine_get = *Geo::GDALc::GDAL_GCP_GCPLine_get;
*GDAL_GCP_GCPLine_set = *Geo::GDALc::GDAL_GCP_GCPLine_set;
*GDAL_GCP_Info_get = *Geo::GDALc::GDAL_GCP_Info_get;
*GDAL_GCP_Info_set = *Geo::GDALc::GDAL_GCP_Info_set;
*GDAL_GCP_Id_get = *Geo::GDALc::GDAL_GCP_Id_get;
*GDAL_GCP_Id_set = *Geo::GDALc::GDAL_GCP_Id_set;
*GDAL_GCP_get_GCPX = *Geo::GDALc::GDAL_GCP_get_GCPX;
*GDAL_GCP_set_GCPX = *Geo::GDALc::GDAL_GCP_set_GCPX;
*GDAL_GCP_get_GCPY = *Geo::GDALc::GDAL_GCP_get_GCPY;
*GDAL_GCP_set_GCPY = *Geo::GDALc::GDAL_GCP_set_GCPY;
*GDAL_GCP_get_GCPZ = *Geo::GDALc::GDAL_GCP_get_GCPZ;
*GDAL_GCP_set_GCPZ = *Geo::GDALc::GDAL_GCP_set_GCPZ;
*GDAL_GCP_get_GCPPixel = *Geo::GDALc::GDAL_GCP_get_GCPPixel;
*GDAL_GCP_set_GCPPixel = *Geo::GDALc::GDAL_GCP_set_GCPPixel;
*GDAL_GCP_get_GCPLine = *Geo::GDALc::GDAL_GCP_get_GCPLine;
*GDAL_GCP_set_GCPLine = *Geo::GDALc::GDAL_GCP_set_GCPLine;
*GDAL_GCP_get_Info = *Geo::GDALc::GDAL_GCP_get_Info;
*GDAL_GCP_set_Info = *Geo::GDALc::GDAL_GCP_set_Info;
*GDAL_GCP_get_Id = *Geo::GDALc::GDAL_GCP_get_Id;
*GDAL_GCP_set_Id = *Geo::GDALc::GDAL_GCP_set_Id;
*GCPsToGeoTransform = *Geo::GDALc::GCPsToGeoTransform;
*TermProgress_nocb = *Geo::GDALc::TermProgress_nocb;
*_ComputeMedianCutPCT = *Geo::GDALc::_ComputeMedianCutPCT;
*_DitherRGB2PCT = *Geo::GDALc::_DitherRGB2PCT;
*_ReprojectImage = *Geo::GDALc::_ReprojectImage;
*_ComputeProximity = *Geo::GDALc::_ComputeProximity;
*_RasterizeLayer = *Geo::GDALc::_RasterizeLayer;
*_Polygonize = *Geo::GDALc::_Polygonize;
*FillNodata = *Geo::GDALc::FillNodata;
*_SieveFilter = *Geo::GDALc::_SieveFilter;
*_RegenerateOverview = *Geo::GDALc::_RegenerateOverview;
*_AutoCreateWarpedVRT = *Geo::GDALc::_AutoCreateWarpedVRT;
*ApplyGeoTransform = *Geo::GDALc::ApplyGeoTransform;
*InvGeoTransform = *Geo::GDALc::InvGeoTransform;
*VersionInfo = *Geo::GDALc::VersionInfo;
*AllRegister = *Geo::GDALc::AllRegister;
*GDALDestroyDriverManager = *Geo::GDALc::GDALDestroyDriverManager;
*GetCacheMax = *Geo::GDALc::GetCacheMax;
*SetCacheMax = *Geo::GDALc::SetCacheMax;
*GetCacheUsed = *Geo::GDALc::GetCacheUsed;
*GetDataTypeSize = *Geo::GDALc::GetDataTypeSize;
*DataTypeIsComplex = *Geo::GDALc::DataTypeIsComplex;
*GetDataTypeName = *Geo::GDALc::GetDataTypeName;
*GetDataTypeByName = *Geo::GDALc::GetDataTypeByName;
*GetColorInterpretationName = *Geo::GDALc::GetColorInterpretationName;
*GetPaletteInterpretationName = *Geo::GDALc::GetPaletteInterpretationName;
*DecToDMS = *Geo::GDALc::DecToDMS;
*PackedDMSToDec = *Geo::GDALc::PackedDMSToDec;
*DecToPackedDMS = *Geo::GDALc::DecToPackedDMS;
*ParseXMLString = *Geo::GDALc::ParseXMLString;
*SerializeXMLTree = *Geo::GDALc::SerializeXMLTree;
*GetDriverCount = *Geo::GDALc::GetDriverCount;
*GetDriverByName = *Geo::GDALc::GetDriverByName;
*_GetDriver = *Geo::GDALc::_GetDriver;
*_Open = *Geo::GDALc::_Open;
*_OpenShared = *Geo::GDALc::_OpenShared;
*IdentifyDriver = *Geo::GDALc::IdentifyDriver;

############# Class : Geo::GDAL::SavedEnv ##############

package Geo::GDAL::SavedEnv;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL );
%OWNER = ();
%ITERATORS = ();
*swig_fct_get = *Geo::GDALc::SavedEnv_fct_get;
*swig_fct_set = *Geo::GDALc::SavedEnv_fct_set;
*swig_data_get = *Geo::GDALc::SavedEnv_data_get;
*swig_data_set = *Geo::GDALc::SavedEnv_data_set;
sub new {
    my $pkg = shift;
    my $self = Geo::GDALc::new_SavedEnv(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::GDALc::delete_SavedEnv($self);
        delete $OWNER{$self};
    }
}

sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


############# Class : Geo::GDAL::MajorObject ##############

package Geo::GDAL::MajorObject;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL );
%OWNER = ();
*GetDescription = *Geo::GDALc::MajorObject_GetDescription;
*SetDescription = *Geo::GDALc::MajorObject_SetDescription;
*GetMetadata = *Geo::GDALc::MajorObject_GetMetadata;
*SetMetadata = *Geo::GDALc::MajorObject_SetMetadata;
*GetMetadataItem = *Geo::GDALc::MajorObject_GetMetadataItem;
*SetMetadataItem = *Geo::GDALc::MajorObject_SetMetadataItem;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


############# Class : Geo::GDAL::Driver ##############

package Geo::GDAL::Driver;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL::MajorObject Geo::GDAL );
%OWNER = ();
%ITERATORS = ();
*swig_ShortName_get = *Geo::GDALc::Driver_ShortName_get;
*swig_ShortName_set = *Geo::GDALc::Driver_ShortName_set;
*swig_LongName_get = *Geo::GDALc::Driver_LongName_get;
*swig_LongName_set = *Geo::GDALc::Driver_LongName_set;
*swig_HelpTopic_get = *Geo::GDALc::Driver_HelpTopic_get;
*swig_HelpTopic_set = *Geo::GDALc::Driver_HelpTopic_set;
*_Create = *Geo::GDALc::Driver__Create;
*CreateCopy = *Geo::GDALc::Driver_CreateCopy;
*Delete = *Geo::GDALc::Driver_Delete;
*Rename = *Geo::GDALc::Driver_Rename;
*Register = *Geo::GDALc::Driver_Register;
*Deregister = *Geo::GDALc::Driver_Deregister;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


############# Class : Geo::GDAL::GCP ##############

package Geo::GDAL::GCP;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL );
%OWNER = ();
%ITERATORS = ();
*swig_GCPX_get = *Geo::GDALc::GCP_GCPX_get;
*swig_GCPX_set = *Geo::GDALc::GCP_GCPX_set;
*swig_GCPY_get = *Geo::GDALc::GCP_GCPY_get;
*swig_GCPY_set = *Geo::GDALc::GCP_GCPY_set;
*swig_GCPZ_get = *Geo::GDALc::GCP_GCPZ_get;
*swig_GCPZ_set = *Geo::GDALc::GCP_GCPZ_set;
*swig_GCPPixel_get = *Geo::GDALc::GCP_GCPPixel_get;
*swig_GCPPixel_set = *Geo::GDALc::GCP_GCPPixel_set;
*swig_GCPLine_get = *Geo::GDALc::GCP_GCPLine_get;
*swig_GCPLine_set = *Geo::GDALc::GCP_GCPLine_set;
*swig_Info_get = *Geo::GDALc::GCP_Info_get;
*swig_Info_set = *Geo::GDALc::GCP_Info_set;
*swig_Id_get = *Geo::GDALc::GCP_Id_get;
*swig_Id_set = *Geo::GDALc::GCP_Id_set;
sub new {
    my $pkg = shift;
    my $self = Geo::GDALc::new_GCP(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    my $self;
    if ($_[0]->isa('SCALAR')) {
        $self = $_[0];
    } else {
        return unless $_[0]->isa('HASH');
        $self = tied(%{$_[0]});
        return unless 0;
    }
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::GDALc::delete_GCP($self);
        delete $OWNER{$self};
    }
    $self->RELEASE_PARENTS();
}

sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


############# Class : Geo::GDAL::Dataset ##############

package Geo::GDAL::Dataset;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL::MajorObject Geo::GDAL );
%OWNER = ();
%ITERATORS = ();
*swig_RasterXSize_get = *Geo::GDALc::Dataset_RasterXSize_get;
*swig_RasterXSize_set = *Geo::GDALc::Dataset_RasterXSize_set;
*swig_RasterYSize_get = *Geo::GDALc::Dataset_RasterYSize_get;
*swig_RasterYSize_set = *Geo::GDALc::Dataset_RasterYSize_set;
*swig_RasterCount_get = *Geo::GDALc::Dataset_RasterCount_get;
*swig_RasterCount_set = *Geo::GDALc::Dataset_RasterCount_set;
sub DESTROY {
    my $self;
    if ($_[0]->isa('SCALAR')) {
        $self = $_[0];
    } else {
        return unless $_[0]->isa('HASH');
        $self = tied(%{$_[0]});
        return unless 0;
    }
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::GDALc::delete_Dataset($self);
        delete $OWNER{$self};
    }
    $self->RELEASE_PARENTS();
}

*_GetDriver = *Geo::GDALc::Dataset__GetDriver;
*_GetRasterBand = *Geo::GDALc::Dataset__GetRasterBand;
*GetProjection = *Geo::GDALc::Dataset_GetProjection;
*GetProjectionRef = *Geo::GDALc::Dataset_GetProjectionRef;
*SetProjection = *Geo::GDALc::Dataset_SetProjection;
*GetGeoTransform = *Geo::GDALc::Dataset_GetGeoTransform;
*SetGeoTransform = *Geo::GDALc::Dataset_SetGeoTransform;
*BuildOverviews = *Geo::GDALc::Dataset_BuildOverviews;
*GetGCPCount = *Geo::GDALc::Dataset_GetGCPCount;
*GetGCPProjection = *Geo::GDALc::Dataset_GetGCPProjection;
*GetGCPs = *Geo::GDALc::Dataset_GetGCPs;
*SetGCPs = *Geo::GDALc::Dataset_SetGCPs;
*FlushCache = *Geo::GDALc::Dataset_FlushCache;
*_AddBand = *Geo::GDALc::Dataset__AddBand;
*CreateMaskBand = *Geo::GDALc::Dataset_CreateMaskBand;
*GetFileList = *Geo::GDALc::Dataset_GetFileList;
*WriteRaster = *Geo::GDALc::Dataset_WriteRaster;
*ReadRaster = *Geo::GDALc::Dataset_ReadRaster;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


############# Class : Geo::GDAL::Band ##############

package Geo::GDAL::Band;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL::MajorObject Geo::GDAL );
%OWNER = ();
%ITERATORS = ();
*swig_XSize_get = *Geo::GDALc::Band_XSize_get;
*swig_XSize_set = *Geo::GDALc::Band_XSize_set;
*swig_YSize_get = *Geo::GDALc::Band_YSize_get;
*swig_YSize_set = *Geo::GDALc::Band_YSize_set;
*swig_DataType_get = *Geo::GDALc::Band_DataType_get;
*swig_DataType_set = *Geo::GDALc::Band_DataType_set;
*GetBand = *Geo::GDALc::Band_GetBand;
*GetBlockSize = *Geo::GDALc::Band_GetBlockSize;
*GetColorInterpretation = *Geo::GDALc::Band_GetColorInterpretation;
*GetRasterColorInterpretation = *Geo::GDALc::Band_GetRasterColorInterpretation;
*SetColorInterpretation = *Geo::GDALc::Band_SetColorInterpretation;
*SetRasterColorInterpretation = *Geo::GDALc::Band_SetRasterColorInterpretation;
*GetNoDataValue = *Geo::GDALc::Band_GetNoDataValue;
*SetNoDataValue = *Geo::GDALc::Band_SetNoDataValue;
*GetUnitType = *Geo::GDALc::Band_GetUnitType;
*GetRasterCategoryNames = *Geo::GDALc::Band_GetRasterCategoryNames;
*SetRasterCategoryNames = *Geo::GDALc::Band_SetRasterCategoryNames;
*GetMinimum = *Geo::GDALc::Band_GetMinimum;
*GetMaximum = *Geo::GDALc::Band_GetMaximum;
*GetOffset = *Geo::GDALc::Band_GetOffset;
*GetScale = *Geo::GDALc::Band_GetScale;
*GetStatistics = *Geo::GDALc::Band_GetStatistics;
*ComputeStatistics = *Geo::GDALc::Band_ComputeStatistics;
*SetStatistics = *Geo::GDALc::Band_SetStatistics;
*GetOverviewCount = *Geo::GDALc::Band_GetOverviewCount;
*GetOverview = *Geo::GDALc::Band_GetOverview;
*Checksum = *Geo::GDALc::Band_Checksum;
*ComputeRasterMinMax = *Geo::GDALc::Band_ComputeRasterMinMax;
*ComputeBandStats = *Geo::GDALc::Band_ComputeBandStats;
*Fill = *Geo::GDALc::Band_Fill;
*ReadRaster = *Geo::GDALc::Band_ReadRaster;
*WriteRaster = *Geo::GDALc::Band_WriteRaster;
*FlushCache = *Geo::GDALc::Band_FlushCache;
*GetRasterColorTable = *Geo::GDALc::Band_GetRasterColorTable;
*GetColorTable = *Geo::GDALc::Band_GetColorTable;
*SetRasterColorTable = *Geo::GDALc::Band_SetRasterColorTable;
*SetColorTable = *Geo::GDALc::Band_SetColorTable;
*GetDefaultRAT = *Geo::GDALc::Band_GetDefaultRAT;
*SetDefaultRAT = *Geo::GDALc::Band_SetDefaultRAT;
*GetMaskBand = *Geo::GDALc::Band_GetMaskBand;
*GetMaskFlags = *Geo::GDALc::Band_GetMaskFlags;
*CreateMaskBand = *Geo::GDALc::Band_CreateMaskBand;
*_GetHistogram = *Geo::GDALc::Band__GetHistogram;
*GetDefaultHistogram = *Geo::GDALc::Band_GetDefaultHistogram;
*SetDefaultHistogram = *Geo::GDALc::Band_SetDefaultHistogram;
*HasArbitraryOverviews = *Geo::GDALc::Band_HasArbitraryOverviews;
*ContourGenerate = *Geo::GDALc::Band_ContourGenerate;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


############# Class : Geo::GDAL::ColorTable ##############

package Geo::GDAL::ColorTable;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL );
%OWNER = ();
sub new {
    my $pkg = shift;
    my $self = Geo::GDALc::new_ColorTable(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    my $self;
    if ($_[0]->isa('SCALAR')) {
        $self = $_[0];
    } else {
        return unless $_[0]->isa('HASH');
        $self = tied(%{$_[0]});
        return unless 0;
    }
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::GDALc::delete_ColorTable($self);
        delete $OWNER{$self};
    }
    $self->RELEASE_PARENTS();
}

*Clone = *Geo::GDALc::ColorTable_Clone;
*_GetPaletteInterpretation = *Geo::GDALc::ColorTable__GetPaletteInterpretation;
*GetCount = *Geo::GDALc::ColorTable_GetCount;
*GetColorEntry = *Geo::GDALc::ColorTable_GetColorEntry;
*GetColorEntryAsRGB = *Geo::GDALc::ColorTable_GetColorEntryAsRGB;
*_SetColorEntry = *Geo::GDALc::ColorTable__SetColorEntry;
*CreateColorRamp = *Geo::GDALc::ColorTable_CreateColorRamp;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


############# Class : Geo::GDAL::RasterAttributeTable ##############

package Geo::GDAL::RasterAttributeTable;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL );
%OWNER = ();
sub new {
    my $pkg = shift;
    my $self = Geo::GDALc::new_RasterAttributeTable(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    my $self;
    if ($_[0]->isa('SCALAR')) {
        $self = $_[0];
    } else {
        return unless $_[0]->isa('HASH');
        $self = tied(%{$_[0]});
        return unless 0;
    }
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::GDALc::delete_RasterAttributeTable($self);
        delete $OWNER{$self};
    }
    $self->RELEASE_PARENTS();
}

*Clone = *Geo::GDALc::RasterAttributeTable_Clone;
*GetColumnCount = *Geo::GDALc::RasterAttributeTable_GetColumnCount;
*GetNameOfCol = *Geo::GDALc::RasterAttributeTable_GetNameOfCol;
*_GetUsageOfCol = *Geo::GDALc::RasterAttributeTable__GetUsageOfCol;
*_GetTypeOfCol = *Geo::GDALc::RasterAttributeTable__GetTypeOfCol;
*_GetColOfUsage = *Geo::GDALc::RasterAttributeTable__GetColOfUsage;
*GetRowCount = *Geo::GDALc::RasterAttributeTable_GetRowCount;
*GetValueAsString = *Geo::GDALc::RasterAttributeTable_GetValueAsString;
*GetValueAsInt = *Geo::GDALc::RasterAttributeTable_GetValueAsInt;
*GetValueAsDouble = *Geo::GDALc::RasterAttributeTable_GetValueAsDouble;
*SetValueAsString = *Geo::GDALc::RasterAttributeTable_SetValueAsString;
*SetValueAsInt = *Geo::GDALc::RasterAttributeTable_SetValueAsInt;
*SetValueAsDouble = *Geo::GDALc::RasterAttributeTable_SetValueAsDouble;
*SetRowCount = *Geo::GDALc::RasterAttributeTable_SetRowCount;
*_CreateColumn = *Geo::GDALc::RasterAttributeTable__CreateColumn;
*GetLinearBinning = *Geo::GDALc::RasterAttributeTable_GetLinearBinning;
*SetLinearBinning = *Geo::GDALc::RasterAttributeTable_SetLinearBinning;
*GetRowOfValue = *Geo::GDALc::RasterAttributeTable_GetRowOfValue;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


############# Class : Geo::GDAL::Transformer ##############

package Geo::GDAL::Transformer;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL );
%OWNER = ();
%ITERATORS = ();
sub new {
    my $pkg = shift;
    my $self = Geo::GDALc::new_Transformer(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::GDALc::delete_Transformer($self);
        delete $OWNER{$self};
    }
}

*TransformPoint = *Geo::GDALc::Transformer_TransformPoint;
*TransformPoints = *Geo::GDALc::Transformer_TransformPoints;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


# ------- VARIABLE STUBS --------

package Geo::GDAL;

*TermProgress = *Geo::GDALc::TermProgress;

    use strict;
    use Carp;
    use Geo::GDAL::Const;
    use Geo::OGR;
    use Geo::OSR;
    our $VERSION = '0.23';
    our $GDAL_VERSION = '1.8.0';
    use vars qw/
	%TYPE_STRING2INT %TYPE_INT2STRING
	%ACCESS_STRING2INT %ACCESS_INT2STRING
	%RESAMPLING_STRING2INT %RESAMPLING_INT2STRING
	%NODE_TYPE_STRING2INT %NODE_TYPE_INT2STRING
	/;
    for my $string (qw/Unknown Byte UInt16 Int16 UInt32 Int32 Float32 Float64 CInt16 CInt32 CFloat32 CFloat64/) {
	my $int = eval "\$Geo::GDAL::Constc::GDT_$string";
	$TYPE_STRING2INT{$string} = $int;
	$TYPE_INT2STRING{$int} = $string;
    }
    for my $string (qw/ReadOnly Update/) {
	my $int = eval "\$Geo::GDAL::Constc::GA_$string";
	$ACCESS_STRING2INT{$string} = $int;
	$ACCESS_INT2STRING{$int} = $string;
    }
    for my $string (qw/NearestNeighbour Bilinear Cubic CubicSpline/) {
	my $int = eval "\$Geo::GDAL::Constc::GRA_$string";
	$RESAMPLING_STRING2INT{$string} = $int;
	$RESAMPLING_INT2STRING{$int} = $string;
    }
    {
	my $int = 0;
	for my $string (qw/Element Text Attribute Comment Literal/) {
	    $NODE_TYPE_STRING2INT{$string} = $int;
	    $NODE_TYPE_INT2STRING{$int} = $string;
	    $int++;
	}
    }
    sub RELEASE_PARENTS {
    }
    sub NodeType {
	my $type = shift;
	return $NODE_TYPE_INT2STRING{$type} if $type =~ /^\d/;
	return $NODE_TYPE_STRING2INT{$type};
    }
    sub NodeData {
	my $node = shift;
	return (Geo::GDAL::NodeType($node->[0]), $node->[1]);
    }
    sub Children {
	my $node = shift;
	return @$node[2..$#$node];
    }
    sub Child {
	my($node, $child) = @_;
	return $node->[2+$child];
    }
    sub GetDataTypeSize {
	my $t = shift;
	$t = $TYPE_INT2STRING{$t} if exists $TYPE_INT2STRING{$t};
	return _GetDataTypeSize($t);
    }
    sub DataTypeValueRange {
    	my $t = shift;
	# these values are from gdalrasterband.cpp
	return (0,255) if $t =~ /^Byte$/;
	return (0,65535) if $t =~ /^UInt16$/;
	return (-32768,32767) if $t =~ /Int16$/; # also CInt
	return (0,4294967295) if $t =~ /^UInt32$/;
	return (-2147483648,2147483647) if $t =~ /Int32$/; # also CInt
	return (-4294967295.0,4294967295.0) if $t =~ /Float32$/; # also CFloat
	return (-4294967295.0,4294967295.0) if $t =~ /Float64$/; # also CFloat
	croak "unsupported data type: $t";
    }
    sub DataTypeIsComplex {
	my $t = shift;
	$t = $TYPE_INT2STRING{$t} if exists $TYPE_INT2STRING{$t};
	return _DataTypeIsComplex($t);
    }
    sub PackCharacter {
	my $t = shift;
	$t = $TYPE_INT2STRING{$t} if exists $TYPE_INT2STRING{$t};
	my $is_big_endian = unpack("h*", pack("s", 1)) =~ /01/; # from Programming Perl
	return 'C' if $t =~ /^Byte$/;
	return ($is_big_endian ? 'n': 'v') if $t =~ /^UInt16$/;
	return 's' if $t =~ /^Int16$/;
	return ($is_big_endian ? 'N' : 'V') if $t =~ /^UInt32$/;
	return 'l' if $t =~ /^Int32$/;
	return 'f' if $t =~ /^Float32$/;
	return 'd' if $t =~ /^Float64$/;
	croak "unsupported data type: $t";
    }
    sub Drivers {
	my @drivers;
	for my $i (0..GetDriverCount()-1) {
	    push @drivers, _GetDriver($i);
	}
	return @drivers;
    }
    sub GetDriver {
	my $driver = shift;
	return _GetDriver($driver) if $driver =~ /^\d/;
	return GetDriverByName($driver);
    }
    *Driver = *GetDriver;
    sub Open {
	my @p = @_;
	$p[1] = $ACCESS_STRING2INT{$p[1]} if $p[1] and exists $ACCESS_STRING2INT{$p[1]};
	return _Open(@p);
    }
    sub OpenShared {
	my @p = @_;
	$p[1] = $ACCESS_STRING2INT{$p[1]} if $p[1] and exists $ACCESS_STRING2INT{$p[1]};
	return _OpenShared(@p);
    }
    sub ComputeMedianCutPCT {
    	$_[6] = 1 if $_[5] and not defined $_[6];
	_ComputeMedianCutPCT(@_);
    }
    sub DitherRGB2PCT {
    	$_[6] = 1 if $_[5] and not defined $_[6];
	_DitherRGB2PCT(@_);
    }
    sub ComputeProximity {
    	$_[4] = 1 if $_[3] and not defined $_[4];
	_ComputeProximity(@_);
    }
    sub RasterizeLayer {
    	$_[8] = 1 if $_[7] and not defined $_[8];
	_RasterizeLayer(@_);
    }
    sub Polygonize {
        my @params = @_;
        $params[6] = 1 if $params[5] and not defined $params[6];
        $params[3] = $params[2]->GetLayerDefn->GetFieldIndex($params[3]) unless $params[3] =~ /^\d/;
	_Polygonize(@params);
    }
    sub SieveFilter {
    	$_[7] = 1 if $_[6] and not defined $_[7];
	_SieveFilter(@_);
    }
    sub RegenerateOverviews {
    	$_[4] = 1 if $_[3] and not defined $_[4];
	_RegenerateOverviews(@_);
    }
    sub RegenerateOverview {
    	$_[4] = 1 if $_[3] and not defined $_[4];
	_RegenerateOverview(@_);
    }
    sub ReprojectImage {
    	$_[8] = 1 if $_[7] and not defined $_[8];
	$_[4] = $RESAMPLING_STRING2INT{$_[4]} if $_[4] and exists $RESAMPLING_STRING2INT{$_[4]};
	return _ReprojectImage(@_);
    }
    sub AutoCreateWarpedVRT {
	$_[3] = $RESAMPLING_STRING2INT{$_[3]} if $_[3] and exists $RESAMPLING_STRING2INT{$_[3]};
	return _AutoCreateWarpedVRT(@_);
    }

    package Geo::GDAL::MajorObject;
    use vars qw/@DOMAINS/;
    use strict;
    sub Domains {
	return @DOMAINS;
    }
    sub Description {
	my($self, $desc) = @_;
	SetDescription($self, $desc) if defined $desc;
	GetDescription($self) if defined wantarray;
    }
    sub Metadata {
	my $self = shift;
	my $metadata = shift if ref $_[0];
	my $domain = shift;
	$domain = '' unless defined $domain;
	SetMetadata($self, $metadata, $domain) if defined $metadata;
	GetMetadata($self, $domain) if defined wantarray;
    }

    package Geo::GDAL::Driver;
    use vars qw/@CAPABILITIES @DOMAINS/;
    use strict;
    @CAPABILITIES = ('Create', 'CreateCopy');
    sub Domains {
	return @DOMAINS;
    }
    sub Name {
	my $self = shift;
	return $self->{ShortName};
    }
    sub Capabilities {
	my $self = shift;
	return @CAPABILITIES unless $self;
	my $h = $self->GetMetadata;
	my @cap;
	for my $cap (@CAPABILITIES) {
	    push @cap, $cap if $h->{'DCAP_'.uc($cap)} eq 'YES';
	}
	return @cap;
    }
    sub TestCapability {
	my($self, $cap) = @_;
	my $h = $self->GetMetadata;
	return $h->{'DCAP_'.uc($cap)} eq 'YES' ? 1 : undef;
    }
    sub Extension {
	my $self = shift;
	my $h = $self->GetMetadata;
	return $h->{DMD_EXTENSION};
    }
    sub MIMEType {
	my $self = shift;
	my $h = $self->GetMetadata;
	return $h->{DMD_MIMETYPE};
    }
    sub CreationOptionList {
	my $self = shift;
	my @options;
	my $h = $self->GetMetadata->{DMD_CREATIONOPTIONLIST};
	if ($h) {
	    $h = Geo::GDAL::ParseXMLString($h);
	    my($type, $value) = Geo::GDAL::NodeData($h);
	    if ($value eq 'CreationOptionList') {
		for my $o (Geo::GDAL::Children($h)) {
		    my %option;
		    for my $a (Geo::GDAL::Children($o)) {
			my(undef, $key) = Geo::GDAL::NodeData($a);
			my(undef, $value) = Geo::GDAL::NodeData(Geo::GDAL::Child($a, 0));
			if ($key eq 'Value') {
			    push @{$option{$key}}, $value;
			} else {
			    $option{$key} = $value;
			}
		    }
		    push @options, \%option;
		}
	    }
	}
	return @options;
    }
    sub CreationDataTypes {
	my $self = shift;
	my $h = $self->GetMetadata;
	return split /\s+/, $h->{DMD_CREATIONDATATYPES};
    }
    sub CreateDataset {
	my @p = @_;
	$p[5] = $Geo::GDAL::TYPE_STRING2INT{$p[5]} if $p[5] and exists $Geo::GDAL::TYPE_STRING2INT{$p[5]};
	return _Create(@p);
    }
    *Create = *CreateDataset;
    *Copy = *CreateCopy;

    package Geo::GDAL::Dataset;
    use strict;
    use vars qw/%BANDS @DOMAINS/;
    @DOMAINS = ("IMAGE_STRUCTURE", "SUBDATASETS", "GEOLOCATION");
    sub Domains {
	return @DOMAINS;
    }
    *GetDriver = *_GetDriver;
    sub Open {
	return Geo::GDAL::Open(@_);
    }
    sub OpenShared {
	return Geo::GDAL::OpenShared(@_);
    }
    sub Size {
	my $self = shift;
	return ($self->{RasterXSize}, $self->{RasterYSize});
    }
    sub Bands {
	my $self = shift;
	my @bands;
	for my $i (1..$self->{RasterCount}) {
	    push @bands, GetRasterBand($self, $i);
	}
	return @bands;
    }
    sub GetRasterBand {
	my($self, $index) = @_;
	my $band = _GetRasterBand($self, $index);
	$BANDS{tied(%{$band})} = $self;
	return $band;
    }
    *Band = *GetRasterBand;
    sub AddBand {
	my @p = @_;
	$p[1] = $Geo::GDAL::TYPE_STRING2INT{$p[1]} if $p[1] and exists $Geo::GDAL::TYPE_STRING2INT{$p[1]};
	return _AddBand(@p);
    }
    sub Projection {
	my($self, $proj) = @_;
	SetProjection($self, $proj) if defined $proj;
	GetProjection($self) if defined wantarray;
    }
    sub GeoTransform {
	my $self = shift;
	SetGeoTransform($self, \@_) if @_ > 0;
	return unless defined wantarray;
	my $t = GetGeoTransform($self);
	return @$t;
    }
    sub GCPs {
	my $self = shift;
	if (@_ > 0) {
	    my $proj = pop @_;
	    SetGCPs($self, \@_, $proj);
	}
	return unless defined wantarray;
	my $proj = GetGCPProjection($self);
	my $GCPs = GetGCPs($self);
	return (@$GCPs, $proj);
    }

    package Geo::GDAL::Band;
    use Carp;
    use UNIVERSAL qw(isa);
    use strict;
    use vars qw/
	%COLOR_INTERPRETATION_STRING2INT %COLOR_INTERPRETATION_INT2STRING @DOMAINS
	/;
    for my $string (qw/Undefined GrayIndex PaletteIndex RedBand GreenBand BlueBand AlphaBand 
		    HueBand SaturationBand LightnessBand CyanBand MagentaBand YellowBand BlackBand/) {
	my $int = eval "\$Geo::GDAL::Constc::GCI_$string";
	$COLOR_INTERPRETATION_STRING2INT{$string} = $int;
	$COLOR_INTERPRETATION_INT2STRING{$int} = $string;
    }
    @DOMAINS = ("IMAGE_STRUCTURE", "RESAMPLING");
    sub Domains {
	return @DOMAINS;
    }
    sub DESTROY {
	my $self;
	if ($_[0]->isa('SCALAR')) {
	    $self = $_[0];
	} else {
	    return unless $_[0]->isa('HASH');
	    $self = tied(%{$_[0]});
	    return unless defined $self;
	}
	delete $ITERATORS{$self};
	if (exists $OWNER{$self}) {
	    delete $OWNER{$self};
	}
	$self->RELEASE_PARENTS();
    }
    sub RELEASE_PARENTS {
	my $self = shift;
	delete $Geo::GDAL::Dataset::BANDS{$self};
    }
    sub Size {
	my $self = shift;
	return ($self->{XSize}, $self->{YSize});
    }
    sub DataType {
	my $self = shift;
	return $Geo::GDAL::TYPE_INT2STRING{$self->{DataType}};
    }
    sub NoDataValue {
	my $self = shift;
	SetNoDataValue($self, $_[0]) if @_ > 0;
	GetNoDataValue($self);
    }
    sub ReadTile {
	my($self, $xoff, $yoff, $xsize, $ysize) = @_;
	$xoff = 0 unless defined $xoff;
	$yoff = 0 unless defined $yoff;
	$xsize = $self->{XSize} - $xoff unless defined $xsize;
	$ysize = $self->{YSize} - $yoff unless defined $ysize;
	my $buf = $self->ReadRaster($xoff, $yoff, $xsize, $ysize);
	my $pc = Geo::GDAL::PackCharacter($self->{DataType});
	my $w = $xsize * Geo::GDAL::GetDataTypeSize($self->{DataType})/8;
	my $offset = 0;
	my @data;
	for (0..$ysize-1) {
	    my $sub = substr($buf, $offset, $w);
	    my @d = unpack($pc."[$xsize]", $sub);
	    push @data, \@d;
	    $offset += $w;
	}
	return \@data;
    }
    sub WriteTile {
	my($self, $data, $xoff, $yoff) = @_;
	$xoff = 0 unless defined $xoff;
	$yoff = 0 unless defined $yoff;
	my $xsize = @{$data->[0]};
	$xsize = $self->{XSize} - $xoff if $xsize > $self->{XSize} - $xoff;
	my $ysize = @{$data};
	$ysize = $self->{YSize} - $yoff if $ysize > $self->{YSize} - $yoff;
	my $pc = Geo::GDAL::PackCharacter($self->{DataType});
	my $w = $xsize * Geo::GDAL::GetDataTypeSize($self->{DataType})/8;
	for my $i (0..$ysize-1) {
	    my $scanline = pack($pc."[$xsize]", @{$data->[$i]});
	    $self->WriteRaster( $xoff, $yoff+$i, $xsize, 1, $scanline );
	}
    }
    sub ColorInterpretation {
	my($self, $ci) = @_;
	if ($ci) {
	    $ci = $COLOR_INTERPRETATION_STRING2INT{$ci} if exists $COLOR_INTERPRETATION_STRING2INT{$ci};
	    SetRasterColorInterpretation($self, $ci);
	    return $ci;
	} else {
	    return $COLOR_INTERPRETATION_INT2STRING{GetRasterColorInterpretation($self)};
	}
    }
    sub ColorTable {
	my $self = shift;
	SetRasterColorTable($self, $_[0]) if @_;
	return unless defined wantarray;
	GetRasterColorTable($self);
    }
    sub CategoryNames {
	my $self = shift;
	SetRasterCategoryNames($self, \@_) if @_;
	return unless defined wantarray;
	my $n = GetRasterCategoryNames($self);
	return @$n;
    }
    sub AttributeTable {
	my $self = shift;
	SetDefaultRAT($self, $_[0]) if @_;
	return unless defined wantarray;
	my $r = GetDefaultRAT($self);
	$Geo::GDAL::RasterAttributeTable::BANDS{$r} = $self;
	return $r;
    }
    sub GetHistogram {
	my $self = shift;
	my %defaults = (Min => -0.5,
			Max => 255.5,
			Buckets => 256, 
			IncludeOutOfRange => 0,
			ApproxOK => 0,
			Progress => undef,
			ProgressData => undef);
	my %params = @_;
	for (keys %params) {
	    croak "unknown parameter: $_" unless exists $defaults{$_};
	}
	for (keys %defaults) {
	    $params{$_} = $defaults{$_} unless defined $params{$_};
	}
	$params{ProgressData} = 1 if $params{Progress} and not defined $params{ProgressData};
	my $h = _GetHistogram($self, $params{Min}, $params{Max}, $params{Buckets},
			      $params{IncludeOutOfRange}, $params{ApproxOK},
			      $params{Progress}, $params{ProgressData});
	return @$h if $h;
    }
    sub Contours {
	my $self = shift;
	my %defaults = (DataSource => undef,
			LayerConstructor => {Name => 'contours'},
			ContourInterval => 100, 
			ContourBase => 0,
			FixedLevels => [], 
			NoDataValue => undef, 
			IDField => -1, 
			ElevField => -1,
			callback => undef,
			callback_data => undef);
	my %params;
	if (!defined($_[0]) or isa($_[0], 'Geo::OGR::DataSource')) {
	    ($params{DataSource}, $params{LayerConstructor},
	     $params{ContourInterval}, $params{ContourBase},
	     $params{FixedLevels}, $params{NoDataValue}, 
	     $params{IDField}, $params{ElevField},
	     $params{callback}, $params{callback_data}) = @_;
	} else {
	    %params = @_;
	}
	for (keys %params) {
	    croak "unknown parameter: $_" unless exists $defaults{$_};
	}
	for (keys %defaults) {
	    $params{$_} = $defaults{$_} unless defined $params{$_};
	}
	$params{DataSource} = Geo::OGR::GetDriver('Memory')->CreateDataSource('ds') 
	    unless defined $params{DataSource};
	my $layer = $params{DataSource}->CreateLayer($params{LayerConstructor});
	my $schema = $layer->GetLayerDefn;
	for ('IDField', 'ElevField') {
	    $params{$_} = $schema->GetFieldIndex($params{ElevField}) unless $params{ElevField} =~ /^[+-]?\d+$/;
	}
	$params{callback_data} = 1 if $params{callback} and not defined $params{callback_data};
	ContourGenerate($self, $params{ContourInterval}, $params{ContourBase}, $params{FixedLevels},
			$params{NoDataValue}, $layer, $params{IDField}, $params{ElevField},
			$params{callback}, $params{callback_data});
	return $layer;
    }

    package Geo::GDAL::ColorTable;
    use strict;
    use vars qw/
	%PALETTE_INTERPRETATION_STRING2INT %PALETTE_INTERPRETATION_INT2STRING
	/;
    for my $string (qw/Gray RGB CMYK HLS/) {
	my $int = eval "\$Geo::GDAL::Constc::GPI_$string";
	$PALETTE_INTERPRETATION_STRING2INT{$string} = $int;
	$PALETTE_INTERPRETATION_INT2STRING{$int} = $string;
    }
    sub create {
	my($pkg, $pi) = @_;
	$pi = $PALETTE_INTERPRETATION_STRING2INT{$pi} if defined $pi and exists $PALETTE_INTERPRETATION_STRING2INT{$pi};
	my $self = Geo::GDALc::new_ColorTable($pi);
	bless $self, $pkg if defined($self);
    }
    sub GetPaletteInterpretation {
	my $self = shift;
	return $PALETTE_INTERPRETATION_INT2STRING{GetPaletteInterpretation($self)};
    }
    sub SetColorEntry {
	@_ == 3 ? _SetColorEntry(@_) : _SetColorEntry(@_[0..1], [@_[2..5]]);
    }
    sub ColorEntry {
	my $self = shift;
	my $index = shift;
	SetColorEntry($self, $index, @_) if @_ > 0;
	GetColorEntry($self, $index) if defined wantarray;
    }
    sub ColorTable {
	my $self = shift;
	my @table;
	if (@_) {
	    my $index = 0;
	    for my $color (@_) {
		push @table, [ColorEntry($self, $index, @$color)];
		$index++;
	    }
	} else {
	    for (my $index = 0; $index < GetCount($self); $index++) {
		push @table, [ColorEntry($self, $index)];
	    }
	}
	return @table;
    }

    package Geo::GDAL::RasterAttributeTable;
    use strict;
    use vars qw/ %BANDS
	%FIELD_TYPE_STRING2INT %FIELD_TYPE_INT2STRING
	%FIELD_USAGE_STRING2INT %FIELD_USAGE_INT2STRING
	/;
    for my $string (qw/Integer Real String/) {
	my $int = eval "\$Geo::GDAL::Constc::GFT_$string";
	$FIELD_TYPE_STRING2INT{$string} = $int;
	$FIELD_TYPE_INT2STRING{$int} = $string;
    }
    for my $string (qw/Generic PixelCount Name Min Max MinMax 
		    Red Green Blue Alpha RedMin 
		    GreenMin BlueMin AlphaMin RedMax GreenMax BlueMax AlphaMax 
		    MaxCount/) {
	my $int = eval "\$Geo::GDAL::Constc::GFU_$string";
	$FIELD_USAGE_STRING2INT{$string} = $int;
	$FIELD_USAGE_INT2STRING{$int} = $string;
    }
    sub FieldTypes {
	return keys %FIELD_TYPE_STRING2INT;
    }
    sub FieldUsages {
	return keys %FIELD_USAGE_STRING2INT;
    }
    sub RELEASE_PARENTS {
	my $self = shift;
	delete $BANDS{$self};
    }
    sub GetUsageOfCol {
	my($self, $col) = @_;
	$FIELD_USAGE_INT2STRING{_GetUsageOfCol($self, $col)};
    }
    sub GetColOfUsage {
	my($self, $usage) = @_;
	_GetColOfUsage($self, $FIELD_USAGE_STRING2INT{$usage});
    }
    sub GetTypeOfCol {
	my($self, $col) = @_;
	$FIELD_TYPE_INT2STRING{_GetTypeOfCol($self, $col)};
    }
    sub Columns {
	my $self = shift;
	my %columns;
	if (@_) { # create columns
	    %columns = @_;
	    for my $name (keys %columns) {
		$self->CreateColumn($name, $columns{$name}{Type}, $columns{$name}{Usage});
	    }
	}
	%columns = ();
	for my $c (0..$self->GetColumnCount-1) {
	    my $name = $self->GetNameOfCol($c);
	    $columns{$name}{Type} = $self->GetTypeOfCol($c);
	    $columns{$name}{Usage} = $self->GetUsageOfCol($c);
	}
	return %columns;
    }
    sub CreateColumn {
	my($self, $name, $type, $usage) = @_;
	_CreateColumn($self, $name, $FIELD_TYPE_STRING2INT{$type}, $FIELD_USAGE_STRING2INT{$usage});
    }
    sub Value {
	my($self, $row, $column) = @_;
	SetValueAsString($self, $row, $column, $_[3]) if defined $_[3];
	return unless defined wantarray;
	GetValueAsString($self, $row, $column);
    }

 1;
