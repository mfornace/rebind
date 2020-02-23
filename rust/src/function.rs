use capi;
use variable::Variable;

/******************************************************************************/

pub struct Function {
    pub ptr: *mut capi::CFunction
}

impl Function {
    pub fn new() -> Function {
        Function{ptr: unsafe {capi::rebind_function_new()}}
    }

    pub fn call(&self, args: Vec<Variable>) -> Variable {
        Variable{ptr: unsafe{capi::rebind_function_call(self.ptr, args[0].ptr, 1)}}
    }
}

impl Clone for Function {
    fn clone(&self) -> Function {
        let p = unsafe{capi::rebind_function_copy(self.ptr)};
        Function{ptr: p}
    }
}

impl Drop for Function {
    fn drop(&mut self) {
        unsafe{capi::rebind_function_destruct(self.ptr)}
    }
}

/******************************************************************************/
