// Example custom build script.
fn main() {
    // Tell Cargo that if the given file changes, to rerun this build script.
    // println!("heyoo");
    println!("cargo:rustc-link-search=/Users/Mark/Projects/nupack/external/rebind/build");
    println!("cargo:rustc-link-lib=dylib=rustrebind");
    // println!("cargo:rerun-if-changed=src/hello.c");
}