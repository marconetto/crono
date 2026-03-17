//##############################################################################
//# Copyright (C) 2002-2004 Marco Aurelio Stelmar Netto <stelmar@cpad.pucrs.br>
//#
//# This program is free software; you can redistribute it and/or modify
//# it under the terms of the GNU General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or
//# (at your option) any later version.
//#
//# This program is distributed in the hope that it will be useful,
//# but WITHOUT ANY WARRANTY; without even the implied warranty of
//# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//# GNU General Public License for more details.
//#
//# You should have received a copy of the GNU General Public License
//# along with this program; if not, write to the Free Software
//# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
//#############################################################################

#ifndef _COMMON_H
#define _COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <pwd.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

#include <sys/stat.h>
#include <unistd.h>


#define NO_SHARE 0       // 0 not shared among processes. Semaphores 
#define TRUE  1
#define FALSE 0


#define crvprintf(args...) do{ if(verbose) printf(args);} while(0)

#if DEBUG
#define dprintf(args...) do {printf(args);} while(0)
#else
#define dprintf(args...)
#endif


#define CR_ERROR  -1 //crono error
#define CR_LOG     2  //just log(withou error)

#define ERROR(msg,par...)  do{ fprintf(stderr,msg,##par);  fprintf(stderr,"\n");exit(1); }while(0)

#define ERROR_FILE_LOG(file) do {fprintf(stderr,"%s: Impossible writing this file.\n" \
                                 "Try to change the path of the logfile.\n",file); exit(1); } while(0)

#define ERROR_SOCKET_CREATE_RETURN(msg,par...) do{ fprintf(stderr,msg,##par); return CR_ERROR; }while(0)
#define ERROR_ALLOC_MEMORY_STDERR do{  fprintf(stderr,"Failed to allocate memory\n"); exit(1); }while(0)

//Operations
#define CR_QVIEW                1
#define CR_ALLOC                2
#define CR_INFO                 3
#define CR_RLS                  4
#define CR_NODES                5
#define CR_GET_ALLOWED_USERS    7  //request from NM to RM
#define CR_NMC_GET_OPERATIONS   8  //request from UI to AM
#define CR_NMC_EXEC_OPERATION   9  //request from UI to AM (AM forwards to RM)

#define CR_CHECK_SYNTAX_FILE    20

                                 
//Allocations
#define CR_ALLOC_SOON     0       //allocate resources as soon as possible
#define CR_ALLOC_FUTURE   1       //allocate resources at a given time
                                 
//Schedule (STATUS)
#define CR_SCHED_STATUS_READY     1
#define CR_SCHED_STATUS_QUEUED    2
#define CR_SCHED_STATUS_RESERVED  3
#define CR_SCHED_STATUS_MPREPS    4
#define CR_SCHED_STATUS_MPOSTPS   5
#define CR_SCHED_STATUS_UPREPS    6
#define CR_SCHED_STATUS_UPOSTPS   7
#define CR_SCHED_STATUS_BATCH     8
#define CR_SCHED_STATUS_QUEUED_C  9 //queued with a conflict with a reserve


//view nodes (crnodes) operations
#define CRNODES_USERNODES         0  //view user nodes
#define CRNODES_USERNODES_INFO    1  //view user nodes + node information
#define CRNODES_ALLNODES          2  //view all cluster nodes
#define CRNODES_ALLNODES_INFO     3  //view all cluster nodes + node inf.

//Node Manager
#define CR_NM_UNLOCK_ACCESS       1
#define CR_NM_LOCK_ACCESS         2
#define CR_NM_KILL_PROC           3
#define CR_NM_RESET_LOCK_ACCESS   4
#define CR_NM_KILL_PROC_LOCK      5 

//Errors                                //they must be negative!!!
#define CR_ERROR_TRANSFER_DATA          -10
#define CR_ERROR_ALLOC_MEMORY           -11

#define CR_EAM_NOT_ACCESS               -30   //User doesn't have access
#define CR_EAM_INVALID_CLUSTER          -31   //Invalid cluster name 
#define CR_EAM_INVALID_TIME             -32   //Invalid time (cralloc)

