#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"

static F_fragList frags = NULL;
static Tr_level outermost = NULL;

struct Tr_access_ {
   Tr_level level;
   F_access access;
};
struct Tr_level_ {
   F_frame frame;
   Tr_level parent;
   Tr_accessList formals;
};
struct patchList_ {
   Temp_label *head;
   patchList tail;
};
struct Cx {
   patchList trues;
   patchList falses;
   T_stm stm;
};
struct Tr_exp_ {
   enum {
      Tr_ex, Tr_nx, Tr_cx
   } kind;
   union {
      T_exp ex;
      T_stm nx;
      struct Cx cx;
   } u;
};

struct Tr_expList_ {
   Tr_exp head;
   Tr_expList tail;
};

static Tr_accessList Tr_allocFormals(Tr_level level, F_accessList accesses);
static patchList PatchList(Temp_label *head, patchList tail);
static void doPatch(patchList tList, Temp_label label);
static patchList joinPatch(patchList first, patchList second);
static Tr_exp Tr_Ex(T_exp exp);
static Tr_exp Tr_Nx(T_stm stm);
static Tr_exp Tr_Cx(struct Cx cx);
static T_exp unEx(Tr_exp e);
static T_stm unNx(Tr_exp e);
static struct Cx unCx(Tr_exp e);
static T_exp Tr_staticLink(Tr_level level, Tr_level target);
static T_expList Tr2T(Tr_expList Tr);

Tr_level Tr_outermost(void) {
   if (!outermost) {
      outermost = checked_malloc(sizeof(*outermost));
      outermost->parent = NULL;
      outermost->frame = F_newFrame(Temp_namedlabel("tigermain"), NULL);
      outermost->formals = NULL;
      return outermost;
   } else
      return outermost;

}

Tr_exp Tr_AddStringFrag(string str) {
   Temp_label label = Temp_newlabel();
   F_frag head = F_StringFrag(label, str);
   frags = F_FragList(head, frags);
   return Tr_Ex(T_Name(label));
}

void Tr_AddFuncFrag(Tr_exp body, Tr_level level) {
   F_frag proc_frag =
           F_ProcFrag(
                   T_Move(
                           T_Temp(F_RV()),
                           unEx(body)
                   ),
                   level->frame
           );
   frags = F_FragList(proc_frag, frags);
}

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail) {
   Tr_expList exps = checked_malloc(sizeof(*exps));
   exps->head = head;
   exps->tail = tail;
   return exps;
}

Tr_access Tr_Access(Tr_level level, F_access access) {
   Tr_access trAccess = checked_malloc(sizeof(*trAccess));
   trAccess->level = level;
   trAccess->access = access;
   return trAccess;
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail) {
   Tr_accessList trAccesses = checked_malloc(sizeof(*trAccesses));
   trAccesses->head = head;
   trAccesses->tail = tail;
   return trAccesses;
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals) {
   Tr_level level = checked_malloc(sizeof(*level));
   level->parent = parent;
   level->frame = F_newFrame(
           name,
           U_BoolList(TRUE, formals)
   );
   level->formals = Tr_allocFormals(
           level,
           F_formals(level->frame)->tail
   );
   return level;
}

Tr_accessList Tr_formals(Tr_level level) {
   return level->formals;
}

Tr_access Tr_allocLocal(Tr_level level, bool escape) {
   return
           Tr_Access(
                   level,
                   F_allocLocal(level->frame, escape)
           );
}

Tr_exp Tr_Nil() {
   return Tr_Ex(T_Const(0));
}

Tr_exp Tr_Int(int i) {
   return Tr_Ex(T_Const(i));
}

F_fragList Tr_getResult(void) {
   return frags;
}

Tr_exp Tr_OpArithmetic(A_oper oper, Tr_exp left, Tr_exp right) {
   return Tr_Ex(T_Binop((T_binOp) oper, unEx(left), unEx(right)));
}

Tr_exp Tr_OpLogical(A_oper oper, Tr_exp left, Tr_exp right, bool cmpstr) {
   T_exp leftexp, rightexp;
   if (cmpstr == 0) {
      leftexp = unEx(left);
      rightexp = unEx(right);
   } else {
      leftexp = F_externalCall("stringEqual", T_ExpList(unEx(left), T_ExpList(unEx(right), NULL)));
      rightexp = T_Const(0);
   }
   T_relOp ops[6] = {T_eq, T_ne, T_lt, T_gt, T_le, T_ge};
   struct Cx cx;
   cx.stm = T_Cjump(
           ops[(int) oper - A_eqOp],
           leftexp,
           rightexp, NULL, NULL
   );
   cx.trues = PatchList(&(cx.stm->u.CJUMP.true), NULL);
   cx.falses = PatchList(&(cx.stm->u.CJUMP.false), NULL);
   return Tr_Cx(cx);
}

