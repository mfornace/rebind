// use std::os::raw::{c_void, c_uchar, c_int};
use std::collections::HashMap;
use std::any::TypeId;
use std::marker::PhantomData;

/******************************************************************************/

pub type Bool = std::os::raw::c_int;
pub type Uint = std::os::raw::c_uint;
pub type Void = std::os::raw::c_void;
pub type Qual = std::os::raw::c_uchar;
pub type Char = std::os::raw::c_char;
use std::ffi::{CString, CStr};

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

#[repr(C)]
pub struct CFunction { pub data: [u8; 0] }

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

type DestroyT = extern fn(*mut Void) -> ();
type CopyT = extern fn(*const Void) -> *mut Void;
type ToValueT = extern fn(&mut Value, *mut Void, Qual) -> Bool;
type ToRefT = extern fn(&mut Ref, *mut Void, Qual) -> Bool;
type AssignIfT = extern fn(*mut Void, &Ref) -> ();
type FromRefT = extern fn(&Ref, &Scope) -> Value;

// #[link(name = "librustrebind")]
extern "C" {
    pub fn rebind_table_insert() -> Index;

    pub fn rebind_table_set(
        index: Index,
        destroy: DestroyT,
        copy: CopyT,
        to_value: ToValueT,
        to_ref: ToRefT,
        assign_if: AssignIfT,
        from_ref: FromRefT
    );

    pub fn rebind_table_add_method(index: Index, name: *const Char, method: &CFunction);

    pub fn rebind_table_add_base(index: Index, base: Index);

    /**************************************************************************/

    pub fn rebind_value_destruct(v: &mut Value);

    pub fn rebind_value_copy(v: &mut Value, o: &Value) -> Bool;

    pub fn rebind_value_method_to_value(v: &Value, name: *const u8, argv: *mut Ref, argn: Uint) -> Value;

    pub fn rebind_value_call_value(v: &Value, argv: *mut Ref, argn: Uint) -> Value;

    pub fn rebind_value_call_ref(v: &Value, argv: *mut Ref, argn: Uint) -> Ref;

    pub fn rebind_lookup(name: *const u8) -> &'static Value;

    /**************************************************************************/

    pub fn rebind_index_name(v: Index) -> *const Char;

    /**************************************************************************/
}

pub fn lookup(name: &str) -> &'static Value {
    unsafe { rebind_lookup(name.as_ptr()) }
}

/******************************************************************************/

pub trait FromValue {
    fn from_matching_value(v: Value) -> Self;
}

impl FromValue for i32 {
    fn from_matching_value(v: Value) -> Self { v.get::<i32>() }
}

impl FromValue for f64 {
    fn from_matching_value(v: Value) -> Self { v.get::<f64>() }
}

// struct Handle<'a> {
//     ptr: *const u8,
//     marker: PhantomData<&'a ()>,
// }

pub fn create_table<T>() -> Index {
    unsafe{ rebind_table_insert() }
}


pub fn fetch<T: 'static>() -> Index {
    let mut tables: HashMap<TypeId, Index> = HashMap::new();

    *tables.entry(TypeId::of::<T>()).or_insert_with(|| create_table::<T>())
}

/******************************************************************************/

pub trait IntoRef {
    fn into_ref(self) -> Ref;
}

impl<T> IntoRef for T {
    fn into_ref(self: T) -> Ref { Ref::new() }
}

pub trait IntoArguments {
    fn into_arguments(self) -> Vec<Ref>;
}

impl IntoArguments for () {
    fn into_arguments(self) -> Vec<Ref> { vec![] }
}

impl<T1> IntoArguments for (T1,) {
    fn into_arguments(self) -> Vec<Ref> { vec![self.0.into_ref()] }
}

impl<T1, T2> IntoArguments for (T1, T2,) {
    fn into_arguments(self) -> Vec<Ref> { vec![self.0.into_ref(), self.1.into_ref()] }
}

/******************************************************************************/

impl Value {
    pub const fn new() -> Value { Value{ptr: std::ptr::null_mut(), ind: Index::new()} }

    pub const fn address(&self) -> *mut Void { self.ptr }

    pub const fn index(&self) -> Index { self.ind }

    pub fn method<T: IntoArguments>(&self, name: &str, args: T) -> Value {
        // let mut argv =
        unsafe {rebind_value_method_to_value(self, name.as_ptr(), std::ptr::null_mut(), 0)}
    }

    pub fn holds<T: 'static>(&self) -> bool {
        self.ind == fetch::<T>()
    }

    pub fn get<T>(self) -> T {
        panic!("ok")
    }

    pub fn call<T: IntoArguments>(&self, args: T) -> Value {
        unsafe {rebind_value_call_value(self, std::ptr::null_mut(), 0)}
    }

    pub fn call_ref<T: IntoArguments>(&self, args: T) -> Ref {
        unsafe {rebind_value_call_ref(self, std::ptr::null_mut(), 0)}
    }

    pub fn cast<T: 'static + FromValue>(self) -> T {
        // Option::<T>::new()
        if let v = self.holds::<T>() {
            return self.get::<T>()
        }

        panic!("bad");
    }

    pub fn cast_ref<'a, T: FromValue>(&'a self) -> Option<&'a T> {
        None
    }
}

pub fn invoke<T: IntoArguments>(name: &str, args: T) -> Value {
    lookup(name).call(args)
}


impl Index {
    pub fn name(&self) -> &str {
        unsafe{std::ffi::CStr::from_ptr(rebind_index_name(*self))}.to_str().unwrap()
    }
}

/******************************************************************************/

impl Drop for Value {
    fn drop(&mut self) {
        unsafe{rebind_value_destruct(self);}
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

// impl<T: FromValue> std::convert::TryFrom<Value> for T {
//     fn try_from(self) -> Result<T, Self::Error> {
//         T::from_matching_value(self)
//     }
// }

/******************************************************************************/

// struct TypedValue<T> {
//     base : Value
// }