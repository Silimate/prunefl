
package pkg2;
    import pkg1::f1;
    function int f2(int x);
        return f1(`MUL(x, 2));
    endfunction
endpackage
