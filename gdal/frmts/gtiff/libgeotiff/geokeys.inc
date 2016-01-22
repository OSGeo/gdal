/* GeoTIFF GeoKey Database */

/* Note: Any changes/additions to this database require */
/* a change in the revision value in geokeys.h          */

/* C database for Geotiff include files.   */
/* the macro ValuePair() must be defined   */
/* by the enclosing include file           */

/*  Revised 28 Sep 1995   NDR -- Added Rev. 1.0 aliases. */

/* 6.2.1 GeoTIFF Configuration Keys */

ValuePair(  GTModelTypeGeoKey,	1024) /* Section 6.3.1.1 Codes       */
ValuePair(  GTRasterTypeGeoKey,	1025) /* Section 6.3.1.2 Codes       */
ValuePair(  GTCitationGeoKey,	1026) /* documentation */

/* 6.2.2 Geographic CS Parameter Keys */

ValuePair(  GeographicTypeGeoKey,	2048) /* Section 6.3.2.1 Codes     */
ValuePair(  GeogCitationGeoKey,	2049) /* documentation             */
ValuePair(  GeogGeodeticDatumGeoKey,	2050) /* Section 6.3.2.2 Codes     */
ValuePair(  GeogPrimeMeridianGeoKey,	2051) /* Section 6.3.2.4 codes     */
ValuePair(  GeogLinearUnitsGeoKey,	2052) /* Section 6.3.1.3 Codes     */
ValuePair(  GeogLinearUnitSizeGeoKey,	2053) /* meters                    */
ValuePair(  GeogAngularUnitsGeoKey,	2054) /* Section 6.3.1.4 Codes     */
ValuePair(  GeogAngularUnitSizeGeoKey,	2055) /* radians                   */
ValuePair(  GeogEllipsoidGeoKey,	2056) /* Section 6.3.2.3 Codes     */
ValuePair(  GeogSemiMajorAxisGeoKey,	2057) /* GeogLinearUnits           */
ValuePair(  GeogSemiMinorAxisGeoKey,	2058) /* GeogLinearUnits           */
ValuePair(  GeogInvFlatteningGeoKey,	2059) /* ratio                     */
ValuePair(  GeogAzimuthUnitsGeoKey,	2060) /* Section 6.3.1.4 Codes     */
ValuePair(  GeogPrimeMeridianLongGeoKey,	2061) /* GeoAngularUnit            */
ValuePair(  GeogTOWGS84GeoKey,          2062) /* 2011 - proposed addition  */

/* 6.2.3 Projected CS Parameter Keys */
/*    Several keys have been renamed,*/
/*    and the deprecated names aliased for backward compatibility */

ValuePair(  ProjectedCSTypeGeoKey,	3072)     /* Section 6.3.3.1 codes   */
ValuePair(  PCSCitationGeoKey,	3073)     /* documentation           */
ValuePair(  ProjectionGeoKey,	3074)     /* Section 6.3.3.2 codes   */
ValuePair(  ProjCoordTransGeoKey,	3075)     /* Section 6.3.3.3 codes   */
ValuePair(  ProjLinearUnitsGeoKey,	3076)     /* Section 6.3.1.3 codes   */
ValuePair(  ProjLinearUnitSizeGeoKey,	3077)     /* meters                  */
ValuePair(  ProjStdParallel1GeoKey,	3078)     /* GeogAngularUnit */
ValuePair(  ProjStdParallelGeoKey,ProjStdParallel1GeoKey) /* ** alias **   */
ValuePair(  ProjStdParallel2GeoKey,	3079)     /* GeogAngularUnit */
ValuePair(  ProjNatOriginLongGeoKey,	3080)     /* GeogAngularUnit */
ValuePair(  ProjOriginLongGeoKey,ProjNatOriginLongGeoKey) /* ** alias **     */
ValuePair(  ProjNatOriginLatGeoKey,	3081)     /* GeogAngularUnit */
ValuePair(  ProjOriginLatGeoKey,ProjNatOriginLatGeoKey)   /* ** alias **     */
ValuePair(  ProjFalseEastingGeoKey,	3082)     /* ProjLinearUnits */
ValuePair(  ProjFalseNorthingGeoKey,	3083)     /* ProjLinearUnits */
ValuePair(  ProjFalseOriginLongGeoKey,	3084)     /* GeogAngularUnit */
ValuePair(  ProjFalseOriginLatGeoKey,	3085)     /* GeogAngularUnit */
ValuePair(  ProjFalseOriginEastingGeoKey,	3086)     /* ProjLinearUnits */
ValuePair(  ProjFalseOriginNorthingGeoKey,	3087)     /* ProjLinearUnits */
ValuePair(  ProjCenterLongGeoKey,	3088)     /* GeogAngularUnit */
ValuePair(  ProjCenterLatGeoKey,	3089)     /* GeogAngularUnit */
ValuePair(  ProjCenterEastingGeoKey,	3090)     /* ProjLinearUnits */
ValuePair(  ProjCenterNorthingGeoKey,	3091)     /* ProjLinearUnits */
ValuePair(  ProjScaleAtNatOriginGeoKey,	3092)     /* ratio   */
ValuePair(  ProjScaleAtOriginGeoKey,ProjScaleAtNatOriginGeoKey)  /* ** alias **   */
ValuePair(  ProjScaleAtCenterGeoKey,	3093)     /* ratio   */
ValuePair(  ProjAzimuthAngleGeoKey,	3094)     /* GeogAzimuthUnit */
ValuePair(  ProjStraightVertPoleLongGeoKey, 3095) /* GeogAngularUnit */
ValuePair(  ProjRectifiedGridAngleGeoKey, 3096)   /* GeogAngularUnit */

/* 6.2.4 Vertical CS Keys */
   
ValuePair(  VerticalCSTypeGeoKey,	4096)  /* Section 6.3.4.1 codes   */
ValuePair(  VerticalCitationGeoKey,	4097)  /* documentation */
ValuePair(  VerticalDatumGeoKey,	4098)  /* Section 6.3.4.2 codes   */
ValuePair(  VerticalUnitsGeoKey,	4099)  /* Section 6.3.1 (.x) codes   */

/* End of Data base */
