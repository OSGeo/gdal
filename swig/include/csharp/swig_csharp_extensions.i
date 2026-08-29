
/******************************************************************************
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
  private HandleRef? $csclassname_swigCPtr;
  private HandleRef swigCPtr { get { lock (m_LockObject) { return $csclassname_swigCPtr ?? throw new ObjectDisposedException(GetType().FullName); } } }
  protected readonly object m_LockObject = new object();
  protected bool swigCMemOwn;
  protected object swigParentRef;

  protected static object ThisOwn_true() { return null; }
  protected object ThisOwn_false() { return this; }

  public $csclassname(IntPtr cPtr, bool cMemoryOwn, object parent) {
    swigCMemOwn = cMemoryOwn;
    swigParentRef = parent;
    $csclassname_swigCPtr = new HandleRef(this, cPtr);
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


// Derived proxy classes
%typemap(csbody_derived) SWIGTYPE %{
  private HandleRef? $csclassname_swigCPtr;
  private HandleRef swigCPtr { get { lock (m_LockObject) { return $csclassname_swigCPtr ?? throw new ObjectDisposedException(GetType().FullName); } } }

  public $csclassname(IntPtr cPtr, bool cMemoryOwn, object parent) : base($modulePINVOKE.$csclassname_SWIGUpcast(cPtr), cMemoryOwn, parent) {
    $csclassname_swigCPtr = new HandleRef(this, cPtr);
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

%typemap(csdispose) SWIGTYPE %{
  ~$csclassname() {
    //Base class finalizer
    Dispose();
  }
%}

%typemap(csdispose_derived) SWIGTYPE %{
  ~$csclassname() {
    //Derrived class finalizer
    Dispose();
  }
%}

%typemap(csconstruct, excode=SWIGEXCODE) SWIGTYPE %{: this($imcall, true, null) {$excode
  }
%}

%typemap(csdisposing, methodname="Dispose", methodmodifiers="public") SWIGTYPE {
  lock(m_LockObject) {
      if($csclassname_swigCPtr.HasValue && $csclassname_swigCPtr.Value.Handle != IntPtr.Zero && swigCMemOwn) {
        swigCMemOwn = false;
        $imcall;
      }
      $csclassname_swigCPtr = null;
      swigParentRef = null;
      GC.SuppressFinalize(this);
    }
  }

%typemap(csdisposing_derived, methodname="Dispose", methodmodifiers="public") SWIGTYPE  {
  lock(m_LockObject) {
      if($csclassname_swigCPtr.HasValue && $csclassname_swigCPtr.Value.Handle != IntPtr.Zero && swigCMemOwn) {
        swigCMemOwn = false;
        $imcall;
      }
      $csclassname_swigCPtr = null;
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

%define DEFINE_EXTERNAL_CLASS(CTYPE, CSTYPE)
%typemap(cstype) (CTYPE*) "CSTYPE"
%typemap(csin) (CTYPE*)  "CSTYPE.getCPtr($csinput)"
%typemap(csout, excode=SWIGEXCODE, new="1") CTYPE *, CTYPE [], CTYPE (CLASS::*) {
    IntPtr cPtr = $imcall;
    CSTYPE ret = (cPtr == IntPtr.Zero) ? null : new CSTYPE(cPtr, $owner, ThisOwn_$owner());$excode
    return ret;
  }
%enddef

%{
typedef struct {
  int32_t  Count;
  int32_t  ItemSize;
  void    *pArray;
} ArrayWithSize;
%}

%define PRIMITIVE_ARRAYS_INOUT(CTYPE, CSTYPE)
%typemap(ctype)  (int nList, CTYPE *pList) "ArrayWithSize*"
%typemap(imtype) (int nList, CTYPE *pList) "in $modulePINVOKE.ArrayWithSize"
%typemap(cstype) (int nList, CTYPE *pList) "CSTYPE[]"
%typemap(in)     (int nList, CTYPE *pList) %{
  $1 = static_cast<$1_ltype>($input->Count);
  $2 = ($2_ltype)$input->pArray;
%}
%typemap(csin,
  pre="    GCHandle h$csinput = GCHandle.Alloc($csinput, GCHandleType.Pinned);
    var temp$csinput = new $modulePINVOKE.ArrayWithSize($csinput?.Length ?? 0, sizeof(CSTYPE), h$csinput.AddrOfPinnedObject());",
  post="      h$csinput.Free();"
) (int nList, CTYPE *pList) "temp$csinput"
/* list of primitives out - list not freed */
%typemap(ctype)  (int *nLen, CTYPE **pList) "ArrayWithSize*"
%typemap(imtype) (int *nLen, CTYPE **pList) "ref $modulePINVOKE.ArrayWithSize"
%typemap(cstype) (int *nLen, CTYPE **pList) "out CSTYPE[]"
%typemap(in)     (int *nLen, CTYPE **pList) %{
  $input->ItemSize = static_cast<int>(sizeof(CTYPE));
  $*1_ltype count$input = static_cast<$*1_ltype>($input->Count);
  $1 = &count$input;
  $2 = ($2_ltype)&$input->pArray;
%}
%typemap(argout) (int *nLen, CTYPE **pList) %{
  $input->Count = static_cast<int64_t>(count$input);
%}
%apply (int *nLen, CTYPE **pList)    {(int *nLen, CTYPE **pList_free)};
%typemap(csin,
  pre="    var temp$csinput = default($modulePINVOKE.ArrayWithSize);",
  terminator="    $csinput = temp$csinput.ToPrimitiveArray<CSTYPE>();"
) (int *nLen, CTYPE **pList) "ref temp$csinput"
/* list of primitives out - list freed */
%typemap(csin,
  pre="    var temp$csinput = default($modulePINVOKE.ArrayWithSize);",
  post="    $csinput = temp$csinput.ToPrimitiveArray<CSTYPE>();
    $module.CPLMemDestroy(temp$csinput.pArray);"
) (int *nLen, CTYPE **pList_free) "ref temp$csinput"
%apply (int *nLen, CTYPE **pList) {(int *nLen, const CTYPE **pList)};
%apply (int *nLen, CTYPE **pList_free) {(int *nLen, const CTYPE **pList_free)};
%enddef //PRIMITIVE_ARRAYS_INOUT

