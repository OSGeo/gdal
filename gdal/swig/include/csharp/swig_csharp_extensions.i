
/******************************************************************************
 * $Id$
 *
 * Name:     swig_csharp_extensions.i
 * Purpose:  Fix for the SWIG Interface problems (early GC)
 *           and implementing SWIGTYPE *DISOWN
 * Author:   Tamas Szekeres
 *
*/

#if (SWIG_VERSION < 0x020000)
// Ensure the class is not marked BeforeFieldInit causing memory corruption with CLR4
%pragma(csharp) imclasscode=%{
  static $imclassname() {
  }
%}
#endif

%typemap(csout, excode=SWIGEXCODE) SWIGTYPE {
    $&csclassname ret = new $&csclassname($imcall, true, null);$excode
    return ret;
  }

%typemap(csout, excode=SWIGEXCODE, new="1") SWIGTYPE & {
    $csclassname ret = new $csclassname($imcall, $owner, ThisOwn_$owner());$excode
    return ret;
  }
%typemap(csout, excode=SWIGEXCODE, new="1") SWIGTYPE *, SWIGTYPE [], SWIGTYPE (CLASS::*) {
    IntPtr cPtr = $imcall;
    $csclassname ret = (cPtr == IntPtr.Zero) ? null : new $csclassname(cPtr, $owner, ThisOwn_$owner());$excode
    return ret;
  }
%typemap(csvarout, excode=SWIGEXCODE2) SWIGTYPE & %{
    get {
      $csclassname ret = new $csclassname($imcall, $owner, ThisOwn_$owner());$excode
      return ret;
    } %}
%typemap(csvarout, excode=SWIGEXCODE2) SWIGTYPE *, SWIGTYPE [], SWIGTYPE (CLASS::*) %{
    get {
      IntPtr cPtr = $imcall;
      $csclassname ret = (cPtr == IntPtr.Zero) ? null : new $csclassname(cPtr, $owner, ThisOwn_$owner());$excode
      return ret;
    } %}
%typemap(csout, excode=SWIGEXCODE) SWIGTYPE *& {
    IntPtr cPtr = $imcall;
    $*csclassname ret = (cPtr == IntPtr.Zero) ? null : new $*csclassname(cPtr, $owner, ThisOwn_$owner());$excode
    return ret;
  }
// Proxy classes (base classes, i.e, not derived classes)
%typemap(csbody) SWIGTYPE %{
  private HandleRef swigCPtr;
  protected bool swigCMemOwn;
  protected object swigParentRef;

  protected static object ThisOwn_true() { return null; }
  protected object ThisOwn_false() { return this; }

  public $csclassname(IntPtr cPtr, bool cMemoryOwn, object parent) {
    swigCMemOwn = cMemoryOwn;
    swigParentRef = parent;
    swigCPtr = new HandleRef(this, cPtr);
  }

  public static HandleRef getCPtr($csclassname obj) {
    return (obj == null) ? new HandleRef(null, IntPtr.Zero) : obj.swigCPtr;
  }
  public static HandleRef getCPtrAndDisown($csclassname obj, object parent) {
    if (obj != null)
    {
      obj.swigCMemOwn = false;
      obj.swigParentRef = parent;
      return obj.swigCPtr;
    }
    else
    {
      return new HandleRef(null, IntPtr.Zero);
    }
  }
  public static HandleRef getCPtrAndSetReference($csclassname obj, object parent) {
    if (obj != null)
    {
      obj.swigParentRef = parent;
      return obj.swigCPtr;
    }
    else
    {
      return new HandleRef(null, IntPtr.Zero);
    }
  }
%}


