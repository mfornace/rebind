
pub type Bool = std::os::raw::c_int;
pub type Uint = std::os::raw::c_uint;
pub type Size = std::os::raw::c_ulonglong;
pub type Void = std::os::raw::c_void;
pub type Qual = std::os::raw::c_uchar;
pub type Char = std::os::raw::c_char;

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