%define VALUE_LIST_INOUT(CTYPE, CSTYPE)
%typemap(ctype)  (int nList, CTYPE *pList) "ArrayWithSize*"
%typemap(imtype) (int nList, CTYPE *pList) "in $modulePINVOKE.ArrayWithSize"
%typemap(cstype) (int nList, CTYPE *pList) "CSTYPE[]"
%typemap(in)     (int nList, CTYPE *pList) %{
  $1 = static_cast<$1_ltype>($input->Count);
  $2 = ($2_ltype)$input->pArray;
%}
%typemap(csin,
  pre="    using (var temp$csinput = $modulePINVOKE.ArrayHelper<CSTYPE>.CreateByValue($csinput, CSTYPE.GetNativeSizeOf(), CSTYPE.getCPtr)) {",
  terminator="    }"
) (int nList, CTYPE *pList) "temp$csinput.GetArrayWithSize()"
%extend CTYPE {
  static int GetNativeSizeOf() {
    return static_cast<int>(sizeof(CTYPE));
  }
}
/* list of values out - elements not owned - list not freed */
%typemap(ctype)  (int *nList, CTYPE **pList) "ArrayWithSize*"
%typemap(imtype) (int *nList, CTYPE **pList) "ref $modulePINVOKE.ArrayWithSize"
%typemap(cstype) (int *nList, CTYPE **pList) "out CSTYPE[]"
%typemap(in)     (int *nList, CTYPE **pList) %{
  $input->ItemSize = static_cast<int>(sizeof(CTYPE));
  $*1_ltype count$input = static_cast<$*1_ltype>($input->Count);
  $1 = &count$input;
  $2 = ($2_ltype)&$input->pArray;
%}
%typemap(argout) (int *nList, CTYPE **pList) %{
  $input->Count = static_cast<int64_t>(count$input);
%}
%typemap(csin,
  pre="    var temp$csinput = default($modulePINVOKE.ArrayWithSize);",
  terminator="    $csinput = temp$csinput.ToValueArray(p => new CSTYPE(p, false, ThisOwn_false()));"
) (int* nList, CTYPE **pList) "ref temp$csinput"
%enddef //VALUE_LIST_INOUT

%define OBJECT_LIST_INOUT(CTYPE, CSTYPE)
%typemap(ctype)  (int object_list_count, CTYPE **poObjects) "ArrayWithSize*"
%typemap(imtype) (int object_list_count, CTYPE **poObjects) "in $modulePINVOKE.ArrayWithSize"
%typemap(cstype) (int object_list_count, CTYPE **poObjects) "CSTYPE[]"
%typemap(in)     (int object_list_count, CTYPE **poObjects) %{
  $1 = static_cast<$1_ltype>($input->Count);
  $2 = ($2_ltype)$input->pArray;
%}
%typemap(csin,
  pre="    using (var temp$csinput = $modulePINVOKE.ArrayHelper<CSTYPE>.CreateByReference($csinput, CSTYPE.getCPtr)) {",
  terminator="    }"
) (int object_list_count, CTYPE **poObjects) "temp$csinput.GetArrayWithSize()"
/* list of objects out - elements not owned - list not freed */
%typemap(ctype)  (int *object_list_count, CTYPE **poObjects) "ArrayWithSize*"
%typemap(imtype) (int *object_list_count, CTYPE **poObjects) "ref $modulePINVOKE.ArrayWithSize"
%typemap(cstype) (int *object_list_count, CTYPE **poObjects) "out CSTYPE[]"
%typemap(in)     (int *object_list_count, CTYPE **poObjects) %{
  $input->ItemSize = static_cast<int>(sizeof(void*));
  $*1_ltype count$input = static_cast<$*1_ltype>($input->Count);
  $1 = &count$input;
  $2 = ($2_ltype)&$input->pArray;
%}
%typemap(argout) (int *object_list_count, CTYPE **poObjects) %{
  $input->Count = static_cast<int64_t>(count$input);
%}
%typemap(csin,
  pre="    var temp$csinput = default($modulePINVOKE.ArrayWithSize);",
  terminator="    $csinput = temp$csinput.ToReferenceArray(p => new CSTYPE(p, false, ThisOwn_false()));"
) (int* object_list_count, CTYPE **poObjects) "ref temp$csinput"
%enddef //OBJECT_LIST_INOUT

