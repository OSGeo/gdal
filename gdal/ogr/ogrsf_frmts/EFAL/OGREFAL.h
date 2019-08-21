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

#ifndef OGR_EFAL_H_INCLUDED
#define OGR_EFAL_H_INCLUDED

#include "ogrsf_frmts.h"
#define EFAL_IN_GDAL

#ifdef ABS
#undef ABS
#endif

#include "MIDefs.h"
#include "EFALAPI.h"
#include "EFAL.h"
#include "EFALLIB.h"

class OGREFALDataSource;

enum EfalOpenMode
{
	EFAL_READ_ONLY, // open for read-only - edit operations are refused
	EFAL_LOCK_READ, // open for read-only with files locked open (BeginReadAccess)
	EFAL_READ_WRITE, // open for read and write - no BeginAccess but edits are allowed
	EFAL_LOCK_WRITE, // open for read and write with the files locked for writing (BeginWriteAccess)
};

extern EFALLIB * efallib;

/************************************************************************/
/*                             OGREFALLayer                              */
/************************************************************************/

class OGREFALLayer : public OGRLayer
{
	EFALHANDLE          hSession;
	EFALHANDLE          hTable;
	EFALHANDLE          hSequentialCursor;
	OGRFeatureDefn     *poFeatureDefn;
	char*               pszTableCSys;
	bool                bHasFieldNames;
	EfalOpenMode        efalOpenMode;
	bool                bNew;
	bool                bNeedEndAccess;
	bool                bCreateNativeX;
	int                 nBlockSize;
	Ellis::MICHARSET    charset;
	bool                bHasBounds;
	double              xmin;
	double              ymin;
	double              xmax;
	double              ymax;
	bool                bInWriteMode;
	char*               pszFilename;
	GIntBig             nLastFID;
	bool                bHasMap;
	OGRSpatialReference* pSpatialReference;

private:
	void                 BuildQuery(wchar_t * szQuery, size_t sz, bool count) const;
	void                 CloseSequentialCursor();
	OGRFeature*          Cursor2Feature(EFALHANDLE hCursor, OGRFeatureDefn* pFeatureDefn);
	int                  CursorIndex2FeatureIndex(EFALHANDLE hCursor, OGRFeatureDefn* pFeatureDefn, unsigned long idxCursor) const;
	CPLString            MapBasicStyle2OGRStyle(const wchar_t * mbStyle) const;
	char*                OGRStyle2MapBasicStyle(const char * ogrStyle) const;
	OGRGeometry*         EFALGeometry2OGRGeometry(GByte* bytes, size_t sz);
	void                 OGRGeometry2EFALGeometry(OGRGeometry*, GByte** pbytes, size_t* psz) const;
	OGRSpatialReference* EFALCSys2OGRSpatialRef(const wchar_t* szCoordSys);
	const wchar_t*       OGRSpatialRef2EFALCSys(const OGRSpatialReference* poSpatialRef);
	bool ExtractBoundsFromCSysString(const char * pszCoordSys, double &dXMin, double &dYMin, double &dXMax, double &dYMax);
	int                  GetTABType(OGRFieldDefn *poField, Ellis::ALLTYPE_TYPE* peTABType, int *pnWidth, int *pnPrecision);
	OGRErr               CreateNewTable();

public:
	OGREFALLayer(EFALHANDLE hSession, EFALHANDLE hTable, EfalOpenMode eEfalOpenMode);
	OGREFALLayer(EFALHANDLE hSession, const char *pszName, const char *pszFilename, bool bNativeX, int BlockSize, Ellis::MICHARSET eCharset);
	virtual ~OGREFALLayer();

