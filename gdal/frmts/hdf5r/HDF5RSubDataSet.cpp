/*
 * HDF5RSubDataSet.cpp
 *
 *  Created on: May 10, 2018
 *      Author: nielson
 */

#define H5_USE_16_API

#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>

#include "HDF5RSubDataSet.h"
#include "HDF5RRasterBand.h"

#include "hdf5.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "Earth.h"

#include "Hdf5rFileAttributes.h"
#include "OpenOptions.h"

CPL_C_START
void GDALRegister_HDF5Rsubds(void);
CPL_C_END

//******************************************************************************
// GDAL Plugin Registration Function (not a class method)
//   declaration occurs in : gdal/gcore/gdal_frmts.h
//******************************************************************************
void GDALRegister_HDF5Rsubds()
{
    if( GDALGetDriverByName("HDF5Rsubds") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("HDF5Rsubds");

#ifdef GDAL_DCAP_RASTER
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
#endif

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Hierarchical Data Format Release 5 for OPIR Raster Data Image");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_hdf5r.html");
#ifdef GDAL_DMD_EXTENSIONS
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "h5 hdf5 h5r hdf5r");
#endif
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "NO");

    // Creation and Open options expressed in XML
    //   - As of GDAL 2.3 they are validated with -co
    //   - gdalinfo --format hdf5r displays the XML
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
                               HDF5RDataSet::OpenOptionsXML );

    poDriver->pfnOpen = HDF5RSubDataSet::Open;
    poDriver->pfnIdentify = HDF5RSubDataSet::Identify;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}

//******************************************************************************
// Constructor
//******************************************************************************
HDF5RSubDataSet::HDF5RSubDataSet()
: scid_( -1 ),
  sca_( -1 ),
  pasGCPList_( nullptr ),
  nGCPCount_( 0 ),
  userSetGcpMax_( true ),
  gcpProjection_( PROJ_WGS84 ),
  userProjectionStr_(),
  gcpXform_( nullptr )
{
}

//******************************************************************************
// Destructor
//******************************************************************************
HDF5RSubDataSet::~HDF5RSubDataSet()
{
    if ((pasGCPList_ != nullptr) && (nGCPCount_ > 0))
    {
        GDALDeinitGCPs(nGCPCount_, pasGCPList_ );
    }

    delete [] pasGCPList_;
    nGCPCount_ = 0;

    delete gcpXform_;
    gcpXform_ = nullptr;
}

//******************************************************************************
// Static Open Method
//   - required of all plugins
//   - Returns a GDAL DataSet if file matches basic tests and loads
//     successfully, nullptr otherwise
//******************************************************************************
GDALDataset* HDF5RSubDataSet::Open(GDALOpenInfo* gdalInfo)
{
    std::string fileDesc = gdalInfo->pszFilename;

    CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::Open() called for: %s", fileDesc.c_str() );

    if (!Identify(gdalInfo))
        return nullptr;

    // R/W access not supported for SUBDATASET Open()
    if (gdalInfo->eAccess == GA_Update)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The HDF5-R driver does not support update access to "
                  "existing datasets. Use Create() instead." );
        return nullptr;
    }

    // Parse the ':' separated components: HDF5R:<filename>:<frame_index>
    //  (Identify call above verified it is good already can ignore rc)
    HDF5RDataSet::Hdf5rSubDataDesc_t subDataDescriptor;
    parseSubDataDescriptor( fileDesc, &subDataDescriptor );

    // Open the HDF-R file and internal components, also gets the number
    // of image frames, OpenHdf5Components reports errors (but not missing
    // datasets)
    Hdf5rReader* hdf5rReader = new Hdf5rReader;
    if (!hdf5rReader->open( subDataDescriptor.fileName ))
    {
        delete hdf5rReader;
        return nullptr;
    }

    // warn if missing any primary HDF5-R datasets
    if (!hdf5rReader->haveGeoLocationData())
        CPLError( CE_Warning, CPLE_AppDefined,
                  "HDF5RSubDataSet::Open: GeoLocationData component not present for %s.",
                  fileDesc.c_str() );
    if (!hdf5rReader->haveFrameMetaData())
        CPLError( CE_Warning, CPLE_AppDefined,
                  "HDF5RSubDataSet::Open: frameMetaData component not present for %s.",
                  fileDesc.c_str() );
    if (!hdf5rReader->haveCalRawData())
        CPLError( CE_Warning, CPLE_AppDefined,
                  "HDF5RSubDataSet::Open: CalRawData component not present for %s.",
                  fileDesc.c_str() );

    // Create GDAL DataSet
    HDF5RSubDataSet* poDS = new HDF5RSubDataSet();

    // Make sure we don't try to do any pam stuff with this dataset.
    poDS->nPamFlags |= GPF_NOSAVE;

    // set some attributes in the GDAL dataset class
    // NOTE: the HDF5RSubDataSet takes ownership of the hdf5r reader (and
    //       eventually deletes it)
    poDS->setHdf5rReader( hdf5rReader );
    poDS->SetDescription( gdalInfo->pszFilename );

    // do remaining file processing in class method loadHdf5File()
    if (!poDS->loadHdf5File( subDataDescriptor.frameIndex,
                             gdalInfo->papszOpenOptions ))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

