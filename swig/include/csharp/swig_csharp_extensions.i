
/******************************************************************************
 * $Id$
 *
 * Name:     swig_csharp_extensions.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Temporary fix for the SWIG Interface problems
 * Author:   Tamas Szekeres
 *

 *
 * $Log$
 * Revision 1.5  2006/12/04 09:48:07  tamas
 * Replacing $imclassname to $modulePINVOKE for supporting SWIG 1.3.27
 *
 * Revision 1.4  2006/11/25 21:23:02  tamas
 * Added default csout, csvarout typemaps
 *
 * Revision 1.3  2006/11/15 21:00:35  tamas
 * Added support for SWIG 1.3.30
 *
 * Revision 1.2  2006/11/11 19:28:39  tamas
 * Support for the default csout typemaps
 *
 * Revision 1.1  2006/11/08 22:41:21  tamas
 * Preliminary fix for SWIG potential problems
 *
 *
*/

// Comment out the following line to revert to the original SWIG behaviour
#define ADVANCED_OBJECT_REF

#ifdef ADVANCED_OBJECT_REF
%typemap(csout, excode=SWIGEXCODE) SWIGTYPE {
    $&csclassname ret = new $&csclassname($imcall, null);$excode
    return ret;
  }
  
%define %owner(OWNER, TYPE)
%typemap(csout, excode=SWIGEXCODE, new="1") TYPE & {
    $csclassname ret = new $csclassname($imcall, $owner? null : OWNER);$excode
    return ret;
  }
%typemap(csout, excode=SWIGEXCODE, new="1") TYPE *, TYPE [], TYPE (CLASS::*) {
    IntPtr cPtr = $imcall;
    $csclassname ret = (cPtr == IntPtr.Zero) ? null : new $csclassname(cPtr, $owner? null : OWNER);$excode
    return ret;
  }
%typemap(csvarout, excode=SWIGEXCODE2) TYPE & %{
    get {
      $csclassname ret = new $csclassname($imcall, $owner? null : OWNER);$excode
      return ret;
    } %}
%typemap(csvarout, excode=SWIGEXCODE2) TYPE *, TYPE [], TYPE (CLASS::*) %{
    get {
      IntPtr cPtr = $imcall;
      $csclassname ret = (cPtr == IntPtr.Zero) ? null : new $csclassname(cPtr, $owner? null : OWNER);$excode
      return ret;
    } %}
%typemap(csout, excode=SWIGEXCODE) TYPE *& {
    IntPtr cPtr = $imcall;
    $*csclassname ret = (cPtr == IntPtr.Zero) ? null : new $*csclassname(cPtr, $owner? null : OWNER);$excode
    return ret;
  }    
%enddef

#define %object_owner %owner(this, SWIGTYPE)
#define %static_owner %owner(new object(), SWIGTYPE)

%object_owner

%owner(new object(), GByte)

// Proxy classes (base classes, ie, not derived classes)
%typemap(csbody) SWIGTYPE %{
  private HandleRef swigCPtr;
  protected object swigCMemOwner;

  internal $csclassname(IntPtr cPtr, object cMemoryOwner) {
    swigCMemOwner = cMemoryOwner;
    swigCPtr = new HandleRef(this, cPtr);
  }

  internal static HandleRef getCPtr($csclassname obj) {
    return (obj == null) ? new HandleRef(null, IntPtr.Zero) : obj.swigCPtr;
  }
  internal static HandleRef getCPtrAndDisown($csclassname obj, object cMemoryOwner) {
    obj.swigCMemOwner = cMemoryOwner;
    return getCPtr(obj);
  }
%}

// Derived proxy classes
%typemap(csbody_derived) SWIGTYPE %{
  private HandleRef swigCPtr;

  internal $csclassname(IntPtr cPtr, object cMemoryOwner) : base($modulePINVOKE.$csclassnameUpcast(cPtr), cMemoryOwner) {
    swigCPtr = new HandleRef(this, cPtr);
  }

  internal static HandleRef getCPtr($csclassname obj) {
    return (obj == null) ? new HandleRef(null, IntPtr.Zero) : obj.swigCPtr;
  }
  internal static HandleRef getCPtrAndDisown($csclassname obj, object cMemoryOwner) {
    obj.swigCMemOwner = cMemoryOwner;
    return getCPtr(obj);
  }
%}

// Typewrapper classes
%typemap(csbody) SWIGTYPE *, SWIGTYPE &, SWIGTYPE [], SWIGTYPE (CLASS::*) %{
  private HandleRef swigCPtr;

  internal $csclassname(IntPtr cPtr, object futureUse) {
    swigCPtr = new HandleRef(this, cPtr);
  }

  protected $csclassname() {
    swigCPtr = new HandleRef(null, IntPtr.Zero);
  }

  internal static HandleRef getCPtr($csclassname obj) {
    return (obj == null) ? new HandleRef(null, IntPtr.Zero) : obj.swigCPtr;
  }
%}

%typemap(csfinalize) SWIGTYPE %{
  ~$csclassname() {
    Dispose();
  }
%}

%typemap(csconstruct, excode=SWIGEXCODE) SWIGTYPE %{: this($imcall, null) {$excode
  }
%}

%typemap(csdestruct, methodname="Dispose", methodmodifiers="public") SWIGTYPE {
  lock(this) {
      if(swigCPtr.Handle != IntPtr.Zero && swigCMemOwner == null) {
        swigCMemOwner = new object();
        $imcall;
      }
      swigCPtr = new HandleRef(null, IntPtr.Zero);
      GC.SuppressFinalize(this);
    }
  }

%typemap(csdestruct_derived, methodname="Dispose", methodmodifiers="public") SWIGTYPE {
  lock(this) {
      if(swigCPtr.Handle != IntPtr.Zero && swigCMemOwner == null) {
        swigCMemOwner = new object();
        $imcall;
      }
      swigCPtr = new HandleRef(null, IntPtr.Zero);
      GC.SuppressFinalize(this);
      base.Dispose();
    }
  }
  
%typemap(csin) SWIGTYPE *DISOWN "$csclassname.getCPtrAndDisown($csinput, this)"

#else //ADVANCED_OBJECT_REF
%typemap(cscode) SWIGTYPE %{
  internal static HandleRef getCPtrAndDisown($csclassname obj) {
    obj.swigCMemOwn = false;
    return getCPtr(obj);
  }
%}

%typemap(csin) SWIGTYPE *DISOWN "$csclassname.getCPtrAndDisown($csinput)"

#define %object_owner
#define %static_owner

#endif
  