%pragma(csharp) imclasscode=%{
internal readonly ref struct ArrayWithSize {
  public readonly int Count;
  public readonly int ItemSize;
  public readonly IntPtr pArray;

  public ArrayWithSize(int count, int itemSize, IntPtr p) {
    Count = count;
    ItemSize = itemSize;
    pArray = p;
  }

  public unsafe TVal[] ToPrimitiveArray<TVal>() where TVal : unmanaged {
    if (pArray == IntPtr.Zero || Count <= 0 || ItemSize != sizeof(TVal)) return new TVal[0];
    TVal[] result = new TVal[Count];
    long toCopy = (long)Count * ItemSize;
    fixed (TVal* pResult = result)
      Buffer.MemoryCopy(pArray.ToPointer(), pResult, toCopy, toCopy);
    return result;
  }

  public TVal[] ToValueArray<TVal>(Func<IntPtr, TVal> objectCreator)
    => FromUnmanaged(objectCreator, byValue: true);

  public TRef[] ToReferenceArray<TRef>(Func<IntPtr, TRef> objectCreator)
    => FromUnmanaged(objectCreator, byValue: false);

  private T[] FromUnmanaged<T>(Func<IntPtr, T> objectCreator, bool byValue) {
    if (pArray == IntPtr.Zero || Count <= 0 || ItemSize <= 0) return new T[0];
    T[] result = new T[Count];
    IntPtr pItem = pArray;
    for (int i = 0; i < Count; i++, pItem = IntPtr.Add(pItem, ItemSize)) {
      IntPtr pCurrent = byValue ? pItem : Marshal.ReadIntPtr(pItem);
      result[i] = pCurrent == IntPtr.Zero ? default(T) : objectCreator(pCurrent);
    }
    return result;
  }
}
internal class ArrayHelper<T> : IDisposable {
  private IntPtr m_ArrayHandle;
  private T[] Objects { get; }
  public int Count { get; }
  public int ItemSize { get; }
  public ArrayWithSize GetArrayWithSize() => new ArrayWithSize(Count, ItemSize, m_ArrayHandle);

  private ArrayHelper(T[] array, int itemSize) {
    ItemSize = itemSize;
    Objects = array;
    if (Objects == null || Objects.Length == 0) return;
    Count = Objects.Length;
    IntPtr memSize = new IntPtr(checked((long)Count * ItemSize));
    m_ArrayHandle = Marshal.AllocHGlobal(memSize);
  }

  public unsafe static ArrayHelper<T> CreateByValue(T[] array, int itemSize, Func<T, HandleRef> handleGetter) {
    var helper = itemSize > 0 ? new ArrayHelper<T>(array, itemSize)
        : throw new ArgumentOutOfRangeException(nameof(itemSize), "Must be a positive integer.");

    byte* pDest = (byte*)helper.m_ArrayHandle;
    for (int i = 0; i < helper.Count; i++, pDest += itemSize) {
      IntPtr handle = handleGetter(helper.Objects[i]).Handle;
      if (handle == IntPtr.Zero)
        throw new NullReferenceException("By-value arrays cannot contain null elements.");
      Buffer.MemoryCopy(handle.ToPointer(), pDest, itemSize, itemSize);
    }
    return helper;
  }

  public unsafe static ArrayHelper<T> CreateByReference(T[] array, Func<T, HandleRef> handleGetter) {
    var helper = new ArrayHelper<T>(array, IntPtr.Size);
    IntPtr* pDest = (IntPtr*)helper.m_ArrayHandle;
    for (int i = 0; i < helper.Count; i++) {
      //Should null reference elements be allowed??
      pDest[i] = handleGetter(helper.Objects[i]).Handle;
    }
    return helper;
  }

  public void Dispose() {
    IntPtr arrayHandle = System.Threading.Interlocked.Exchange(ref m_ArrayHandle, IntPtr.Zero);
    if (arrayHandle != IntPtr.Zero)
      System.Runtime.InteropServices.Marshal.FreeHGlobal(arrayHandle);
    System.GC.SuppressFinalize(this);
  }
  ~ArrayHelper() => Dispose();
}
%}
