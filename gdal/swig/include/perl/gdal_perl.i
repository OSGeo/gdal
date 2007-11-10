/*
 * $Id$
 *
 * perl specific code for gdal bindings.
 */

/*
 * $Log$
 * Revision 1.5  2006/12/09 17:57:38  ajolma
 * added sub PackCharacter, the uses, and the VERSION
 *
 * Revision 1.4  2005/09/16 19:17:46  kruland
 * Call UseExceptions on module init.
 *
 * Revision 1.3  2005/09/13 18:35:50  kruland
 * Rename GetMetadata_Dict to GetMetadata and ignore GetMetadata_List.
 *
 * Revision 1.2  2005/09/13 17:36:28  kruland
 * Whoops!  import typemaps_perl.i.
 *
 * Revision 1.1  2005/09/13 16:08:45  kruland
 * Added perl specific modifications for gdal and ogr.
 *
 *
 */

%init %{
  /* gdal_perl.i %init code */
  UseExceptions();
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%include cpl_exceptions.i

%rename (GetMetadata) GetMetadata_Dict;
%ignore GetMetadata_List;

%import typemaps_perl.i

%import destroy.i

ALTERED_DESTROY(GDALBandShadow, GDALc, delete_Band)
ALTERED_DESTROY(GDALColorTableShadow, GDALc, delete_ColorTable)
ALTERED_DESTROY(GDALConstShadow, GDALc, delete_Const)
ALTERED_DESTROY(GDALDatasetShadow, GDALc, delete_Dataset)
ALTERED_DESTROY(GDALDriverShadow, GDALc, delete_Driver)
ALTERED_DESTROY(GDAL_GCP, GDALc, delete_GCP)
ALTERED_DESTROY(GDALMajorObjectShadow, GDALc, delete_MajorObject)
ALTERED_DESTROY(GDALRasterAttributeTableShadow, GDALc, delete_RasterAttributeTable)

%perlcode %{
    use Carp;
    use Geo::GDAL::Const;
    use Geo::OGR;
    use Geo::OSR;
    our $VERSION = '0.21';
    sub PackCharacter {
	$_ = shift;
	my $is_big_endian = unpack("h*", pack("s", 1)) =~ /01/; # from Programming Perl
	if ($_ == $Geo::GDAL::Const::GDT_Byte) { return 'C'; }
	if ($_ == $Geo::GDAL::Const::GDT_UInt16) { return $is_big_endian ? 'n': 'v'; }
	if ($_ == $Geo::GDAL::Const::GDT_Int16) { return 's'; }
	if ($_ == $Geo::GDAL::Const::GDT_UInt32) { return $is_big_endian ? 'N' : 'V'; }
	if ($_ == $Geo::GDAL::Const::GDT_Int32) { return 'l'; }
	if ($_ == $Geo::GDAL::Const::GDT_Float32) { return 'f'; }
	if ($_ == $Geo::GDAL::Const::GDT_Float64) { return 'd'; }
	croak "unsupported data type: $_";
    }
%}
