#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "errormsg.h"
#include "tree.h"
#include "assem.h"
#include "frame.h"
#include "codegen.h"
#include "table.h"

#define INST_SIZE 40
static AS_instrList iList = NULL, last = NULL;

/* prototypes */
static void emit(AS_instr inst);
static Temp_temp munchExp(T_exp e);
static void munchStm(T_stm s);
static Temp_tempList munchArgs(T_expList args);

/* definitions */
void emit(AS_instr inst) {
   if (last != NULL)
      last = last->tail = AS_InstrList(inst, NULL);
   else last = iList = AS_InstrList(inst, NULL);
}

static Temp_temp munchExp(T_exp e) {
   Temp_temp ret = Temp_newtemp();

   switch (e->kind) {
      case T_BINOP: {
         Temp_temp l = munchExp(e->u.BINOP.left), r = munchExp(e->u.BINOP.right);
         char *oper_str[] = {"addq", "subq", "imulq", "", "andq", "orq", "salq", "shrq", "sarq"};
         switch (e->u.BINOP.op) {
            case T_div: {
               emit(AS_Move("movq `s0, `d0", L(1, F_RAX()), L(1, l)));
               emit(AS_Oper("idivq `s0", L(2, F_RAX(), F_RDX()), L(3, F_RAX(), F_RDX(), r), NULL));
               emit(AS_Move("movq `s0, `d0", L(1, ret), L(1, F_RAX())));
               return ret;
            }
            default: {
               char *inst = checked_malloc(INST_SIZE);
               sprintf(inst, "%s `s0, `d0", oper_str[e->u.BINOP.op]);
               emit(AS_Move("movq `s0, `d0", L(1, ret), L(1, l)));
               emit(AS_Oper(inst, L(1, ret), L(2, ret, r), NULL));
               return ret;
            }
         }
      }

      case T_MEM:
         emit(AS_Oper("movq (`s0), `d0", L(1, ret), L(1, munchExp(e->u.MEM)), NULL));
         return ret;

      case T_TEMP:
         return e->u.TEMP;

      case T_ESEQ:
         munchStm(e->u.ESEQ.stm);
         return munchExp(e->u.ESEQ.exp);

      case T_NAME: {
         char *inst = checked_malloc(INST_SIZE);
         sprintf(inst, "movq $%s, `d0", Temp_labelstring(e->u.NAME));
         emit(AS_Oper(inst, L(1, ret), NULL, NULL));
         return ret;
      }

      case T_CONST: {
         char *inst = checked_malloc(INST_SIZE);
         sprintf(inst, "movq $%d, `d0", e->u.CONST);
         emit(AS_Oper(inst, L(1, ret), NULL, NULL));
         return ret;
      }

      case T_CALL: {
         if (e->u.CALL.fun->kind == T_NAME) {
            Temp_tempList src = munchArgs(e->u.CALL.args);
            char *inst = checked_malloc(INST_SIZE);
            sprintf(inst, "callq %s", Temp_labelstring(e->u.CALL.fun->u.NAME));
            emit(AS_Oper(inst, F_callersaves(), src, NULL));

            unsigned count = 0;
            T_expList args = e->u.CALL.args;
            for (; args; args = args->tail)
               ++count;
            count > 6 ? (count -= 6) : (count = 0);
            count *= F_wordsize;
            return ret;
         } else
            assert(0);
      }
   }
   return NULL;
}

