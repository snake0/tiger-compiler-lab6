#ifndef LIVENESS_H
#define LIVENESS_H


typedef struct Live_moveList_ *Live_moveList;
struct Live_moveList_ {
   G_node src, dst;
   Live_moveList tail;
};

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail);

struct Live_graph {
   G_graph ig;
   Live_moveList moves;
   G_table priorities;
   G_nodeList precolored;
};

Temp_temp Live_gtemp(G_node ig_n);

struct Live_graph Live_liveness(G_graph fg);

#endif
