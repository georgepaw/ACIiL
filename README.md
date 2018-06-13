# ACRIiL
Automatic Checkpoint/Restart Insertion in LLVM (ACRIiL)

ECC methods still require Checkpoint/Restart as Detectable Uncorrectable Errors can occur.
Current application level checkpointing frameworks, such as Scalable Checkpoint/Restart or Fault Tolerance Interface, provide features, like multi-level checkpointing, that make CR more scalable. The downside of these frameworks is that they require modifications to the application code which might be challenging to implement for some HPC users.

We introduce Automatic Checkpoint/Restart Insertion in LLVM (ACRIiL), a compile-time tool which attempts to solve this problem by finding safe and optimal checkpoint locations. ACRIiL then inserts calls to the CR library via an interface which is implemented using one of the checkpointing frameworks.
This tool is still in early development stages and future work includes support for multi-threading, offloading devices and MPI.

This was a (failed) project that I did for few months during my PhD, but it served as a good learning exercise of LLVM.

This does work with simple programs (check the `acriil_dyn` folder), does not support structs, and pointer aliasing info is a bit iffy.
No support for multithreading or MPI.
This is an LTO pass so a compatible linker is required.
At the moment the interface is implemented using my own checkpointing framework that just saves to disk.

Might be useful if anyone is intrested in traversing the CFG etc.
