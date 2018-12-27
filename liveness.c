#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"
#include "table.h"

T Live_gtemp(G_node ig_n) { return G_nodeInfo(ig_n); }

static void enterLiveMap(G_table t, G_node flowNode, Set temps) { G_enter(t, flowNode, temps); }

static Set lookupLiveMap(G_table t, G_node flownode) { return (Set) G_look(t, flownode); }

/* ========== tool functions ================ */
bool Set_in(Set s, T t) {
   for_each(i, s) {
      if (i->head == t)return TRUE;
   }
   return FALSE;
}

unsigned Set_size(Set s) {
   unsigned size = 0;
   for_each(i, s) { ++size; }
   return size;
}

Set Set_diff(Set s1, Set s2) {
   Set ret = NULL;
   for_each(i, s1) {
      if (!Set_in(s2, i->head))
         ret = Temp_TempList(i->head, ret);
   }
   return ret;
}

Set Set_union(Set s1, Set s2) {
   Set ret = NULL;
   for_each(i, s1) {
      ret = Temp_TempList(i->head, ret);
   }
   for_each(i, s2) {
      if (!Set_in(s1, i->head))
         ret = Temp_TempList(i->head, ret);
   }
   return ret;
}

bool Set_equal(Set s1, Set s2) {
   for_each(i, s1) {
      if (!Set_in(s2, i->head))
         return FALSE;
   }
   if (Set_size(s1) != Set_size(s2))
      return FALSE;
   return TRUE;
}

MSet MSet_union(MSet m1, MSet m2) {
   MSet ret = NULL;
   for (MSet m = m1; m; m = m->tail)
      ret = Live_MoveList(m->src, m->dst, ret);
   for (MSet m = m2; m; m = m->tail)
      if (!MSet_in(m1, m->src, m->dst))
         ret = Live_MoveList(m->src, m->dst, ret);
   return ret;
}

MSet MSet_diff(MSet m1, MSet m2) {
   MSet ret = NULL;
   for (MSet a = m1; a; a = a->tail)
      if (!MSet_in(m2, a->src, a->dst))
         ret = Live_MoveList(a->src, a->dst, ret);
   return ret;
}

bool MSet_in(MSet m, G_node src, G_node dst) {
   for (; m; m = m->tail)
      if (m->src == src && m->dst == dst)
         return TRUE;
   return FALSE;
}

MSet Live_MoveList(G_node src, G_node dst, MSet tail) {
   MSet lm = (MSet) checked_malloc(sizeof(*lm));
   lm->src = src;
   lm->dst = dst;
   lm->tail = tail;
   return lm;
}

/* ========== implement begin ================ */
static G_node Ig_Node(G_graph graph, T t);
static void Ig_Edge(G_graph graph, T t1, T t2);
static G_table buildLiveOut();
static G_graph buildIg(G_table liveOut);
static void initContext(G_graph flow);

/* static variables */
static MSet moves;
static G_nodeList precolored;
static G_table degree;
static TAB_table t2n;
static G_graph flowGraph;

struct Live_graph Live_liveness(G_graph flow) {
   initContext(flow);
   /* build liveOut table */
   G_table tab_out = buildLiveOut();
   /* build interference graph */
   G_graph graph = buildIg(tab_out);
   struct Live_graph result = {graph, moves, degree, precolored};
   return result;
}

static void initContext(G_graph flow) {
   moves = NULL;
   precolored = NULL;
   degree = G_empty();
   t2n = TAB_empty();
   flowGraph = flow;
}

G_node Ig_Node(G_graph graph, T t) {
   G_node node = TAB_look(t2n, t);
   if (node) return node;
   node = G_Node(graph, t);
   TAB_enter(t2n, t, node);
   int *count = checked_malloc(sizeof(int));
   *count = 0;
   G_enter(degree, node, count);
   return node;
}

void Ig_Edge(G_graph graph, T t1, T t2) {
   if (t1 == t2 || t1 == F_FP() || t2 == F_FP()) return;
   G_node u = Ig_Node(graph, t1), v = Ig_Node(graph, t2);
   ++*((int *) G_look(degree, u));
   ++*((int *) G_look(degree, v));
   G_addAdj(u, v);
   if (!Set_in(F_registers(), t1))
      G_addEdge(u, v);
   if (!Set_in(F_registers(), t2))
      G_addEdge(v, u);
}


static G_table buildLiveOut() {
   G_table tab_in = G_empty(), tab_out = G_empty();
   G_nodeList flowNodes = G_nodes(flowGraph);
   bool dirty;
   do {
      dirty = FALSE;
      for (G_nodeList nodes = flowNodes; nodes; nodes = nodes->tail) {
         G_node node = nodes->head;
         Set in = lookupLiveMap(tab_in, node);
         Set out = lookupLiveMap(tab_out, node);
         Set inp = Set_union(FG_use(node), Set_diff(out, FG_def(node)));
         Set outp = NULL;
         for (G_nodeList succ = G_succ(node); succ; succ = succ->tail)
            outp = Set_union(outp, lookupLiveMap(tab_in, succ->head));
         if (!Set_equal(in, inp)) {
            dirty = TRUE;
            enterLiveMap(tab_in, node, inp);
         }
         if (!Set_equal(out, outp)) {
            dirty = TRUE;
            enterLiveMap(tab_out, node, outp);
         }
      }
   } while (dirty);
   return tab_out;
}

G_graph buildIg(G_table liveOut) {
   Set regs = F_registers();
   G_graph ig = G_Graph();
   G_nodeList flowNodes = G_nodes(flowGraph);

   /* precolored nodes */
   for_each(s1, regs) {
      for_each(s2, regs) {
         Ig_Edge(ig, s1->head, s2->head);
      }
      precolored = G_NodeList(Ig_Node(ig, s1->head), precolored);
   }
   /* def nodes */
   for (G_nodeList nodes = flowNodes; nodes; nodes = nodes->tail) {
      for_each(def, FG_def(nodes->head)) {
         Ig_Node(ig, def->head);
      }
   }
   /* add edges */
   for (G_nodeList nodes = flowNodes; nodes; nodes = nodes->tail) {
      G_node node = nodes->head;
      Set out = lookupLiveMap(liveOut, node);
      if (FG_isMove(node)) {
         out = Set_diff(out, FG_use(node));
         for_each(def, FG_def(node)) {
            for_each(use, FG_use(node)) {
               if (use->head != F_FP() && def->head != F_FP())
                  moves =
                          MSet_union(
                                  Live_MoveList(
                                          Ig_Node(ig, use->head),
                                          Ig_Node(ig, def->head), NULL),
                                  moves
                          );
            }
         }
      }
      for_each(def, FG_def(node)) {
         for_each(outs, out) {
            Ig_Edge(ig, def->head, outs->head);
         }
      }
   }
   return ig;
}

