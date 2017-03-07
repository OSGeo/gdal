/* Perl specific Band methods, i.e., use Perl data structures directly in the C++ code */

%extend GDALRasterBandShadow {
  
    %apply (int nList, double* pList) {(int nFixedLevelCount, double *padfFixedLevels)};
    %apply (int defined, double value) {(int bUseNoData, double dfNoDataValue)};
    CPLErr ContourGenerate(double dfContourInterval, double dfContourBase,
                           int nFixedLevelCount, double *padfFixedLevels,
                           int bUseNoData, double dfNoDataValue,
                           OGRLayerShadow *hLayer, int iIDField, int iElevField,
                           GDALProgressFunc progress = NULL,
                           void* progress_data = NULL) {
        return GDALContourGenerate( self, dfContourInterval, dfContourBase,
                                    nFixedLevelCount, padfFixedLevels,
                                    bUseNoData, dfNoDataValue,
                                    hLayer, iIDField, iElevField,
                                    progress,
                                    progress_data );
    }
    %clear (int nFixedLevelCount, double *padfFixedLevels);
    %clear (int bUseNoData, double dfNoDataValue);

    SV *ClassCounts(GDALProgressFunc callback = NULL,
                    void* callback_data = NULL) {
        GDALDataType dt = GDALGetRasterDataType(self);
        if (!(dt == GDT_Byte || dt == GDT_UInt16 || dt == GDT_Int16 || dt == GDT_UInt32 || dt == GDT_Int32)) {
            do_confess("Data type of the raster band is not integer.", 1);
        }
        HV* hash = newHV();
        int XBlockSize, YBlockSize;
        GDALGetBlockSize( self, &XBlockSize, &YBlockSize );
        int XBlocks = (GDALGetRasterBandXSize(self) + XBlockSize - 1) / XBlockSize;
        int YBlocks = (GDALGetRasterBandYSize(self) + YBlockSize - 1) / YBlockSize;
        void *data = CPLMalloc(XBlockSize * YBlockSize * GDALGetDataTypeSizeBytes(dt));
        for (int yb = 0; yb < YBlocks; ++yb) {
            if (callback) {
                double p = (double)yb/(double)YBlocks;
                if (!callback(p, "", callback_data)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    hv_undef(hash);
                    hash = NULL;
                    break;
                }
            }
            for (int xb = 0; xb < XBlocks; ++xb) {
                int XValid, YValid;
                CPLErr e = GDALReadBlock(self, xb, yb, data);
                GDALGetActualBlockSize(self, xb, yb, &XValid, &YValid);
                for (int iY = 0; iY < YValid; ++iY) {
                    for (int iX = 0; iX < XValid; ++iX) {
                        int32_t k;
                        switch(dt) {
                        case GDT_Byte:
                          k = ((GByte*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_UInt16:
                          k = ((GUInt16*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_Int16:
                          k = ((GInt16*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_UInt32:
                          k = ((GUInt32*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_Int32:
                          k = ((GInt32*)(data))[iX + iY * XBlockSize];
                          break;
                        }
                        char key[12];
                        int klen = sprintf(key, "%i", k);
                        SV* sv;
                        SV** sv2 = hv_fetch(hash, key, klen, 0);
                        if (sv2 && SvOK(*sv2)) {
                            sv = *sv2;
                            sv_setiv(sv, SvIV(sv)+1);
                            SvREFCNT_inc(sv);
                        } else {
                            sv = newSViv(1);
                        }
                        if (!hv_store(hash, key, klen, sv, 0))
                            SvREFCNT_dec(sv);
                    }
                }
            }
        }
        CPLFree(data);
        if (hash)
            return newRV_noinc((SV*)hash);
        else
            return &PL_sv_undef;
    }

    void Reclassify(SV* hash_ref,
                    GDALProgressFunc callback = NULL,
                    void* callback_data = NULL) {
        GDALDataType dt = GDALGetRasterDataType(self);
        if (!(dt == GDT_Byte || dt == GDT_UInt16 || dt == GDT_Int16 || dt == GDT_UInt32 || dt == GDT_Int32)) {
            do_confess("Data type of the raster band is not integer.", 1);
        }
        if (!(SvROK(hash_ref) && (SvTYPE(SvRV(hash_ref)) == SVt_PVHV))) do_confess(NEED_REF, 1);
        HV* hash = (HV*)SvRV(hash_ref);
        bool has_default = false;
        int32_t def;
        SV** sv = hv_fetch(hash, "*", 1, 0);
        if (sv && SvOK(*sv)) {
            has_default = true;
            def = SvIV(*sv);
        }
        int has_no_data;
        double no_data = GDALGetRasterNoDataValue(self, &has_no_data);
        int XBlockSize, YBlockSize;
        GDALGetBlockSize( self, &XBlockSize, &YBlockSize );
        int XBlocks = (GDALGetRasterBandXSize(self) + XBlockSize - 1) / XBlockSize;
        int YBlocks = (GDALGetRasterBandYSize(self) + YBlockSize - 1) / YBlockSize;
        void *data = CPLMalloc(XBlockSize * YBlockSize * GDALGetDataTypeSizeBytes(dt));
        for (int yb = 0; yb < YBlocks; ++yb) {
            if (callback) {
                double p = (double)yb/(double)YBlocks;
                if (!callback(p, "", callback_data)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    break;
                }
            }
            for (int xb = 0; xb < XBlocks; ++xb) {
                int XValid, YValid;
                CPLErr e = GDALReadBlock(self, xb, yb, data);
                GDALGetActualBlockSize(self, xb, yb, &XValid, &YValid);
                for (int iY = 0; iY < YValid; ++iY) {
                    for (int iX = 0; iX < XValid; ++iX) {
                        int32_t k;
                        switch(dt) {
                        case GDT_Byte:
                          k = ((GByte*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_UInt16:
                          k = ((GUInt16*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_Int16:
                          k = ((GInt16*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_UInt32:
                          k = ((GUInt32*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_Int32:
                          k = ((GInt32*)(data))[iX + iY * XBlockSize];
                          break;
                        }
                        char key[12];
                        int klen = sprintf(key, "%i", k);
                        SV** sv = hv_fetch(hash, key, klen, 0);
                        if (sv && SvOK(*sv)) {
                            k = SvIV(*sv);
                        } else if (has_default) {
                            if (!(has_no_data && k == no_data))
                                k = def;
                            else
                                continue;
                        }
                        switch(dt) {
                        case GDT_Byte:
                          ((GByte*)(data))[iX + iY * XBlockSize] = k;
                          break;
                        case GDT_UInt16:
                          ((GUInt16*)(data))[iX + iY * XBlockSize] = k;
                          break;
                        case GDT_Int16:
                          ((GInt16*)(data))[iX + iY * XBlockSize] = k;
                          break;
                        case GDT_UInt32:
                          ((GUInt32*)(data))[iX + iY * XBlockSize] = k;
                          break;
                        case GDT_Int32:
                          ((GInt32*)(data))[iX + iY * XBlockSize] = k;
                          break;
                        }
                    }
                }
                e = GDALWriteBlock(self, xb, yb, data);
            }
        }
        CPLFree(data);
    }

}
