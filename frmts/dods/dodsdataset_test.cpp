// -*- C++ -*-

// This file is part of the OPeNDAP/GDAL driver.

// Copyright (c) 2003 OPeNDAP, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
// Author: James Gallagher <jgallagher@opendap.org>
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.
 
#include <cppunit/TextTestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "dodsdataset.h"

CPL_CVSID("$Id$");

using namespace CppUnit;

class DODSDatasetTest : public TestFixture {
private:
    DODSDataset *poDataset;
    DODSDataset *poDS;
    string url_1, url_2;

public:
    DODSDatasetTest() : 
	url_1("http://localhost/dods-test/nph-dods/data/nc/fnoc1.nc?u[0][lat][lon]"),
	url_2("http://localhost/dods-test/nph-dods/data/nc/fnoc1.nc?v[0:15][lat][lon]")
    {
	// Register all the GDAL drivers. If you only want one driver
	// registered, look at gdalallregister.cpp. 
	GDALAllRegister();
    }

    ~DODSDatasetTest() {}
    
    void setUp () {
	poDS = new DODSDataset;
    }

    void tearDown() {
	delete poDS;
    }

    CPPUNIT_TEST_SUITE( DODSDatasetTest );

    CPPUNIT_TEST(connect_to_server_test);
    CPPUNIT_TEST(parse_input_test);
    CPPUNIT_TEST(verify_layer_spec_test);
    CPPUNIT_TEST(get_var_info_test);
    CPPUNIT_TEST(build_constraint_test);
    CPPUNIT_TEST(gdal_open_test);
    CPPUNIT_TEST(get_geo_info_test);
    CPPUNIT_TEST(get_projection_ref_test);

    CPPUNIT_TEST_SUITE_END();

    void connect_to_server_test() {
	poDS->d_oURL = "http://localhost/dods-test/nph-dods/data/nc/fnoc1.nc";
	try {
	    AISConnect *poAISConn = poDS->connect_to_server();
	    CPPUNIT_ASSERT(poAISConn);
	}
	catch (Error &e) { 
	    cerr << "Error: " << e.get_error_message() << endl;
	    CPPUNIT_ASSERT(!"This should not throw and error");
	}

	poDS->d_oURL = "ftp://localhost/dods-test/nph-dods/data/nc/fnoc1.nc";
	try {
	    AISConnect *poAISConn = poDS->connect_to_server();
	    CPPUNIT_ASSERT(!poAISConn);
	}
	catch (Error &e) { 
	    DBG(cerr << e.get_error_message() << endl);
	    CPPUNIT_ASSERT("Threw  error");
	}

	poDS->d_oURL = "http://localhost/index.html";
	try {
	    AISConnect *poAISConn = poDS->connect_to_server();
	    CPPUNIT_ASSERT(!poAISConn);
	}
	catch (Error &e) { 
	    DBG(cerr << e.get_error_message() << endl);
	    CPPUNIT_ASSERT("Threw  error");
	}
    }

    void parse_input_test() {
	poDS->parse_input(url_1);
	CPPUNIT_ASSERT(poDS->d_oURL == "http://localhost/dods-test/nph-dods/data/nc/fnoc1.nc");
	CPPUNIT_ASSERT(poDS->d_oVarName == "u");
	CPPUNIT_ASSERT(poDS->d_oBandExpr == "[0][lat][lon]");

	poDS->parse_input(string("http://localhost/fnoc1.nc?bogus[lat][lon]"));
	CPPUNIT_ASSERT(poDS->d_oURL == "http://localhost/fnoc1.nc");
	CPPUNIT_ASSERT(poDS->d_oVarName == "bogus");
	CPPUNIT_ASSERT(poDS->d_oBandExpr == "[lat][lon]");

	poDS->parse_input(string("http://localhost/fnoc1.nc?bogus[0][10:20][lat][lon]"));
	CPPUNIT_ASSERT(poDS->d_oURL == "http://localhost/fnoc1.nc");
	CPPUNIT_ASSERT(poDS->d_oVarName == "bogus");
	CPPUNIT_ASSERT(poDS->d_oBandExpr == "[0][10:20][lat][lon]");

	// Try invalid input. All should throw Error.
	try {
	    poDS->parse_input(string("http://localhost/nph-dods/fnoc1.ncu[0]"));
	    CPPUNIT_ASSERT(!"Failed to throw Error!");
	}
	catch (Error &e) { CPPUNIT_ASSERT("Threw Error"); }

	try {
	    poDS->parse_input(string("http://localhost/nph-dods/fnoc1.nc?u"));
	    CPPUNIT_ASSERT(!"Failed to throw Error!");
	}
	catch (Error &e) { CPPUNIT_ASSERT("Threw Error"); }

	try {
	    poDS->parse_input(string("http://localhost/nph-dods/fnoc1.nc?[0]"));
	    CPPUNIT_ASSERT(!"Failed to throw Error!");
	}
	catch (Error &e) { CPPUNIT_ASSERT("Threw Error"); }

	try {
	    poDS->parse_input(string("?u[0]"));
	    CPPUNIT_ASSERT(!"Failed to throw Error!");
	}
	catch (Error &e) { CPPUNIT_ASSERT("Threw Error"); }
    }

