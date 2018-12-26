#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "errormsg.h"
#include "table.h"

Temp_tempList FG_def(G_node n) {
   AS_instr i = G_nodeInfo(n);
   switch (i->kind) {
      case I_OPER:
         return i->u.OPER.dst;
      case I_LABEL:
         return 0;
      case I_MOVE:
         return i->u.MOVE.dst;
   }
   return 0;
}

Temp_tempList FG_use(G_node n) {
   AS_instr i = G_nodeInfo(n);
   switch (i->kind) {
      case I_OPER:
         return i->u.OPER.src;
      case I_LABEL:
         return 0;
      case I_MOVE:
         return i->u.MOVE.src;
   }
   return 0;
}

bool FG_isMove(G_node n) {
   AS_instr i = G_nodeInfo(n);
   return (i)->kind == I_MOVE;
}

G_graph FG_AssemFlowGraph(AS_instrList il, F_frame f) {
   G_graph afg = G_Graph();
   G_node n = NULL, prev_n = NULL;
   AS_instr i = NULL, prev_i = NULL;
   TAB_table label_table = TAB_empty();

   for (; il; il = il->tail, prev_n = n, prev_i = i) {
      i = il->head;
      n = G_Node(afg, i);
      if (prev_n && !(prev_i->kind == I_OPER && !prev_i->u.OPER.cond)) //TODO ?
         G_addEdge(prev_n, n);
      if ((i)->kind == I_LABEL)
         TAB_enter(label_table, i->u.LABEL.label, n);
   }

   G_nodeList nodes = G_nodes(afg);
   for (; nodes; nodes = nodes->tail) {
      i = G_nodeInfo(nodes->head);
      if ((i->kind == I_OPER && i->u.OPER.jumps)) {
         Temp_labelList labels = i->u.OPER.jumps->labels;
         for (; labels; labels = labels->tail) {
            G_node succ = TAB_look(label_table, labels->head);
            G_addEdge(nodes->head, succ);
         }
      }
   }
   return afg;
}
