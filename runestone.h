#ifndef RUNESTONE_H
#define RUNESTONE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define RS_COLOR_RESET "\033[0m"
#define RS_COLOR_RED "\033[31m"
#define RS_COLOR_GREEN "\033[32m"
#define RS_COLOR_YELLOW "\033[33m"
#define RS_COLOR_BLUE "\033[34m"
#define RS_COLOR_MAGENTA "\033[35m"
#define RS_COLOR_CYAN "\033[36m"
#define RS_COLOR_WHITE "\033[37m"
#define RS_COLOR_BOLD "\033[1m"
#define RS_COLOR_UNDERLINE "\033[4m"

// TODO: should probably be dynamically resizable
#define RS_MAX_INSTR 1024
#define RS_MAX_BB 1024
#define RS_MAX_REGS 256

#define RS_REGMAP_INIT_CAPACITY 16

typedef uint8_t rs_register_t;

#define RS_REG_SPILL UINT8_MAX

typedef struct {
  enum {
    RS_OPERAND_TYPE_NULL,
    RS_OPERAND_TYPE_INT64,
    RS_OPERAND_TYPE_ADDR,
    RS_OPERAND_TYPE_REG,
    RS_OPERAND_TYPE_BB,
  } type;
  union {
    int64_t int64;
    size_t addr;
    uint8_t vreg;
    size_t bb_id;
  };
} rs_operand_t;

#define RS_TEMPORARY_VREG UINT8_MAX

#define RS_OPERAND_NULL ((rs_operand_t){.type = RS_OPERAND_TYPE_NULL})
#define RS_OPERAND_INT64(value)                                                \
  ((rs_operand_t){.type = RS_OPERAND_TYPE_INT64, .int64 = (value)})
#define RS_OPERAND_ADDR(value)                                                 \
  ((rs_operand_t){.type = RS_OPERAND_TYPE_ADDR, .addr = (value)})
#define RS_OPERAND_REG(value)                                                  \
  ((rs_operand_t){.type = RS_OPERAND_TYPE_REG, .vreg = (value)})
#define RS_OPERAND_BB(value)                                                   \
  ((rs_operand_t){.type = RS_OPERAND_TYPE_BB, .bb_id = (value)})

typedef struct {
  rs_register_t reg;
  uint8_t vreg;
  ptrdiff_t start, end;
} rs_lifetime_t;

typedef enum {
  RS_OPCODE_MOVE,
  RS_OPCODE_COPY,

  RS_OPCODE_LOAD,
  RS_OPCODE_STORE,

  RS_OPCODE_ADD,
  RS_OPCODE_SUB,
  RS_OPCODE_MULT,
  RS_OPCODE_DIV,

  RS_OPCODE_RET,
  RS_OPCODE_BR,
} rs_opcode_t;

static const char *rs_opcode_names[] = {
    [RS_OPCODE_MOVE] = "move", [RS_OPCODE_COPY] = "copy",

    [RS_OPCODE_LOAD] = "load", [RS_OPCODE_STORE] = "store",

    [RS_OPCODE_ADD] = "add",   [RS_OPCODE_SUB] = "sub",
    [RS_OPCODE_MULT] = "mult", [RS_OPCODE_DIV] = "div",

    [RS_OPCODE_RET] = "ret",   [RS_OPCODE_BR] = "br",
};

typedef struct {
  rs_opcode_t opcode;
  rs_operand_t dst, src1, src2;
} rs_instr_t;

typedef struct {
  /*
   * The name of the block
   **/
  char *name;

  /*
   * List of all the instructions.
   **/
  rs_instr_t *instrs;
  size_t instrs_count;
} rs_basic_block_t;

/*
 * A map implementation for the register map, used for
 * register allocation. The key is the virtual register
 * number, and the value is the hardware register number.
 */
typedef struct {
  size_t key;
  rs_register_t value;
} rs_map_entry_t;
typedef struct {
  rs_map_entry_t *entries;
  size_t count;
  size_t capacity;
} rs_register_map_t;

#define RS_TARGETS                                                             \
  RS_TARGET(x86_64_linux_nasm, X86_64_LINUX_NASM, 14, "rax", "rbx", "rcx",     \
            "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13",       \
            "r14", "r15")                                                      \
  RS_TARGET(aarch64_macos_gas, AARCH64_MACOS_GAS, 17, "x9", "x10", "x11",      \
            "x12", "x13", "x14", "x15", "x19", "x20", "x21", "x22", "x23",     \
            "x24", "x25", "x26", "x27", "x28")

