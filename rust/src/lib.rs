// #[macro_use]
// mod macros;
// mod submod;

mod submod;

pub mod variable;
// pub mod function;
pub mod capi;

pub use variable::Value;
// pub use function::Function;

// #[repr(C)]
pub struct Goo {
    base: Value
    // pub many: ::std::os::raw::c_int,
    // pub wow: ::std::os::raw::c_char,
}

/******************************************************************************/

static mut blah2: Value = Value::new();

pub unsafe fn init() {
    blah2 = Value::new();
}

pub fn fun(x: i32, y: f64) -> f64 {
    // static blah: i32 = 1;
    // static mut blah: Value = Value::new();
    // static blah: String = "aaa".to_string();
    unsafe {blah2.call(())}.cast::<f64>().unwrap()
}

/******************************************************************************/

impl Goo {
    pub fn x(&self) -> i32 { self.base.method(".x", ()).cast::<i32>().unwrap() }
}

/******************************************************************************/

pub fn foo1() {

    use submod::foo;
    //   let _ = fmt!("...");
    foo();
    // let x = unsafe {
    //     capi::rebind_add()
    // };
    // println!("Hello {}", x);

    let v = Value::new();
    let _v2 = v.clone();
    let v3 = v;

    let t = v3.index();
    println!("OK {}", t.name());
    // println!("Hello {}", v.many);

    // unsafe {
    //     rebind_destruct(&mut v as *mut Value);
    // }

    // println!("Hello {}", v.many);
}