//******************************************************************************
// Read one complete frame of the source HDF5-R file into the GDAL datasubset
//******************************************************************************
bool HDF5RSubDataSet::loadHdf5File( unsigned frameIndex,
                                    char* const* ooList )
{
    bool rc = true;

    //--------------------------------------------------------------------------
    // get the command line GDAL open options (-oo)
    //--------------------------------------------------------------------------
    OpenOptions* openOptions = new OpenOptions;
    std::vector<Hdf5rAttributeBase*> attrLists;
    attrLists.push_back( openOptions );
    loadMapsFromMetadataList( ooList,
                              attrLists,
                              nullptr,
                              "Cmdline Open() option" );

    int32_t noGcp = FALSE;  // FALSE == set GCP's, TRUE == set affine xform
    openOptions->getValue( "NO_GCP", &noGcp );

    int32_t gcpMax = -225 ;  // Max GCPs allowed, 0 or negative ==> no maximum
    openOptions->getValue( "GCP_MAX", &gcpMax );
    if (gcpMax < 0)
    {
    	gcpMax = -gcpMax;
    	userSetGcpMax_ = false;
    }

    int32_t attrWarnMissing = 1; // Warn on missing HDF5-R attributes
    openOptions->getValue( "ATTR_WARN", &attrWarnMissing );

    int32_t attrRw = TRUE; // FALSE == attributes from fattrWarnMissing
                           // TRUE  == substitute single frame values
    openOptions->getValue( "ATTR_RW", &attrRw );

    double satLongitude = std::nan( "" );
    openOptions->getValue( "SAT_LON", &satLongitude );

    int32_t blankOffEarth = TRUE;
    openOptions->getValue( "BLANK_OFF_EARTH", &blankOffEarth );
    hdf5rReader_->blankOffEarthOnRead( blankOffEarth );



    // Set the requested GCP projection from the open option
    // constructed default is WGS84 if not specified
    openOptions->getValue( "PROJ", &userProjectionStr_ );
    if (!userProjectionStr_.empty())
    {
        if (userProjectionStr_ == "nsper")
        	gcpProjection_ = PROJ_NSPER;
        else if (userProjectionStr_ != "wgs84" )
        	gcpProjection_ = PROJ_USER;
    }

    //--------------------------------------------------------------------------
    // Get image dimensions for selected frame and set GDAL raster size
    // if no image then we abandon reading the HDF5-R file
    //--------------------------------------------------------------------------
    if (hdf5rReader_->getImageDimensions( frameIndex,
                                          &nRasterYSize,   // rows
                                          &nRasterXSize )) // cols
    {
        //--------------------------------------------------------------------------
        // get file level attributes from the HDF5 file
        //--------------------------------------------------------------------------
        // get the HDF5-R file attribute map which contains default values
        Hdf5rFileAttributes* fileAttributes = new Hdf5rFileAttributes;
        hdf5rReader_->fillFileAttrMap( fileAttributes->getAttrMap(), attrWarnMissing );

        //--------------------------------------------------------------------------
        // get geoLocationData attributes from the HDF5 file
        //--------------------------------------------------------------------------
        Hdf5rGeoLocAttributes* geoLocAttributes = new Hdf5rGeoLocAttributes;
        hdf5rReader_->fillGeoLocAttrMap( geoLocAttributes->getAttrMap(), attrWarnMissing );

        //--------------------------------------------------------------------------
        // get frame attributes from the HDF5-R file
        //--------------------------------------------------------------------------
        Hdf5rFrameData* hdf5rFrameData = new Hdf5rFrameData;
        if (hdf5rReader_->getFrameMetaData( frameIndex, hdf5rFrameData ))
        {
            CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::Open() frame Attributes: \n%s",
                      hdf5rFrameData->getFrameDataConstPtr()->toString().c_str() );
        }

        //--------------------------------------------------------------------------
        // Frame attributes contain the true raster size for the number of lines
        //--------------------------------------------------------------------------
        int32_t numLines = hdf5rFrameData->getFrameDataConstPtr()->numLines;
        int32_t numChannels = hdf5rFrameData->getFrameDataConstPtr()->numChannels;
        if (numLines < nRasterYSize)
        {
        	nRasterYSize = numLines;
        	hdf5rReader_->setRows( numLines );
        }
        if (numChannels < nRasterXSize)
        {
        	nRasterXSize = numChannels;
        	hdf5rReader_->setColumns( numChannels );
        }

        //--------------------------------------------------------------------------
        // Get SummaryMetaData from the HDF5-R file only if ATTR_RW is false
        //--------------------------------------------------------------------------
        // instantiate summary metadata default values and structure pointers
        std::vector<const CompoundBase*> errorInfoVect;
        std::vector<const CompoundBase*> seqInfoVect;

        // only read from file, if in attribute 'read-only' mode,
        // otherwise values are written for the single frame later in
        // setSingleFrameMetaData()
        if (!attrRw)
        {
            // Load from HDF5-R file
            hdf5rReader_->getSummaryMetadata( errorInfoVect, seqInfoVect );
        }

        //--------------------------------------------------------------------------
        // Create the GDAL RasterBand -- data is not loaded until the
        // IReadBlock method() is called, so no status to check here
        //--------------------------------------------------------------------------
        SetBand( 1, new HDF5RRasterBand( this,             // pointer to the DataSet
                                         1,                // band number starts at 1
                                         frameIndex,
                                         nRasterYSize,     // rows a.k.a lines
                                         nRasterXSize ) ); // cols a.k.a detectors in line
        CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::Open() set nRasterYSize (rows): %d nRrasterXsize (cols): %d",
                  nRasterYSize, nRasterXSize );

        //----------------------------------------------------------------------
        //  get (and check) the LOS grid -- then build GCPs
        //----------------------------------------------------------------------
        m3d::Vector satEcfMeters = hdf5rFrameData->getFrameDataPtr()->satPosECF;
        const Hdf5rLosGrid_t* losGrid = hdf5rReader_->getLosGrid( frameIndex,
                                                                  geoLocAttributes,
                                                                  satEcfMeters,
                                                                  earth_ );
        if (losGrid)
        {
            CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::Open() getLosGrid succeeded." );

            // Set the OGR projection reference
            if (gcpProjection_ == PROJ_WGS84)
            {
            	setWgs84OgrSpatialRef();
            }
            else
            {
            	if (gcpProjection_ == PROJ_NSPER)
            	{
            		std::pair<double, double> lat_lon = earth_.toGeocentricLatLon( satEcfMeters );
            		setNsperOgrSpatialRef( lat_lon.first * Earth::radToDeg,
            				lat_lon.second * Earth::radToDeg,
							satEcfMeters.magnitude() - earth_.getEquatorialRadius());
            	}
            	else
            	{
            		setUserOgrSpatialRef( userProjectionStr_ );
            	}

            	// get the GCP transform from WGS84 to the specified frame
            	gcpXform_ = buildGcpOgrXform( ogcWktProjectionInfo_ );
            }

            //------------------------------------------------------------------
            // if a satellite longitude override was supplied with the SAT_LON
            // open option, then apply that now
            // (this capability is usually only used to obtain data sets with
            //  some off-Earth data)
            //------------------------------------------------------------------
            if (!std::isnan( satLongitude ))
            {
                // compute geosync satellite vector from supplied longitude
                satEcfMeters = earth_.toEcef( 0.0,
                                              Earth::degToRad * satLongitude,
                                              Earth::GEO_SYNC_ALTITUDE_METERS );

                // set the frame meta-data satellite location
                hdf5rFrameData->getFrameDataPtr()->satPosECF[0] = satEcfMeters.i();
                hdf5rFrameData->getFrameDataPtr()->satPosECF[1] = satEcfMeters.j();
                hdf5rFrameData->getFrameDataPtr()->satPosECF[2] = satEcfMeters.k();

                // fetch the off-earth value for latitude/longitude
                double offEarthValue = -9999.0;
                geoLocAttributes->getValue( "OFF_EARTH_value", &offEarthValue );

                // rebuild the LOS grid
                hdf5rReader_->changeLosGridReference( satEcfMeters,
                                                      offEarthValue );
            }

            //------------------------------------------------------------------
            // build the GCP list and set flag if count is greater than 0
            //------------------------------------------------------------------
            haveGcps_ = (buildGcpListFromLosGrid( *losGrid, gcpMax ) > 0);
            if (haveGcps_)
            {
                // if open option requested to use affine transform instead
                // of the GCPs
                if (noGcp)
                {
                    // build the affine transform from the GCPs
                    double transform[GDAL_XFORM_SZ];
                    if (GDALGCPsToGeoTransform( nGCPCount_, pasGCPList_, transform, TRUE ))
                    {
                        CPLDebug( HDF5R_DEBUG_STR, "Setting Affine Transform %f %f %f %f %f %f",
                                  transform[0], transform[1], transform[2],
                                  transform[3], transform[4], transform[5] );

                        SetGeoTransform( transform );
                        SetProjection( ogcWktProjectionInfo_.c_str() );
                    }
                    else
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "HDF5RSubDataSet::loadHdf5File GDALGCPsToGeoTransform"
                                  " failed to return an affine transform from"
                                  " GCPs (which came from the GEO Grid)" );
                }
                else // use GCPs
                {
                    // set the GCP count, data, and projection/coordinate reference
                    SetGCPs( nGCPCount_, pasGCPList_, ogcWktProjectionInfo_.c_str() );

                    CPLDebug( HDF5R_DEBUG_STR, "Setting GCPs" );
                }
            }
        }
        else
            CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::Open() getLosGrid failed." );

        //--------------------------------------------------------------------------
        // Modify file and summary attributes for the single frame
        //--------------------------------------------------------------------------
        if (attrRw)
        {
            setSingleFrameMetaData( hdf5rFrameData,
                                    losGrid,
                                    fileAttributes,
                                    &errorInfoVect,
                                    &seqInfoVect );
        }

        //--------------------------------------------------------------------------
        // Set GDAL name-value attributes corresponding to HDF5-R attributes
        //--------------------------------------------------------------------------
        // Pointer to name-value list using the CSLSetNameValue and friends
        char** nvList = nullptr;

        // Add attributes to the GDAL default domain (file level)
        //    - File level attributes
        loadGdalAttributes( fileAttributes, nvList );

        //    - GeoLocation attributes
        loadGdalAttributes( geoLocAttributes, nvList );

        //    - Frame attributes
        loadGdalCompoundAttributes( hdf5rFrameData,
                                    hdf5rFrameData->getFrameDataConstPtr()->frameNumber,
                                    nvList );

        //    - Summary attributes
        for (unsigned i=0; i<errorInfoVect.size(); ++i)
            loadGdalCompoundAttributes( errorInfoVect[i],
                                        i,
                                        nvList );

        for (unsigned i=0; i<seqInfoVect.size(); ++i)
            loadGdalCompoundAttributes( seqInfoVect[i],
                                        i,
                                        nvList );

        //    - TIFFTAG_DATETIME
        loadGdalTiffTimeTag( hdf5rFrameData->getFrameDataConstPtr(), nvList );

        // Establish the GDAL name-value attributes
        SetMetadata( nvList );

        // Cleanup
        CSLDestroy( nvList );
        delete geoLocAttributes;
        delete fileAttributes;
        delete hdf5rFrameData;

        for (unsigned i=0; i<errorInfoVect.size(); ++i)
            delete errorInfoVect[i];

        for (unsigned i=0; i<seqInfoVect.size(); ++i)
            delete seqInfoVect[i];
    }
    else
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "HDF5RSubDataSet::Open() failed to get image dimensions for %s",
                  hdf5rReader_->getFileName().c_str() );

    delete openOptions;

    return rc;
}

