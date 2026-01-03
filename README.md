# on_demand_garbage_collector
- Achieving temporal safety blocks in C. Essentially creating an on demand
conservative garbage collector, which aims to defend against Use-After-Free
bugs. The idea, is that since most **UAF** bugs occur inside code that handles
user input, we can apply the overhead of a garbage collector to only that
portion of code, thus, not affecting the rest of the program 
- A program can be in two states:
    * temporal unsafety
    * temporal safety
- During temporal safety mode, the **GC** is enabled and all allocations are
redirected to the isolated 'safe' heap
- The 'safe' heap can ONLY be accessed (read/write) during temporal safety
mode. This heap isolation is achieved via [Intel Memory Protection
Keys](https://www.kernel.org/doc/html/latest/core-api/protection-keys.html),
which is a fast method to change the permissions of memory in **user space**

## Concept:
- Developers can wrap code regions that are most critical in Safe Blocks
- Inside these Safe Blocks, using our runtime defense, we can guarantee that no
UAF bug can take place.
- Outside Safe Blocks, there will be no performance penalty, since the program
will follow normal behavior.
```
void foo(...) {
    bar(...);
    safe {
        char *user_input = get_input(...);
        char *retval = parse(user_input);
        process(retval);
    }
    baz(...);
}
```
![Local image](./images/process_image.png)

## Usage:
- macros:
    * ENTER_SAFE_BLOCK
    * EXIT_SAFE_BLOCK
    * EXEMPT(foo) # all allocations made within foo are not tracked by **GC**
- include header "safe_blocks.h"
- run with LD_PRELOAD=/path/to/libruntime.so \<target\>

## Setup:

- ### Docker (**optional**):
    * Use dockerfile found in repository. It installs all the necessary
    libraries, and some useful programs (e.g. vim, gdb). 
    * build image: `docker build -t temp-safety-blks-img .`
    * build container: `docker run --privileged -it --name temp-safety-blks-cnt temp-safety-blks-img`
    * use the `--priviliged` flag to give the container access to intel pkeys

- ### Hardware Support:
    * Hardware needs to support **Intel Memory Protection Keys (pkeys)**

- ### Dependenices:
    * clang
    * mimalloc

- ### Build runtime library/collector:
    * build with `make` in `src/`
    * shared library will be build in `release` or `debug` dir of either
    directory the project is built in

- ### Test collector:
    * run `python test.py` in dir `tests`
