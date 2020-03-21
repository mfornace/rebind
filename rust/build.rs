// Example custom build script.
fn main() {
    // Tell Cargo that if the given file changes, to rerun this build script.
    // println!("heyoo");
    println!("cargo:rustc-link-search=/Users/Mark/Projects/rebind/build");
    println!("cargo:rustc-link-lib=dylib=rebind_rust");
    // println!("cargo:rerun-if-changed=src/hello.c");
}