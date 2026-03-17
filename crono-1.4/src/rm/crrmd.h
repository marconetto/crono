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
#include <sys/time.h> 
#include <math.h>
#define NOUSER          -1
#define BUFNODES        50000 //buffer to store the nodes in the queue file
#define MAX_REQS        999
#define MAX_SIZE_NPROCS 5   //max size of the string which has the number of
                            // processes (ex.: '32' = 2)
typedef struct
{  int amport,
       rmport,
       nmport,
       share_nodes,  //number of users that can share the nodes at the same time
       single,       //single machine or a cluster
       conntimeout,  //Connection Timeout
       tmpreps,
       tmpostps;
   char logfile[S_PATH],
        queuefile[S_PATH],
        amhost[MAXHOSTNAMELEN],
        cluster[MAXHOSTNAMELEN],
        piddir[S_PATH];
}SCrm;

typedef struct
{ 
   int nprocs;     //number of processes
   char *type,     //type name (ex.: ipf,e800,e60)
        *arch,     //architecture name (ex.: ia64,ia32)
        *desc,     //node description
        *typebase;     //if this type is based on another
        
}SRMTypenode; //Node Type information

typedef struct
{ 
   char *host; //node hostname
   int ntypes,
       *idtypes;
}SRMnode;    //Node information

extern char verbose;                  //Verbose mode
extern int nnodes;                    //Number of nodes
extern int rm_sd;                     //Request Manager socket descriptor
extern int netimes;                   //Number of Event times
extern time_t *etimes;                //Event times
extern sem_t semreserve;              //semaphore to control the reserves
extern sem_t semqueue;                //semaphore to use the queue
extern sem_t sem_reserve_alarm;
extern SCrm cluster;                  //Cluster information
extern SReq *firstqueue,*lastqueue;   //queue of requests
extern pthread_t reserve_tid;
extern pthread_mutex_t mutex_pw;
extern SRMnode *nodes;               //Cluster nodes
//#############################################################################
//##PROTOTYPES

void  attend_req(SReq *req);
void  crrm_config(char *file);
void  crrmd_getargs(int argc,char **argv,char *retrieve_queue);
void  crrmd_usage();
char  exists_rid(int req);
int   get_newrid();
void  get_nodesfile();
int   idnodetype2nnodes(int nodetype);
int   nodetype2idnodetype(char *nodetype);
void  reqcpy(SReq **dest, SReq *src);
void  request_nm_lock_node(int uid,int operation);
void  rls_client_using(int uid,int rid);
void  rm_alloc_client(int csd, int packetsize);
void  rm_check_requested_resources_exist(SReq *req);
void  rm_conn_timeout();
void  *rm_handle_client_thread(void *arg);
void  rm_nmc_client(int csd,int packetsize);
int   rm_make_allocation(SReq newreq, char schedule);
void  rm_nodes_client(int csd,int packetsize);
void  rm_qview_client(int csd,int packetsize);
void  rm_qview_get_gaps_now(int nreqs, SGapsnow **gapsnow, int *ngapsnow);
char  rm_qview_get_gaps_now_nodebusy(int node, time_t now, int *timeslot);
void  rm_reset_lock_access_nm();
void  rm_rls_client(int csd,int packetsize);
void  rm_send_allowed_users_to_nm(int csd,int packetsize);
void  rm_send_operation_to_nm(char *node,SNmc nmc,char **output );
void  set_req_status(int rid, int status);
void  rm_sleep (unsigned long secs);
char  *rm_uid2nodes(int uid, char **usernodes,char oper);
time_t set_req_tstart_tfinish(int rid,long time_mpreps);
void  try_to_attend_new_req();
char  uid_time2is_exclusive_allocation(int uid,time_t currenttime);
void  update_freenodes();
char  user_has_node_access(char *node,int uid);
void  *user_time_thread(void *arg);
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
