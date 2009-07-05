#ifndef ASM_X86_64_H_NNNVOAL1
#define ASM_X86_64_H_NNNVOAL1

/*
	XXX: DO NOT include this file in exposed headers, as it pollutes the symbol space.
*/

#include "snow/basic.h"
#include "snow/intern.h"
#include "snow/linkbuffer.h"

typedef enum RM_MODE {
	RM_ADDRESS = 0,
	RM_ADDRESS_DISP8 = 1,
	RM_ADDRESS_DISP32 = 2,
	RM_REGISTER = 3
} RM_MODE;

typedef enum REX_FLAGS {
	NO_REX = 0,
	REX_EXTEND_RM = 1,
	REX_EXTEND_SIB_INDEX = 1 << 1,
	REX_EXTEND_REG = 1 << 2,
	REX_WIDE_OPERAND = 1 << 3,
} REX_FLAGS;

typedef enum COND {
	CC_OVERFLOW = 0,
	CC_NOT_OVERFLOW = 1,
	CC_BELOW = 2,
	CC_CARRY = 2,
	CC_NOT_BELOW = 3,
	CC_NOT_CARRY = 3,
	CC_ZERO = 4,
	CC_EQUAL = 4,
	CC_NOT_ZERO = 5,
	CC_NOT_EQUAL = 5,
	CC_NOT_ABOVE = 6,
	CC_ABOVE = 7,
	CC_SIGN = 8,
	CC_NOT_SIGN = 9,
	CC_PARITY_EVEN = 0xA,
	CC_PARITY_ODD = 0xB,
	CC_LESS = 0xC,
	CC_GREATER_EQUAL = 0xD,
	CC_LESS_EQUAL = 0xE,
	CC_GREATER = 0xF
} COND;

static const byte BASE_REX = 0x40 | REX_WIDE_OPERAND;

// sizeof(Op) must be <= 8
// This struct does not support instructions that take both an immediate and a displacement. Snow doesn't need them.
typedef struct SnOp {
	union {
		signed imm : 32;
		signed disp : 32;
	};
	
	unsigned imm_size : 27;
	
	bool address : 1;
	bool ext : 1;
	unsigned reg : 3;
} SnOp;


static inline SnOp make_op(unsigned reg, bool ext, bool address, signed imm)
{
	SnOp op;
	op.reg = reg;
	op.ext = ext;
	op.address = address;
	op.imm_size = 0;
	op.imm = imm;
	return op;
}

static inline SnOp make_address(SnOp reg, signed disp)
{
	reg.address = true;
	reg.disp = disp;
	reg.imm_size = disp > 127 || disp <= -128 ? 4 : 1;
	return reg;
}

static inline SnOp make_immediate(signed imm, unsigned bytes)
{
	SnOp op;
	op.reg = 0;
	op.ext = 0;
	op.address = false;
	op.imm_size = bytes;
	op.imm = imm;
	return op;
}

static inline SnOp make_opcode_ext(byte ext) {
	return make_op(ext, false, false, 0);
}

#define REGISTER(index, ext) (make_op(index, ext, false, 0))
#define ADDRESS(reg, disp) (make_address(reg, disp))
#define IMMEDIATE(imm, size) (make_immediate(imm, size))

#define RAX REGISTER(0, false)
#define RCX REGISTER(1, false)
#define RDX REGISTER(2, false)
#define RBX REGISTER(3, false)
#define RSP REGISTER(4, false)
#define RBP REGISTER(5, false)
#define RSI REGISTER(6, false)
#define RDI REGISTER(7, false)
#define R8  REGISTER(0, true)
#define R9  REGISTER(1, true)
#define R10 REGISTER(2, true)
#define R11 REGISTER(3, true)
#define R12 REGISTER(4, true)
#define R13 REGISTER(5, true)
#define R14 REGISTER(6, true)
#define R15 REGISTER(7, true)


static inline void emit(SnLinkBuffer* lb, byte b) {
	snow_linkbuffer_push(lb, b);
}

static inline byte rex_for_rm(SnOp rm) {
	return rm.ext * REX_EXTEND_RM;
}

static inline byte rex_for_reg(SnOp reg) {
	return reg.ext * REX_EXTEND_REG;
}

static inline byte rex_for_operands(SnOp reg, SnOp rm) {
	return rex_for_reg(reg) | rex_for_rm(rm);
}

static inline RM_MODE mod_for_operand(SnOp addr) {
	if (!addr.address)
		return RM_REGISTER;
		
	RM_MODE mod;
	switch (addr.imm_size) {
		case 0:
			mod = RM_ADDRESS;
			break;
		case 1:
			mod = RM_ADDRESS_DISP8;
			break;
		case 4:
			mod = RM_ADDRESS_DISP32;
			break;
		default:
			ASSERT(false && "Wrong displacement size!");
	}
	
	if (mod == RM_ADDRESS && addr.reg == RBP.reg && !addr.ext) {
		return RM_ADDRESS_DISP32;
	}
	return mod;
}

