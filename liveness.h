#ifndef LIVENESS_H
#define LIVENESS_H

/* Set functions */

typedef Temp_tempList Set;
typedef Temp_temp T;

#define for_each(i, s) for(Set i = s; (i); (i) = (i)->tail)

bool Set_in(Set s, T t);
unsigned Set_size(Set s);
Set Set_diff(Set s1, Set s2);
Set Set_union(Set s1, Set s2);
bool Set_equal(Set s1, Set s2);

/* move list functions */
typedef struct Live_moveList_ *Live_moveList;
typedef Live_moveList MSet;

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
   Live_moveList moves;
   G_table degree;
   G_nodeList precolored;
};

Temp_temp Live_gtemp(G_node ig_n);

struct Live_graph Live_liveness(G_graph flow);

#endif
