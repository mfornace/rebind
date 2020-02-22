// #[macro_use]
// mod macros;
mod submod;

// #[repr(C)]
// pub struct Variable {
//     pub many: ::std::os::raw::c_int,
//     pub wow: ::std::os::raw::c_char,
// }

// impl Variable {
//     pub fn new() -> Variable { Variable{many: 15, wow: 12} }
// }

/******************************************************************************/

pub enum CVariable {}

/******************************************************************************/

pub struct Variable {
    pub ptr: *mut CVariable
}

/******************************************************************************/

// #[link(name = "librustrebind")]
extern "C" {
    pub fn rebind_add() -> std::os::raw::c_int;

    pub fn rebind_destruct(v: *mut CVariable);

    pub fn rebind_variable_new() -> *mut CVariable;

    pub fn rebind_variable_copy(v: *mut CVariable) -> *mut CVariable;
}

/******************************************************************************/

impl Variable {
    pub fn new() -> Variable {
        Variable{ptr: unsafe {rebind_variable_new()}}
    }
}

impl Clone for Variable {
    fn clone(&self) -> Variable {
        let p = unsafe{rebind_variable_copy(self.ptr)};
        Variable{ptr: p}
    }
}

impl Drop for Variable {
    fn drop(&mut self) {
        unsafe{rebind_destruct(self.ptr)}
    }
}

/******************************************************************************/

pub fn foo1() {

    use submod::foo;
    //   let _ = fmt!("...");
    foo();
    let x = unsafe {
        rebind_add()
    };
    println!("Hello {}", x);

    let v = Variable::new();
    let v2 = v.clone();
    let v3 = v;
    // println!("Hello {}", v.many);

    // unsafe {
    //     rebind_destruct(&mut v as *mut Variable);
    // }

    // println!("Hello {}", v.many);
}