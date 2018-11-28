YOPL: Your Own Programming Language
==================================
Tired of your programming language? Why not build your own!

YOPL allows you to define your own programming language.
Using an __LL(\*)__ parser, fast __RE2__ regular expression matching, and an __LLVM__ backend, you can have a new programming language in notime.
The LL(\*) parser can also be used on its own, e.g. to create your own config file reader.
    
Progress
========
- LL(\*) parser with RE2 matching is working
- LLVM backend is work in progress

Building
=======
YOPL depends on RE2 from Google, a fast regular expression matcher. It can be installed throught the package manager on most distributions, e.g. on debian it can be installed using `sudo apt-get install libre2-dev`

Then build using the makefile:
`> make`

    
Examples
=======
Examples are being added soon

Author
======
Marijn Stollenga

Licence
============    
Licence for all files in this repository: MPLv2.0, See LICENCE file

