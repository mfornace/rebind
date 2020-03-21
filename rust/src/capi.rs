// use std::os::raw::{c_void, c_uchar, c_int};

/******************************************************************************/

type Bool = std::os::raw::c_int;
type Void = std::os::raw::c_void;
type Qual = std::os::raw::c_uchar;
type Char = std::os::raw::c_char;

/******************************************************************************/

#[repr(C)]
pub struct Table { pub data: [u8; 0] }

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Ref {
    ptr: *mut Void,
    ind: *const Table,
    qual: Qual
}

impl Ref {
    pub fn new() -> Ref { Ref{ptr: std::ptr::null_mut(), ind: std::ptr::null(), qual: 0} }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Index { pub data: [f64; 2] }

/******************************************************************************/

#[repr(C)]
pub struct CFunction { pub data: [u8; 0] }

#[repr(C)]
pub struct Scope { pub data: [u8; 0] }

#[repr(C)]
pub struct Value {
    ptr: *mut Void,
    ind: *const Table,
}

impl Value {
    pub fn new() -> Value { Value{ptr: std::ptr::null_mut(), ind: std::ptr::null()} }
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
    pub fn rebind_Table_insert() -> *mut Table;

    pub fn rebind_Table_set(table: *mut Table,
        index: Index,
        destroy: DestroyT,
        copy: CopyT,
        to_value: ToValueT,
        to_ref: ToRefT,
        assign_if: AssignIfT,
        from_ref: FromRefT
    );

    pub fn rebind_Table_add_method(table: &mut Table, name: *const Char, method: &CFunction);

    pub fn rebind_Table_add_base(table: &mut Table, base: Index);

    /**************************************************************************/

    pub fn rebind_Ref_index(v: &Ref) -> Index;

    pub fn rebind_Ref_has_value(v: &Ref) -> Bool;

    /**************************************************************************/

    pub fn rebind_Value_index(v: &Value) -> Index;

    pub fn rebind_Value_has_value(v: &Value) -> Bool;

    pub fn rebind_Value_destruct(v: &mut Value);

    pub fn rebind_Value_copy(v: &mut Value, o: &Value) -> Bool;

    pub fn rebind_Value_move(v: &mut Value, o: &mut Value);

    /**************************************************************************/

    pub fn rebind_Index_name(v: Index) -> *const Char;

    /**************************************************************************/

    pub fn rebind_Function_new() -> *mut CFunction;

    pub fn rebind_Function_copy(f: *mut CFunction) -> *mut CFunction;

    pub fn rebind_Function_call(f: *mut CFunction, v: *mut Value, n: Qual) -> *mut Value;

    pub fn rebind_Function_destruct(f: *mut CFunction);
}

/******************************************************************************/


