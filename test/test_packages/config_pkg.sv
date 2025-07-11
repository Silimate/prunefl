package config_pkg;
    parameter int DATA_WIDTH = 4;
    
    typedef enum logic [1:0] {
        IDLE,
        BUSY,
        DONE,
        ERROR
    } state_e;
endpackage
