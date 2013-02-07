perf to callgrind converter
==========================

Features:
- multiple event support
- annotated source code with inlined functions
- annotated assembly
- annotated function list
- kernel annotations

Note that callgraphs and kernel module annotations are not supported yet. 
The converter can be invoked as follows: perf convert -i perf.data -o output_filename.
