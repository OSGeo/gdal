/* Perl specific Band methods, i.e., use Perl data structures directly in the C++ code */

%header %{
  double NVClassify(int comparison, double nv, AV* classifier, const char **error) {
     /* recursive, return nv < classifier[0] ? classifier[1] : classifier[2]
        sets error if there are not three values in the classifier,
        first is not a number, or second or third are not a number of arrayref
     */
     SV** f = av_fetch(classifier, 0, 0);
     SV** s = av_fetch(classifier, 1, 0);
     SV** t = av_fetch(classifier, 2, 0);
     if (f && SvNOK(*f)) {
         switch(comparison) {
         case 0: /* lt */
         if (nv < SvNV(*f))
             t = s;
         break;
         case 1: /* lte */
         if (nv <= SvNV(*f))
             t = s;
         break;
         case 2: /* gt */
         if (nv > SvNV(*f))
             t = s;
         break;
         case 3: /* gte */
         if (nv >= SvNV(*f))
             t = s;
         break;
         }
         if (t && SvNOK(*t))
             return SvNV(*t);
         else if (t && SvROK(*t) && (SvTYPE(SvRV(*t)) == SVt_PVAV))
             return NVClassify(comparison, nv, (AV*)(SvRV(*t)), error);
         else
             *error = "The decision in a classifier must be a number or a reference to a classifier.";
     } else
         *error = "The first value in a classifier must be a number.";
     return 0;
  }
  void NVClass(int comparison, double nv, AV* classifier, int *klass, const char **error) {
     /* recursive, return in klass nv < classifier[0] ? classifier[1] : classifier[2]
        sets error if there are not three values in the classifier,
        first is not a number, or second or third are not a number of arrayref
     */
     SV** f = av_fetch(classifier, 0, 0);
     SV** s = av_fetch(classifier, 1, 0);
     SV** t = av_fetch(classifier, 2, 0);
     if (f && SvNOK(*f)) {
         ++*klass;
         switch(comparison) {
         case 0: /* lt */
         if (nv < SvNV(*f))
             --*klass;
             t = s;
         break;
         case 1: /* lte */
         if (nv <= SvNV(*f))
             --*klass;
             t = s;
         break;
         case 2: /* gt */
         if (nv > SvNV(*f))
             --*klass;
             t = s;
         break;
         case 3: /* gte */
         if (nv >= SvNV(*f))
             --*klass;
             t = s;
         break;
         }
         if (t && SvNOK(*t))
             return;
         else if (t && SvROK(*t) && (SvTYPE(SvRV(*t)) == SVt_PVAV))
             NVClass(comparison, nv, (AV*)(SvRV(*t)), klass, error);
         else
             *error = "The decision in a classifier must be a number or a reference to a classifier.";
     } else
         *error = "The first value in a classifier must be a number.";
  }
  AV* to_array_classifier(SV* classifier, int* comparison, const char **error) {
      if (SvROK(classifier) && (SvTYPE(SvRV(classifier)) == SVt_PVAV)) {
          SV** f = av_fetch((AV*)SvRV(classifier), 0, 0);
          SV** s = av_fetch((AV*)SvRV(classifier), 1, 0);
          if (f && SvPOK(*f)) {
              char *c = SvPV_nolen(*f);
              if (strcmp(c, "<") == 0)
                  *comparison = 0;
              else if (strcmp(c, "<=") == 0)
                  *comparison = 1;
              else if (strcmp(c, ">") == 0)
                  *comparison = 2;
              else if (strcmp(c, ">=") == 0)
                  *comparison = 3;
              else {
                  *error = "The first element in classifier object must be a comparison.";
                  return NULL;
              }
          }
          if (s && SvROK(*s) && (SvTYPE(SvRV(*s)) == SVt_PVAV))
              return (AV*)SvRV(*s);
          else
              *error = "The second element in classifier object must be an array reference.";
      } else
          *error = NEED_ARRAY_REF;
      return NULL;
  }
%}

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
            do_confess("ClassCounts without classifier requires an integer band.", 1);
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

    SV *ClassCounts(SV* classifier,
                    GDALProgressFunc callback = NULL,
                    void* callback_data = NULL) {
                    
        const char *error = NULL;
        GDALDataType dt = GDALGetRasterDataType(self);
        if (!(dt == GDT_Float32 || dt == GDT_Float64)) {
            do_confess("ClassCounts with classifier requires a float band.", 1);
        }

        AV* array_classifier = NULL;
        int comparison = 0;

        array_classifier = to_array_classifier(classifier, &comparison, &error);
        if (error) do_confess(error, 1);

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
                        double nv = 0;
                        switch(dt) {
                        case GDT_Float32:
                          nv = ((float*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_Float64:
                          nv = ((double*)(data))[iX + iY * XBlockSize];
                          break;
                        }
                        int k = 0;
                        NVClass(comparison, nv, array_classifier, &k, &error);
                        if (error) goto fail;
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
        fail:
        CPLFree(data);
        do_confess(error, 1);
        return &PL_sv_undef;
    }

    void Reclassify(SV* classifier,
                    GDALProgressFunc callback = NULL,
                    void* callback_data = NULL) {
                    
        const char *error = NULL;
        
        GDALDataType dt = GDALGetRasterDataType(self);
        
        bool is_integer_raster = true;
        HV* hash_classifier = NULL;
        bool has_default = false;
        int32_t deflt = 0;

        AV* array_classifier = NULL;
        int comparison = 0;
        
        if (dt == GDT_Byte || dt == GDT_UInt16 || dt == GDT_Int16 || dt == GDT_UInt32 || dt == GDT_Int32) {
            if (SvROK(classifier) && (SvTYPE(SvRV(classifier)) == SVt_PVHV)) {
                hash_classifier = (HV*)SvRV(classifier);
                SV** sv = hv_fetch(hash_classifier, "*", 1, 0);
                if (sv && SvOK(*sv)) {
                    has_default = true;
                    deflt = SvIV(*sv);
                }
            } else {
                do_confess(NEED_HASH_REF, 1);
            }
        } else if (dt == GDT_Float32 || dt == GDT_Float64) {
            is_integer_raster = false;
            array_classifier = to_array_classifier(classifier, &comparison, &error);
            if (error) do_confess(error, 1);
        } else {
            do_confess("Only integer and float rasters can be reclassified.", 1);
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
                        int32_t k = 0;
                        double nv = 0;
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
                        case GDT_Float32:
                          nv = ((float*)(data))[iX + iY * XBlockSize];
                          break;
                        case GDT_Float64:
                          nv = ((double*)(data))[iX + iY * XBlockSize];
                          break;
                        }

                        if (is_integer_raster) {
                            char key[12];
                            int klen = sprintf(key, "%i", k);
                            SV** sv = hv_fetch(hash_classifier, key, klen, 0);
                            if (sv && SvOK(*sv)) {
                                k = SvIV(*sv);
                            } else if (has_default) {
                                if (!(has_no_data && k == no_data))
                                    k = deflt;
                                else
                                    continue;
                            }
                        } else {
                            nv = NVClassify(comparison, nv, array_classifier, &error);
                            if (error) goto fail;
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
                        case GDT_Float32:
                          ((float*)(data))[iX + iY * XBlockSize] = nv;
                          break;
                        case GDT_Float64:
                          ((double*)(data))[iX + iY * XBlockSize] = nv;
                          break;
                        }
                    }
                }
                e = GDALWriteBlock(self, xb, yb, data);
            }
        }
        CPLFree(data);
        return;
        fail:
        CPLFree(data);
        do_confess(error, 1);        
        return;
    }

}
