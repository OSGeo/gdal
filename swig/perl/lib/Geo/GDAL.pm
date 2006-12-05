# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
package Geo::GDAL;
use Geo::GDAL::Const; 
use Geo::OGR; 
use Geo::OSR; 
our $VERSION = '0.20';
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
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

*UseExceptions = *Geo::GDALc::UseExceptions;
*DontUseExceptions = *Geo::GDALc::DontUseExceptions;
*Debug = *Geo::GDALc::Debug;
*Error = *Geo::GDALc::Error;
*PushErrorHandler = *Geo::GDALc::PushErrorHandler;
*PopErrorHandler = *Geo::GDALc::PopErrorHandler;
*ErrorReset = *Geo::GDALc::ErrorReset;
*GetLastErrorNo = *Geo::GDALc::GetLastErrorNo;
*GetLastErrorType = *Geo::GDALc::GetLastErrorType;
*GetLastErrorMsg = *Geo::GDALc::GetLastErrorMsg;
*PushFinderLocation = *Geo::GDALc::PushFinderLocation;
*PopFinderLocation = *Geo::GDALc::PopFinderLocation;
*FinderClean = *Geo::GDALc::FinderClean;
*FindFile = *Geo::GDALc::FindFile;
*SetConfigOption = *Geo::GDALc::SetConfigOption;
*GetConfigOption = *Geo::GDALc::GetConfigOption;
*CPLBinaryToHex = *Geo::GDALc::CPLBinaryToHex;
*CPLHexToBinary = *Geo::GDALc::CPLHexToBinary;
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
*AllRegister = *Geo::GDALc::AllRegister;
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
*GetDriver = *Geo::GDALc::GetDriver;
*Open = *Geo::GDALc::Open;
*OpenShared = *Geo::GDALc::OpenShared;
*AutoCreateWarpedVRT = *Geo::GDALc::AutoCreateWarpedVRT;
*GeneralCmdLineProcessor = *Geo::GDALc::GeneralCmdLineProcessor;

############# Class : Geo::GDAL::MajorObject ##############

package Geo::GDAL::MajorObject;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( Geo::GDAL );
%OWNER = ();
*GetDescription = *Geo::GDALc::MajorObject_GetDescription;
*SetDescription = *Geo::GDALc::MajorObject_SetDescription;
*GetMetadata = *Geo::GDALc::MajorObject_GetMetadata;
*SetMetadata = *Geo::GDALc::MajorObject_SetMetadata;
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
*Create = *Geo::GDALc::Driver_Create;
*CreateCopy = *Geo::GDALc::Driver_CreateCopy;
*Delete = *Geo::GDALc::Driver_Delete;
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
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::GDALc::delete_GCP($self);
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
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::GDALc::delete_Dataset($self);
        delete $OWNER{$self};
    }
}

*GetDriver = *Geo::GDALc::Dataset_GetDriver;
*GetRasterBand = *Geo::GDALc::Dataset_GetRasterBand;
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
*AddBand = *Geo::GDALc::Dataset_AddBand;
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
*GetBlockSize = *Geo::GDALc::Band_GetBlockSize;
*GetRasterColorInterpretation = *Geo::GDALc::Band_GetRasterColorInterpretation;
*SetRasterColorInterpretation = *Geo::GDALc::Band_SetRasterColorInterpretation;
*GetNoDataValue = *Geo::GDALc::Band_GetNoDataValue;
*SetNoDataValue = *Geo::GDALc::Band_SetNoDataValue;
*GetMinimum = *Geo::GDALc::Band_GetMinimum;
*GetMaximum = *Geo::GDALc::Band_GetMaximum;
*GetOffset = *Geo::GDALc::Band_GetOffset;
*GetScale = *Geo::GDALc::Band_GetScale;
*GetStatistics = *Geo::GDALc::Band_GetStatistics;
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
*SetRasterColorTable = *Geo::GDALc::Band_SetRasterColorTable;
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
%ITERATORS = ();
sub new {
    my $pkg = shift;
    my $self = Geo::GDALc::new_ColorTable(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::GDALc::delete_ColorTable($self);
        delete $OWNER{$self};
    }
}

*Clone = *Geo::GDALc::ColorTable_Clone;
*GetPaletteInterpretation = *Geo::GDALc::ColorTable_GetPaletteInterpretation;
*GetCount = *Geo::GDALc::ColorTable_GetCount;
*GetColorEntry = *Geo::GDALc::ColorTable_GetColorEntry;
*GetColorEntryAsRGB = *Geo::GDALc::ColorTable_GetColorEntryAsRGB;
*SetColorEntry = *Geo::GDALc::ColorTable_SetColorEntry;
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

1;
