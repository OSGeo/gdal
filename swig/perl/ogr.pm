# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
package ogr;
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
require osr;
package ogrc;
bootstrap ogr;
package ogr;
@EXPORT = qw( );

# ---------- BASE METHODS -------------

package ogr;

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

package ogr;

*UseExceptions = *ogrc::UseExceptions;
*DontUseExceptions = *ogrc::DontUseExceptions;
*CreateGeometryFromWkb = *ogrc::CreateGeometryFromWkb;
*CreateGeometryFromWkt = *ogrc::CreateGeometryFromWkt;
*CreateGeometryFromGML = *ogrc::CreateGeometryFromGML;
*GetDriverCount = *ogrc::GetDriverCount;
*GetOpenDSCount = *ogrc::GetOpenDSCount;
*SetGenerate_DB2_V72_BYTE_ORDER = *ogrc::SetGenerate_DB2_V72_BYTE_ORDER;
*RegisterAll = *ogrc::RegisterAll;
*GetOpenDS = *ogrc::GetOpenDS;
*Open = *ogrc::Open;
*OpenShared = *ogrc::OpenShared;
*GetDriverByName = *ogrc::GetDriverByName;
*GetDriver = *ogrc::GetDriver;

############# Class : ogr::Driver ##############

package ogr::Driver;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( ogr );
%OWNER = ();
%ITERATORS = ();
*swig_name_get = *ogrc::Driver_name_get;
*swig_name_set = *ogrc::Driver_name_set;
*CreateDataSource = *ogrc::Driver_CreateDataSource;
*CopyDataSource = *ogrc::Driver_CopyDataSource;
*Open = *ogrc::Driver_Open;
*DeleteDataSource = *ogrc::Driver_DeleteDataSource;
*TestCapability = *ogrc::Driver_TestCapability;
*GetName = *ogrc::Driver_GetName;
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


############# Class : ogr::DataSource ##############

package ogr::DataSource;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( ogr );
%OWNER = ();
%ITERATORS = ();
*swig_name_get = *ogrc::DataSource_name_get;
*swig_name_set = *ogrc::DataSource_name_set;
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        ogrc::delete_DataSource($self);
        delete $OWNER{$self};
    }
}

*GetRefCount = *ogrc::DataSource_GetRefCount;
*GetSummaryRefCount = *ogrc::DataSource_GetSummaryRefCount;
*GetLayerCount = *ogrc::DataSource_GetLayerCount;
*GetDriver = *ogrc::DataSource_GetDriver;
*GetName = *ogrc::DataSource_GetName;
*DeleteLayer = *ogrc::DataSource_DeleteLayer;
*CreateLayer = *ogrc::DataSource_CreateLayer;
*CopyLayer = *ogrc::DataSource_CopyLayer;
*GetLayerByIndex = *ogrc::DataSource_GetLayerByIndex;
*GetLayerByName = *ogrc::DataSource_GetLayerByName;
*TestCapability = *ogrc::DataSource_TestCapability;
*ExecuteSQL = *ogrc::DataSource_ExecuteSQL;
*ReleaseResultSet = *ogrc::DataSource_ReleaseResultSet;
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


############# Class : ogr::Layer ##############