//******************************************************************************
// Build the WKT string for WGS-84 lat-lon projection
//******************************************************************************
OGRErr HDF5RSubDataSet::setWgs84OgrSpatialRef()
{
    // Set up the OGR Spatial Reference info
    // From http://www.gdal.org/osr_tutorial.html
    OGRSpatialReference oSRS;
    OGRErr rc = oSRS.SetWellKnownGeogCS( "WGS84" );

    if (rc == OGRERR_NONE)
    {
        // Extract the "Well Known Text" string and save in the Dataset attribute
        // for use by the GetProjectionRef() method
        char *wktStr;
        oSRS.exportToWkt(&wktStr);
        ogcWktProjectionInfo_ = wktStr; // to std::string (not a pointer copy)
        CPLFree(wktStr);
        CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::Open() OGR string: %s",
                  ogcWktProjectionInfo_.c_str() );
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "HDF5RSubDataSet::setWgs84OgrSpatialRef()"
                  " failed to set WGS84 reference. OGRErr code: %d", rc );
    }

    return rc;
}

//******************************************************************************
// Set the WKT string for Near Side Perspective Projection
//******************************************************************************
OGRErr HDF5RSubDataSet::setNsperOgrSpatialRef(double lat, double lon, double altitude_m)
{
	// Set up the OGR Spatial Reference info
	// From http://www.gdal.org/osr_tutorial.html
	OGRSpatialReference oSRS;

	CPLString oProj4String;
	oProj4String.Printf( "+proj=nsper +wktext +lat_0=%.18g +lon_0=%.18g +h=%.18g "
			"+x_0=0.0 +y_0=0.0 +ellps=WGS84 +units=m",
			lat, lon, altitude_m );

	OGRErr rc = oSRS.SetFromUserInput( oProj4String.c_str() );

	if (rc == OGRERR_NONE)
	{
		char *pszWKT = NULL;
		oSRS.exportToWkt( &pszWKT );
		//printf( "NSPER WKT: %s\n", pszWKT );
		ogcWktProjectionInfo_ = pszWKT;
		CPLFree(pszWKT);

		CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::Open() NSPER OGR string: %s",
				ogcWktProjectionInfo_.c_str() );
	}
	else
	{
		CPLError( CE_Warning, CPLE_AppDefined,
				"HDF5RSubDataSet::setWgs84OgrSpatialRef()"
				" failed to set NSPER reference. OGRErr code: %d", rc );
	}

	return rc;
}

