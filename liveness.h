#ifndef LIVENESS_H
#define LIVENESS_H

#define forEachTemp(i, s) for(TSet (i) = (s); ((i) != NULL); (i) = (i)->tail)
#define forEachNode(i, l) for(GSet (i) = (l); ((i) != NULL); (i) = (i)->tail)
#define forEachMove(i, m) for(MSet (i) = (m); ((i) != NULL); (i) = (i)->tail)

typedef struct Live_moveList_ *Live_moveList;

typedef G_nodeList GSet;
typedef Temp_tempList TSet;
typedef Live_moveList MSet;
typedef Temp_temp T;
typedef G_node G;

/* tset functions */
bool T_in(TSet s, T t);
unsigned T_size(TSet s);
TSet T_diff(TSet s1, TSet s2);
TSet T_union(TSet s1, TSet s2);
bool T_equal(TSet s1, TSet s2);


/* mset functions */
struct Live_moveList_ {
   G src, dst;
   MSet tail;
};
MSet Live_MoveList(G src, G dst, MSet tail);
MSet M_union(MSet m1, MSet m2);
MSet M_diff(MSet m1, MSet m2);
bool M_in(MSet m, G src, G dst);

/* gset functions */
GSet G_union(GSet u, GSet v);
GSet G_diff(GSet u, GSet v);
bool G_in(GSet g, G n);


struct Live_graph {
   G_graph graph;
   MSet moves;
   G_table degree;
   GSet precolored;
};


Temp_temp Live_gtemp(G ig_n);

struct Live_graph Live_liveness(G_graph flow);

#endif
