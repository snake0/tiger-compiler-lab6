
/*Lab5: This header file is not complete. Please finish it with more definition.*/

#ifndef FRAME_H
#define FRAME_H

#include "tree.h"

#define F_REGNUM 16

extern const int F_wordsize;
extern Temp_temp (*F_REG[F_REGNUM])(void);
extern char *F_REGNAME[F_REGNUM];

typedef struct F_frame_ *F_frame;
typedef struct F_access_ *F_access;

typedef struct F_accessList_ *F_accessList;
struct F_accessList_ {F_access head;F_accessList tail;};

typedef struct F_frag_ *F_frag;
struct F_frag_ {
	enum {F_stringFrag, F_procFrag} kind;
	union {
		struct {Temp_label label;string str;} stringg;
		struct {T_stm body;F_frame frame;} proc;
	} u;
};

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ {F_frag head;F_fragList tail;};

Temp_temp F_FP(void);
Temp_temp F_RV(void);
Temp_temp F_RAX(void);
Temp_temp F_RBX(void);
Temp_temp F_RCX(void);
Temp_temp F_RDX(void);
Temp_temp F_RSI(void);
Temp_temp F_RDI(void);
Temp_temp F_RBP(void);
Temp_temp F_RSP(void);
Temp_temp F_R8(void);
Temp_temp F_R9(void);
Temp_temp F_R10(void);
Temp_temp F_R11(void);
Temp_temp F_R12(void);
Temp_temp F_R13(void);
Temp_temp F_R14(void);
Temp_temp F_R15(void);

Temp_map F_tempMap(void);
Temp_tempList F_registers(void);
Temp_tempList F_callersaves(void);
Temp_tempList F_calleesaves(void);

F_accessList F_AccessList(F_access head, F_accessList tail);
F_frame F_newFrame(Temp_label name, U_boolList formals);
F_accessList F_formals(F_frame f);
F_access F_allocLocal(F_frame f, bool escape);
F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);
F_fragList F_FragList(F_frag head, F_fragList tail);
T_exp F_externalCall(string s, T_expList args);
T_exp F_Exp(F_access access, T_exp fp);
Temp_label F_name(F_frame f);

#endif
