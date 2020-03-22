use std::collections::HashMap;
use std::any::TypeId;

use capi;
pub use capi::{Index, Table, Value, Ref, CFunction};

/******************************************************************************/

pub fn create_table<T>() -> Index {
    unsafe{ capi::rebind_table_insert() }
}


pub fn fetch<T: 'static>() -> Index {
    let mut tables: HashMap<TypeId, Index> = HashMap::new();

    *tables.entry(TypeId::of::<T>()).or_insert_with(|| create_table::<T>())
}

/******************************************************************************/

impl Index {
    pub fn name(&self) -> &str {
        unsafe{std::ffi::CStr::from_ptr(capi::rebind_index_name(*self))}.to_str().unwrap()
    }
}

/******************************************************************************/

impl Drop for Value {
    fn drop(&mut self) {
        unsafe{capi::rebind_value_destruct(self);}
    }
}

/******************************************************************************/

impl Clone for Value {
    fn clone(&self) -> Value {
        let mut v = Value::new();
        if unsafe{capi::rebind_value_copy(&mut v, self) == 0} { panic!("copy failed!"); }
        v
    }
}

/******************************************************************************/