Tr_exp Tr_Call(Temp_label label, Tr_expList params, Tr_level caller, Tr_level callee) {
   if (callee != Tr_outermost())
      return
              Tr_Ex(
                      T_Call(
                              T_Name(label),
                              T_ExpList(
                                      Tr_staticLink(caller, callee->parent),
                                      Tr2T(params)
                              )
                      )
              );
   else
      return
              Tr_Ex(
                      T_Call(
                              T_Name(label),
                              Tr2T(params)
                      )
              );
}

static T_stm mkRecordFields(Temp_temp r, int order, Tr_expList fieldExpList) {
   if (!fieldExpList)
      return T_Exp(T_Const(0));
   else {
      return
              T_Seq(
                      T_Move(
                              T_Mem(
                                      T_Binop(
                                              T_plus, T_Temp(r),
                                              T_Const(order * F_WORDSIZE)
                                      )
                              ),
                              unEx(fieldExpList->head)),
                      mkRecordFields(r, order - 1, fieldExpList->tail)
              );
   }
}

Tr_exp Tr_RecordCreation(int order, Tr_expList fieldList) {
   Temp_temp r = Temp_newtemp();
   return
           Tr_Ex(
                   T_Eseq(
                           T_Move(
                                   T_Temp(r),
                                   F_externalCall("malloc", T_ExpList(T_Const(order * F_WORDSIZE), NULL))
                           ),
                           T_Eseq(mkRecordFields(r, order - 1, fieldList), T_Temp(r))
                   )
           );
}

Tr_exp Tr_Seq(Tr_exp e1, Tr_exp e2) {
   T_exp ex = T_Eseq(unNx(e1), unEx(e2));
   return Tr_Ex(ex);
}

Tr_exp Tr_Assign(Tr_exp lvalue, Tr_exp exp) {
   return
           Tr_Nx(
                   T_Move(
                           unEx(lvalue),
                           unEx(exp)
                   )
           );
}


Tr_exp Tr_IfThen(Tr_exp test, Tr_exp then) {
   struct Cx cx = unCx(test);
   Temp_label true = Temp_newlabel(), false = Temp_newlabel();
   doPatch(cx.trues, true);
   doPatch(cx.falses, false);
   return
           Tr_Nx(
                   T_Seq(
                           cx.stm,
                           T_Seq(
                                   T_Label(true),
                                   T_Seq(
                                           unNx(then), T_Label(false)
                                   )
                           )
                   )
           );
}

Tr_exp Tr_IfThenElse(Tr_exp test, Tr_exp then, Tr_exp elsee) {
   struct Cx cx = unCx(test);
   Temp_label true = Temp_newlabel(), false = Temp_newlabel();
   Temp_label done = Temp_newlabel();
   doPatch(cx.trues, true);
   doPatch(cx.falses, false);

   Temp_temp ret = Temp_newtemp();
   return
           Tr_Ex(
                   T_Eseq(
                           cx.stm,
                           T_Eseq(
                                   T_Label(true),//true label
                                   T_Eseq(
                                           T_Move(T_Temp(ret), unEx(then)),
                                           T_Eseq(
                                                   T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
                                                   T_Eseq(
                                                           T_Label(false),
                                                           T_Eseq(
                                                                   T_Move(T_Temp(ret), unEx(elsee)),
                                                                   T_Eseq(
                                                                           T_Jump(T_Name(done),
                                                                                  Temp_LabelList(done, NULL)),
                                                                           T_Eseq(
                                                                                   T_Label(done),
                                                                                   T_Temp(ret)
                                                                           )
                                                                   )
                                                           )
                                                   )
                                           )
                                   )
                           )
                   )
           );
}

