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