static inline void emit_rex(SnLinkBuffer* lb, byte rex) {
	if (rex)
		emit(lb, BASE_REX | rex);
}

static inline void emit_imm(SnLinkBuffer* lb, SnOp op) {
	unsigned i;
	signed imm = op.imm;
	for (i = 0; i < op.imm_size; ++i) {
		emit(lb, ((byte*)&imm)[i]);
	}
}

static inline void emit_disp(SnLinkBuffer* lb, SnOp rm) {
	RM_MODE mod = mod_for_operand(rm);
	if (mod != RM_REGISTER) {
		emit_imm(lb, rm);
	}
}

static inline void emit_modrm(SnLinkBuffer* lb, byte mod, byte reg, byte rm) {
	// masking to ensure that nothing spills over
	mod = (mod << 6) & 0xc0;
	reg = (reg << 3) & 0x38;
	rm &= 0x07;
	emit(lb, mod | reg | rm);
}

static inline void emit_operands(SnLinkBuffer* lb, SnOp reg, SnOp rm) {
	emit_modrm(lb, mod_for_operand(rm), reg.reg, rm.reg);
	emit_disp(lb, rm);
}

static inline void emit_instr(SnLinkBuffer* lb, byte opcode, SnOp reg, SnOp rm) {
	emit_rex(lb, rex_for_operands(reg, rm));
	emit(lb, opcode);
	emit_operands(lb, reg, rm);
}

/*
	Glossary:
		I = Immediate
		D = Destination
		S = Source
		X = Opcode extension
*/

#define INSTR_ID(name, opcode) static inline void asm_##name##_id(SnLinkBuffer* lb, SnOp imm, SnOp reg) { emit_instr(lb, opcode, reg, 0); emit_imm(lb, imm); }
#define INSTR_SD(name, opcode) static inline void asm_##name(SnLinkBuffer* lb, SnOp reg, SnOp rm) { emit_instr(lb, opcode, reg, rm); }
#define INSTR_DS(name, opcode) static inline void asm_##name##_r(SnLinkBuffer* lb, SnOp reg, SnOp rm) { emit_instr(lb, opcode, reg, rm); }
#define INSTR_X(name, opcode, ext) static inline void asm_##name(SnLinkBuffer* lb, SnOp rm) { emit_instr(lb, opcode, make_opcode_ext(ext), rm); }
#define INSTR_XI(name, opcode, ext) static inline void asm_##name##_id(SnLinkBuffer* lb, SnOp imm, SnOp rm) { emit_instr(lb, opcode, make_opcode_ext(ext), rm); emit_imm(lb, imm); }

INSTR_XI(add, 0x81, 0)
INSTR_SD(add, 0x01)
INSTR_DS(add, 0x03)

INSTR_X(call, 0xff, 2)

INSTR_XI(cmp, 0x81, 7)
INSTR_SD(cmp, 0x39)
INSTR_DS(cmp, 0x3b)

INSTR_SD(mov, 0x89)
INSTR_DS(mov, 0x8b)

INSTR_XI(sub, 0x81, 5)
INSTR_SD(sub, 0x29)
INSTR_DS(sub, 0x2b)


// Special instructions

static inline void asm_mov_id(SnLinkBuffer* lb, SnOp imm, SnOp rm) {
	emit_rex(lb, rex_for_rm(rm));
	emit(lb, 0xb8 + rm.reg);
	emit_imm(lb, make_immediate(imm.imm, 8));
}

static inline void asm_j(SnLinkBuffer* lb, COND cc, signed offset) {
	emit(lb, 0x0f);
	emit(lb, 0x80 + cc);
	emit_imm(lb, make_immediate(offset, 4));
}

static inline void asm_jmp(SnLinkBuffer* lb, signed offset) {
	emit(lb, 0xe9);
	emit_imm(lb, make_immediate(offset, 4));
}

static inline void asm_enter(SnLinkBuffer* lb, unsigned stack_size, unsigned level) {
	emit(lb, 0xc8);
	emit_imm(lb, make_immediate(stack_size, 2));
	emit_imm(lb, make_immediate(level, 1));
}

static inline void asm_leave(SnLinkBuffer* lb) {
	emit(lb, 0xc9);
}

static inline void asm_ret(SnLinkBuffer* lb) {
	emit(lb, 0xc3);
}

static inline void asm_int3(SnLinkBuffer* lb) {
	emit(lb, 0xcc);
}

static inline void asm_nop(SnLinkBuffer* lb) {
	emit(lb, 0x90);
}

#endif /* end of include guard: ASM_X86_64_H_NNNVOAL1 */
