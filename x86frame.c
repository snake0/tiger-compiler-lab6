#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"

#define DEF_REG(f, r) Temp_temp f(void) { return (r) ? (r) : ((r) = Temp_newtemp()); }

/* static variables */

const int F_WORDSIZE = 8;
static Temp_tempList callersaves = NULL, calleesaves = NULL;
static Temp_map temp_map = NULL;
static Temp_tempList registers = NULL;
static Temp_temp reg[F_REGNUM];

static char *REG_NAME[F_REGNUM] =
        {
                "%rax", "%rbx", "%rcx", "%rdx",
                "%rsi", "%rdi", "%rbp", "%rsp",
                "%r8", "%r9", "%r10", "%r11",
                "%r12", "%r13", "%r14", "%r15",
        };
Temp_temp (*F_REG[F_REGNUM])(void) =
        {
                F_RAX, F_RBX, F_RCX, F_RDX,
                F_RSI, F_RDI, F_RBP, F_RSP,
                F_R8, F_R9, F_R10, F_R11,
                F_R12, F_R13, F_R14, F_R15
        };

/* structs */
struct F_frame_ {
   Temp_label name;
   F_accessList formals;
   int size;
};
struct F_access_ {
   enum {
      inFrame, inReg
   } kind;
   union {
      int offset; //inFrame
      Temp_temp reg; //inReg
   } u;
};

/* prototypes */
static F_access F_InFrame(int offset);
static F_access F_InReg(Temp_temp reg);
static F_accessList F_allocFormals(U_boolList formals);

/* registers */
DEF_REG(F_RAX, reg[0])

DEF_REG(F_RBX, reg[1])

DEF_REG(F_RCX, reg[2])

DEF_REG(F_RDX, reg[3])

DEF_REG(F_RSI, reg[4])

DEF_REG(F_RDI, reg[5])

DEF_REG(F_RBP, reg[6])

DEF_REG(F_RSP, reg[7])

DEF_REG(F_R8, reg[8])

DEF_REG(F_R9, reg[9])

DEF_REG(F_R10, reg[10])

DEF_REG(F_R11, reg[11])

DEF_REG(F_R12, reg[12])

DEF_REG(F_R13, reg[13])

DEF_REG(F_R14, reg[14])

DEF_REG(F_R15, reg[15])

Temp_map F_tempMap(void) {
   if (!temp_map) {
      temp_map = Temp_empty();
      for (int i = 0; i < F_REGNUM; ++i) {
         Temp_enter(temp_map, F_REG[i](), REG_NAME[i]);
      }
   }
   return temp_map;
}

Temp_temp F_FP(void) { return F_RBP(); }

Temp_temp F_RV(void) { return F_RAX(); }

Temp_tempList F_callersaves(void) {
   if (!callersaves)
      callersaves = L(2, F_R10(), F_R11());
   return callersaves;
}

Temp_tempList F_calleesaves(void) {
   if (!calleesaves)
      calleesaves = L(6,
                      F_RBX(), F_RBP(), F_R12(),
                      F_R13(), F_R14(), F_R15());
   return calleesaves;
}

Temp_tempList F_registers(void) {
   if (!registers)
      registers = L(14,
                    F_RAX(), F_RBX(), F_RCX(), F_RDX(),
                    F_RSI(), F_RDI(), F_R8(), F_R9(),
                    F_R10(), F_R11(), F_R12(), F_R13(),
                    F_R14(), F_R15());
   return registers;
}

/* functions */

F_access F_allocLocal(F_frame f, bool escape) {
   return escape ?
          F_InFrame(++(f->size) * -F_WORDSIZE) :
          F_InReg(Temp_newtemp());
}

F_frame F_newFrame(Temp_label name, U_boolList formals) {
   F_frame frame = checked_malloc(sizeof(*frame));
   frame->formals = F_allocFormals(formals);
   frame->name = name;
   frame->size = 0;
   return frame;
}

F_accessList F_formals(F_frame f) {
   return f->formals;
}

F_accessList F_AccessList(F_access head, F_accessList tail) {
   F_accessList accessList = checked_malloc(sizeof(*accessList));
   accessList->head = head;
   accessList->tail = tail;
   return accessList;
}

T_exp F_externalCall(string s, T_expList args) {
   return T_Call(T_Name(Temp_namedlabel(s)), args);
}

T_exp F_Exp(F_access access, T_exp fp) {
   return access->kind == inFrame ?
          T_Mem(T_Binop(T_plus, fp, T_Const(access->u.offset))) :
          T_Temp(access->u.reg);
}

F_frag F_StringFrag(Temp_label label, string str) {
   F_frag frag = checked_malloc(sizeof(*frag));
   frag->kind = F_stringFrag;
   frag->u.stringg.label = label;
   frag->u.stringg.str = str;
   return frag;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
   F_frag frag = checked_malloc(sizeof(*frag));
   frag->kind = F_procFrag;
   frag->u.proc.body = body;
   frag->u.proc.frame = frame;
   return frag;
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
   F_fragList fragList = checked_malloc(sizeof(*fragList));
   fragList->head = head;
   fragList->tail = tail;
   return fragList;
}

Temp_label F_name(F_frame f) {
   return f->name;
}

/* helper functions */

static F_access F_InReg(Temp_temp reg) {
   F_access access = checked_malloc(sizeof(*access));
   access->kind = inReg;
   access->u.reg = reg;
   return access;
}

static F_access F_InFrame(int offset) {
   F_access access = checked_malloc(sizeof(*access));
   access->kind = inFrame;
   access->u.offset = offset;
   return access;
}

static F_accessList F_allocFormals(U_boolList formals) {
   if (!formals)
      return NULL;
   unsigned formal_count = 0;
   formals = formals->tail;
   F_access static_link = F_InFrame(F_WORDSIZE);
   F_accessList formalAccessList = F_AccessList(static_link, NULL);
   for (; formals; formals = formals->tail) {
      ++formal_count;
      F_access newFormal = formal_count > 6 || formals->head ?
                           F_InFrame(((formal_count - 5) * F_WORDSIZE)) :
                           F_InReg(Temp_newtemp());
      formalAccessList = F_AccessList(newFormal, formalAccessList);
   }
   return formalAccessList;
}