//******************************************************************************
// Set the WKT string from user Open Option input
//******************************************************************************
OGRErr HDF5RSubDataSet::setUserOgrSpatialRef(const CPLString& userStr)
{
	// Set up the OGR Spatial Reference info
	// From http://www.gdal.org/osr_tutorial.html
	OGRSpatialReference oSRS;

	OGRErr rc = oSRS.SetFromUserInput( userStr.c_str() );

	if (rc == OGRERR_NONE)
	{
		char *pszWKT = NULL;
		oSRS.exportToWkt( &pszWKT );
		//printf( "USER WKT: %s\n", pszWKT );
		ogcWktProjectionInfo_ = pszWKT;
		CPLFree(pszWKT);

		CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::Open() USER OGR string: %s",
				ogcWktProjectionInfo_.c_str() );
	}
	else
	{
		CPLError( CE_Warning, CPLE_AppDefined,
				"HDF5RSubDataSet::setWgs84OgrSpatialRef()"
				" failed to set USER reference. OGRErr code: %d", rc );
	}

	return rc;
}

//******************************************************************************
// Get the OGR spatial reference transform from WGS84 to target frame
//******************************************************************************
OGRCoordinateTransformation* HDF5RSubDataSet::buildGcpOgrXform( const std::string ogrWkt )
{
	// Source (the HDF5-R GEO-location grid is always in WGS84 lat, lon
    OGRSpatialReference oSrcSRS;
    oSrcSRS.SetWellKnownGeogCS( "WGS84" );

    // Target supplied by caller
    OGRSpatialReference oTgtSRS;
    oTgtSRS.SetFromUserInput( ogrWkt.c_str() );

    OGRCoordinateTransformation* rc = OGRCreateCoordinateTransformation( &oSrcSRS,
    		                                                             &oTgtSRS );
    if (!rc)
    {
    	CPLError( CE_Warning, CPLE_AppDefined,
    			"HDF5RSubDataSet::buildGcpOgrXform()"
    			" failed to build transform from WGS84 to %s."
    		    " Sticking with GCPs in the WGS84 reference", ogrWkt.c_str() );
    }

    return rc;
}

