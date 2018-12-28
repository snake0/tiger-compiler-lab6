#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "env.h"
#include "semant.h"
#include "helper.h"
#include "translate.h"
#include "table.h"

static bool equal(Ty_ty left, Ty_ty right);
static Ty_ty actTy(Ty_ty ty);
static Ty_tyList mkTyList(S_table tenv, A_fieldList args);
static Ty_fieldList mkTyFieldList(S_table tenv, A_fieldList fieldList);
static U_boolList mkEscList(A_fieldList fieldList);

struct expty {
   Tr_exp exp;
   Ty_ty ty;
};

struct expty expTy(Tr_exp exp, Ty_ty ty) {
   struct expty e;
   e.exp = exp;
   e.ty = ty;
   return e;
};

F_fragList SEM_transProg(A_exp exp) {
   Tr_level mainFrame = Tr_outermost();
   struct expty e = transExp(E_base_venv(), E_base_tenv(), exp, mainFrame, NULL);
   Tr_AddFuncFrag(e.exp, mainFrame);
   return Tr_getResult();
}

/* ============================ exp translator ============================ */

struct expty transCallExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   E_enventry x = S_look(venv, get_callexp_func(a));
   if (!x || x->kind != E_funEntry) {
      EM_error(a->pos, "undefined function %s", S_name(get_callexp_func(a)));
      return expTy(Tr_Nil(), Ty_Int());
   } else {
      A_expList argsty = get_callexp_args(a);
      Ty_tyList formals = get_func_tylist(x);
      struct expty exp;
      Tr_expList argsExpList = NULL;
      for (; argsty && formals; argsty = argsty->tail,
              formals = formals->tail) {
         exp = transExp(venv, tenv, argsty->head, cur_l, br_label);
         if (!equal(formals->head, exp.ty))
            EM_error(argsty->head->pos, "para type mismatch");
         argsExpList = Tr_ExpList(exp.exp, argsExpList);
      }
      if (argsty)
         EM_error(a->pos, "too many params in function %s", S_name(get_callexp_func(a)));
      if (formals)
         EM_error(a->pos, "too few params in function %s", S_name(get_callexp_func(a)));
      return expTy(Tr_Call(get_func_label(x), argsExpList, cur_l, get_func_level(x)), get_func_res(x));
   }
}

struct expty transOpExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   A_oper oper = get_opexp_oper(a);
   struct expty leftexpty = transExp(venv, tenv, get_opexp_left(a), cur_l, br_label);
   struct expty rightexpty = transExp(venv, tenv, get_opexp_right(a), cur_l, br_label);
   Ty_ty leftty = actTy(leftexpty.ty);
   Ty_ty rightty = actTy(rightexpty.ty);
   if (A_plusOp <= oper && oper <= A_divideOp) {
      if (leftty->kind != Ty_int)
         EM_error(get_opexp_leftpos(a), "integer required");
      if (rightty->kind != Ty_int)
         EM_error(get_opexp_rightpos(a), "integer required");
      return expTy(Tr_OpArithmetic(oper, leftexpty.exp, rightexpty.exp), Ty_Int());
   } else {
      if (!equal(leftty, rightty))
         EM_error(a->pos, "same type required");
      return expTy(Tr_OpLogical(oper, leftexpty.exp, rightexpty.exp, get_expty_kind(leftexpty) == Ty_string), Ty_Int());
   }
}

