/*
 * temp.c - functions to create and manipulate temporary variables which are
 *          used in the IR tree representation before it has been determined
 *          which variables are to go into registers.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "frame.h"

struct Temp_temp_ {
   int num;
};

int Temp_int(Temp_temp t) {
   return t->num;
}

string Temp_labelstring(Temp_label s) {
   return S_name(s);
}

static int labels = 0;

Temp_label Temp_newlabel(void) {
   char buf[100];
   sprintf(buf, "L%d", labels++);
   return Temp_namedlabel(String(buf));
}

/* The label will be created only if it is not found. */
Temp_label Temp_namedlabel(string s) {
   return S_Symbol(s);
}

static int temps = 100;

Temp_temp Temp_newtemp(void) {
   Temp_temp p = (Temp_temp) checked_malloc(sizeof(*p));
   p->num = temps++;
   {
      char r[16];
      sprintf(r, "%d", p->num);
      Temp_enter(Temp_nameMap(), p, String(r));
   }
   return p;
}


struct Temp_map_ {
   TAB_table tab;
   Temp_map under;
};


Temp_map Temp_nameMap(void) {
   static Temp_map m = NULL;
   if (!m) m = Temp_empty();
   return m;
}

Temp_map newMap(TAB_table tab, Temp_map under) {
   Temp_map m = checked_malloc(sizeof(*m));
   m->tab = tab;
   m->under = under;
   return m;
}

Temp_map Temp_empty(void) {
   return newMap(TAB_empty(), NULL);
}

Temp_map Temp_layerMap(Temp_map over, Temp_map under) {
   if (over == NULL)
      return under;
   else return newMap(over->tab, Temp_layerMap(over->under, under));
}

void Temp_enter(Temp_map m, Temp_temp t, string s) {
   assert(m && m->tab);
   TAB_enter(m->tab, t, s);
}

string Temp_look(Temp_map m, Temp_temp t) {
   string s;
   assert(m && m->tab);
   s = TAB_look(m->tab, t);
   if (s) return s;
   else if (m->under) return Temp_look(m->under, t);
   else return NULL;
}

Temp_tempList Temp_TempList(Temp_temp h, Temp_tempList t) {
   Temp_tempList p = (Temp_tempList) checked_malloc(sizeof(*p));
   p->head = h;
   p->tail = t;
   return p;
}

Temp_labelList Temp_LabelList(Temp_label h, Temp_labelList t) {
   Temp_labelList p = (Temp_labelList) checked_malloc(sizeof(*p));
   p->head = h;
   p->tail = t;
   return p;
}

static FILE *outfile;

void showit(Temp_temp t, string r) {
   fprintf(outfile, "t%d -> %s\n", t->num, r);
}

void Temp_dumpMap(FILE *out, Temp_map m) {
   outfile = out;
   TAB_dump(m->tab, (void (*)(void *, void *)) showit);
   if (m->under) {
      fprintf(out, "---------\n");
      Temp_dumpMap(out, m->under);
   }
}

Temp_tempList L(unsigned argsNum, ...) {
   Temp_tempList ret = NULL;
   va_list arglist;
   va_start(arglist, argsNum);
   for (unsigned i = 0; i < argsNum; ++i)
      ret = Temp_TempList(va_arg(arglist, Temp_temp), ret);
   va_end(arglist);
   return ret;
}

unsigned int Lsize(Temp_tempList l) {
   unsigned int ret = 0;
   for (; l; l = l->tail)
      ++ret;
   return ret;
}

bool Temp_insertList(Temp_tempList *lp, Temp_temp t) {
   Temp_tempList l = *lp;
   if (!Temp_inTempList(l, t)) {
      l = Temp_TempList(t, l);
      *lp = l;
      return TRUE;
   } else return FALSE;
}

bool Temp_inTempList(Temp_tempList l, Temp_temp t) {
   for (; l; l = l->tail)
      if (l->head == t)
         return TRUE;
   return FALSE;
}

string Temp_name(Temp_temp t) {
   static Temp_map name_m;
   if (!name_m)
      name_m = Temp_layerMap(F_nameMap(), Temp_nameMap());
   return Temp_look(name_m, t);
}