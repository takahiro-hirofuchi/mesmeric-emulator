/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */
#ifndef __COMMON_H
#define __COMMON_H

// for error log
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE);} while(0)

#define exit_with_message(...) \
    do { fprintf(stderr,__VA_ARGS__); exit(EXIT_FAILURE);} while(0)

#ifdef DEBUG
#define DEBUG_PRINT(...)\
    (printf("%s %u @%s: ",__FILE__,__LINE__,__func__),\
     printf(""__VA_ARGS__))
#else
#define DEBUG_PRINT(...)
#endif

#endif