package ogr::Layer;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( ogr );
%OWNER = ();
*GetRefCount = *ogrc::Layer_GetRefCount;
*SetSpatialFilter = *ogrc::Layer_SetSpatialFilter;
*SetSpatialFilterRect = *ogrc::Layer_SetSpatialFilterRect;
*GetSpatialFilter = *ogrc::Layer_GetSpatialFilter;
*SetAttributeFilter = *ogrc::Layer_SetAttributeFilter;
*ResetReading = *ogrc::Layer_ResetReading;
*GetName = *ogrc::Layer_GetName;
*GetFeature = *ogrc::Layer_GetFeature;
*GetNextFeature = *ogrc::Layer_GetNextFeature;
*SetNextByIndex = *ogrc::Layer_SetNextByIndex;
*SetFeature = *ogrc::Layer_SetFeature;
*CreateFeature = *ogrc::Layer_CreateFeature;
*DeleteFeature = *ogrc::Layer_DeleteFeature;
*SyncToDisk = *ogrc::Layer_SyncToDisk;
*GetLayerDefn = *ogrc::Layer_GetLayerDefn;
*GetFeatureCount = *ogrc::Layer_GetFeatureCount;
*GetExtent = *ogrc::Layer_GetExtent;
*TestCapability = *ogrc::Layer_TestCapability;
*CreateField = *ogrc::Layer_CreateField;
*StartTransaction = *ogrc::Layer_StartTransaction;
*CommitTransaction = *ogrc::Layer_CommitTransaction;
*RollbackTransaction = *ogrc::Layer_RollbackTransaction;
*GetSpatialRef = *ogrc::Layer_GetSpatialRef;
*GetFeatureRead = *ogrc::Layer_GetFeatureRead;
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


############# Class : ogr::Feature ##############

package ogr::Feature;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( ogr );
%OWNER = ();
%ITERATORS = ();
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        ogrc::delete_Feature($self);
        delete $OWNER{$self};
    }
}

sub new {
    my $pkg = shift;
    my $self = ogrc::new_Feature(@_);
    bless $self, $pkg if defined($self);
}

*GetDefnRef = *ogrc::Feature_GetDefnRef;
*SetGeometry = *ogrc::Feature_SetGeometry;
*SetGeometryDirectly = *ogrc::Feature_SetGeometryDirectly;
*GetGeometryRef = *ogrc::Feature_GetGeometryRef;
*Clone = *ogrc::Feature_Clone;
*Equal = *ogrc::Feature_Equal;
*GetFieldCount = *ogrc::Feature_GetFieldCount;
*GetFieldDefnRef = *ogrc::Feature_GetFieldDefnRef;
*GetFieldAsString = *ogrc::Feature_GetFieldAsString;
*GetFieldAsInteger = *ogrc::Feature_GetFieldAsInteger;
*GetFieldAsDouble = *ogrc::Feature_GetFieldAsDouble;
*IsFieldSet = *ogrc::Feature_IsFieldSet;
*GetFieldIndex = *ogrc::Feature_GetFieldIndex;
*GetFID = *ogrc::Feature_GetFID;
*SetFID = *ogrc::Feature_SetFID;
*DumpReadable = *ogrc::Feature_DumpReadable;
*UnsetField = *ogrc::Feature_UnsetField;
*SetField = *ogrc::Feature_SetField;
*SetFrom = *ogrc::Feature_SetFrom;
*GetStyleString = *ogrc::Feature_GetStyleString;
*SetStyleString = *ogrc::Feature_SetStyleString;
*GetFieldType = *ogrc::Feature_GetFieldType;
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


############# Class : ogr::FeatureDefn ##############

package ogr::FeatureDefn;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( ogr );
%OWNER = ();
%ITERATORS = ();
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        ogrc::delete_FeatureDefn($self);
        delete $OWNER{$self};
    }
}

sub new {
    my $pkg = shift;
    my $self = ogrc::new_FeatureDefn(@_);
    bless $self, $pkg if defined($self);
}

*GetName = *ogrc::FeatureDefn_GetName;
*GetFieldCount = *ogrc::FeatureDefn_GetFieldCount;
*GetFieldDefn = *ogrc::FeatureDefn_GetFieldDefn;
*GetFieldIndex = *ogrc::FeatureDefn_GetFieldIndex;
*AddFieldDefn = *ogrc::FeatureDefn_AddFieldDefn;
*GetGeomType = *ogrc::FeatureDefn_GetGeomType;
*SetGeomType = *ogrc::FeatureDefn_SetGeomType;
*GetReferenceCount = *ogrc::FeatureDefn_GetReferenceCount;
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


############# Class : ogr::FieldDefn ##############

