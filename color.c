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

static const int K = 16;
static unsigned inf = (unsigned) -1;
/* ============================ static var ============================ */

/* ============================ prototypes ============================ */
static void build(struct Live_graph lg, TSet regs);
static void simplify(void);
static void coalesce(void);
static void freeze(void);
static void selectSpill(void);


/* ============================ worker functions ============================ */
void build(struct Live_graph lg, TSet regs){

}


static bool isPrecolored(G_node ig_n);

bool isPrecolored(G_node ig_n) {
   T t = Live_gtemp(ig_n);
   return T_in(F_registers(), t);
}

struct COL_result COL_color(struct Live_graph lg, TSet regs) {
   build(lg,regs);
   struct COL_result ret;
   return ret;
}
