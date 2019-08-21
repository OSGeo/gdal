/*****************************************************************************
* Copyright 2016 Pitney Bowes Inc.
* 
* Licensed under the MIT License (the “License”); you may not use this file 
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT 

* Unless required by applicable law or agreed to in writing, software 
* distributed under the License is distributed on an “AS IS” WITHOUT 
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and 
* limitations under the License.
*****************************************************************************/

#include "APIDef.h"
#include <memory>

class MRRDataset;

class MRRRasterBand : public GDALPamRasterBand
{
private:
	friend class					MRRRasterDataset;
	MIR_FieldType					nFieldType;							//Field type this band belongs to
	uint32_t						nEvent;								//zero based EventIndex of MRR this band belongs to.
	uint32_t						nField;								//zero based FieldIndex of MRR this band belongs to.
	uint32_t						nMRRBandIndex;						//zero based bandIndex (as per MRR SDK).
	uint32_t						nResolution;						//zero based level index.
	MIR_DataType					nMIRDataType;						//GDAL compatible data type
	uint32_t						nSizeInBytes;
	SMIR_Statistics*				pStatistics;
	bool							bIteratorInitialized;				//flag to indicate whether iterator has been initialized
	uint32_t						nIteratorHandle;
	int								nOverviewLevel;
	std::vector<std::unique_ptr<MRRRasterBand>> vOverviewBands;		//store overviews.
	uint32_t						nXBlocksCount;
	uint32_t						nYBlocksCount;

	//Private Methods
	const uint32_t&					MRRBandIndex() { return nMRRBandIndex; }
	unsigned int&					GetIterator() { return nIteratorHandle; }
	bool 							BeginIterator();
	bool							ReleaseIterator();
	bool							StatisticsEnsureInitialized(bool bSummary, int bApproxOk, bool bCompute = true, int nBins = 0);
	const SMIR_Statistics*			GetStats() const { return pStatistics; }
	void							ReleaseStats();

	virtual CPLErr					IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int, 
										GDALDataType, GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg* psExtraArg);

	virtual CPLErr					IReadBlock(int, int, void *);

public:

	MRRRasterBand (MRRDataset *, const MIR_FieldType &, const int& nFieldIndex, const int& nBandIndex, const int& nOverview,
		const MIR_DataType& nMIRDataType, const GDALDataType& nGDALDataType, const int&, const int&, const uint32_t&, const uint32_t&);
	~MRRRasterBand ();

	//Statistics related method
	virtual double					GetMinimum(int *pbSuccess = NULL);
	virtual double					GetMaximum(int *pbSuccess = NULL);
	virtual CPLErr					ComputeRasterMinMax(int, double*);
	virtual CPLErr					GetStatistics(int bApproxOK, int bForce, double *pdfMin, double *pdfMax, double *pdfMean, double *padfStdDev);
	virtual CPLErr					ComputeStatistics(int bApproxOK, double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev, GDALProgressFunc, void *pProgressData);
	virtual CPLErr					GetDefaultHistogram(double *pdfMin, double *pdfMax, int *pnBuckets, GUIntBig ** ppanHistogram, int bForce, GDALProgressFunc, void *pProgressData);

	//Color table related methods
	GDALColorInterp					GetColorInterpretation();
	GDALColorTable*					GetColorTable();
	virtual char**					GetCategoryNames();

	//Overview related method
	virtual int						HasArbitraryOverviews() { return vOverviewBands.size() != 0; }
	virtual int						GetOverviewCount() { return (int)vOverviewBands.size(); }
	virtual GDALRasterBand*			GetOverview(int);
};
