// use std::os::raw::{c_void, c_uchar, c_int};
use std::collections::HashMap;
use std::any::TypeId;

/******************************************************************************/

pub type Bool = std::os::raw::c_int;
pub type Uint = std::os::raw::c_uint;
pub type Size = std::os::raw::c_ulonglong;
pub type Void = std::os::raw::c_void;
pub type Qual = std::os::raw::c_uchar;
pub type Char = std::os::raw::c_char;

// use std::ffi::{CString, CStr};

/******************************************************************************/

#[repr(C)]
pub struct Table { _private: [u8; 0] }

#[repr(C)]
#[derive(Clone, Copy, PartialEq)]
pub struct Index { ptr: *const Table }

impl Index {
    pub const fn new() -> Index { Index{ptr: std::ptr::null()} }
}

/******************************************************************************/

pub enum Qualifier { Const, Lvalue, Rvalue }

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Ref {
    ptr: *mut Void,
    ind: Index,
    qual: Qual,
}

impl Ref {
    pub fn new() -> Ref { Ref{ptr: std::ptr::null_mut(), ind: Index::new(), qual: 0} }

    pub fn address(&self) -> *mut Void { self.ptr }

    pub fn index(&self) -> Index { self.ind }

    pub fn has_value(&self) -> bool { self.ptr != std::ptr::null_mut() }

    pub fn qualifier(&self) -> Qualifier {
        match self.qual {
            0 => Qualifier::Const,
            1 => Qualifier::Lvalue,
            2 => Qualifier::Rvalue,
            _ => panic!("Invalid qualifier")
        }
    }
}

/******************************************************************************/

// #[repr(C)]
// pub struct CFunction { pub data: [u8; 0] }

#[repr(C)]
pub struct Scope { pub data: [u8; 0] }

#[repr(C)]
pub struct Value {
    ptr: *mut Void,
    ind: Index,
}

unsafe impl Sync for Value {

}

/******************************************************************************/

#[repr(C)]
pub struct StrView (*const u8, Size);

impl From<&str> for StrView {
    fn from(s: &str) -> StrView { StrView{0: s.as_ptr(), 1: s.len() as Size} }
}

#[repr(C)]
pub struct Args (*mut Ref, Size);

impl Args {
    fn new() -> Args {Args{0: std::ptr::null_mut(), 1: 0 as Size}}
}

/******************************************************************************/

pub type DropT = extern fn(*mut Void) -> ();
pub type CopyT = Option<extern fn(*const Void) -> *mut Void>;
pub type ToValueT = Option<extern fn(&mut Value, *mut Void, Qual) -> Bool>;
pub type ToRefT = Option<extern fn(&mut Ref, *mut Void, Qual) -> Bool>;
pub type AssignIfT = Option<extern fn(*mut Void, &Ref) -> Bool>;
pub type FromRefT = Option<extern fn(&mut Value, &Ref, &Scope) -> Bool>;

// #[link(name = "librustrebind")]
extern "C" {
    pub fn rebind_table_emplace(
        name: StrView,
        data: *const Void,
        drop: DropT,
        copy: CopyT,
        to_value: ToValueT,
        to_ref: ToRefT,
        assign_if: AssignIfT,
        from_ref: FromRefT
    ) -> Index;

    // pub fn rebind_table_add_method(index: Index, name: *const Char, method: &CFunction);

    pub fn rebind_table_add_base(index: Index, base: Index);

    /**************************************************************************/

    pub fn rebind_init() -> Bool;

    /**************************************************************************/

    pub fn rebind_value_drop(v: *mut Value);

    pub fn rebind_value_copy(v: *mut Value, o: *const Value) -> Bool;

    pub fn rebind_value_method_to_value(v: *const Value, name: StrView, argv: Args) -> Value;

    pub fn rebind_value_call_value(o: *mut Value, v: *const Value, argv: Args) -> Bool;

    pub fn rebind_value_call_ref(v: *const Value, argv: Args) -> Ref;

    pub fn rebind_lookup(name: StrView) -> *const Value;

    /**************************************************************************/

    pub fn rebind_index_name(v: Index) -> *const Char;

    /**************************************************************************/
}

pub fn initialize() {
    println!("initializing...");
    let ok = unsafe {rebind_init()};
    if ok == 0 {
        panic!("rebind initialization failed");
    }
    println!("initialized...");
}

pub fn lookup(name: &str) -> &'static Value {
    println!("looking up...: {}", name);
    unsafe {
        println!("wait what");
        println!("ok...: {}", name);
        let ptr = rebind_lookup(StrView::from(name));
        println!("ok...: {} {}", name, ptr as u64);
        if ptr != std::ptr::null() {return &(*ptr)}
    }
    panic!("bad lookup")
}

/******************************************************************************/

pub trait FromValue {
    fn from_matching_value(v: Value) -> Self;
}

impl FromValue for i32 {
    fn from_matching_value(v: Value) -> Self { v.target::<i32>().unwrap() }
}