//******************************************************************************
// Static Identity Method
//   - required of all plugins
//   - Returns a 1 if file matches basic tests, 0 otherwise
//******************************************************************************
int HDF5RSubDataSet::Identify(GDALOpenInfo* gdalInfo)
{
    int rc = GDAL_IDENTIFY_FALSE;
    std::string fileDesc = gdalInfo->pszFilename;

    // parse potential SUBDATASET descriptor, if successful and header matches
    // then we are good to go
    HDF5RDataSet::Hdf5rSubDataDesc_t subDataDescriptor;
    if (parseSubDataDescriptor( fileDesc, &subDataDescriptor )
                               && (subDataDescriptor.hdr == "HDF5R"))
    {
        CPLDebug(HDF5R_DEBUG_STR, "HDF5RSubDataSet::Identify():\n"
                  "\tthis is an HDF5-R SUBDATASET...\n"
                  "\thdr: %s\n\tfile: %s\n\tframeIndex: %d",
                  subDataDescriptor.hdr.c_str (),
                  subDataDescriptor.fileName.c_str (),
                  subDataDescriptor.frameIndex );
        rc = GDAL_IDENTIFY_TRUE;
    }

    CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::Identify() for: %s result=%d",
              fileDesc.c_str (), rc );

    return rc;
}

