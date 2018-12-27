#ifndef LIVENESS_H
#define LIVENESS_H

#define forEachTemp(i, s) for(TSet (i) = (s); (i); (i) = (i)->tail)
#define forEachNode(i, l) for(GSet (i) = (l); (i); (i) = (i)->tail)
#define forEachMove(i, m) for(MSet (i) = (m); (i); (i) = (i)->tail)

typedef struct Live_moveList_ *Live_moveList;

typedef G_nodeList GSet;
typedef Temp_tempList TSet;
typedef Live_moveList MSet;

/* tset functions */
typedef Temp_temp T;
bool TSet_in(TSet s, T t);
unsigned TSet_size(TSet s);
TSet TSet_diff(TSet s1, TSet s2);
TSet TSet_union(TSet s1, TSet s2);
bool TSet_equal(TSet s1, TSet s2);

/* mset functions */
struct Live_moveList_ {
   G_node src, dst;
   MSet tail;
};
MSet Live_MoveList(G_node src, G_node dst, MSet tail);
MSet MSet_union(MSet m1, MSet m2);
MSet MSet_diff(MSet m1, MSet m2);
bool MSet_in(MSet m, G_node src, G_node dst);


struct Live_graph {
   G_graph graph;
   MSet moves;
   G_table degree;
   GSet precolored;
};



Temp_temp Live_gtemp(G_node ig_n);

struct Live_graph Live_liveness(G_graph flow);

#endif
