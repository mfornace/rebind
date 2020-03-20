
/******************************************************************************/

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Ref { _private: [u8; 24] }

#[repr(C)]
#[derive(Clone)]
#[repr(C)] pub struct Value { _private: [u8; 16] }

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Index { _private: [u8; 16] }

#[repr(C)]
pub struct CFunction { _private: [u8; 0] }

/******************************************************************************/

// #[link(name = "librustrebind")]
extern "C" {

    pub fn rebind_ref_index(v: *Ref) -> Index;

    pub fn rebind_ref_has_value(v: *Ref) -> std::os::raw::c_bool;



    pub fn rebind_value_index(v: *Value) -> Index;

    pub fn rebind_value_has_value(v: *Value) -> std::os::raw::c_bool;

    pub fn rebind_value_construct(v: *mut Value);

    pub fn rebind_value_destruct(v: *mut Value);

    pub fn rebind_value_copy(v: *mut Value, o: *Value) -> std::os::raw::c_bool;

    pub fn rebind_value_move(v: *mut Value, o: *mut Value);



    pub fn rebind_index_name(v: Index) -> *const std::os::raw::c_char;



    pub fn rebind_function_new() -> *mut CFunction;

    pub fn rebind_function_copy(f: *mut CFunction) -> *mut CFunction;

    pub fn rebind_function_call(f: *mut CFunction, v: *mut Value, n: std::os::raw::c_uint) -> *mut Value;

    pub fn rebind_function_destruct(f: *mut CFunction);
}

/******************************************************************************/

impl Ref {
    pub fn new() -> Ref {
        // Ref{ptr: unsafe {capi::rebind_function_new()}}
    }
}


/******************************************************************************/

impl Value {
    pub fn new() -> Value {
        // Ref{ptr: unsafe {capi::rebind_function_new()}}
    }
}

/******************************************************************************/

impl Index {
    pub fn name(&self) -> &str {
        unsafe{std::ffi::CStr::from_ptr(rebind_index_name(*self))}.to_str().unwrap()
    }
}

/******************************************************************************/

impl Clone for Value {
    pub fn clone(&self) -> Value {
        let mut v = Value::new();
        if not unsafe{rebind_value_copy(v, self)} { panic!("copy failed!"); }
        v
    }
}

/******************************************************************************/

