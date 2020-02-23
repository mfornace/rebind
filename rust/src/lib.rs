// #[macro_use]
// mod macros;
// mod submod;

mod submod;

pub mod variable;
pub mod function;
pub mod capi;

pub use variable::Variable;
pub use function::Function;

// #[repr(C)]
// pub struct Variable {
//     pub many: ::std::os::raw::c_int,
//     pub wow: ::std::os::raw::c_char,
// }

/******************************************************************************/

pub fn foo1() {

    use submod::foo;
    //   let _ = fmt!("...");
    foo();
    let x = unsafe {
        capi::rebind_add()
    };
    println!("Hello {}", x);

    let v = Variable::new();
    let v2 = v.clone();
    let v3 = v;

    let t = v3.index();
    println!("OK {}", t.name());
    // println!("Hello {}", v.many);

    // unsafe {
    //     rebind_destruct(&mut v as *mut Variable);
    // }

    // println!("Hello {}", v.many);
}