// Bootstrap code
//
// This is invoked once the C++ functions have been bound into various
// pieces of the global namespace. It is up to this module to provide nice
// JavaScript wrappers on top of the low-level functions.
//
// This file is allowed (and expected) to modify the global namespace.

console = {
    log : function(s) {
        sys.write(1, s + '\n');
    },

    error : function(s) {
        sys.write(2, s + '\n');
    }
};
