
// -*- C++ -*-

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
// Authors:
//          James Gallagher <jgallagher@opendap.org>
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

// A Sample program showing how to open a DODS dataset using the DODS-GDAL
// driver. Assume that the DODS URL, fully constrained, is passed in via the
// first parameter to this program. 6/3/2002 jhrg

#include <iostream>

#include <gdal_priv.h>

using namespace std;

int
main(int argc, char *argv[])
{
    GDALDataset *poDataset;

    // Register all the GDAL drivers. If you only want one driver registered,
    // look at gdalallregister.cpp. 
    GDALAllRegister();

    cerr << "Opening the dataset." << endl;

    poDataset = static_cast<GDALDataset *>(GDALOpen(argv[1], GA_ReadOnly));
    if(poDataset == NULL) {
	cerr << "Could not read the DODS dataset: " << argv[1] << endl;
	exit(1);
    }

    // Now that we have the DODS dataset open, lets read the rasterband data. 
    for (int i = 0; i < poDataset->GetRasterCount(); i++) {
	cerr << "Band Number: " << i+1 << endl;

	GDALRasterBand *poBand = poDataset->GetRasterBand(i+1); 

	// Get the block size. Data is read in unit of nBlockXSize by
	// nBlockYSize. It's not the size of the rasterband itself. For that,
	// read on...
	int nBlockXSize, nBlockYSize;
	poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
	cerr << "Block = " << nBlockXSize << "x" << nBlockYSize << endl;
	cerr << "Type = " << GDALGetDataTypeName(poBand->GetRasterDataType())
	     << endl;

	// Note that we don't put any information in the color interpretation
	// fields of GDAL

	// Compute the min and max values of this rasterband.
	int bGotMin, bGotMax;
	double adfMinMax[2];
	adfMinMax[0] = poBand->GetMinimum(&bGotMin);
	adfMinMax[1] = poBand->GetMaximum(&bGotMax);
	if(!(bGotMin && bGotMax))
	    GDALComputeRasterMinMax((GDALRasterBandH)poBand, TRUE, adfMinMax);

	cerr << "Min = " << adfMinMax[0] << ", Max = " << adfMinMax[1] << endl;
        
	// We don't support Overview or Colortables

	// Here's how to read data values, note that this call converts that
	// values into Float32s, and stores them in an array of floats, but you
	// can use the API to get the datatype constant for the RasterBand's
	// underlying type and use that instead.
	int nXSize = poBand->GetXSize();
	int nYSize = poBand->GetYSize();

	float *pafData = (float *) CPLMalloc(sizeof(float)*nXSize*nYSize);
	poBand->RasterIO(GF_Read, 0, 0, nXSize, nYSize, 
			 pafData, nXSize, nYSize, GDT_Float32, 
			 0, 0);

	for (int y = 0; y < nYSize; ++y)
	    for (int x = 0; x < nXSize; ++x)
		cout << *(pafData + (y * nYSize) + x) << " ";
	cout << endl;
    }

    delete poDataset;
}

// $Log$
// Revision 1.1  2003/12/12 23:28:17  jimg
// Added.
//
// Revision 1.1  2003/12/12 22:52:20  jimg
// Added.
//
