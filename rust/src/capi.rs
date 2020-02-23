
/******************************************************************************/

#[repr(C)] pub struct CVariable { _private: [u8; 0] }

#[repr(C)] pub struct CFunction { _private: [u8; 0] }

#[repr(C)]
#[derive(Clone, Copy)] 
pub struct TypeIndex { _private: [u8; 16] }

/******************************************************************************/

// #[link(name = "librustrebind")]
extern "C" {
    pub fn rebind_add() -> std::os::raw::c_int;

    pub fn rebind_destruct(v: *mut CVariable);

    pub fn rebind_variable_new() -> *mut CVariable;

    pub fn rebind_variable_copy(v: *mut CVariable) -> *mut CVariable;
    
    pub fn rebind_variable_type(v: *mut CVariable) -> TypeIndex;
    


    pub fn rebind_type_index_name(v: TypeIndex) -> *const std::os::raw::c_char;
    


    pub fn rebind_function_new() -> *mut CFunction;
    
    pub fn rebind_function_copy(f: *mut CFunction) -> *mut CFunction;

    pub fn rebind_function_call(f: *mut CFunction, v: *mut CVariable, n: std::os::raw::c_uint) -> *mut CVariable;
    
    pub fn rebind_function_destruct(f: *mut CFunction);
}

impl TypeIndex {
    pub fn name(&self) -> &str {
        unsafe{std::ffi::CStr::from_ptr(rebind_type_index_name(*self))}.to_str().unwrap()
    }
}

/******************************************************************************/