    void verify_layer_spec_test() {
	DODSDataset::dim_spec ds;
	ds.type = DODSDataset::dim_spec::lat;
	poDS->d_oBandSpec.push_back(ds);
	ds.type = DODSDataset::dim_spec::lon;
	poDS->d_oBandSpec.push_back(ds);
	ds.type = DODSDataset::dim_spec::index; ds.start = 0;
	poDS->d_oBandSpec.push_back(ds);
	poDS->d_iVarRank = 3;
	try {
	    poDS->verify_layer_spec();
	    CPPUNIT_ASSERT("Worked!");
	}
	catch (Error &e) {CPPUNIT_ASSERT(!"Should not throw Error"); }

	// Missing dimension
	poDS->d_iVarRank = 4;
	try {
	    poDS->verify_layer_spec();
	    CPPUNIT_ASSERT(!"Should have thrown Error!");
	}
	catch (Error &e) {
	    DBG(cerr << "Message: " << e.get_error_message() << endl);
	    CPPUNIT_ASSERT("Should throw Error");
	}

	// Add a fourth dim spec; should work
	ds.type = DODSDataset::dim_spec::range; ds.start = 0; ds.stop = 7;
	poDS->d_oBandSpec.push_back(ds);
	try {
	    poDS->verify_layer_spec();
	    CPPUNIT_ASSERT("Worked!");
	}
	catch (Error &e) {CPPUNIT_ASSERT(!"Should not throw Error"); }

	// Only one range spec allowed; should fail
	ds.type = DODSDataset::dim_spec::range; ds.start = 0; ds.stop = 8;
	poDS->d_iVarRank = 5;
	poDS->d_oBandSpec.push_back(ds);
	try {
	    poDS->verify_layer_spec();
	    CPPUNIT_ASSERT(!"Should have failed!");
	}
	catch (Error &e) {
	    DBG(cerr << "Message: " << e.get_error_message() << endl);
	    CPPUNIT_ASSERT("Should throw Error");
	}

	// We are allowed any number of index specs; should work
	ds.type = DODSDataset::dim_spec::index; ds.start = 1; 
	poDS->d_oBandSpec[4]= ds;
	try {
	    poDS->verify_layer_spec();
	    CPPUNIT_ASSERT("Should work!");
	}
	catch (Error &e) {CPPUNIT_ASSERT(!"Should not throw Error"); }
    }

    void get_var_info_test() {
	AISConnect ac("http://localhost/dods-test/nph-dods/data/nc/fnoc1.nc");
	DAS das;
	DDS dds;
	ac.request_das(das);
	ac.request_dds(dds);
	poDS->d_oVarName = "u";
	DODSDataset::dim_spec ds;
	ds.type = DODSDataset::dim_spec::range; ds.start = 2; ds.stop = 8;
	poDS->d_oBandSpec.push_back(ds);
	ds.type = DODSDataset::dim_spec::lat;
	poDS->d_oBandSpec.push_back(ds);
	ds.type = DODSDataset::dim_spec::lon;
	poDS->d_oBandSpec.push_back(ds);
	poDS->d_iVarRank = 3;

	try {
	    poDS->get_var_info(das, dds);
	    CPPUNIT_ASSERT(poDS->d_iVarRank == 3);
	    CPPUNIT_ASSERT(poDS->d_iNumBands == 7);
	    CPPUNIT_ASSERT(poDS->nRasterYSize == 17);
	    CPPUNIT_ASSERT(poDS->nRasterXSize == 21);
	    CPPUNIT_ASSERT(poDS->d_eDatatype == GDT_Int16);
	}
	catch (Error &e) {
	    cerr << "Error: " << e.get_error_message() << endl;
	    CPPUNIT_ASSERT(!"Should not throw Error");
	}
    }

    void gdal_open_test() {
	poDataset = static_cast<DODSDataset *>(GDALOpen(url_1.c_str(), 
							GA_ReadOnly));

	CPPUNIT_ASSERT(poDataset->GetVarName() == "u");
	CPPUNIT_ASSERT(poDataset->GetNumBands() == 1);
	CPPUNIT_ASSERT(poDataset->GetDatatype() == GDT_Int16);

	poDataset = static_cast<DODSDataset *>(GDALOpen(url_2.c_str(), 
							GA_ReadOnly));

	CPPUNIT_ASSERT(poDataset->GetVarName() == "v");
	CPPUNIT_ASSERT(poDataset->GetNumBands() == 16);
	CPPUNIT_ASSERT(poDataset->GetDatatype() == GDT_Int16);
    }

