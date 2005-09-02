/*
 * $Id$
 *
 * ruby specific code for ogr bindings.
 */

/*
 * $Log$
 * Revision 1.1  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 */

%init %{

  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }
  
%}

//**************   Ruby specific extensions ***************
/*
%extend OGRDriverShadow {
	static OGRDriverShadow* GetDriverByName( char const *name ) {
  	return (OGRDriverShadow*) OGRGetDriverByName( name );
	}
  
	static OGRDriverShadow* GetDriver(int driver_number) {
  	return (OGRDriverShadow*) OGRGetDriver(driver_number);
	}  
}
*/

/* Replace GetLayerByIndex and GetLayerByName by GetLayer */
%ignore OGRDataSourceShadow::GetLayerByIndex;
%ignore OGRDataSourceShadow::GetLayerByName;

%extend OGRDataSourceShadow {
/*
	%newobject Open;
	static OGRDataSourceShadow *Open( const char * filename, int update=0 ) {
	    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpen(filename,update, NULL);
	    return ds;
	  }
	
	%newobject OpenShared;
	static
	OGRDataSourceShadow *OpenShared( const char * filename, int update=0 ) {
	    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpenShared(filename,update, NULL);
	    return ds;
	}
*/
	OGRLayerShadow *GetLayer(VALUE object) {
		// get field index
		switch (TYPE(object)) {
			case T_STRING: {
				char* name = StringValuePtr(object);
				return OGR_DS_GetLayerByName(self, name);
				break;
			}
			case T_FIXNUM: {
				int index = NUM2INT(object);
				return OGR_DS_GetLayer(self, index);
				break;
			}
			default:
				SWIG_exception(SWIG_TypeError, "Value must be a string or integer.");
		}
	}

}

// Extend the layers class by adding the method each to support
// the ruby enumerator mixin.
%mixin OGRLayerShadow "Enumerable";
%extend OGRLayerShadow {
  %newobject OGRLayerShadow::each;
  void each() {
	OGRFeatureShadow* feature = NULL;
 	while (feature = (OGRFeatureShadow*) OGR_L_GetNextFeature(self))
 	{
		/* Convert the pointer to a Ruby object.  Note we set the flag
		   to one manually to show this is a new object */
		VALUE object = SWIG_NewPointerObj((void *) feature, $descriptor(OGRFeatureShadow *), 1);			

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

%import typemaps_ruby.i
