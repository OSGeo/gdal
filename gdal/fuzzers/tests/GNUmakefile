
include ../../GDALmake.opt

EXE_LIST = 	test_gdal_fuzzer$(EXE) test_ogr_fuzzer$(EXE) \
		test_osr_set_from_user_input_fuzzer$(EXE) \
		test_wkb_import_fuzzer$(EXE) \
		test_wkt_import_fuzzer$(EXE)

default:	$(OBJ:.o=.$(OBJ_EXT)) $(EXE_LIST)

clean:
	$(RM) *.o $(O_OBJ) $(EXE_LIST)

test_gdal_fuzzer$(EXE): test_gdal_fuzzer.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LDFLAGS) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

test_ogr_fuzzer$(EXE): test_ogr_fuzzer.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LDFLAGS) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

test_osr_set_from_user_input_fuzzer$(EXE): test_osr_set_from_user_input_fuzzer.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LDFLAGS) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

test_wkb_import_fuzzer$(EXE): test_wkb_import_fuzzer.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LDFLAGS) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

test_wkt_import_fuzzer$(EXE): test_wkt_import_fuzzer.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LDFLAGS) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

check: $(EXE_LIST)
	./test_gdal_fuzzer$(EXE)
	./test_ogr_fuzzer$(EXE)
	./test_osr_set_from_user_input_fuzzer$(EXE)
	./test_wkb_import_fuzzer$(EXE)
	./test_wkt_import_fuzzer$(EXE)
