use std::collections::HashMap;
use std::any::TypeId;

use capi;
pub use capi::{Index, Table, Value, Ref, CFunction};

/******************************************************************************/

pub fn create_table<T>() -> &'static Table {
    unsafe{ &*(capi::rebind_Table_insert()) }
}

pub fn fetch<T: 'static>() -> &'static Table {
    let mut tables: HashMap<TypeId, &'static Table> = HashMap::new();

    tables.entry(TypeId::of::<T>()).or_insert_with(|| create_table::<T>())
}

/******************************************************************************/

impl Ref {
    // pub fn new() -> Ref {
    //     Ref{ptr: std::ptr::null_mut(), ind: std::ptr::null(), qual: 0} // Ref{ptr: unsafe {capi::rebind_function_new()}}
    // }

    pub fn index(&self) -> Index {
        unsafe{capi::rebind_Ref_index(self)}
    }
}

/******************************************************************************/

impl Value {
    pub fn index(&self) -> Index {
        unsafe{capi::rebind_Value_index(self)}
    }
}

impl Drop for Value {
    fn drop(&mut self) {
        unsafe{capi::rebind_Value_destruct(self);}
    }
}

/******************************************************************************/

impl Index {
    pub fn name(&self) -> &str {
        unsafe{std::ffi::CStr::from_ptr(capi::rebind_Index_name(*self))}.to_str().unwrap()
    }
}

/******************************************************************************/

impl Clone for Value {
    fn clone(&self) -> Value {
        let mut v = Value::new();
        if unsafe{capi::rebind_Value_copy(&mut v, self) == 0} { panic!("copy failed!"); }
        v
    }
}

/******************************************************************************/