struct expty transRecordExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   Ty_ty recordty = S_look(tenv, get_recordexp_typ(a));
   recordty = actTy(recordty);
   if (!recordty) {
      EM_error(a->pos, "undefined type %s", S_name(get_recordexp_typ(a)));
      return expTy(Tr_Nil(), Ty_Int());
   } else if (recordty->kind != Ty_record) {
      EM_error(a->pos, "not a record type");
      return expTy(Tr_Nil(), Ty_Int());
   } else {
      A_efieldList args = get_recordexp_fields(a);
      Ty_fieldList formals = recordty->u.record;
      Tr_expList argsexp = NULL;
      struct expty argexpty;
      int order = 0;
      for (; args && formals; args = args->tail,
              formals = formals->tail, ++order) {
         argexpty = transExp(venv, tenv, args->head->exp, cur_l, br_label);
         if (args->head->name != formals->head->name ||
             !equal(formals->head->ty, argexpty.ty))
            EM_error(args->head->exp->pos, "type mismatch");
         argsexp = Tr_ExpList(argexpty.exp, argsexp);
      }
      if (args || formals) {
         EM_error(a->pos, "type mismatch");
         return expTy(Tr_RecordCreation(order, argsexp), recordty);
      } else return expTy(Tr_RecordCreation(order, argsexp), recordty);
   }
}

struct expty transSeqExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   struct expty endexpty;
   Tr_exp seqexp = Tr_Nil();
   Ty_ty endty = Ty_Void();
   A_expList expseq = get_seqexp_seq(a);
   for (; expseq; expseq = expseq->tail) {
      endexpty = transExp(venv, tenv, expseq->head, cur_l, br_label);
      seqexp = Tr_Seq(seqexp, endexpty.exp);
      endty = endexpty.ty;
   }
   endexpty = expTy(seqexp, endty);
   return endexpty;
}

struct expty transAssignExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   Ty_ty ty = Ty_Void();
   A_var var = get_assexp_var(a);
   A_exp exp = get_assexp_exp(a);
   struct expty lvalueexpty = transVar(venv, tenv, var, cur_l, br_label),
           rightexpty = transExp(venv, tenv, exp, cur_l, br_label);
   if (var->kind == A_simpleVar) {
      E_enventry var_entry = S_look(venv, get_simplevar_sym(var));
      if (var_entry->readonly)
         EM_error(a->pos, "loop variable can't be assigned");
   }
   if (!equal(lvalueexpty.ty, rightexpty.ty))
      EM_error(a->pos, "unmatched assign exp");
   Tr_exp assignexp = Tr_Assign(lvalueexpty.exp, rightexpty.exp);
   return expTy(assignexp, ty);
}

struct expty transIfExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   A_exp test = get_ifexp_test(a), then = get_ifexp_then(a);
   A_exp elsee = get_ifexp_else(a);
   struct expty testexpty = transExp(venv, tenv, test, cur_l, br_label);
   Ty_ty testty = actTy(testexpty.ty);
   if (testty->kind != Ty_int)
      EM_error(test->pos, "integer required");
   struct expty thenexpty = transExp(venv, tenv, then, cur_l, br_label);
   Tr_exp ifthenexp;
   if (!elsee) {
      Ty_ty thenty = actTy(thenexpty.ty);
      if (thenty->kind != Ty_void)
         EM_error(a->pos, "if-then exp's body must produce no value");
      ifthenexp = Tr_IfThen(testexpty.exp, thenexpty.exp);
      return expTy(ifthenexp, Ty_Void());
   } else {
      struct expty elseexpty = transExp(venv, tenv, elsee, cur_l, br_label);
      if (!equal(thenexpty.ty, elseexpty.ty))
         EM_error(a->pos, "then exp and else exp type mismatch");
      ifthenexp = Tr_IfThenElse(testexpty.exp, thenexpty.exp, elseexpty.exp);
      return expTy(ifthenexp, thenexpty.ty);
   }
}

struct expty transWhileExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   Temp_label done = Temp_newlabel();
   A_exp test = get_whileexp_test(a), body = get_whileexp_body(a);
   struct expty testexpty = transExp(venv, tenv, test, cur_l, br_label);
   Ty_ty testty = actTy(testexpty.ty);
   if (testty->kind != Ty_int)
      EM_error(test->pos, "integer required");
   struct expty bodyexpty = transExp(venv, tenv, body, cur_l, done);
   Ty_ty bodyty = actTy(bodyexpty.ty);
   if (!(bodyty->kind != Ty_void))
      return expTy(Tr_While(testexpty.exp, bodyexpty.exp, done), Ty_Void());
   EM_error(body->pos, "while body must produce no value");
   return expTy(Tr_While(testexpty.exp, bodyexpty.exp, done), Ty_Void());
}

