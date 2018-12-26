#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "escape.h"
#include "table.h"
#include "escape.h"
#include "helper.h"
#include "env.h"

static void traverseVar(S_table env, int depth, A_var v);
static void traverseExp(S_table env, int depth, A_exp a);
static void traverseDec(S_table env, int depth, A_dec d);

void Esc_findEscape(A_exp exp) {
   traverseExp(S_empty(), 1, exp);
}

void traverseVar(S_table env, int depth, A_var v) {
   switch (v->kind) {
      case A_simpleVar: {
         E_enventry x = S_look(env, get_simplevar_sym(v));
         if (get_esc_depth(x) < depth)
            *get_esc_escape(x) = TRUE;
         break;
      }

      case A_fieldVar:
         traverseVar(env, depth, get_fieldvar_var(v));
         break;

      case A_subscriptVar:
         traverseVar(env, depth, get_subvar_var(v));
         traverseExp(env, depth, get_subvar_exp(v));
   }
}

void traverseExp(S_table env, int depth, A_exp a) {
   switch (a->kind) {
      case A_varExp:
         traverseVar(env, depth, a->u.var);
         break;

      case A_callExp: {
         A_expList args = get_callexp_args(a);
         for (; args; args = args->tail)
            traverseExp(env, depth, args->head);
         break;
      }

      case A_opExp:
         traverseExp(env, depth, get_opexp_left(a));
         traverseExp(env, depth, get_opexp_right(a));
         break;

      case A_recordExp: {
         A_efieldList fields = get_recordexp_fields(a);
         for (; fields; fields = fields->tail)
            traverseExp(env, depth, fields->head->exp);
         break;
      }

      case A_seqExp: {
         A_expList expList = get_seqexp_seq(a);
         for (; expList; expList = expList->tail)
            traverseExp(env, depth, expList->head);
         break;
      }

      case A_assignExp: {
         traverseVar(env, depth, get_assexp_var(a));
         traverseExp(env, depth, get_assexp_exp(a));
         break;
      }

      case A_ifExp: {
         A_exp test = get_ifexp_test(a);
         A_exp then = get_ifexp_then(a);
         A_exp elsee = get_ifexp_else(a);
         traverseExp(env, depth, test);
         traverseExp(env, depth, then);
         if (elsee)
            traverseExp(env, depth, elsee);
         break;
      }

      case A_whileExp: {
         traverseExp(env, depth, get_whileexp_test(a));
         traverseExp(env, depth, get_whileexp_body(a));
         break;
      }

      case A_forExp: {
         traverseExp(env, depth, get_forexp_lo(a));
         traverseExp(env, depth, get_forexp_hi(a));
         S_beginScope(env);
         get_forexp_escape(a) = FALSE;
         S_enter(env, get_forexp_var(a), E_EscEntry(depth, &(get_forexp_escape(a))));
         traverseExp(env, depth, get_forexp_body(a));
         S_endScope(env);
         break;
      }

      case A_letExp: {
         A_decList decList = get_letexp_decs(a);
         S_beginScope(env);
         for (; decList; decList = decList->tail)
            traverseDec(env, depth, decList->head);
         traverseExp(env, depth, get_letexp_body(a));
         S_endScope(env);
         break;
      }

      case A_arrayExp: {
         traverseExp(env, depth, get_arrayexp_size(a));
         traverseExp(env, depth, get_arrayexp_init(a));
      }
      default:;
   }
}

void traverseDec(S_table env, int depth, A_dec d) {
   switch (d->kind) {
      case A_functionDec: {
         A_fundecList funcdecs = get_funcdec_list(d);
         for (; funcdecs; funcdecs = funcdecs->tail) {
            A_fundec funcdec = funcdecs->head;

            S_beginScope(env);
            A_fieldList params = funcdec->params;
            for (; params; params = params->tail) {
               params->head->escape = FALSE;
               S_enter(env, params->head->name, E_EscEntry(depth + 1, &params->head->escape));
            }
            traverseExp(env, depth + 1, funcdec->body);
            S_endScope(env);
         }
         break;
      }

      case A_varDec: {
         traverseExp(env, depth, get_vardec_init(d));
         get_vardec_escape(d) = FALSE;
         S_enter(env, get_vardec_var(d), E_EscEntry(depth, &get_vardec_escape(d)));
         break;
      }
      default:;
   }
}