package ogr::FieldDefn;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( ogr );
%OWNER = ();
%ITERATORS = ();
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        ogrc::delete_FieldDefn($self);
        delete $OWNER{$self};
    }
}

sub new {
    my $pkg = shift;
    my $self = ogrc::new_FieldDefn(@_);
    bless $self, $pkg if defined($self);
}

*GetName = *ogrc::FieldDefn_GetName;
*GetNameRef = *ogrc::FieldDefn_GetNameRef;
*SetName = *ogrc::FieldDefn_SetName;
*GetType = *ogrc::FieldDefn_GetType;
*SetType = *ogrc::FieldDefn_SetType;
*GetJustify = *ogrc::FieldDefn_GetJustify;
*SetJustify = *ogrc::FieldDefn_SetJustify;
*GetWidth = *ogrc::FieldDefn_GetWidth;
*SetWidth = *ogrc::FieldDefn_SetWidth;
*GetPrecision = *ogrc::FieldDefn_GetPrecision;
*SetPrecision = *ogrc::FieldDefn_SetPrecision;
*GetFieldTypeName = *ogrc::FieldDefn_GetFieldTypeName;
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


############# Class : ogr::Geometry ##############

package ogr::Geometry;
use vars qw(@ISA %OWNER %ITERATORS %BLESSEDMEMBERS);
@ISA = qw( ogr );
%OWNER = ();
%ITERATORS = ();
sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        ogrc::delete_Geometry($self);
        delete $OWNER{$self};
    }
}

sub new {
    my $pkg = shift;
    my $self = ogrc::new_Geometry(@_);
    bless $self, $pkg if defined($self);
}

*ExportToWkt = *ogrc::Geometry_ExportToWkt;
*ExportToWkb = *ogrc::Geometry_ExportToWkb;
*ExportToGML = *ogrc::Geometry_ExportToGML;
*AddPoint = *ogrc::Geometry_AddPoint;
*AddGeometryDirectly = *ogrc::Geometry_AddGeometryDirectly;
*AddGeometry = *ogrc::Geometry_AddGeometry;
*Clone = *ogrc::Geometry_Clone;
*GetGeometryType = *ogrc::Geometry_GetGeometryType;
*GetGeometryName = *ogrc::Geometry_GetGeometryName;
*GetArea = *ogrc::Geometry_GetArea;
*GetPointCount = *ogrc::Geometry_GetPointCount;
*GetX = *ogrc::Geometry_GetX;
*GetY = *ogrc::Geometry_GetY;
*GetZ = *ogrc::Geometry_GetZ;
*GetGeometryCount = *ogrc::Geometry_GetGeometryCount;
*SetPoint = *ogrc::Geometry_SetPoint;
*GetGeometryRef = *ogrc::Geometry_GetGeometryRef;
*GetBoundary = *ogrc::Geometry_GetBoundary;
*ConvexHull = *ogrc::Geometry_ConvexHull;
*Buffer = *ogrc::Geometry_Buffer;
*Intersection = *ogrc::Geometry_Intersection;
*Union = *ogrc::Geometry_Union;
*Difference = *ogrc::Geometry_Difference;
*SymmetricDifference = *ogrc::Geometry_SymmetricDifference;
*Distance = *ogrc::Geometry_Distance;
*Empty = *ogrc::Geometry_Empty;
*Intersect = *ogrc::Geometry_Intersect;
*Equal = *ogrc::Geometry_Equal;
*Disjoint = *ogrc::Geometry_Disjoint;
*Touches = *ogrc::Geometry_Touches;
*Crosses = *ogrc::Geometry_Crosses;
*Within = *ogrc::Geometry_Within;
*Contains = *ogrc::Geometry_Contains;
*Overlaps = *ogrc::Geometry_Overlaps;
*TransformTo = *ogrc::Geometry_TransformTo;
*Transform = *ogrc::Geometry_Transform;
*GetSpatialReference = *ogrc::Geometry_GetSpatialReference;
*AssignSpatialReference = *ogrc::Geometry_AssignSpatialReference;
*CloseRings = *ogrc::Geometry_CloseRings;
*FlattenTo2D = *ogrc::Geometry_FlattenTo2D;
*GetEnvelope = *ogrc::Geometry_GetEnvelope;
*Centroid = *ogrc::Geometry_Centroid;
*WkbSize = *ogrc::Geometry_WkbSize;
*GetCoordinateDimension = *ogrc::Geometry_GetCoordinateDimension;
*SetCoordinateDimension = *ogrc::Geometry_SetCoordinateDimension;
*GetDimension = *ogrc::Geometry_GetDimension;
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

