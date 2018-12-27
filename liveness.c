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

static void enterLiveMap(G_table t, G_node flowNode, TSet temps) { G_enter(t, flowNode, temps); }

static TSet lookupLiveMap(G_table t, G_node flownode) { return (TSet) G_look(t, flownode); }

MSet Live_MoveList(G_node src, G_node dst, MSet tail) {
   MSet lm = (MSet) checked_malloc(sizeof(*lm));
   lm->src = src;
   lm->dst = dst;
   lm->tail = tail;
   return lm;
}

/* ============================ tool functions ============================ */
bool TSet_in(TSet s, T t) {
   forEachTemp(i, s) {
      if (i->head == t)return TRUE;
   }
   return FALSE;
}

unsigned TSet_size(TSet s) {
   unsigned size = 0;
   forEachTemp(i, s) { ++size; }
   return size;
}

TSet TSet_diff(TSet s1, TSet s2) {
   TSet ret = NULL;
   forEachTemp(i, s1) {
      if (!TSet_in(s2, i->head))
         ret = Temp_TempList(i->head, ret);
   }
   return ret;
}

TSet TSet_union(TSet s1, TSet s2) {
   TSet ret = NULL;
   forEachTemp(i, s1) {
      ret = Temp_TempList(i->head, ret);
   }
   forEachTemp(i, s2) {
      if (!TSet_in(s1, i->head))
         ret = Temp_TempList(i->head, ret);
   }
   return ret;
}

bool TSet_equal(TSet s1, TSet s2) {
   forEachTemp(i, s1) {
      if (!TSet_in(s2, i->head))
         return FALSE;
   }
   if (TSet_size(s1) != TSet_size(s2))
      return FALSE;
   return TRUE;
}

MSet MSet_union(MSet m1, MSet m2) {
   MSet ret = NULL;
   forEachMove(m, m1) {
      ret = Live_MoveList(m->src, m->dst, ret);
   }
   forEachMove(m, m2) {
      if (!MSet_in(m1, m->src, m->dst))
         ret = Live_MoveList(m->src, m->dst, ret);
   }
   return ret;
}

MSet MSet_diff(MSet m1, MSet m2) {
   MSet ret = NULL;
   forEachMove(a, m1) {
      if (!MSet_in(m2, a->src, a->dst))
         ret = Live_MoveList(a->src, a->dst, ret);
   }
   return ret;
}

bool MSet_in(MSet m, G_node src, G_node dst) {
   forEachMove(m0, m) {
      if (m0->src == src && m0->dst == dst)
         return TRUE;
   }
   return FALSE;
}

/* ============================ implement begin ============================ */
static G_node Ig_Node(G_graph graph, T t);
static void Ig_Edge(G_graph graph, T t1, T t2);
static G_table buildLiveOut();
static G_graph buildIg(G_table liveOut);
static void initContext(G_graph flow);

/* static variables */
static MSet moves;
static GSet precolored;
static G_table degree;
static TAB_table t2n;
static GSet flowNodes;

struct Live_graph Live_liveness(G_graph flow) {
   initContext(flow);
   G_table tab_out = buildLiveOut();
   G_graph graph = buildIg(tab_out);
   struct Live_graph result = {graph, moves, degree, precolored};
   return result;
}

static void initContext(G_graph flow) {
   moves = NULL;
   precolored = NULL;
   degree = G_empty();
   t2n = TAB_empty();
   flowNodes = G_rnodes(flow);
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
   if (!TSet_in(F_registers(), t1))
      G_addEdge(u, v);
   if (!TSet_in(F_registers(), t2))
      G_addEdge(v, u);
}


static G_table buildLiveOut() {
   G_table tab_in = G_empty(), tab_out = G_empty();
   bool dirty;
   do {
      dirty = FALSE;
      forEachNode(nodes, flowNodes) {
         G_node node = nodes->head;
         TSet in = lookupLiveMap(tab_in, node);
         TSet out = lookupLiveMap(tab_out, node);

         /* in = use U (out-def) out = U(succ) in */
         TSet inp = TSet_union(FG_use(node), TSet_diff(out, FG_def(node)));
         TSet outp = NULL;
         forEachNode(succ, G_succ(node)) {
            outp = TSet_union(outp, lookupLiveMap(tab_in, succ->head));
         }
         if (!TSet_equal(in, inp)) {
            dirty = TRUE;
            enterLiveMap(tab_in, node, inp);
         }
         if (!TSet_equal(out, outp)) {
            dirty = TRUE;
            enterLiveMap(tab_out, node, outp);
         }
      }
   } while (dirty);
   return tab_out;
}

G_graph buildIg(G_table liveOut) {
   TSet regs = F_registers();
   G_graph ig = G_Graph();
   /* precolored nodes */
   forEachTemp(s1, regs) {
      forEachTemp(s2, regs) {
         Ig_Edge(ig, s1->head, s2->head);
      }
      precolored = G_NodeList(Ig_Node(ig, s1->head), precolored);
   }
   /* def nodes */
   forEachNode(nodes, flowNodes) {
      forEachTemp(def, FG_def(nodes->head)) {
         Ig_Node(ig, def->head);
      }
   }
   /* add edges */
   forEachNode(nodes, flowNodes) {
      G_node node = nodes->head;
      TSet out = lookupLiveMap(liveOut, node);
      if (FG_isMove(node)) {
         out = TSet_diff(out, FG_use(node));
         /* build move list */
         forEachTemp(def, FG_def(node)) {
            forEachTemp(use, FG_use(node)) {
               if (use->head != F_FP() && def->head != F_FP())
                  moves = MSet_union(Live_MoveList(Ig_Node(ig, use->head), Ig_Node(ig, def->head), NULL), moves);
            }
         }
      }
      forEachTemp(def, FG_def(node)) {
         forEachTemp(outs, out) {
            Ig_Edge(ig, def->head, outs->head);
         }
      }
   }
   return ig;
}