struct expty transForExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   Temp_label done = Temp_newlabel();
   S_symbol var = get_forexp_var(a);
   A_exp lo = get_forexp_lo(a), hi = get_forexp_hi(a);
   A_exp body = get_forexp_body(a);
   struct expty loexpty = transExp(venv, tenv, lo, cur_l, br_label);
   Ty_ty loty = actTy(loexpty.ty);
   if (loty->kind != Ty_int)
      EM_error(lo->pos, "for lo should be int");
   struct expty hiexpty = transExp(venv, tenv, hi, cur_l, br_label);
   Ty_ty hity = actTy(hiexpty.ty);
   if (hity->kind != Ty_int)
      EM_error(hi->pos, "for hi should be int");
   S_beginScope(venv);
   Tr_access access = Tr_allocLocal(cur_l, a->u.forr.escape);
   S_enter(venv, var, E_ROVarEntry(access, Ty_Int()));
   struct expty bodyexpty = transExp(venv, tenv, body, cur_l, done);
   Ty_ty bodyty = actTy(bodyexpty.ty);
   if (bodyty->kind != Ty_void)
      EM_error(body->pos, "for body should be void");
   S_endScope(venv);
   Tr_exp forexp = Tr_For(access, cur_l, loexpty.exp, hiexpty.exp, bodyexpty.exp, done);
   return expTy(forexp, Ty_Void());
}

struct expty transLetExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   Tr_exp seq = Tr_Nil();
   A_decList decs = get_letexp_decs(a);
   A_exp body = get_letexp_body(a);
   S_beginScope(venv);
   S_beginScope(tenv);
   for (; decs; decs = decs->tail)
      seq = Tr_Seq(seq, transDec(venv, tenv, decs->head, cur_l, br_label));
   struct expty letexpty = transExp(venv, tenv, body, cur_l, br_label);
   letexpty.exp = Tr_Seq(seq, letexpty.exp);
   S_endScope(tenv);
   S_endScope(venv);
   return letexpty;
}

struct expty transArrayExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   S_symbol typ = get_arrayexp_typ(a);
   A_exp size = get_arrayexp_size(a), init = get_arrayexp_init(a);
   Ty_ty arrayty = S_look(tenv, typ);
   arrayty = actTy(arrayty);
   if (!arrayty) {
      EM_error(a->pos, "undefined type %s", typ);
      return expTy(Tr_Nil(), Ty_Int());
   } else {
      if (arrayty->kind == Ty_array) {
         struct expty sizeexpty = transExp(venv, tenv, size, cur_l, br_label);
         Ty_ty sizety = actTy(sizeexpty.ty);
         if (sizety->kind != Ty_int)
            EM_error(size->pos, "int required");
         struct expty initexpty = transExp(venv, tenv, init, cur_l, br_label);
         if (!equal(initexpty.ty, get_ty_array(arrayty)))
            EM_error(size->pos, "array init type mismatch");
         return expTy(Tr_ArrayCreation(sizeexpty.exp, initexpty.exp), arrayty);
      } else {
         EM_error(a->pos, "array required");
         return expTy(Tr_Nil(), Ty_Int());
      }
   }
}

/* ============================ var translator ============================ */

struct expty transSimpleVar(S_table venv, S_table tenv, A_var v, Tr_level cur_l, Temp_label br_label) {
   S_symbol id = get_simplevar_sym(v);
   E_enventry var_entry = S_look(venv, id);
   if (!var_entry || var_entry->kind != E_varEntry) {
      EM_error(v->pos, "undefined variable %s", S_name(id));
      return expTy(Tr_Nil(), Ty_Int());
   } else {
      Ty_ty type = get_varentry_type(var_entry);
      return expTy(Tr_simpleVar(get_var_access(var_entry), cur_l), type);
   }
}

