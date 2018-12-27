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

static bool isPrecolored(G_node n);

bool isPrecolored(G_node ig_n) {
   Temp_temp t = Live_gtemp(ig_n);
   return Set_in(F_registers(), t);
}

struct COL_result COL_color(G_graph ig, Temp_map initial, Temp_tempList regs, Live_moveList moves) {
   struct COL_result ret;
   return ret;
}
