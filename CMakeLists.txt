cmake_minimum_required(VERSION 3.13)
project(lab6 C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(lab6
        absyn.c
        absyn.h
        assem.c
        assem.h
        canon.c
        canon.h
        codegen.c
        codegen.h
        color.c
        color.h
        env.c
        env.h
        errormsg.c
        errormsg.h
        escape.c
        escape.h
        flowgraph.c
        flowgraph.h
        frame.h
        graph.c
        graph.h
        helper.h
        liveness.c
        liveness.h
        main.c
        parse.c
        parse.h
        prabsyn.c
        prabsyn.h
        printtree.c
        printtree.h
        regalloc.c
        regalloc.h
        runtime.c
        semant.c
        semant.h
        symbol.c
        symbol.h
        table.c
        table.h
        temp.c
        temp.h
        translate.c
        translate.h
        tree.c
        tree.h
        types.c
        types.h
        util.c
        util.h
        x86frame.c)