	const char*          GetFilename() const { return pszFilename; }
	void                 SetSpatialRef(OGRSpatialReference *poSpatialRef);
	bool                 IsNew() const { return bNew; }
	void                 SetBounds(double xmin, double ymin, double xmax, double ymax);
	bool                 IsBoundsSet() const { return bHasBounds; }
	/* ************************************************************************************
	* OGR interface
	* ************************************************************************************ */
	/* ----------------------------------------------------
	*  Metadata and Descriptive Methods
	* ---------------------------------------------------- */
	int                  TestCapability(const char *) override;
	virtual OGRFeatureDefn*      GetLayerDefn() override { return poFeatureDefn; }
	virtual OGRSpatialReference* GetSpatialRef() override { return bHasMap ? pSpatialReference : nullptr; }
	virtual OGRwkbGeometryType   GetGeomType() override { return poFeatureDefn->GetGeomType(); }
	virtual OGRErr       GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
	virtual OGRErr       GetExtent(int /*iGeomField*/, OGREnvelope *psExtent, int bForce) override
	{
		return GetExtent(psExtent, bForce);
	}
	virtual const char*          GetFIDColumn() override { return ""; } // TODO: Not a base class method???
	virtual const char*          GetGeometryColumn() override { return bHasMap ? "OBJ" : ""; }
	//
	//
	//virtual char **     GetMetadata(const char *pszDomain = NULL) override;
	//virtual const char *GetMetadataItem(const char * pszName, const char * pszDomain = "") override;
	//virtual char **     GetMetadataDomainList() override;
	//virtual CPLErr      SetMetadata(char ** papszMetadata,const char * pszDomain = "") override;
	//virtual CPLErr      SetMetadataItem(const char * pszName, const char * pszValue, const char * pszDomain = "") override;
	//
	//Find the index of field in the layer.
	//virtual int 	FindFieldIndex(const char *pszFieldName, int bExactMatch)

	/* ----------------------------------------------------
	*  Query / Feature Access Methods
	* ---------------------------------------------------- */
	virtual GIntBig      GetFeatureCount(int bForce = TRUE) override;
	virtual OGRFeature*  GetFeature(GIntBig nFID) override;
	void                 ResetReading() override;
	OGRFeature *         GetNextFeature() override;
	// virtual OGRErr       SetIgnoredFields(const char **papszFields) override; //Set which fields can be omitted when retrieving features from the layer. - does not appear to be implemented by many data providers, probably not worth the effort for TAB

	// For these we will just use the base class and access the protected member variables directly...
	//virtual void         SetSpatialFilter(OGRGeometry * geomFilter) override;
	//virtual void         SetSpatialFilterRect(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY) override;
	//
	//virtual void         SetSpatialFilter(int iGeomField, OGRGeometry * geomFilter) override 
	//{ 
	//	return SetSpatialFilter(geomFilter); 
	//}
	//
	//virtual void         SetSpatialFilterRect(int iGeomField, double dfMinX, double dfMinY, double dfMaxX, double dfMaxY) override
	//{
	//	return SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
	//}
	//
	//virtual OGRErr       SetAttributeFilter(const char *pszQuery) override;

	 /* ----------------------------------------------------
	 * Feature Editing (Insert, Update, Delete) Methods
	 * ---------------------------------------------------- */
	 // Insert, Update, and Delete
	virtual OGRErr      ISetFeature(OGRFeature *poFeature) override;
	virtual OGRErr      ICreateFeature(OGRFeature *poFeature) override;
	virtual OGRErr      DeleteFeature(GIntBig nFID) override;
	//
	//Flush pending changes to disk.
	//virtual OGRErr SyncToDisk()
	//
	//For datasources which support transactions, StartTransaction creates a transaction.
	//virtual OGRErr StartTransaction() CPL_WARN_UNUSED_RESULT
	//
	//For datasources which support transactions, CommitTransaction commits a transaction.
	//virtual OGRErr CommitTransaction() CPL_WARN_UNUSED_RESULT
	//
	//For datasources which support transactions, RollbackTransaction will roll back a datasource to its state before the start of the current transaction.
	//virtual OGRErr RollbackTransaction()

	 /* ----------------------------------------------------
	 * Create / Alter Table Methods
	 * ---------------------------------------------------- */
	 //
	 //Create a new field on a layer.
	virtual OGRErr CreateField(OGRFieldDefn *poField, int bApproxOK = TRUE) override;
	//
	//Create a new geometry field on a layer.
	//virtual OGRErr CreateGeomField(OGRGeomFieldDefn *poField, int bApproxOK = TRUE)
	//
	//Delete an existing field on a layer.
	//virtual OGRErr DeleteField(int iField)
	//
	//Reorder all the fields of a layer.
	//virtual OGRErr ReorderFields(int *panMap)
	//
	//Alter the definition of an existing field on a layer.
	//virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn, int nFlagsIn)
	//
	//Returns layer style table.
	//virtual OGRStyleTable * GetStyleTable()
	//
	//Set layer style table.
	//virtual void SetStyleTableDirectly(OGRStyleTable *poStyleTable)
	//
	//Set layer style table.
	//virtual void SetStyleTable(OGRStyleTable *poStyleTable)
	//