//******************************************************************************
// Build GDAL name=value strings from an attribute map
//******************************************************************************
bool HDF5RSubDataSet::loadGdalAttributes( const Hdf5rAttributeBase* attributes,
                                          char**& nvList ) const
{
    const Hdf5rAttributeBase::H5AttrMap_t& fileAttrMap = attributes->getConstAttrMap();
    Hdf5rAttributeBase::H5AttrMap_t::const_iterator i = fileAttrMap.begin();
    while (i != fileAttrMap.end())
    {
        const Hdf5rAttributeBase::H5Attr_t& attr = i->second;
        const std::string& attrName = i->first;
        // Add the HDF5-R scalar attributes
        nvList = CSLSetNameValue( nvList, attrName.c_str(), attr.toString().c_str() );
        ++i;
    }
    return true;
}

//******************************************************************************
// Load the frame attributes from the class attribute frameAttributes_
//     - returns false and does nothing if frameAttributes_ is null
//******************************************************************************
bool HDF5RSubDataSet::loadGdalCompoundAttributes( const CompoundBase* compound,
                                                  unsigned attrIndex,
                                                  char**& nvList ) const
{
    const CompoundBase::CompoundElementMap_t& attrMap = compound->getAttrMap();
    const CompoundBase::CompoundData_t* attrData = compound->getConstCompoundDataPtr();

    Hdf5rFrameData::CompoundElementMap_t::const_iterator imap = attrMap.begin();
    while( imap != attrMap.end())
    {
        // reference to the element components
        const std::string& gdalAttrNameFormat = imap->first;
        const Hdf5rFrameData::CompoundElement_t& frameEl = imap->second;

        // add frame number to the attribute name
        std::string gdalAttrName = compound->formatAttribute( gdalAttrNameFormat,
                                                              attrIndex );

        // Add the HDF5-R scalar attributes
        nvList = CSLSetNameValue( nvList,
                                  gdalAttrName.c_str(),
                                  frameEl.toString( attrData ).c_str() );
        ++imap;
    }

    return true;
}