static void munchStm(T_stm s) {
   switch (s->kind) {
      case T_SEQ:
         munchStm(s->u.SEQ.left);
         munchStm(s->u.SEQ.right);
         break;

      case T_LABEL: {
         char *inst = checked_malloc(INST_SIZE);
         sprintf(inst, "%s", Temp_labelstring(s->u.LABEL));
         emit(AS_Label(inst, s->u.LABEL));
         break;
      }

      case T_JUMP: {
         T_exp e = s->u.JUMP.exp;
         if (e->kind == T_NAME) {
            char *inst = checked_malloc(INST_SIZE);
            sprintf(inst, "jmp %s", Temp_labelstring(e->u.NAME));
            emit(AS_Jmp(inst, AS_Targets(s->u.JUMP.jumps)));
         } else
            assert(0);
         break;
      }

      case T_CJUMP: {
         char *oper_str[] = {"je", "jne", "jl", "jg", "jle", "jge"};
         Temp_temp l = munchExp(s->u.CJUMP.left), r = munchExp(s->u.CJUMP.right);
         char *inst = checked_malloc(INST_SIZE);
         sprintf(inst, "%s `j0", oper_str[s->u.CJUMP.op]);
         emit(AS_Oper("cmp `s0, `s1", NULL, L(2, l, r), NULL));
         emit(AS_Oper(inst, NULL, NULL, AS_Targets(Temp_LabelList(s->u.CJUMP.true, NULL))));
         break;
      }

      case T_MOVE: {
         T_exp dst = s->u.MOVE.dst, src = s->u.MOVE.src;
         if (dst->kind == T_MEM) {
            if (dst->u.MEM->kind == T_BINOP
                && dst->u.MEM->u.BINOP.op == T_plus
                && dst->u.MEM->u.BINOP.right->kind == T_CONST) {
               /* MOVE(MEM(BINOP(PLUS, e1, CONST(n))), e2) */
               T_exp e1 = dst->u.MEM->u.BINOP.left, e2 = src;
               int aConst = dst->u.MEM->u.BINOP.right->u.CONST;
               char *inst = checked_malloc(INST_SIZE);
               sprintf(inst, "movq `s0, %d(`s1)", aConst);
               emit(AS_Oper(inst, NULL, L(2, munchExp(e1), munchExp(e2)), NULL));
            } else if (dst->u.MEM->kind == T_BINOP
                       && dst->u.MEM->u.BINOP.op == T_plus
                       && dst->u.MEM->u.BINOP.left->kind == T_CONST) {
               /*MOVE(MEM(BINOP(PLUS, CONST(n), e1)), e2) */
               T_exp e1 = dst->u.MEM->u.BINOP.right, e2 = src;
               int aConst = dst->u.MEM->u.BINOP.left->u.CONST;
               char *inst = checked_malloc(INST_SIZE);
               sprintf(inst, "movq `s0, %d(`s1)", aConst);
               emit(AS_Oper(inst, NULL, L(2, munchExp(e1), munchExp(e2)), NULL));
            } else {
               /* MOVE(MEM(e1), e2) */
               T_exp e1 = dst->u.MEM, e2 = src;
               emit(AS_Oper("movq `s0, (`s1)", NULL, L(2, munchExp(e1), munchExp(e2)), NULL));
            }

         } else if (dst->kind == T_TEMP) {
            if (src->kind == T_CONST) {
               char *inst = checked_malloc(INST_SIZE);
               sprintf(inst, "movq $%d, `d0", src->u.CONST);
               emit(AS_Oper(inst, L(1, munchExp(dst)), NULL, NULL));
            } else if (src->kind == T_NAME) {
               char *inst = checked_malloc(INST_SIZE);
               sprintf(inst, "movq $%s, `d0", Temp_labelstring(src->u.NAME));
               emit(AS_Oper(inst, L(1, munchExp(dst)), NULL, NULL));
            } else
               emit(AS_Move("movq `s0, `d0", L(1, munchExp(dst)), L(1, munchExp(src))));
         } else
            assert(0);
         break;
      }

      case T_EXP:
         munchExp(s->u.EXP);
   }
}

Temp_tempList munchArgs(T_expList args) {
   T_expList rargs = NULL;
   for (; args; args = args->tail)
      rargs = T_ExpList(args->head, rargs);
   Temp_tempList ret = NULL;
   for (T_expList a = rargs; a; a = a->tail)
      ret = Temp_TempList(munchExp(a->head), ret);
   for (; rargs; rargs = rargs->tail)
      emit(AS_Oper("pushq `s0", NULL, L(1, munchExp(rargs->head)), NULL));
   return ret;
}


AS_instrList F_codegen(F_frame f, T_stmList stmList) {
   AS_instrList list;
   T_stmList sl;
   for (sl = stmList; sl; sl = sl->tail)
      munchStm(sl->head);
   list = iList;
   iList = last = NULL;
   return list;
}
