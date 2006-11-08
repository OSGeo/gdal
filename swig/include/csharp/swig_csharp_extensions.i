
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
 * Revision 1.1  2006/11/08 22:41:21  tamas
 * Preliminary fix for SWIG potential problems
 *
 *
*/

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

  internal $csclassname(IntPtr cPtr, object cMemoryOwner) : base($imclassname.$csclassnameUpcast(cPtr), cMemoryOwner) {
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

  internal $csclassname(IntPtr cPtr, bool futureUse) {
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

%typemap(csdestruct, methodname="Dispose") SWIGTYPE {
    if(swigCPtr.Handle != IntPtr.Zero && swigCMemOwner == null) {
      //swigCMemOwner = new object();
      $imcall;
    }
    swigCPtr = new HandleRef(null, IntPtr.Zero);
    GC.SuppressFinalize(this);
  }

%typemap(csdestruct_derived, methodname="Dispose") SWIGTYPE {
    if(swigCPtr.Handle != IntPtr.Zero && swigCMemOwner == null) {
      //swigCMemOwner = new object();
      $imcall;
    }
    swigCPtr = new HandleRef(null, IntPtr.Zero);
    GC.SuppressFinalize(this);
    base.Dispose();
  }
  
%typemap(csin) SWIGTYPE *DISOWN "$csclassname.getCPtrAndDisown($csinput, this)"
  