//******************************************************************************
//  method to build the GDAL attribute for TIFFTAG_DATETIME
//******************************************************************************
bool HDF5RSubDataSet::loadGdalTiffTimeTag( const Hdf5rFrameData::FrameData_t* frameData,
                                           char**& nvList ) const
{
    if ((!frameData) || (!nvList))
        return false;

    // -------------------------- TIFFTAG_DATETIME -----------------------
    // Compute Unix Epoch time from year, day-of-year, and seconds-of-day,
    // and the cumulative number of leap days
    int dyear = frameData->year - 1970;
    time_t utime = 86400 * (365 * dyear + (dyear-1)/4 + frameData->day)
    		+ int( frameData->secondsOfDay );
    CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::loadGdalTiffTimeTag"
              "Unix epoch time: %ld", utime );

    // Use mktime to get unix time at start of day, then add seconds of
    // day and regenerate the broken down time
    struct tm tmstruct;
    bzero( &tmstruct, sizeof(struct tm) );
    gmtime_r( &utime, &tmstruct );

    // build and set the TIFF time tag string
    std::ostringstream oss;
    oss << (tmstruct.tm_year + 1900) << ":"
            << std::setfill( '0' )
    << std::setw( 2 ) << (tmstruct.tm_mon + 1) << ":"
    << std::setw( 2 ) << tmstruct.tm_mday << " "
    << std::setw( 2 ) << tmstruct.tm_hour << ":"
    << std::setw( 2 ) << tmstruct.tm_min << ":"
    << std::setw( 2 ) << tmstruct.tm_sec;
    nvList = CSLSetNameValue( nvList, "TIFFTAG_DATETIME", oss.str().c_str() );

    return true;
}

