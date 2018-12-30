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

T Live_gtemp(G ig_n) { return G_nodeInfo(ig_n); }

static void enterLiveMap(G_table t, G flowNode, TSet temps) { G_enter(t, flowNode, temps); }

static TSet lookupLiveMap(G_table t, G flownode) { return (TSet) G_look(t, flownode); }

MSet Live_MoveList(G src, G dst, MSet tail) {
   MSet lm = (MSet) checked_malloc(sizeof(*lm));
   lm->src = src;
   lm->dst = dst;
   lm->tail = tail;
   return lm;
}

/* ============================ set operators ============================ */
bool T_in(TSet s, T t) {
   forEachTemp(i, s) if (i->head == t)return TRUE;
   return FALSE;
}

unsigned T_size(TSet s) {
   unsigned size = 0;
   forEachTemp(i, s) ++size;
   return size;
}

TSet T_diff(TSet s1, TSet s2) {
   TSet ret = NULL;
   forEachTemp(i, s1) {
      if (!T_in(s2, i->head))
         ret = Temp_TempList(i->head, ret);
   }
   return ret;
}

TSet T_union(TSet s1, TSet s2) {
   TSet ret = NULL;
   forEachTemp(i, s1) {
      ret = Temp_TempList(i->head, ret);
   }
   forEachTemp(i, s2) {
      if (!T_in(s1, i->head))
         ret = Temp_TempList(i->head, ret);
   }
   return ret;
}

bool T_equal(TSet s1, TSet s2) {
   forEachTemp(i, s1) {
      if (!T_in(s2, i->head))
         return FALSE;
   }
   if (T_size(s1) != T_size(s2))
      return FALSE;
   return TRUE;
}

MSet M_union(MSet m1, MSet m2) {
   MSet ret = NULL;
   forEachMove(m, m1) {
      ret = Live_MoveList(m->src, m->dst, ret);
   }
   forEachMove(m, m2) {
      if (!M_in(m1, m->src, m->dst))
         ret = Live_MoveList(m->src, m->dst, ret);
   }
   return ret;
}

MSet M_diff(MSet m1, MSet m2) {
   MSet ret = NULL;
   forEachMove(m, m1) {
      if (!M_in(m2, m->src, m->dst))
         ret = Live_MoveList(m->src, m->dst, ret);
   }
   return ret;
}

bool M_in(MSet m, G src, G dst) {
   forEachMove(m0, m)if (m0->src == src && m0->dst == dst) return TRUE;
   return FALSE;
}

bool G_in(GSet g, G n) {
   forEachNode(g0, g) if (g0->head == n)return TRUE;
   return FALSE;
}

GSet G_union(GSet u, GSet v) {
   GSet ret = NULL;
   forEachNode(g0, u) {
      ret = G_NodeList(g0->head, ret);
   }
   forEachNode(g0, v) {
      if (!G_in(u, g0->head))
         ret = G_NodeList(g0->head, ret);
   }
   return ret;
}

GSet G_diff(GSet u, GSet v) {
   GSet ret = NULL;
   forEachNode(i, u) {
      if (!G_in(v, i->head))
         ret = G_NodeList(i->head, ret);
   }
   return ret;
}

/* ============================ implement begin ============================ */
static G Ig_Node(G_graph graph, T t);
static void Ig_Edge(G_graph graph, T t1, T t2);
static G_table buildLiveOut();
static G_graph buildIg(G_table liveOut);
static void initContext(G_graph flow);

/* static variables */
static MSet moves;
static GSet precolored;
static G_table priorities;
static TAB_table t2n; // just for better performance
static GSet flowNodes;

struct Live_graph Live_liveness(G_graph flow) {
   initContext(flow);
   G_table tab_out = buildLiveOut();
   G_graph graph = buildIg(tab_out);
   /* return interference graph, movelist, precolored nodes and priorities */
   struct Live_graph result = {graph, moves, priorities, precolored};
   return result;
}

static void initContext(G_graph flow) {
   moves = NULL;
   precolored = NULL;
   priorities = G_empty();
   t2n = TAB_empty();
   flowNodes = G_rnodes(flow);
}

G Ig_Node(G_graph graph, T t) {
   G node = TAB_look(t2n, t);
   if (node) return node;
   node = G_Node(graph, t);
   TAB_enter(t2n, t, node);
   int *count = checked_malloc(sizeof(int));
   *count = 0;
   G_enter(priorities, node, count);
   return node;
}

void Ig_Edge(G_graph graph, T t1, T t2) {
   if (t1 == t2 || t1 == F_FP() || t2 == F_FP()) return;
   G u = Ig_Node(graph, t1), v = Ig_Node(graph, t2);
   ++*((int *) G_look(priorities, u));
   ++*((int *) G_look(priorities, v));
   G_addAdj(u, v);
   if (!T_in(F_registers(), t1))
      G_addEdge(u, v);
   if (!T_in(F_registers(), t2))
      G_addEdge(v, u);
}


static G_table buildLiveOut() {
   G_table tab_in = G_empty(), tab_out = G_empty();
   bool dirty;
   do {
      dirty = FALSE;
      forEachNode(nodes, flowNodes) {
         G node = nodes->head;
         TSet in = lookupLiveMap(tab_in, node);
         TSet out = lookupLiveMap(tab_out, node);

         /* in = use U (out-def) out = U(succ) in */
         TSet inp = T_union(FG_use(node), T_diff(out, FG_def(node)));
         TSet outp = NULL;
         forEachNode(succ, G_succ(node)) {
            outp = T_union(outp, lookupLiveMap(tab_in, succ->head));
         }
         if (!T_equal(in, inp)) {
            dirty = TRUE;
            enterLiveMap(tab_in, node, inp);
         }
         if (!T_equal(out, outp)) {
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
      G node = nodes->head;
      TSet out = lookupLiveMap(liveOut, node);
      if (FG_isMove(node)) {
         out = T_diff(out, FG_use(node));
         /* build move list */
         forEachTemp(def, FG_def(node)) {
            forEachTemp(use, FG_use(node)) {
               if (use->head != F_FP() && def->head != F_FP())
                  moves = M_union(Live_MoveList(Ig_Node(ig, use->head), Ig_Node(ig, def->head), NULL), moves);
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