	/* ----------------------------------------------------
	 * GeoProcessing Methods
	 * ---------------------------------------------------- */
	 //
	 //Intersection of two layers.
	 //OGRErr Intersection(OGRLayer *pLayerMethod, OGRLayer *pLayerResult, char **papszOptions = nullptr, GDALProgressFunc pfnProgress = nullptr, void *pProgressArg = nullptr)
	 //
	 //Union of two layers.
	 //OGRErr Union(OGRLayer *pLayerMethod, OGRLayer *pLayerResult, char **papszOptions = nullptr, GDALProgressFunc pfnProgress = nullptr, void *pProgressArg = nullptr)
	 //
	 //Symmetrical difference of two layers.
	 //OGRErr SymDifference(OGRLayer *pLayerMethod, OGRLayer *pLayerResult, char **papszOptions, GDALProgressFunc pfnProgress, void *pProgressArg)
	 //
	 //Identify the features of this layer with the ones from the identity layer.
	 //OGRErr Identity(OGRLayer *pLayerMethod, OGRLayer *pLayerResult, char **papszOptions = nullptr, GDALProgressFunc pfnProgress = nullptr, void *pProgressArg = nullptr)
	 //
	 //Update this layer with features from the update layer.
	 //OGRErr Update(OGRLayer *pLayerMethod, OGRLayer *pLayerResult, char **papszOptions = nullptr, GDALProgressFunc pfnProgress = nullptr, void *pProgressArg = nullptr)
	 //
	 //Clip off areas that are not covered by the method layer.
	 //OGRErr Clip(OGRLayer *pLayerMethod, OGRLayer *pLayerResult, char **papszOptions = nullptr, GDALProgressFunc pfnProgress = nullptr, void *pProgressArg = nullptr)
	 //
	 //Remove areas that are covered by the method layer.
	 //OGRErr Erase(OGRLayer *pLayerMethod, OGRLayer *pLayerResult, char **papszOptions = nullptr, GDALProgressFunc pfnProgress = nullptr, void *pProgressArg = nullptr)
};

/************************************************************************/
/*                           OGREFALDataSource                           */
/************************************************************************/
/*
 * From the GDAL documentation: ... the same ​GDALDataset object should not be accessed by
 *     several threads at the same time. But of course, it is fine to use 2 different
 *     handles pointing to the same file in 2 threads
 *
 * This corresponds well to an EFAL Session which is not multithread safe
 * but allows the same table to be open in multiple sessions for multiple threads.
*/
class OGREFALDataSource : public GDALDataset
{
	char                *pszName;
	char                *pszDirectory;

	OGRLayer          **papoLayers;
	int                 nLayers;

	int                 bUpdate;
	EfalOpenMode        efalOpenMode;
	int                 bSingleFile;
	int                 bSingleLayerAlreadyCreated;
	bool                bCreateNativeX;
	Ellis::MICHARSET    charset;
	int                 nBlockSize;

public:
	OGREFALDataSource();
	virtual ~OGREFALDataSource();

	int                 Open(GDALOpenInfo* poOpenInfo, int bTestOpen);

	int                 GetLayerCount() override { return (bSingleFile && !bSingleLayerAlreadyCreated) ? 0 : nLayers; }
	OGRLayer            *GetLayer(int) override;

	virtual OGRLayer   *ICreateLayer(const char *pszName,
		OGRSpatialReference *poSpatialRef = NULL,
		OGRwkbGeometryType eGType = wkbUnknown,
		char ** papszOptions = NULL) override;

	int                 TestCapability(const char *) override;

	int                 Create(const char *pszFileName, char ** papszOptions);
	char                **GetFileList() override;
	static CPLString    GetRealExtension(CPLString osFilename);
};

#endif /* ndef OGR_EFAL_H_INCLUDED */