    void build_constraint_test() {
	poDS->d_oVarName = "u";
	poDS->nRasterXSize = 21;
	poDS->nRasterYSize = 17;
	DODSDataset::dim_spec ds;
	ds.type = DODSDataset::dim_spec::range; ds.start = 2; ds.stop = 8;
	poDS->d_oBandSpec.push_back(ds);
	ds.type = DODSDataset::dim_spec::lat;
	poDS->d_oBandSpec.push_back(ds);
	ds.type = DODSDataset::dim_spec::lon;
	poDS->d_oBandSpec.push_back(ds);
	poDS->d_iVarRank = 3;

	string c1 = poDS->BuildConstraint(0, 0, 10, 10, 0);
	DBG(cerr << "C1: " << c1 <<endl);
	CPPUNIT_ASSERT(c1 == "u[2][0:9][0:9]");

	string c2 = poDS->BuildConstraint(4, 5, 4, 6, 3);
	DBG(cerr << "C2: " << c2 <<endl);
	CPPUNIT_ASSERT(c2 == "u[5][5:10][4:7]");

	string c3 = poDS->BuildConstraint(3);
	DBG(cerr << "C3: " << c3 <<endl);
	CPPUNIT_ASSERT(c3 == "u[5][0:16][0:20]");
    }

    void get_geo_info_test() {
	try {
	    cerr << endl 
		 << "These tests can take several minutes. They will fail without a network connection or if either the GSO or GSFC servers off line." << endl;

	    AISConnect ac("http://dods.gso.uri.edu/cgi-bin/nph-dods/DODS/pathfdr/1986/12/k86336175759.hdf", "example_conf.d/ais_database.xml");
	    DAS das;
	    DDS dds;
	    ac.request_das(das);
	    ac.request_dds(dds);
	    poDS->d_oVarName = "dsp_band_1";

	    poDS->get_geo_info(das, dds);
	    // sp_nav_earth_toplat: 71.1722
	    // dsp_nav_earth_botlat: 4.8278
	    // dsp_nav_earth_leflon: -112.11
	    // dsp_nav_earth_ritlon: -27.8897
	    CPPUNIT_ASSERT(poDS->d_dfLLLat == 4.8278);
	    CPPUNIT_ASSERT(poDS->d_dfLLLon == -112.11);
	    CPPUNIT_ASSERT(poDS->d_dfURLat == 71.1722);
	    CPPUNIT_ASSERT(poDS->d_dfURLon == -27.8897);

	    // MODIS level 3...
	    AISConnect ac2("http://g0dug03u.ecs.nasa.gov/daac-bin/dods/nph-dods/datapool/MOAA/MYD08_E3.003/2003.07.28/MYD08_E3.A2003209.003.2003234054010.hdf", "example_conf.d/ais_database.xml");
	    DAS das2;
	    DDS dds2;
	    ac2.request_das(das2);
	    ac2.request_dds(dds2);
	    poDS->d_oURL = "http://g0dug03u.ecs.nasa.gov/daac-bin/dods/nph-dods/datapool/MOAA/MYD08_E3.003/2003.07.28/MYD08_E3.A2003209.003.2003234054010.hdf";
	    poDS->d_oVarName = "mod08.Data%20Fields.Scattering_Angle_Mean";
	    poDS->get_geo_info(das2, dds2);

	    CPPUNIT_ASSERT(poDS->d_dfLLLat == -89.5);
	    CPPUNIT_ASSERT(poDS->d_dfLLLon == -179.5);
	    CPPUNIT_ASSERT(poDS->d_dfURLat == 89.5);
	    CPPUNIT_ASSERT(poDS->d_dfURLon == 179.5);
	}
	catch (Error &e) {
	    cerr << "Message: " << e.get_error_message() << endl;
	    CPPUNIT_ASSERT(!"Should not throw Error");
	}
    }

    void get_projection_ref_test() {
	try {
	    AISConnect ac("http://dods.gso.uri.edu/cgi-bin/nph-dods/DODS/pathfdr/1986/12/k86336175759.hdf", "example_conf.d/ais_database.xml");
	    DAS das;
	    DDS dds;
	    ac.request_das(das);
	    ac.request_dds(dds);
	    poDS->d_oVarName = "dsp_band_1";

	    poDS->get_geo_info(das, dds);

	    cerr << endl << "The projection string is: " << endl
		 << poDS->GetProjectionRef() << endl;
	}
	catch (Error &e) {
	    cerr << "Message: " << e.get_error_message() << endl;
	    CPPUNIT_ASSERT(!"Should not throw, check AIS stuff.");
	}
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DODSDatasetTest);

int 
main( int argc, char* argv[] )
{
    CppUnit::TextTestRunner runner;

    runner.addTest( CppUnit::TestFactoryRegistry::getRegistry().makeTest() );

    runner.run();

    return 0;
}

// $Log$
// Revision 1.1  2003/12/12 23:28:17  jimg
// Added.
//
// Revision 1.1  2003/12/12 22:52:20  jimg
// Added.
//