struct expty transFieldVar(S_table venv, S_table tenv, A_var v, Tr_level cur_l, Temp_label br_label) {
   struct expty lvalueexpty = transVar(venv, tenv, get_fieldvar_var(v), cur_l, br_label);
   Ty_ty lvaluety = actTy(lvalueexpty.ty);
   if (lvaluety->kind == Ty_record) {
      Ty_fieldList fieldList = get_ty_record(lvaluety);
      S_symbol fieldName = get_fieldvar_sym(v);
      int order = 0;
      while (fieldList && fieldList->head->name != fieldName) {
         fieldList = fieldList->tail;
         ++order;
      }
      if (fieldList) {
         Tr_exp tr_exp = Tr_fieldVar(lvalueexpty.exp, order);
         return expTy(tr_exp, actTy(fieldList->head->ty));
      } else {
         EM_error(v->pos, "field %s doesn't exist", S_name(fieldName));
         return expTy(NULL, Ty_Int());
      }
   } else {
      EM_error(v->pos, "not a record type");
      return expTy(NULL, Ty_Int());
   }
}

struct expty transSubscriptVar(S_table venv, S_table tenv, A_var v, Tr_level cur_l, Temp_label br_label) {
   A_var lvalue = get_subvar_var(v);
   A_exp index = get_subvar_exp(v);
   struct expty varexpty = transVar(venv, tenv, lvalue, cur_l, br_label);
   Ty_ty varty = actTy(varexpty.ty);
   if (varty->kind != Ty_array) {
      EM_error(lvalue->pos, "array required");
      return expTy(Tr_Nil(), Ty_Int());
   }
   struct expty expexpty = transExp(venv, tenv, index, cur_l, br_label);
   Ty_ty expty = actTy(expexpty.ty);
   if (expty->kind != Ty_int)
      EM_error(index->pos, "int required");
   return expTy(Tr_subscriptVar(varexpty.exp, expexpty.exp), get_array(varexpty));
}

/* ============================ dec translator ============================ */

Tr_exp transFunctionDec(S_table venv, S_table tenv, A_dec d, Tr_level cur_l, Temp_label br_label) {
   TAB_table name_table = TAB_empty();
   A_fundecList fundecs = get_funcdec_list(d);
   for (A_fundecList fundec = fundecs; fundec; fundec = fundec->tail) {
      A_fundec dec = fundec->head;
      if (TAB_look(name_table, dec->name))
         EM_error(d->pos, "two functions have the same name\n", S_name(dec->name));
      TAB_enter(name_table, dec->name, (void *) 1);
   }
   for (A_fundecList fundec = fundecs; fundec; fundec = fundec->tail) {
      A_fundec dec = fundec->head;
      Ty_ty result_ty = Ty_Void();
      if (dec->result && strcmp(S_name(dec->result), "") != 0) {
         result_ty = S_look(tenv, dec->result);
         if (!result_ty) {
            EM_error(dec->pos, "undefined result type %s", S_name(dec->result));
            result_ty = Ty_Void();
         }
      }
      Temp_label fun_label = Temp_namedlabel(S_name(fundec->head->name));
      U_boolList formal_escapes = mkEscList(dec->params);
      Tr_level fun_level = Tr_newLevel(cur_l, fun_label, formal_escapes);
      Ty_tyList formaltys = mkTyList(tenv, dec->params);
      E_enventry fun_entry = E_FunEntry(fun_level, fun_label, formaltys, result_ty);
      S_enter(venv, dec->name, fun_entry);
   }
   for (A_fundecList fundec = fundecs; fundec; fundec = fundec->tail) {
      A_fundec dec = fundec->head;
      E_enventry fun_entry = S_look(venv, dec->name);
      Tr_accessList formalAccessList = Tr_formals(get_func_level(fun_entry));
      A_fieldList fieldList = dec->params;
      S_beginScope(venv);
      for (; fieldList; fieldList = fieldList->tail,
              formalAccessList = formalAccessList->tail) {
         Ty_ty type = S_look(tenv, fieldList->head->typ);
         E_enventry var_entry = E_VarEntry(formalAccessList->head, type);
         S_enter(venv, fieldList->head->name, var_entry);
      }
      struct expty body_exp = transExp(venv, tenv, dec->body, get_func_level(fun_entry), br_label);
      S_endScope(venv);
      Ty_ty formalty = actTy(get_func_res(fun_entry));
      Ty_ty argty = actTy(body_exp.ty);
      if (!equal(formalty, argty))
         EM_error(fundecs->head->pos, "return type error");
      Tr_AddFuncFrag(body_exp.exp, get_func_level(fun_entry));
   }
   return Tr_Nil();
}