#define RS_TARGET(lower, _, count, ...)                                        \
  static const char *rs_target_##lower##_reg_names[] = {__VA_ARGS__};
RS_TARGETS
#undef RS_TARGET

/*
 * Used to specify the output target.
 *
 * Format: <ARCH>-<OS>-<ASSEMBLER>
 **/
typedef enum {
#define RS_TARGET(_1, x, _2, ...) RS_TARGET_##x,
  RS_TARGETS
#undef RS_TARGET
      RS_COUNT_TARGETS,
} rs_target_t;

typedef struct {
  /*
   * The currently selected target.
   **/
  rs_target_t target;

  /*
   * List of all the basic blocks.
   **/
  rs_basic_block_t **basic_blocks;
  size_t basic_blocks_count;

  /*
   * The index of the current basic block.
   *
   *   -1 if no basic block is selected.
   **/
  ptrdiff_t current_basic_block;

  /*
   * List of all the *virtual* register lifetimes. Used for
   * register alloction.
   **/
  rs_lifetime_t lifetimes[256];

  /*
   * List of all usable *hardware* registers. Used for
   * register allocation. This signifies which regsiters are
   * free and which ones are used, `true` meaning that it

   **/
  bool *register_pool;

  /*
   * The register map, used for register allocation.
   **/
  rs_register_map_t register_map;

  /*
   * The size of the stack.
   **/
  size_t stack_size;

  /*
   * The next destination virtual register
   **/
  size_t next_dst_vreg;
} rs_t;

void rs_init(rs_t *rs, rs_target_t target);
void rs_free(rs_t *rs);

/*
 * If name is NULL, a name will be automatically assigned.
 **/
size_t rs_append_basic_block(rs_t *rs, const char *name);
void rs_position_at_basic_block(rs_t *rs, size_t block_id);

void rs_build_instr(rs_t *rs, rs_instr_t instr);
rs_operand_t rs_build_move(rs_t *rs, rs_operand_t src);
rs_operand_t rs_build_copy(rs_t *rs, rs_operand_t src);
rs_operand_t rs_build_load(rs_t *rs, rs_operand_t src);
void rs_build_store(rs_t *rs, rs_operand_t src1, rs_operand_t src2);
rs_operand_t rs_build_add(rs_t *rs, rs_operand_t src1, rs_operand_t src2);
rs_operand_t rs_build_sub(rs_t *rs, rs_operand_t src1, rs_operand_t src2);
rs_operand_t rs_build_mult(rs_t *rs, rs_operand_t src1, rs_operand_t src2);
rs_operand_t rs_build_div(rs_t *rs, rs_operand_t src1, rs_operand_t src2);
void rs_build_ret(rs_t *rs, rs_operand_t src);
void rs_build_br(rs_t *rs, rs_operand_t src);

bool rs_instr_is_terminator(rs_instr_t instr);

rs_register_t rs_get_free_register(rs_t *rs);
rs_register_t rs_get_register(rs_t *rs, size_t vreg);

size_t rs_get_register_count(rs_target_t target);
const char **rs_get_register_names(rs_target_t target);

void rs_analyze_lifetimes(rs_t *rs);

void rs_finalize(rs_t *rs);

void rs_dump_instr(rs_t *rs, FILE *fp, rs_instr_t instr);
void rs_dump(rs_t *rs, FILE *fp);

void rs_generate(rs_t *rs, FILE *fp);

#define RS_TARGET(lower, ...)                                                  \
  void rs_generate_##lower(rs_t *rs, FILE *fp);                                \
  void rs_generate_instr_##lower(rs_t *rs, FILE *fp, rs_instr_t instr);        \
  void rs_generate_operand_##lower(rs_t *rs, FILE *fp, rs_operand_t operand,   \
                                   bool dereference);
RS_TARGETS
#undef RS_TARGET

void rs_regmap_init(rs_register_map_t *map);
void rs_regmap_free(rs_register_map_t *map);
void rs_regmap_insert(rs_register_map_t *map, size_t key, rs_register_t value);
rs_register_t rs_regmap_get(rs_register_map_t *map, size_t key);
bool rs_regmap_contains(rs_register_map_t *map, size_t key);
void rs_regmap_remove(rs_register_map_t *map, size_t key);

#endif // RUNESTONE_H
