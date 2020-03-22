// use std::os::raw::{c_void, c_uchar, c_int};

/******************************************************************************/

pub type Bool = std::os::raw::c_int;
pub type Void = std::os::raw::c_void;
pub type Qual = std::os::raw::c_uchar;
pub type Char = std::os::raw::c_char;

/******************************************************************************/

#[repr(C)]
pub struct Table { _private: [u8; 0] }

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Index { ptr: *const Table }

impl Index {
    pub fn new() -> Index { Index{ptr: std::ptr::null()} }
}

/******************************************************************************/

pub enum Qualifier { Const, Lvalue, Rvalue }

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Ref {
    ptr: *mut Void,
    ind: Index,
    qual: Qual
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

impl Value {
    pub fn new() -> Value { Value{ptr: std::ptr::null_mut(), ind: Index::new()} }

    pub fn address(&self) -> *mut Void { self.ptr }

    pub fn index(&self) -> Index { self.ind }
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

    /**************************************************************************/

    pub fn rebind_value_destruct(v: &mut Value);

    pub fn rebind_value_copy(v: &mut Value, o: &Value) -> Bool;

    pub fn rebind_value_move(v: &mut Value, o: &mut Value);

    /**************************************************************************/

    pub fn rebind_index_name(v: Index) -> *const Char;

    /**************************************************************************/

    pub fn rebind_function_new() -> *mut CFunction;

    pub fn rebind_function_copy(f: *mut CFunction) -> *mut CFunction;

    pub fn rebind_function_call(f: *mut CFunction, v: *mut Value, n: Qual) -> *mut Value;

    pub fn rebind_function_destruct(f: *mut CFunction);
}

/******************************************************************************/