#define CR_EAM_INVALID_NNODES           -33   //Invalid arguments
#define CR_EAM_INVALID_OPERATION        -34   //Invalid operation (crnmc) 
#define CR_EAM_CANNOT_CONNECT_RM        -35   //RM is down

#define CR_EAM_CANNOT_CONNECT_AM         -36
#define CR_EAM_NO_OPERATIONS_AVAILABLE   -37
#define CR_EAM_INVALID_NODETYPE          -38    //some node type is wrong


#define CR_ERM_RESERVE_COLLISION   -50   //Collision detected on reserve
#define CR_ERM_USER_NOT_QUEUED     -51   //user trying to release,
                                         //but he/she doesn't make a request yet

#define CR_ERM_NO_OPERATIONS_EXECUTED          -52 //crnmc
#define CR_ERM_USER_ALREADY_ALLOC              -53 //cralloc
#define CR_RESERVE_TSTART_GREATER_TFINISH      -54 //cralloc
#define CR_RESERVE_TSTART_LESSER_NOW           -55 //cralloc
#define CR_ERM_RESERVE_UID_EXISTS_THIS_PERIOD  -56 //cralloc
#define CR_ERM_NRESERVES_EXCEEDED              -57 //cralloc
#define CR_RESERVE_NOT_DELETED                 -58 //crrls

//good answers
#define CR_OK                  1 
#define CR_REQ_QUEUED          2
#define CR_RESOURCES_RELEASED  3
#define CR_REQUEST_CANCELLED   4


#define AM 0         //Access Manager
#define RM 1         //Request Manager

//socket options
#define LINGER_SECS 1
#define QLEN     20     //size of request queue


#define S_PATH                256     //max size of path 
#define S_LINEF               256     //max size of a line file
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN        64
#endif
#define CR_MAX_COMMAND_SIZE   256
#define CR_MAX_MSG            100

#ifndef _POSIX_LOGIN_NAME_MAX
#define _POSIX_LOGIN_NAME_MAX 9
#endif

#define CR_DEFAULT_TMPREPS    2       //master pre-processing(seconds)
#define CR_DEFAULT_TMPOSTPS   2       //master post-processing(seconds)

#define PWD_BUFFER           256      //used by getpwuid_r() function

typedef struct
{ 
   int type,
       size;
}SPheader;            //Crono Packet Header

typedef struct
{
   int nnodes;  //number of nodes
   char *type;  //type of the nodes 
}STypennodes;    

typedef struct
{ 
   int uid,           //userid 
       requid,         //requester id
       time,           //time of allocation
       nnodes,         //number of nodes
       when,           //allocation (as soon as) and reserve (at a given time)
       nreserves;      //number of reserves allowed to be queued
                       //  nreserves is defined and set by the Access Manager

   long tstart,        //start time
        tfinish;       //finish time
 
   char shared,        //share nodes with other users
        qualitative;   //qualitative or quantitative allocation
   char *cluster,      //cluster name
        *bjscript,     //batch job script
        *path;         //working path
                    
   void *nodes;        //<ntypes><node type><nnodes><node type><nnodes>....
                       //*nodes -> used by the qualitative allocation

}SAlloc;            //Struct for allocation

#define S_PACK_SALLOC  (sizeof(SAlloc)-4*sizeof(void*)) //size of sockets packet


typedef struct
{
   int uid;       //userid
   char oper,     //user nodes, all cluster nodes, with or without information
        *cluster; //cluster name
}SNodes;

#define S_PACK_SNODES  (sizeof(SNodes)-sizeof(void*)) //size of sockets packet

typedef struct
{  int uid;         //userid
   char *cluster;   //cluster name
}SInforeq;         //used for execute crinfo

typedef struct
{  int uid,         //userid
       requid,      //requester id
       rid;         //request id
   char *cluster;   //cluster name
}SRls;

typedef struct
{
   int uid;         //userid
   char type,       //show information regarding node types
        *cluster;   //cluster name
}SQview;

#define S_PACK_SQVIEW  (sizeof(SQview)-sizeof(void*)) //size of sockets packet


