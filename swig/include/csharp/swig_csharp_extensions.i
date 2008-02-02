
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
// Proxy classes (base classes, ie, not derived classes)
%typemap(csbody) SWIGTYPE %{
  private HandleRef swigCPtr;
  protected bool swigCMemOwn;
  protected object swigParentRef;
  
  protected static object ThisOwn_true() { return null; }
  protected object ThisOwn_false() { return this; }

  internal $csclassname(IntPtr cPtr, bool cMemoryOwn, object parent) {
    swigCMemOwn = cMemoryOwn;
    swigParentRef = parent;
    swigCPtr = new HandleRef(this, cPtr);
  }

  internal static HandleRef getCPtr($csclassname obj) {
    return (obj == null) ? new HandleRef(null, IntPtr.Zero) : obj.swigCPtr;
  }
  internal static HandleRef getCPtrAndDisown($csclassname obj, object parent) {
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
  internal static HandleRef getCPtrAndSetReference($csclassname obj, object parent) {
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

// Derived proxy classes
%typemap(csbody_derived) SWIGTYPE %{
  private HandleRef swigCPtr;

  internal $csclassname(IntPtr cPtr, bool cMemoryOwn, object parent) : base($modulePINVOKE.$csclassnameUpcast(cPtr), cMemoryOwn, parent) {
    swigCPtr = new HandleRef(this, cPtr);
  }

  internal static HandleRef getCPtr($csclassname obj) {
    return (obj == null) ? new HandleRef(null, IntPtr.Zero) : obj.swigCPtr;
  }
  internal static HandleRef getCPtrAndDisown($csclassname obj, object parent) {
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
  internal static HandleRef getCPtrAndSetReference($csclassname obj, object parent) {
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

// Typewrapper classes
%typemap(csbody) SWIGTYPE *, SWIGTYPE &, SWIGTYPE [], SWIGTYPE (CLASS::*) %{
  private HandleRef swigCPtr;

  internal $csclassname(IntPtr cPtr, bool futureUse, object parent) {
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

%typemap(csconstruct, excode=SWIGEXCODE) SWIGTYPE %{: this($imcall, true, null) {$excode
  }
%}

%typemap(csdestruct, methodname="Dispose", methodmodifiers="public") SWIGTYPE {
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

%typemap(csdestruct_derived, methodname="Dispose", methodmodifiers="public") TYPE {
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

