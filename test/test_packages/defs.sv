
`define ADD(x, y) ((x) + (y))
`define MUL(x, y) ((x) * (y))
`define MACRO_FUNC `ADD(`MUL(2, 3), 4)
`define DW_MACRO config_pkg::DATA_WIDTH
`define STATE_E_MACRO config_pkg::state_e
