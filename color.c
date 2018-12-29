#include <stdio.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "color.h"
#include "table.h"

/* ============================ static var ============================ */
static const unsigned K = 14;
static const unsigned INF = (unsigned) -1;

static G_graph graph;

static GSet precolored;
static GSet simplifyWorklist, spillWorklist, freezeWorklist;
static GSet coalescedNodes,spilledNodes, coloredNodes;
static GSet selectStack;

static MSet coalescedMoves, constrainedMoves, frozenMoves;
static MSet worklistMoves;
static MSet activeMoves;

/* ============================ prototypes ============================ */
static void build(struct Live_graph lg, TSet regs);
static void simplify(void);
static void coalesce(void);
static void freeze(void);
static void selectSpill(void);


/* ============================ worker functions ============================ */
void build(struct Live_graph lg, TSet regs) {
   precolored = lg.precolored;

}


static bool isPrecolored(G_node ig_n);

bool isPrecolored(G_node ig_n) {
   T t = Live_gtemp(ig_n);
   return T_in(F_registers(), t);
}

struct COL_result COL_color(struct Live_graph lg, TSet regs) {
   build(lg, regs);
   struct COL_result ret;
   return ret;
}