Tr_exp Tr_While(Tr_exp test, Tr_exp body, Temp_label done_label) {
   struct Cx cx = unCx(test);
   Temp_label t = Temp_newlabel(), body_label = Temp_newlabel();
   doPatch(cx.trues, body_label);
   doPatch(cx.falses, done_label);
   return
           Tr_Nx(
                   T_Seq(
                           T_Label(t),
                           T_Seq(
                                   cx.stm,
                                   T_Seq(
                                           T_Label(body_label),
                                           T_Seq(
                                                   unNx(body),
                                                   T_Seq(
                                                           T_Jump(T_Name(t), Temp_LabelList(t, NULL)),
                                                           T_Label(done_label))
                                           )
                                   )
                           )
                   )
           );
}

Tr_exp Tr_For(Tr_access access, Tr_level level, Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done_label) {
   Tr_exp loop_var = Tr_simpleVar(access, level);
   Temp_temp limitReg = Temp_newtemp();
   Temp_label body_label = Temp_newlabel(), incre_label = Temp_newlabel();
   return
           Tr_Nx(
                   T_Seq(
                           T_Move(unEx(loop_var), unEx(lo)),
                           T_Seq(
                                   T_Move(T_Temp(limitReg), unEx(hi)),
                                   T_Seq(
                                           T_Cjump(T_gt, unEx(loop_var), T_Temp(limitReg), done_label, body_label),
                                           T_Seq(
                                                   T_Label(body_label),
                                                   T_Seq(
                                                           unNx(body),
                                                           T_Seq(
                                                                   T_Cjump(T_eq, unEx(loop_var), T_Temp(limitReg),
                                                                           done_label, incre_label),
                                                                   T_Seq(
                                                                           T_Label(incre_label),
                                                                           T_Seq(
                                                                                   T_Move(unEx(loop_var),
                                                                                          T_Binop(T_plus,
                                                                                                  unEx(loop_var),
                                                                                                  T_Const(1))),
                                                                                   T_Seq(
                                                                                           T_Jump(T_Name(body_label),
                                                                                                  Temp_LabelList(
                                                                                                          body_label,
                                                                                                          NULL)),
                                                                                           T_Label(done_label)
                                                                                   )
                                                                           )
                                                                   )
                                                           )
                                                   )
                                           )
                                   )
                           )
                   )
           );
}

Tr_exp Tr_Break(Temp_label done_label) {
   return
           Tr_Nx(
                   T_Jump(
                           T_Name(done_label),
                           Temp_LabelList(done_label, NULL)
                   )
           );
}

Tr_exp Tr_ArrayCreation(Tr_exp size, Tr_exp init) {
   Temp_temp arrayPtr = Temp_newtemp();
   Temp_temp sizeReg = Temp_newtemp(), initReg = Temp_newtemp();
   return
           Tr_Ex(
                   T_Eseq(
                           T_Move(T_Temp(sizeReg), unEx(size)),
                           T_Eseq(
                                   T_Move(T_Temp(initReg), unEx(init)),
                                   T_Eseq(
                                           T_Move(
                                                   T_Temp(arrayPtr),
                                                   F_externalCall(
                                                           "initArray",
                                                           T_ExpList(
                                                                   T_Binop(T_mul, T_Temp(sizeReg), T_Const(F_WORDSIZE)),
                                                                   T_ExpList(T_Temp(initReg), NULL)
                                                           )
                                                   )
                                           ), T_Temp(arrayPtr)
                                   )
                           )
                   )

           );
}

Tr_exp Tr_simpleVar(Tr_access access, Tr_level level) {
   return
           Tr_Ex(
                   F_Exp(
                           access->access,
                           Tr_staticLink(level, access->level)
                   )
           );
}

Tr_exp Tr_fieldVar(Tr_exp start, int order) {
   return
           Tr_Ex(
                   T_Mem(
                           T_Binop(
                                   T_plus,
                                   unEx(start),
                                   T_Const(F_WORDSIZE * order)
                           )
                   )
           );
}

Tr_exp Tr_subscriptVar(Tr_exp start, Tr_exp off) {
   return Tr_Ex(
           T_Mem(
                   T_Binop(
                           T_plus,
                           T_Binop(
                                   T_mul, unEx(off),
                                   T_Const(F_WORDSIZE)
                           ),
                           unEx(start)
                   )
           )
   );
}

// helpers definitions

static patchList PatchList(Temp_label *head, patchList tail) {
   patchList list;
   list = (patchList) checked_malloc(sizeof(struct patchList_));
   list->head = head;
   list->tail = tail;
   return list;
}

