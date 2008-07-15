
/******************************************************************************
 * $Id$
 *
 * Name:     swig_csharp_extensions.i
 * Purpose:  Fix for the SWIG Interface problems (early GC) 
 *           and implementing SWIGTYPE *DISOWN 
 * Author:   Tamas Szekeres
 *
*/

%typemap(csout, excode=SWIGEXCODE) SWIGTYPE {
    $&csclassname ret = new $&csclassname($imcall, null);$excode
    return ret;
  }
  
%define %implement_class(TYPE)
%typemap(csout, excode=SWIGEXCODE, new="1") TYPE & {
    $csclassname ret = new $csclassname($imcall, ThisOwn_$owner());$excode
    return ret;
  }
%typemap(csout, excode=SWIGEXCODE, new="1") TYPE *, TYPE [], TYPE (CLASS::*) {
    IntPtr cPtr = $imcall;
    $csclassname ret = (cPtr == IntPtr.Zero) ? null : new $csclassname(cPtr, ThisOwn_$owner());$excode
    return ret;
  }
%typemap(csvarout, excode=SWIGEXCODE2) TYPE & %{
    get {
      $csclassname ret = new $csclassname($imcall, ThisOwn_$owner());$excode
      return ret;
    } %}
%typemap(csvarout, excode=SWIGEXCODE2) TYPE *, TYPE [], TYPE (CLASS::*) %{
    get {
      IntPtr cPtr = $imcall;
      $csclassname ret = (cPtr == IntPtr.Zero) ? null : new $csclassname(cPtr, ThisOwn_$owner());$excode
      return ret;
    } %}
%typemap(csout, excode=SWIGEXCODE) TYPE *& {
    IntPtr cPtr = $imcall;
    $*csclassname ret = (cPtr == IntPtr.Zero) ? null : new $*csclassname(cPtr, ThisOwn_$owner());$excode
    return ret;
  }
// Proxy classes (base classes, ie, not derived classes)
%typemap(csbody) TYPE %{
  private HandleRef swigCPtr;
  protected object swigCMemOwner;
  
  protected static object ThisOwn_true() { return null; }
  protected object ThisOwn_false() { return this; }

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
%typemap(csbody_derived) TYPE %{
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
%typemap(csbody) TYPE *, TYPE &, TYPE [], TYPE (CLASS::*) %{
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

%typemap(csfinalize) TYPE %{
  ~$csclassname() {
    Dispose();
  }
%}

%typemap(csconstruct, excode=SWIGEXCODE) TYPE %{: this($imcall, null) {$excode
  }
%}

%typemap(csdestruct, methodname="Dispose", methodmodifiers="public") TYPE {
  lock(this) {
      if(swigCPtr.Handle != IntPtr.Zero && swigCMemOwner == null) {
        swigCMemOwner = new object();
        $imcall;
      }
      swigCPtr = new HandleRef(null, IntPtr.Zero);
      GC.SuppressFinalize(this);
    }
  }

%typemap(csdestruct_derived, methodname="Dispose", methodmodifiers="public") TYPE {
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
          
%enddef

%typemap(csin) SWIGTYPE *DISOWN "$csclassname.getCPtrAndDisown($csinput, ThisOwn_false())"

%pragma(csharp) modulecode=%{
  internal class $moduleObject : IDisposable {
	public virtual void Dispose() {
      
    }
  }
  internal static $moduleObject the$moduleObject = new $moduleObject();
  protected static object ThisOwn_true() { return null; }
  protected static object ThisOwn_false() { return the$moduleObject; }
%}

/******************************************************************************
 * Generic functions to marshal SWIGTYPE arrays                               *
 *****************************************************************************/
  
%define IMPLEMENT_ARRAY_MARSHALER(CTYPE)
%csmethodmodifiers __WriteCArrayItem_##CTYPE "private";
%csmethodmodifiers __ReadCArrayItem_##CTYPE "private";
%csmethodmodifiers __AllocCArray_##CTYPE "private";
%csmethodmodifiers __FreeCArray_##CTYPE "private";
    %apply (void *buffer_ptr) {CTYPE* carray};
    %apply (void *buffer_ptr) {CTYPE* __AllocCArray_##CTYPE};
    void __WriteCArrayItem_##CTYPE(CTYPE* carray, int index, CTYPE* value) {
       carray[index] = *value;
    }
    CTYPE* __ReadCArrayItem_##CTYPE(CTYPE* carray, int index) {
       return &carray[index];
    }
    CTYPE* __AllocCArray_##CTYPE(int size) {
       return (CTYPE*)CPLMalloc(size * sizeof(CTYPE));
    }
    void __FreeCArray_##CTYPE(CTYPE* carray) {
       if (carray)
        CPLFree(carray);
    }
    %clear CTYPE* carray;
    %clear CTYPE* __AllocCArray_##CTYPE;
%enddef

%define IMPLEMENT_ARRAY_MARSHALER_STATIC(CTYPE)
%csmethodmodifiers __WriteCArrayItem_##CTYPE "internal";
%csmethodmodifiers __ReadCArrayItem_##CTYPE "internal";
%csmethodmodifiers __AllocCArray_##CTYPE "internal";
%csmethodmodifiers __FreeCArray_##CTYPE "internal";
    %apply (void *buffer_ptr) {CTYPE* carray};
    %apply (void *buffer_ptr) {CTYPE* __AllocCArray_##CTYPE};
%inline %{
    void __WriteCArrayItem_##CTYPE(CTYPE* carray, int index, CTYPE* value) {
       carray[index] = *value;
    }
%}
%inline %{
    CTYPE* __ReadCArrayItem_##CTYPE(CTYPE* carray, int index) {
       return &carray[index];
    }
%}
%inline %{
    CTYPE* __AllocCArray_##CTYPE(int size) {
       return (CTYPE*)CPLMalloc(size * sizeof(CTYPE));
    }
%}
%inline %{
    void __FreeCArray_##CTYPE(CTYPE* carray) {
       if (carray)
        CPLFree(carray);
    }
%}
    %clear CTYPE* carray;
    %clear CTYPE* __AllocCArray_##CTYPE;
%enddef

