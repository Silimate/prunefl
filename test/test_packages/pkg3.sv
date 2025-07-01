
package pkg3;
    import pkg1::f1;
    class BaseClass;
        virtual function int compute(int x);
            return `ADD(x, 1);
        endfunction
    endclass

    class DerivedClass extends BaseClass;
        function int compute(int x);
            return f1(super.compute(x)); // uses pkg1::f1 and macro
        endfunction
    endclass
endpackage
