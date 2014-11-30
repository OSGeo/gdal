/*
 * $Id$
 *
 * ruby specific code for ogr bindings.
 */

/*
 * $Log$
 * Revision 1.4  2006/01/17 04:42:16  cfis
 * Added some renames that are not covered by -autorename since they start with the text OGR.
 *
 * Revision 1.3  2005/09/26 08:18:21  cfis
 * Moved renames to typemaps_ruby.i.  Now %include typemaps_ruby.i instead of %import (we want to inline the code).
 *
 * Revision 1.2  2005/09/18 07:34:58  cfis
 * Added support for exceptions, removed some outdated code.
 *
 * Revision 1.1  2005/09/02 16:19:23  kruland
 * Major reorganization to accommodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 */

/* Include default Ruby typemaps */
%include typemaps_ruby.i

/* Include exception handling code */
%include cpl_exceptions.i

/* Setup a few renames */
%rename(get_driver_count) OGRGetDriverCount;
%rename(get_open_dscount) OGRGetOpenDSCount;
%rename(set_generate_db2_v72_byte_order) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename(register_all) OGRRegisterAll;


%init %{

  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }

  /* Setup exception handling */
  UseExceptions();
%}

/* Replace GetLayerByIndex and GetLayerByName by GetLayer */
%ignore OGRDataSourceShadow::GetLayerByIndex;
%ignore OGRDataSourceShadow::GetLayerByName;

%extend OGRDataSourceShadow {

	OGRLayerShadow *GetLayer(VALUE whichLayer) {
		// get field index
		switch (TYPE(whichLayer)) {
			case T_STRING: {
				char* name = StringValuePtr(whichLayer);
				return OGR_DS_GetLayerByName(self, name);
				break;
			}
			case T_FIXNUM: {
				int index = NUM2INT(whichLayer);
				return OGR_DS_GetLayer(self, index);
				break;
			}
			default:
				SWIG_exception(SWIG_TypeError, "Value must be a string or integer.");
		}
	}

	/* Override the way that ReleaseResultSet is handled - we
	   want to apply a typemap that unlinks the layer from
		its underlying C++ object since this method destroys
		the C++ object */
//	%typemap(freearg) OGRLayerShadow *layer {
		/* %typemap(freearg) OGRLayerShadow *layer */
//		DATA_PTR(argv[0]) = 0;
//	}
  // void ReleaseResultSet(OGRLayerShadow *layer) {
    // OGR_DS_ReleaseResultSet(self, layer);
//   }
}

/* Extend the layers class by adding support for ruby enumerable mixin. */
%mixin OGRLayerShadow "Enumerable";

/* Replace GetNextFeature by each */
%ignore OGRDataSourceShadow::GetLayerByIndex;

%extend OGRLayerShadow {
	/*~OGRLayerShadow () {
		FreeResultSet();
	}

	%typemap(in) OGRDatasourceShadow *ds {
		SWIG_ConvertPtr($input, (void **) &$1, $1_descriptor, SWIG_POINTER_DISOWN);
	   rb_iv_set($input, "__swigtype__", self);
	}
   void ReleaseResultSet(OGRDatasourceShadow *ds, OGRLayerShadow *layer) {
     OGR_DS_ReleaseResultSet(self, layer);
   }
*/
	%newobject OGRLayerShadow::each;
	void each() {
		OGRFeatureShadow* feature = NULL;

 		while (feature = (OGRFeatureShadow*) OGR_L_GetNextFeature(self))
 		{
			/* Convert the pointer to a Ruby object.  Note we set the flag
		   to one manually to show this is a new object */
			VALUE object = SWIG_NewPointerObj((void *) feature, $descriptor(OGRFeatureShadow *), SWIG_POINTER_OWN);			

			/* Now invoke the block specified for this method. */
			rb_yield(object);
		}
  }
}

%extend OGRFeatureShadow {
	VALUE GetField(VALUE object) {
		VALUE result;

		int index;

		// get field index
		switch (TYPE(object)) {
			case T_STRING:
				index = OGR_F_GetFieldIndex(self, StringValuePtr(object));
				break;
			case T_FIXNUM:
				index = NUM2INT(object);
				break;
			default:
				SWIG_exception(SWIG_TypeError, "Value must be a string or integer.");
		}
		
		int count = OGR_F_GetFieldCount(self);
		
		if (index < 0 || index > count) {
			SWIG_exception(SWIG_IndexError, "Illegal field requested.");
		}

		// is the field unset?
	  if (!OGR_F_IsFieldSet(self, index)) {
	  	result = Qnil;
	  	return result;
	  }
	  
	  // get field type
    OGRFieldType field_type = (OGRFieldType) OGR_Fld_GetType(OGR_F_GetFieldDefnRef( self, index));

		switch (field_type) {
			case OFTInteger: {
				const int value = OGR_F_GetFieldAsInteger(self, index);
				result = INT2NUM(value);
				break;
			}

			case OFTIntegerList: {
				int len = 0;
				const int* list = OGR_F_GetFieldAsIntegerList(self, index, &len);
				
				result = rb_ary_new2(len);
				
		    for ( int i = 0; i < len; ++i, ++list ) {
					VALUE item = INT2NUM(*list);
		      rb_ary_store(result, item, i);
		    }
		    break;
			}

			case OFTReal: {
				const double value = OGR_F_GetFieldAsDouble(self, index);
				return rb_float_new(value);
				break;
			}

			case OFTRealList: {
				int len = 0;
				const double* list = OGR_F_GetFieldAsDoubleList(self, index, &len);
				
				result = rb_ary_new2(len);
				
		    for ( int i = 0; i < len; ++i, ++list ) {
					VALUE item = rb_float_new(*list);
		      rb_ary_store(result, item, i);
		    }
		    break;
			}

			case OFTString: {
				const char* value = (const char *) OGR_F_GetFieldAsString(self, index);
				return rb_str_new2(value);
				break;
			}

			case OFTStringList:
/*				int len3 = 0;
				const char** string_list = OGR_F_GetFieldAsStringList(self, index, &len);
				
				result = rb_ary_new2(len3);
				
		    for ( int i = 0; i < len; ++i, ++string_list ) {
					VALUE item = rb_str_new2(*string_list);
		      rb_ary_store(result, item, i);
		    }*/
		    result = Qnil;
		    break;
			default:
				SWIG_exception(SWIG_TypeError, "Unsupported field type.");
		}
	
		return result;		
	}        
}


