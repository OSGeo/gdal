/* remove unwanted name duplication */
%rename (X) GCPX;
%rename (Y) GCPY;
%rename (Z) GCPZ;
%rename (Column) GCPPixel;
%rename (Row) GCPLine;

/* Make room for Perl interface */

%rename (_GetDataTypeSize) GDALGetDataTypeSize;
%rename (_DataTypeIsComplex) GDALDataTypeIsComplex;

%rename (_GetDriver) GetDriver;

%rename (_Open) Open;
%newobject _Open;

%rename (_OpenShared) OpenShared;
%newobject _OpenShared;

/* those that need callback_data check: */

%rename (_ComputeMedianCutPCT) ComputeMedianCutPCT;
%rename (_DitherRGB2PCT) DitherRGB2PCT;
%rename (_ReprojectImage) ReprojectImage;
%rename (_ComputeProximity) ComputeProximity;
%rename (_RasterizeLayer) RasterizeLayer;
%rename (_Polygonize) Polygonize;
%rename (_SieveFilter) SieveFilter;
%rename (_RegenerateOverviews) RegenerateOverviews;
%rename (_RegenerateOverview) RegenerateOverview;

%rename (_AutoCreateWarpedVRT) AutoCreateWarpedVRT;
%newobject _AutoCreateWarpedVRT;

%rename (_Create) Create;
%newobject _Create;

%rename (_GetRasterBand) GetRasterBand;
%rename (_AddBand) AddBand;

%rename (_GetPaletteInterpretation) GetPaletteInterpretation;
%rename (_GetHistogram) GetHistogram;

%rename (_SetColorEntry) SetColorEntry;

%rename (_GetUsageOfCol) GetUsageOfCol;
%rename (_GetColOfUsage) GetColOfUsage;
%rename (_GetTypeOfCol) GetTypeOfCol;
%rename (_CreateColumn) CreateColumn;

%rename (Stat) VSIStatL;
