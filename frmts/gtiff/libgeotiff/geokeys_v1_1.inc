/* GeoTIFF GeoKey Database for OGC GeoTIFF v1.1 */

/* C database for Geotiff include files.   */
/* the macro ValuePair() must be defined   */
/* by the enclosing include file           */

/* GeoTIFF Configuration Keys */

#ifndef ONLY_GEOTIFF_V1_1_CHANGES
ValuePair(  GTModelTypeGeoKey,	1024)
ValuePair(  GTRasterTypeGeoKey,	1025)
ValuePair(  GTCitationGeoKey,	1026)
#endif

/* Geodetic CRS Parameter Keys */

ValuePair(  GeodeticCRSGeoKey,	2048)
ValuePair(  GeodeticCitationGeoKey,	2049)
ValuePair(  GeodeticDatumGeoKey,	2050)
ValuePair(  PrimeMeridianGeoKey,	2051)
#ifndef ONLY_GEOTIFF_V1_1_CHANGES
ValuePair(  GeogLinearUnitsGeoKey,	2052)
ValuePair(  GeogLinearUnitSizeGeoKey,	2053)
ValuePair(  GeogAngularUnitsGeoKey,	2054)
ValuePair(  GeogAngularUnitSizeGeoKey,	2055)
#endif
ValuePair(  EllipsoidGeoKey,	2056)
ValuePair(  EllipsoidSemiMajorAxisGeoKey,	2057)
ValuePair(  EllipsoidSemiMinorAxisGeoKey,	2058)
ValuePair(  EllipsoidInvFlatteningGeoKey,	2059)
#ifndef ONLY_GEOTIFF_V1_1_CHANGES
ValuePair(  GeogAzimuthUnitsGeoKey,	2060)
#endif
ValuePair(  PrimeMeridianLongitudeGeoKey,	2061)

#ifndef ONLY_GEOTIFF_V1_1_CHANGES
ValuePair(  GeogTOWGS84GeoKey,          2062) /* 2011 - proposed addition  */
#endif

/* Projected CRS Parameter Keys */

ValuePair(  ProjectedCRSGeoKey,	3072)
ValuePair(  ProjectedCitationGeoKey,	3073)
#ifndef ONLY_GEOTIFF_V1_1_CHANGES
ValuePair(  ProjectionGeoKey,	3074)
#endif
ValuePair(  ProjMethodGeoKey,	3075)
#ifndef ONLY_GEOTIFF_V1_1_CHANGES
ValuePair(  ProjLinearUnitsGeoKey,	3076)
ValuePair(  ProjLinearUnitSizeGeoKey,	3077)
ValuePair(  ProjStdParallel1GeoKey,	3078)
ValuePair(  ProjStdParallelGeoKey,ProjStdParallel1GeoKey)   /* ** alias **     */
ValuePair(  ProjStdParallel2GeoKey,	3079)
ValuePair(  ProjNatOriginLongGeoKey,	3080)
ValuePair(  ProjOriginLongGeoKey,ProjNatOriginLongGeoKey)   /* ** alias **     */
ValuePair(  ProjNatOriginLatGeoKey,	3081)
ValuePair(  ProjOriginLatGeoKey,ProjNatOriginLatGeoKey)   /* ** alias **     */
ValuePair(  ProjFalseEastingGeoKey,	3082)
ValuePair(  ProjFalseNorthingGeoKey,	3083)
ValuePair(  ProjFalseOriginLongGeoKey,	3084)
ValuePair(  ProjFalseOriginLatGeoKey,	3085)
ValuePair(  ProjFalseOriginEastingGeoKey,	3086)
ValuePair(  ProjFalseOriginNorthingGeoKey,	3087)
ValuePair(  ProjCenterLongGeoKey,	3088)
ValuePair(  ProjCenterLatGeoKey,	3089)
ValuePair(  ProjCenterEastingGeoKey,	3090)
ValuePair(  ProjCenterNorthingGeoKey,	3091)
ValuePair(  ProjScaleAtNatOriginGeoKey,	3092)
ValuePair(  ProjScaleAtOriginGeoKey,ProjScaleAtNatOriginGeoKey)  /* ** alias **   */
ValuePair(  ProjScaleAtCenterGeoKey,	3093)
ValuePair(  ProjAzimuthAngleGeoKey,	3094)
ValuePair(  ProjStraightVertPoleLongGeoKey, 3095)
ValuePair(  ProjRectifiedGridAngleGeoKey, 3096)
#endif

/* Vertical CRS Parameter Keys */

ValuePair(  VerticalGeoKey,	4096)
#ifndef ONLY_GEOTIFF_V1_1_CHANGES
ValuePair(  VerticalCitationGeoKey,	4097)
ValuePair(  VerticalDatumGeoKey,	4098)
ValuePair(  VerticalUnitsGeoKey,	4099)
#endif

/* End of Data base */
