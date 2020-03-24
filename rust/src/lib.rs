// #[macro_use]
// mod macros;
// mod submod;

mod submod;

pub mod variable;
// pub mod function;
pub mod capi;

pub use variable::Value;
// pub use function::Function;

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

// this is pretty bad, global variables plus an init that needs to be called
static mut static_solution: Value = Value::new();

pub unsafe fn init() {
    static_solution = Value::new();
}

pub fn static_fun(x: i32, y: f64) -> f64 {
    // static blah: i32 = 1;
    // static mut blah: Value = Value::new();
    // static blah: String = "aaa".to_string();
    unsafe {static_solution.call(())}.cast::<f64>()
}

/******************************************************************************/

// this is...sort of annoying...need to pass around state, only good in getting rid of hash lookup really...
pub struct StateSolution {
    bases: [Value; 2]
}

impl StateSolution {
    pub fn new() -> StateSolution { StateSolution{bases: [Value::new(), Value::new()]} }
    pub fn method1(&self) -> f64 { self.bases[0].call(()).cast::<f64>() }
    pub fn method2(&self) -> f64 { self.bases[1].call(()).cast::<f64>() }
}

/******************************************************************************/

// this could be easily macroed away, I think. Only downside I think is the lookup
// ...that's not that big a deal compared to other lookups though...
pub fn lookup_solution() -> f64 {
    capi::invoke("myfun", ()).cast::<f64>()
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