impl FromValue for f64 {
    fn from_matching_value(v: Value) -> Self { v.target::<f64>().unwrap() }
}

// struct Handle<'a> {
//     ptr: *const u8,
//     marker: PhantomData<&'a ()>,
// }

/******************************************************************************/

extern "C" fn drop<T>(s: *mut Void) {
    unsafe {Box::<T>::from_raw(s as *mut T);}
}

extern "C" fn copy<T>(s: *const Void) -> *mut Void {
    std::ptr::null_mut()
}

extern "C" fn to_value<T>(v: &mut Value, s: *mut Void, q: u8) -> Bool {
    false as Bool
}

extern "C" fn to_ref<T>(r: &mut Ref, s: *mut Void, q: u8) -> Bool {
    false as Bool
}

extern "C" fn assign_if<T>(s: *mut Void, r: &Ref) -> Bool {
    false as Bool
}

extern "C" fn from_ref<T>(v: &mut Value, r: &Ref, s: &Scope) -> Bool {
    false as Bool
}

/******************************************************************************/

pub fn create_table<T: 'static>(name: &str) -> Index {
    unsafe{ rebind_table_emplace(
        StrView::from(name),
        std::mem::transmute::<TypeId, *const Void>(TypeId::of::<T>()),
        drop::<T>,
        Some(copy::<T>),
        Some(to_value::<T>),
        Some(to_ref::<T>),
        Some(assign_if::<T>),
        Some(from_ref::<T>),
    ) }
}

pub fn fetch<T: 'static>() -> Index {
    let mut tables: HashMap<TypeId, Index> = HashMap::new();

    *tables.entry(TypeId::of::<T>()).or_insert_with(|| create_table::<T>("dummy"))
}

/******************************************************************************/

pub trait IntoRef {
    fn into_ref(self) -> Ref;
}

impl<T> IntoRef for T {
    fn into_ref(self: T) -> Ref { Ref::new() }
}

pub trait IntoArgView {
    fn into_arguments(self) -> Vec<Ref>;
}

impl IntoArgView for () {
    fn into_arguments(self) -> Vec<Ref> { vec![] }
}

impl<T1> IntoArgView for (T1,) {
    fn into_arguments(self) -> Vec<Ref> { vec![self.0.into_ref()] }
}

impl<T1, T2> IntoArgView for (T1, T2,) {
    fn into_arguments(self) -> Vec<Ref> { vec![self.0.into_ref(), self.1.into_ref()] }
}

/******************************************************************************/

impl Value {
    pub const fn new() -> Value { Value{ptr: std::ptr::null_mut(), ind: Index::new()} }

    pub const fn address(&self) -> *mut Void { self.ptr }

    pub const fn index(&self) -> Index { self.ind }

    pub fn method<T: IntoArgView>(&self, name: &str, args: T) -> Value {
        // let mut argv =
        unsafe {rebind_value_method_to_value(self, StrView::from(name), Args::new())}
    }

    pub fn is<T: 'static>(&self) -> bool {
        self.ind == fetch::<T>()
    }

    pub fn target<T: 'static>(self) -> Option<T> {
        if self.is::<T>() { unsafe{ Some(*Box::from_raw(self.ptr as *mut T)) } }
        else { None }
    }

    pub fn call<T: IntoArgView>(&self, args: T) -> Value {
        let mut o = Value::new();
        let ok = unsafe { rebind_value_call_value(&mut o, self, Args::new()) };
        if ok == 0 {
            panic!("function failed");
        }
        return o;
    }

    pub fn call_ref<T: IntoArgView>(&self, args: T) -> Ref {
        unsafe {rebind_value_call_ref(self, Args::new())}
    }

    pub fn cast<T: 'static + FromValue>(self) -> T {
        // Option::<T>::new()
        match self.target::<T>() {
            None => panic!("bad"),
            Some(t) => t
        }
    }

    pub fn cast_ref<'a, T: FromValue>(&'a self) -> Option<&'a T> {
        None
    }
}

/******************************************************************************/

impl Drop for Value {
    fn drop(&mut self) {
        println!("dropping value");
        unsafe{rebind_value_drop(self);}
        println!("dropped value");
    }
}

/******************************************************************************/

impl Clone for Value {
    fn clone(&self) -> Value {
        let mut v = Value::new();
        if unsafe{rebind_value_copy(&mut v, self) == 0} { panic!("copy failed!"); }
        v
    }
}

/******************************************************************************/

impl Index {
    pub fn name(&self) -> &str {
        unsafe{std::ffi::CStr::from_ptr(rebind_index_name(*self))}.to_str().unwrap()
    }
}

/******************************************************************************/

pub fn invoke<T: IntoArgView>(name: &str, args: T) -> Value {
    let v: &Value = lookup(name);
    println!("got the lookup!");
    v.call(args)
}

// impl<T: FromValue> std::convert::TryFrom<Value> for T {
//     fn try_from(self) -> Result<T, Self::Error> {
//         T::from_matching_value(self)
//     }
// }

/******************************************************************************/

// struct TypedValue<T> {
//     base : Value
// }