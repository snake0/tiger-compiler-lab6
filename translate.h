#ifndef TRANSLATE_H
#define TRANSLATE_H

#include "util.h"
#include "absyn.h"
#include "temp.h"
#include "frame.h"

typedef struct Tr_exp_ *Tr_exp;
typedef struct Tr_expList_ *Tr_expList;
typedef struct Tr_access_ *Tr_access;
typedef struct Tr_accessList_ *Tr_accessList;
typedef struct Tr_level_ *Tr_level;
typedef struct patchList_ *patchList;
struct Tr_accessList_ {
   Tr_access head;
   Tr_accessList tail;
};

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail);
Tr_access Tr_Access(Tr_level level, F_access access);
Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail);
Tr_level Tr_outermost(void);
Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);
Tr_accessList Tr_formals(Tr_level level);
Tr_access Tr_allocLocal(Tr_level level, bool escape);
Tr_exp Tr_simpleVar(Tr_access access, Tr_level level);
Tr_exp Tr_fieldVar(Tr_exp start, int order);
Tr_exp Tr_subscriptVar(Tr_exp start, Tr_exp off);
Tr_exp Tr_Nil(void);
Tr_exp Tr_Int(int i);
Tr_exp Tr_Call(Temp_label label, Tr_expList params, Tr_level caller, Tr_level callee);
Tr_exp Tr_OpArithmetic(A_oper oper, Tr_exp left, Tr_exp right);
Tr_exp Tr_OpLogical(A_oper oper, Tr_exp left, Tr_exp right, bool cmpstr);
Tr_exp Tr_RecordCreation(int order, Tr_expList fieldList);
Tr_exp Tr_ArrayCreation(Tr_exp size, Tr_exp init);
Tr_exp Tr_Seq(Tr_exp e1, Tr_exp e2);
Tr_exp Tr_Assign(Tr_exp lvalue, Tr_exp exp);
Tr_exp Tr_IfThen(Tr_exp test, Tr_exp then);
Tr_exp Tr_IfThenElse(Tr_exp test, Tr_exp then, Tr_exp elsee);
Tr_exp Tr_While(Tr_exp test, Tr_exp body, Temp_label done_label);
Tr_exp Tr_For(Tr_access access, Tr_level level, Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done);
Tr_exp Tr_Break(Temp_label done_label);
void Tr_AddFuncFrag(Tr_exp body, Tr_level level);
Tr_exp Tr_AddStringFrag(string str);
F_fragList Tr_getResult(void);

#endif