typedef struct
{ 
   int nnodes,     //number of nodes
       nshare,     //share option
       fshare,     //free nodes(share)
       fexclusive, //free node(exclusive)
       nreqs;      //number of requests in the queue
   time_t time;    //RM time
}SQview_rminfo;    //Some information about the Request Manager

typedef struct
{
   int nnodes,    //number of nodes
       idtype;    //type identifier
}SReqnodetype;    //Number of nodes of each type requested

typedef struct sreq
{
   int uid,             //userid
       rid,             //requisition id
       nnodes,          //number of nodes
       time,            //time of allocation
       status;          //status of request
   long tstart,tfinish; //times(start,finish)

   char qualitative,    //qualitative or quantitative allocation
        shared,          //allocation mode(temporal,spatial)
        when,           //allocation (as soon as) and reserve (at a given time)
        exec_tpreps;    //execute pre-processing (flag)

   pid_t sid;           //session id (for batch job mode)
   pthread_t  tid; 
   int ntypes,              //number of node types
       *nodes;              //user nodes list
   SReqnodetype *nodetypes; //Number of nodes of each type requested
   char *bjscript,          //batch job script
        *path,              //user working path
        *additinfo;         //additional information
                            //  Only used in user interface
                            //  (it can be user login or nodetype name)
   struct sreq *prox;
}SReq;                      //requests in RM's queue

#define S_PACK_SREQ  (sizeof(SReq)-\
                       (sizeof(pid_t)+sizeof(pthread_t)+sizeof(int)+5*(sizeof(void*))))
                     //size of sockets packet


typedef struct
{
   int shared,    
       exclusive; 
}SFreenodes;    //Number of free nodes 


typedef struct
{  int n_time,           //time of allocation(normal)
       s_time,           //time of allocation(special)
       n_nnodes,         //number of nodes(normal)
       s_nnodes,         //number of nodes(special)
       n_nreserves,      //number of reserves(normal)
       s_nreserves;      //number of reserves(special)
   char *nodetype;       //node type (one or more separed by ',')
}SInfo;  //Information about access rights

#define S_PACK_SINFO      (sizeof(SInfo)-sizeof(void*)) //size of sockets packet

typedef struct
{
   int uid,          //userid
       oper,         //exec or get operations
       all;          //flag(all nodes)
   char *cluster,    //cluster name
        *operation,  //user operation
        *nodes;      //list of nodes
}SNmc;               //Node Manager Client

typedef struct
{ 
  int amport,      //Access Manager Port
      rmport,      //Request Manager Port
      nmport,      //Node Manager Port
      share,       //number of users that can share the nodes at same the time 
      single,      //single machine or cluster
      pam,         //PAM support
      conntimeout, //Connection Timeout 
      tmpreps,     //time for the master pre-processing execution
      tmpostps;    //time for the master post-processing execution
 
   char logfile[S_PATH],
        queuefile[S_PATH],
        piddir[S_PATH],
        amhost[MAXHOSTNAMELEN],
        rmhost[MAXHOSTNAMELEN],
        cluster[MAXHOSTNAMELEN];
}SConf; //General struture for the daemons configuration

typedef struct
{
   int timeslot,
       nnodes;
   
}SGapsnow; //Used in showgapsnow UI API function (and in RM as well)


void cr_file_error(FILE *fp,char *fmt,...);
void cr_kill_daemon(char *daemon,char *cluster,char *piddir);
int  cr_pid_file_exists(char *daemon,char *cluster,char *piddir);
void cr_write_pid(char *daemon,char *cluster,char *piddir,char *logfile);
int  create_client_tcpsocket(char *host,int port);
void create_path(char *file);
int  create_server_tcpsocket(int port);
void crget_config(char *cluster,SConf *conf,char *dirconf);
void crono_log(char *logfile,int er, char *format,...);
char file_exists(char *file);
int  login2uid(char *login);
char file_permission_to_write(char *file);
int  uid2gid(int uid,int *gid);
char *uid2login(int uid, char *login);
char *uid2home(int uid, char *home);

#endif
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
