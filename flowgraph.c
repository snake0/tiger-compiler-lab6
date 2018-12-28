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
   G_graph flow = G_Graph();
   G_node node, prevNode = NULL;
   AS_instr instr;
   TAB_table label_table = TAB_empty();
   if (il) {
      node = G_Node(flow, il->head);
      if (il->head->kind == I_LABEL)
         TAB_enter(label_table, il->head->u.LABEL.label, node);
      il = il->tail;
      prevNode = node;
   }
   for (; il; il = il->tail, prevNode = node) {
      instr = il->head;
      node = G_Node(flow, instr);
      G_addEdge(prevNode, node);
      if (instr->kind == I_LABEL)
         TAB_enter(label_table, instr->u.LABEL.label, node);
   }
   G_nodeList nodes = G_nodes(flow);
   for (; nodes; nodes = nodes->tail) {
      instr = G_nodeInfo(nodes->head);
      if ((instr->kind == I_OPER && instr->u.OPER.jumps)) {
         Temp_labelList labels = instr->u.OPER.jumps->labels;
         for (; labels; labels = labels->tail) {
            G_node succ = TAB_look(label_table, labels->head);
            G_addEdge(nodes->head, succ);
         }
      }
   }
   return flow;
}
