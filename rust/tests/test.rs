// #[macro_use]
// mod macros;
// mod submod;
extern crate rebind;
use rebind::{Value, invoke, initialize};

/******************************************************************************/

pub struct Goo {
    base: Value
}

impl AsRef<Value> for Goo {
    fn as_ref(&self) -> &Value { &self.base }
}

impl std::ops::Deref for Goo {
    type Target = Value;
    fn deref(&self) -> &Value { &self.base }
}

impl std::ops::DerefMut for Goo {
    fn deref_mut(&mut self) -> &mut Value { &mut self.base }
}

impl AsMut<Value> for Goo {
    fn as_mut(&mut self) -> &mut Value { &mut self.base }
}

impl Goo {
    pub fn x(&self) -> i32 { self.method(".x", ()).cast::<i32>() }
}

/******************************************************************************/

// this could be easily macroed away, I think. Only downside I think is the lookup
// ...that's not that big a deal compared to other lookups though...
pub fn lookup_solution() -> f64 {
    invoke("easy", ()).cast::<f64>()
}

/******************************************************************************/

#[derive(Debug)]
struct A {
    x: f64
}

#[derive(Debug)]
struct B<'a> {
    b: &'a mut f64
}

impl A {
    fn get_x(&mut self) -> &mut f64 { &mut self.x }
}

fn mutate(f: &mut f64) {
    *f = 2.0;
}

fn mutate2(f: &mut f64, g: &mut f64) {
    *f = 2.0;
}

#[test]
pub fn test_ref() {
    let mut a = A{x: 1.0};
    a.x = 2.0;
    println!("{:?}", a);
    *a.get_x() = 3.0;
    println!("{:?}", a);
    // mutate2(&mut a.x, &mut a.x);
}

/******************************************************************************/

#[test]
pub fn test_fun() {
    initialize();
    println!("test fun");
    lookup_solution();
}

use rebind::*;

pub struct Type<T> {
    marker: std::marker::PhantomData::<T>
}

impl<T> Type::<T> {
    fn new() -> Type::<T> {
        Type::<T>{ marker: std::marker::PhantomData }
    }
}

/******************************************************************************/

trait CopyC {
    fn copy_fn(&self) -> CopyT;
}

trait NoCopyC {
    fn copy_fn(&self) -> CopyT;
}

/******************************************************************************/

trait ToValueC {
    fn to_value_fn(&self) -> ToValueT;
}

trait NoToValueC {
    fn to_value_fn(&self) -> ToValueT;
}

/******************************************************************************/

trait ToRefC {
    fn to_ref_fn(&self) -> ToRefT;
}

trait NoToRefC {
    fn to_ref_fn(&self) -> ToRefT; // { std::ptr::null() as ToRefT }
}

/******************************************************************************/

trait AssignIfC {
    fn work(&self) -> AssignIfT;
    fn assign_if_fn(&self) -> AssignIfT { self.work() }
}

trait NoAssignIfC {
    fn assign_if_fn(&self) -> AssignIfT;
}

/******************************************************************************/

// extern "C" fn no_copy<T>(s: *const Void) -> *mut Void {
//     println!("no_copy {}", std::any::type_name::<T>());
//     std::ptr::null_mut()
// }

extern "C" fn yes_copy<T: Clone>(s: *const Void) -> *mut Void {
    println!("yes_copy {}", std::any::type_name::<T>());
    let copy = unsafe { (*(s as *const T)).clone() };
    let x = Box::<T>::new(copy);
    Box::into_raw(x) as *mut Void
}

impl<T> NoCopyC for &Type::<T> {
    fn copy_fn(&self) -> CopyT { None }
}

impl<T: Clone> CopyC for Type::<T> {
    fn copy_fn(&self) -> CopyT { Some(yes_copy::<T>) }
}

/******************************************************************************/

struct Blah {}

#[test]
pub fn test_fun3() {
    let t = Type::<i32>::new();
    // let blah: () = (&t).copyc();
}

/******************************************************************************/

pub fn foo1() {

    // use submod::foo;
    //   let _ = fmt!("...");
    // foo();
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