//******************************************************************************
// Build the GCP list from a valid LosGrid
//******************************************************************************
int HDF5RSubDataSet::buildGcpListFromLosGrid( const Hdf5rLosGrid_t& losGrid,
                                              int gcpMax )
{
    int iGcp = 0;

    if (losGrid.isValid())
    {
        // columns == pixels in GCP-speak
        int nPixels = losGrid.getNcols();

        // rows == lines in GCP-speak
        int nLines = losGrid.getNrows();

        // initial number of GCPs is the number of on-Earth points in the grid
        int szGcpList = losGrid.getNumOnEarth();

        // limit the number of GCPs to less than gcpMax. Some drivers,
        // notably gtiff, limit the number of GCPs
        //      note that gcpMax <= 0 indicates no limit
        int factor1d = 1;
        if ((gcpMax > 0) && (szGcpList > gcpMax))
        {
            factor1d = int(std::ceil( std::sqrt( double(szGcpList) / double(gcpMax) ) ));
            szGcpList = ((nPixels + factor1d - 1)/factor1d) * ((nLines + factor1d - 1)/factor1d);

            if (!userSetGcpMax_)
              CPLError( CE_Warning, CPLE_AppDefined,
                      "HDF5RSubDataSet::buildGcpListFromLosGrid: Limited the GCP count.\n"
                      "\t GCP_MAX=%d Input Count=%d Reduction factor=%d (each dimension)\n"
                      "\t Resulting count=%d from (rows=%d/%d=%d  * columns=%d/%d=%d)\n"
                      "\t Override: Most GDAL commands support open option: -oo GCP_MAX=N  for no limit: N=0\n"
                      "\t However:  Many drivers, notably geotiff, limit the GCP list size.\n",
                      gcpMax, nPixels * nLines, factor1d, szGcpList,
                      nLines, factor1d, (nLines + factor1d - 1)/factor1d,
                      nPixels, factor1d, (nPixels + factor1d - 1)/factor1d );
        }

        // remove old GCP list if present
        if ((pasGCPList_ != nullptr) && (nGCPCount_ > 0))
        {
            GDALDeinitGCPs(nGCPCount_, pasGCPList_ );
        }
        delete [] pasGCPList_;

        // allocate the GCP set and initialize it
        nGCPCount_ = 0;
        pasGCPList_ = new GDAL_GCP[szGcpList];
        GDALInitGCPs( szGcpList, pasGCPList_ );

        // iterate over lines incrementing by the 1D reduction factor
        // do not include overhang grid point (the nlines-1)
        for (int iLine=0; iLine<nLines-1; iLine += factor1d)
        {
            // iterate over pixels
            // do not include overhang grid point (the -1)
            for (int iPixel=0; iPixel<nPixels-1; iPixel += factor1d)
            {
                // reference to the grid entry
                const Hdf5rLosGrid_t::Hdf5rLosData_t& losData = losGrid( iLine, iPixel );

                // if on-Earth, load the GCP and increment the count
                if (!losData.oth_)
                {
                    // verify that iGcp is in range (if not then there is
                    // a discrepancy between the true number of on-Earth points
                    // and the value returned by losGrid->getNumOnEarth()
                    if (iGcp < szGcpList)
                    {
                        GDAL_GCP& gcp = pasGCPList_[iGcp];
                        CPLFree( gcp.pszId );
                        gcp.pszId = CPLStrdup( std::to_string( iGcp ).c_str() );
                        gcp.dfGCPPixel = double( iPixel * losGrid.getColStepSize() + 0.5 );
                        gcp.dfGCPLine  = double( iLine  * losGrid.getRowStepSize() + 0.5 );

                        double x = losData.map_X;
                        double y = losData.map_Y;

                        if (gcpXform_)
                          gcpXform_->Transform( 1, &x, &y );

                        gcp.dfGCPX = x;
                        gcp.dfGCPY = y;

                        gcp.dfGCPZ = 0.0;

                        // debug print of first point
                        if (iGcp == 0)
                        {
                            CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::buildGcpListFromLosGrid"
                                      "Input first pt line %f pixel %f lat-lon: %f %f",
                                      gcp.dfGCPPixel, gcp.dfGCPLine,
                                      gcp.dfGCPY, gcp.dfGCPX );
                        }

                        ++iGcp;
                    }
                    else
                    {
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "HDF5RSubDataSet::buildGcpListFromLosGrid: LosGrid on-Earth counts"
                                  " are inconsistent. GCP list truncated." );
                    }
                }
            } // pixel (column) loop
        } // line (row) loop

        nGCPCount_ = iGcp;

        CPLDebug( HDF5R_DEBUG_STR, "HDF5RSubDataSet::buildGcpListFromLosGrid"
                  " Number of GCPs generated: %d (in: %d gcpMax: %d 1D factor: %d)",
                  iGcp, szGcpList, gcpMax, factor1d );

    } // grid is OK

    return iGcp;
}