#if SWIG_VERSION > 0x020000
// Derived proxy classes
%typemap(csbody_derived) SWIGTYPE %{
  private HandleRef swigCPtr;

  public $csclassname(IntPtr cPtr, bool cMemoryOwn, object parent) : base($modulePINVOKE.$csclassname_SWIGUpcast(cPtr), cMemoryOwn, parent) {
    swigCPtr = new HandleRef(this, cPtr);
  }

  public static HandleRef getCPtr($csclassname obj) {
    return (obj == null) ? new HandleRef(null, IntPtr.Zero) : obj.swigCPtr;
  }
  public static HandleRef getCPtrAndDisown($csclassname obj, object parent) {
    if (obj != null)
    {
      obj.swigCMemOwn = false;
      obj.swigParentRef = parent;
      return obj.swigCPtr;
    }
    else
    {
      return new HandleRef(null, IntPtr.Zero);
    }
  }
  public static HandleRef getCPtrAndSetReference($csclassname obj, object parent) {
    if (obj != null)
    {
      obj.swigParentRef = parent;
      return obj.swigCPtr;
    }
    else
    {
      return new HandleRef(null, IntPtr.Zero);
    }
  }
%}
#else
// Derived proxy classes
%typemap(csbody_derived) SWIGTYPE %{
  private HandleRef swigCPtr;

  public $csclassname(IntPtr cPtr, bool cMemoryOwn, object parent) : base($modulePINVOKE.$csclassnameUpcast(cPtr), cMemoryOwn, parent) {
    swigCPtr = new HandleRef(this, cPtr);
  }
  public static HandleRef getCPtr($csclassname obj) {
    return (obj == null) ? new HandleRef(null, IntPtr.Zero) : obj.swigCPtr;
  }
  public static HandleRef getCPtrAndDisown($csclassname obj, object parent) {
    if (obj != null)
    {
      obj.swigCMemOwn = false;
      obj.swigParentRef = parent;
      return obj.swigCPtr;
    }
    else
    {
      return new HandleRef(null, IntPtr.Zero);
    }
  }
  public static HandleRef getCPtrAndSetReference($csclassname obj, object parent) {
    if (obj != null)
    {
      obj.swigParentRef = parent;
      return obj.swigCPtr;
    }
    else
    {
      return new HandleRef(null, IntPtr.Zero);
    }
  }
%}
#endif

// Typewrapper classes
%typemap(csbody) SWIGTYPE *, SWIGTYPE &, SWIGTYPE [], SWIGTYPE (CLASS::*) %{
  private HandleRef swigCPtr;

  public $csclassname(IntPtr cPtr, bool futureUse, object parent) {
    swigCPtr = new HandleRef(this, cPtr);
  }

  protected $csclassname() {
    swigCPtr = new HandleRef(null, IntPtr.Zero);
  }

  public static HandleRef getCPtr($csclassname obj) {
    return (obj == null) ? new HandleRef(null, IntPtr.Zero) : obj.swigCPtr;
  }
%}

%typemap(csdispose) SWIGTYPE %{
  ~$csclassname() {
    Dispose();
  }
%}

%typemap(csconstruct, excode=SWIGEXCODE) SWIGTYPE %{: this($imcall, true, null) {$excode
  }
%}

%typemap(csdisposing, methodname="Dispose", methodmodifiers="public") SWIGTYPE {
  lock(this) {
      if(swigCPtr.Handle != IntPtr.Zero && swigCMemOwn) {
        swigCMemOwn = false;
        $imcall;
      }
      swigCPtr = new HandleRef(null, IntPtr.Zero);
      swigParentRef = null;
      GC.SuppressFinalize(this);
    }
  }

%typemap(csdisposing_derived, methodname="Dispose", methodmodifiers="public") TYPE {
  lock(this) {
      if(swigCPtr.Handle != IntPtr.Zero && swigCMemOwn) {
        swigCMemOwn = false;
        $imcall;
      }
      swigCPtr = new HandleRef(null, IntPtr.Zero);
      swigParentRef = null;
      GC.SuppressFinalize(this);
      base.Dispose();
    }
  }

%typemap(csin) SWIGTYPE *DISOWN "$csclassname.getCPtrAndDisown($csinput, ThisOwn_false())"
%typemap(csin) SWIGTYPE *SETREFERENCE "$csclassname.getCPtrAndSetReference($csinput, ThisOwn_false())"

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


%define DEFINE_EXTERNAL_CLASS(CTYPE, CSTYPE)
%typemap(cstype) (CTYPE*) "CSTYPE"
%typemap(csin) (CTYPE*)  "CSTYPE.getCPtr($csinput)"
%typemap(csout, excode=SWIGEXCODE, new="1") CTYPE *, CTYPE [], CTYPE (CLASS::*) {
    IntPtr cPtr = $imcall;
    CSTYPE ret = (cPtr == IntPtr.Zero) ? null : new CSTYPE(cPtr, $owner, ThisOwn_$owner());$excode
    return ret;
  }
%enddef