void doPatch(patchList tList, Temp_label label) {
   for (; tList; tList = tList->tail)
      *(tList->head) = label;
}

patchList joinPatch(patchList first, patchList second) {
   if (!first) return second;
   for (; first->tail; first = first->tail);
   first->tail = second;
   return first;
}


static Tr_exp Tr_Ex(T_exp exp) {
   Tr_exp trEx = checked_malloc(sizeof(*trEx));
   trEx->kind = Tr_ex;
   trEx->u.ex = exp;
   return trEx;
}

static Tr_exp Tr_Nx(T_stm stm) {
   Tr_exp trNx = checked_malloc(sizeof(*trNx));
   trNx->kind = Tr_nx;
   trNx->u.nx = stm;
   return trNx;
}

static Tr_exp Tr_Cx(struct Cx cx) {
   Tr_exp trCx = checked_malloc(sizeof(*trCx));
   trCx->kind = Tr_cx;
   trCx->u.cx = cx;
   return trCx;
}

static T_exp unEx(Tr_exp e) {
   switch (e->kind) {
      case Tr_ex:
         return e->u.ex;
      case Tr_nx:
         return T_Eseq(e->u.nx, T_Const(0));
      case Tr_cx: {
         Temp_temp r = Temp_newtemp();
         Temp_label t = Temp_newlabel(), f = Temp_newlabel();
         doPatch(e->u.cx.trues, t);
         doPatch(e->u.cx.falses, f);
         return T_Eseq(
                 T_Move(T_Temp(r), T_Const(1)),
                 T_Eseq(
                         e->u.cx.stm,
                         T_Eseq(
                                 T_Label(f),
                                 T_Eseq(
                                         T_Move(T_Temp(r), T_Const(0)),
                                         T_Eseq(T_Label(t), T_Temp(r))
                                 )
                         )
                 )
         );
      }
   }
   return 0;
}

static T_stm unNx(Tr_exp e) {
   switch (e->kind) {
      case Tr_ex:
         return T_Exp(e->u.ex);
      case Tr_nx:
         return e->u.nx;
      case Tr_cx: {
         Temp_label empty = Temp_newlabel();
         doPatch(e->u.cx.trues, empty);
         doPatch(e->u.cx.falses, empty);
         return T_Seq(e->u.cx.stm, T_Label(empty));
      }
   }
   return 0;
}

static struct Cx unCx(Tr_exp e) {
   switch (e->kind) {
      case Tr_nx:
         assert(0);
      case Tr_cx:
         return e->u.cx;
      case Tr_ex: {
         struct Cx cx;
         if (e->u.ex->kind == T_CONST) {
            cx.stm = T_Jump(T_Name(NULL), Temp_LabelList(NULL, NULL));
            if (e->u.ex->u.CONST) {
               cx.trues = NULL;
               cx.falses =
                       PatchList(
                               &cx.stm->u.JUMP.exp->u.NAME,
                               PatchList(&cx.stm->u.JUMP.jumps->head, NULL)
                       );
            } else {
               cx.trues =
                       PatchList(
                               &cx.stm->u.JUMP.exp->u.NAME,
                               PatchList(&cx.stm->u.JUMP.jumps->head, NULL)
                       );
               cx.falses = NULL;
            }
            return cx;
         }
         cx.stm = T_Cjump(T_ne, e->u.ex, T_Const(0), NULL, NULL);
         cx.trues = PatchList(&cx.stm->u.CJUMP.true, NULL);
         cx.falses = PatchList(&cx.stm->u.CJUMP.false, NULL);
         return cx;
      }
   }
}

static T_expList Tr2T(Tr_expList Tr) {
   T_expList expList = NULL;
   for (Tr_expList trExpList = Tr; trExpList; trExpList = trExpList->tail)
      expList = T_ExpList(unEx(trExpList->head), expList);
   return expList;
}

static Tr_accessList Tr_allocFormals(Tr_level level, F_accessList accesses) {
   return
           accesses ?
           Tr_AccessList(
                   Tr_Access(level, accesses->head),
                   Tr_allocFormals(level, accesses->tail)
           ) :
           NULL;
}

static T_exp Tr_staticLink(Tr_level level, Tr_level target) {
   T_exp ex = T_Temp(F_FP());
   for (; level && level != target; level = level->parent) {
      if (level == outermost)
         break;
      ex = F_Exp(F_formals(level->frame)->head, ex);
   }
   return ex;
}

