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

/* static variables */
static G_graph ig;
static Live_moveList moves;
static G_table priorities;
static G_table in_table, out_table;
static G_nodeList precolored;

/* static functions */
static void enterLiveMap(G_table t, G_node fg_n, Temp_tempList temps);
static Temp_tempList lookupLiveMap(G_table t, G_node fg_n);
static void initContext(void);

struct Live_graph Live_liveness(G_graph fg) {
   struct Live_graph lg;
   G_nodeList fg_rns = G_rnodes(fg);
   initContext();

   bool dirty;
   do {
      dirty = FALSE;
      for (G_nodeList fg_ns = fg_rns; fg_ns; fg_ns = fg_ns->tail) {
         G_node fg_n = fg_ns->head;
         Temp_tempList
                 in_set = lookupLiveMap(in_table, fg_n),
                 out_set = lookupLiveMap(out_table, fg_n),
                 use_set = FG_use(fg_n),
                 def_set = FG_def(fg_n);

         for (; use_set; use_set = use_set->tail)
            dirty = Temp_insertList(&in_set, use_set->head);
         for (; out_set; out_set = out_set->tail)
            if (!Temp_inTempList(def_set, out_set->head))
               dirty = Temp_insertList(&in_set, out_set->head);

         out_set = lookupLiveMap(out_table, fg_n);
         G_nodeList succ_ns = G_succ(fg_n);
         for (; succ_ns; succ_ns = succ_ns->tail) {
            Temp_tempList succ_in_set = lookupLiveMap(in_table, succ_ns->head);
            for (; succ_in_set; succ_in_set = succ_in_set->tail) {
               dirty = Temp_insertList(&out_set, succ_in_set->head);
            }
         }
         if (dirty) {
            enterLiveMap(in_table, fg_n, in_set);
            enterLiveMap(out_table, fg_n, out_set);
         }
      }
   } while (dirty);
   lg.ig = ig;
   lg.moves = moves;
   lg.priorities = priorities;
   lg.precolored = precolored;
   return lg;
}

static void enterLiveMap(G_table t, G_node fg_n, Temp_tempList temps) {
   G_enter(t, fg_n, temps);
}

static Temp_tempList lookupLiveMap(G_table t, G_node fg_n) {
   return G_look(t, fg_n);
}


Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
   Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
   lm->src = src;
   lm->dst = dst;
   lm->tail = tail;
   return lm;
}

Temp_temp Live_gtemp(G_node ig_n) {
   return G_nodeInfo(ig_n);
}

void initContext(void) {
   static bool inited = FALSE;
   if (inited) return;
   ig = G_Graph();
   moves = NULL;
   out_table = in_table = priorities = G_empty();
   precolored = NULL;
   inited = TRUE;
}