package ogr;

*wkb25Bit = *ogrc::wkb25Bit;
*wkbUnknown = *ogrc::wkbUnknown;
*wkbPoint = *ogrc::wkbPoint;
*wkbLineString = *ogrc::wkbLineString;
*wkbPolygon = *ogrc::wkbPolygon;
*wkbMultiPoint = *ogrc::wkbMultiPoint;
*wkbMultiLineString = *ogrc::wkbMultiLineString;
*wkbMultiPolygon = *ogrc::wkbMultiPolygon;
*wkbGeometryCollection = *ogrc::wkbGeometryCollection;
*wkbNone = *ogrc::wkbNone;
*wkbLinearRing = *ogrc::wkbLinearRing;
*wkbPoint25D = *ogrc::wkbPoint25D;
*wkbLineString25D = *ogrc::wkbLineString25D;
*wkbPolygon25D = *ogrc::wkbPolygon25D;
*wkbMultiPoint25D = *ogrc::wkbMultiPoint25D;
*wkbMultiLineString25D = *ogrc::wkbMultiLineString25D;
*wkbMultiPolygon25D = *ogrc::wkbMultiPolygon25D;
*wkbGeometryCollection25D = *ogrc::wkbGeometryCollection25D;
*OFTInteger = *ogrc::OFTInteger;
*OFTIntegerList = *ogrc::OFTIntegerList;
*OFTReal = *ogrc::OFTReal;
*OFTRealList = *ogrc::OFTRealList;
*OFTString = *ogrc::OFTString;
*OFTStringList = *ogrc::OFTStringList;
*OFTWideString = *ogrc::OFTWideString;
*OFTWideStringList = *ogrc::OFTWideStringList;
*OFTBinary = *ogrc::OFTBinary;
*OFTDate = *ogrc::OFTDate;
*OFTTime = *ogrc::OFTTime;
*OFTDateTime = *ogrc::OFTDateTime;
*OJUndefined = *ogrc::OJUndefined;
*OJLeft = *ogrc::OJLeft;
*OJRight = *ogrc::OJRight;
*wkbXDR = *ogrc::wkbXDR;
*wkbNDR = *ogrc::wkbNDR;
*OLCRandomRead = *ogrc::OLCRandomRead;
*OLCSequentialWrite = *ogrc::OLCSequentialWrite;
*OLCRandomWrite = *ogrc::OLCRandomWrite;
*OLCFastSpatialFilter = *ogrc::OLCFastSpatialFilter;
*OLCFastFeatureCount = *ogrc::OLCFastFeatureCount;
*OLCFastGetExtent = *ogrc::OLCFastGetExtent;
*OLCCreateField = *ogrc::OLCCreateField;
*OLCTransactions = *ogrc::OLCTransactions;
*OLCDeleteFeature = *ogrc::OLCDeleteFeature;
*OLCFastSetNextByIndex = *ogrc::OLCFastSetNextByIndex;
*ODsCCreateLayer = *ogrc::ODsCCreateLayer;
*ODsCDeleteLayer = *ogrc::ODsCDeleteLayer;
*ODrCCreateDataSource = *ogrc::ODrCCreateDataSource;
*ODrCDeleteDataSource = *ogrc::ODrCDeleteDataSource;
1;