Tr_exp transVarDec(S_table venv, S_table tenv, A_dec d, Tr_level cur_l, Temp_label br_label) {
   struct expty initexpty = transExp(venv, tenv, d->u.var.init, cur_l, br_label);
   S_symbol typ = get_vardec_typ(d), var = get_vardec_var(d);
   A_exp init = get_vardec_init(d);
   Tr_access access = Tr_allocLocal(cur_l, d->u.var.escape);
   if (strcmp(S_name(typ), "") == 0) {
      if (actTy(initexpty.ty)->kind == Ty_nil)
         EM_error(init->pos, "init should not be nil without type specified");
      S_enter(venv, var, E_VarEntry(access, initexpty.ty));
   } else {
      Ty_ty ty = S_look(tenv, typ);
      if (!ty)
         EM_error(init->pos, "undefined type %s", S_name(typ));
      Ty_ty formalty = actTy(ty);
      Ty_ty argty = actTy(initexpty.ty);
      if (!equal(formalty, argty))
         EM_error(init->pos, "type mismatch");
      S_enter(venv, var, E_VarEntry(access, formalty));
   }
   return Tr_Assign(Tr_simpleVar(access, cur_l), initexpty.exp);
}

Tr_exp transTypeDec(S_table venv, S_table tenv, A_dec d, Tr_level cur_l, Temp_label br_label) {
   TAB_table name_table = TAB_empty();
   A_nametyList typedecs = get_typedec_list(d);
   for (A_nametyList typedec = typedecs; typedec; typedec = typedec->tail) {
      if (TAB_look(name_table, typedec->head->name) != NULL)
         EM_error(d->pos, "two types have the same name\n", S_name(typedec->head->name));
      TAB_enter(name_table, typedec->head->name, (void *) 1);
   }
   for (A_nametyList typedec = typedecs; typedec; typedec = typedec->tail)
      S_enter(tenv, typedec->head->name, Ty_Name(typedec->head->name, NULL));
   for (A_nametyList typedec = typedecs; typedec; typedec = typedec->tail) {
      Ty_ty ty = S_look(tenv, typedec->head->name);
      get_ty_name(ty).ty = transTy(tenv, typedec->head->ty);
   }
   for (A_nametyList typedec = typedecs; typedec; typedec = typedec->tail) {
      Ty_ty init = S_look(tenv, typedec->head->name);
      Ty_ty type = get_ty_name(actTy(init)).ty;
      while (type && type->kind == Ty_name)
         if (type == init) {
            EM_error(d->pos, "typedec cycle detected");
            init->u.name.ty = Ty_Int();
            break;
         } else type = get_ty_name(actTy(init)).ty;
   }
   return Tr_Nil();
}

/* ============================ worker functions ============================ */

struct expty transVar(S_table venv, S_table tenv, A_var v, Tr_level cur_l, Temp_label br_label) {
   struct expty (*transXXXVar[3])(S_table, S_table, A_var, Tr_level, Temp_label) = {
           transSimpleVar, transFieldVar, transSubscriptVar};
   return transXXXVar[v->kind](venv, tenv, v, cur_l, br_label);
}

struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label br_label) {
   struct expty (*transXXXExp[15])(S_table, S_table, A_exp, Tr_level, Temp_label) = {
           0, 0, 0, 0, transCallExp, transOpExp,
           transRecordExp, transSeqExp, transAssignExp,
           transIfExp, transWhileExp, transForExp,
           0, transLetExp, transArrayExp};
   switch (a->kind) {
      case A_varExp:
         return transVar(venv, tenv, a->u.var, cur_l, br_label);
      case A_nilExp:
         return expTy(Tr_Nil(), Ty_Nil());
      case A_intExp:
         return expTy(Tr_Int(a->u.intt), Ty_Int());
      case A_stringExp:
         return expTy(Tr_AddStringFrag(a->u.stringg), Ty_String());
      case A_breakExp:
         return expTy(br_label ? Tr_Break(br_label) : Tr_Nil(), Ty_Void());
      default:
         return transXXXExp[a->kind](venv, tenv, a, cur_l, br_label);
   }
}


Tr_exp transDec(S_table venv, S_table tenv, A_dec d, Tr_level cur_l, Temp_label br_label) {
   Tr_exp(*transXXXDec[3])(S_table, S_table, A_dec, Tr_level, Temp_label) = {
           transFunctionDec, transVarDec, transTypeDec};
   return transXXXDec[d->kind](venv, tenv, d, cur_l, br_label);
}

/* ============================ tool functions ============================ */

Ty_ty transTy(S_table tenv, A_ty a) {
   switch (a->kind) {
      case A_nameTy: {
         S_symbol tyname = get_ty_name(a);
         Ty_ty ty = S_look(tenv, tyname);
         if (ty != NULL)
            return Ty_Name(tyname, ty);
         EM_error(a->pos, "undefined type %s", S_name(tyname));
         return Ty_Int();
      }
      case A_arrayTy: {
         S_symbol tyname = get_ty_array(a);
         Ty_ty ty = S_look(tenv, tyname);
         if (!ty) {
            EM_error(a->pos, "undefined type %s", S_name(tyname));
            return Ty_Int();
         }
         return Ty_Array(ty);
      }
      case A_recordTy:
         return Ty_Record(mkTyFieldList(tenv, get_ty_record(a)));
   }
   return NULL;
}


static Ty_fieldList mkTyFieldList(S_table tenv, A_fieldList fieldList) {
   Ty_ty type = S_look(tenv, fieldList->head->typ);
   if (!type) {
      EM_error(fieldList->head->pos, "undefined type %s", S_name(fieldList->head->typ));
      type = Ty_Int();
   }
   Ty_field field = Ty_Field(fieldList->head->name, type);
   return Ty_FieldList(field, !fieldList->tail ? NULL : mkTyFieldList(tenv, fieldList->tail));
}

static bool equal(Ty_ty left, Ty_ty right) {
   bool ret = FALSE;
   left = actTy(left);
   right = actTy(right);
   if (left->kind == Ty_array) {
      if (left == right)
         ret = TRUE;
   } else if (left->kind == Ty_record) {
      if (left == right || right->kind == Ty_nil)
         ret = TRUE;
   } else if (right->kind == Ty_record) {
      if (left == right || left->kind == Ty_nil)
         ret = TRUE;
   } else if (left->kind == right->kind)
      ret = TRUE;
   return ret;
}

static U_boolList mkEscList(A_fieldList fieldList) {
   return fieldList ? U_BoolList(fieldList->head->escape, mkEscList(fieldList->tail)) : NULL;
}

static Ty_ty actTy(Ty_ty ty) {
   while (ty && ty->kind == Ty_name)
      ty = get_ty_name(ty).ty;
   return ty;
}

static Ty_tyList mkTyList(S_table tenv, A_fieldList args) {
   if (!args) return NULL;
   else {
      Ty_ty ty = S_look(tenv, args->head->typ);
      if (!ty) {
         EM_error(args->head->pos, "undefined type %s", S_name(args->head->typ));
         ty = Ty_Int();
      }
      return Ty_TyList(ty, mkTyList(tenv, args->tail));
   }
}
