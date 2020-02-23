use capi;
use capi::TypeIndex;

/******************************************************************************/

pub struct Variable {
    pub ptr: *mut capi::CVariable
}

impl Variable {
    pub fn new() -> Variable {
        Variable{ptr: unsafe {capi::rebind_variable_new()}}
    }

    pub fn index(&self) -> TypeIndex {
        unsafe {capi::rebind_variable_type(self.ptr)}
    }
}

impl Clone for Variable {
    fn clone(&self) -> Variable {
        let p = unsafe{capi::rebind_variable_copy(self.ptr)};
        Variable{ptr: p}
    }
}

impl Drop for Variable {
    fn drop(&mut self) {
        unsafe{capi::rebind_destruct(self.ptr)}
    }
}

/******************************************************************************/
