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
//##############################################################################
#include "common.h"
#include "crrmd.h"
#include "crrm_schedule.h"
#include "crrm_reserve.h"
#include "crrm_em.h"
//############################################################################## 
SCrm cluster;
char dirconf[S_PATH],       //configuration directory
     verbose=FALSE;         //verbose mode
int **idsusing,             //vector of ids of users using the nodes   
    netimes=0,              //number of event times
    ntypes=0,
    nnodes=0;
int rm_sd;                 //Request Manager socket descriptor (extern variable)
time_t *etimes=NULL;

SFreenodes *freenodes=NULL; //[0]-> total,[1]-> for ntype 0,[2] for ntype 1,...
SRMTypenode *nodetypes=NULL; 
SRMnode *nodes=NULL;

sem_t semidsu;              //semaphore to use *idsusing
sem_t semqueue;             //semaphore to use the queue

SReq *firstqueue=NULL,*lastqueue=NULL; //queue of requests
pthread_t reserve_tid=-1;
//##############################################################################
//## CRONO REQUEST MANAGER DAEMON (crrmd)
//##############################################################################
int main (int argc, char **argv)
{
  struct   sockaddr_in cad;       //structure to hold client address
  int csd,                        //client socket descriptor
      alen,                       //length of address
      i;
  char recover_queue=TRUE;       //check if the queue must be retrieved
  pthread_t  hand_tid;  //thread id


  crrmd_getargs(argc,argv,&recover_queue);

  if(cr_pid_file_exists("crrmd",cluster.cluster,cluster.piddir))
  {  printf("Verify if you've already executed crrmd daemon for the \"%s\" "
            "cluster\n",cluster.cluster);
     printf("If not, erase the file: %s/%s_%s.pid\n",cluster.piddir,"crrmd",
            cluster.cluster);
     exit(1);
  }
  if(!file_permission_to_write(cluster.logfile))
     ERROR("%s: Impossible for writing this file.\n"
           "Try to change the path of the logfile.\n",cluster.logfile);

  if(!file_permission_to_write(cluster.queuefile))
     ERROR("%s: Impossible for writing this file.\n"
           "Try to change the path of the queuefile.\n",cluster.queuefile);

  printf("Starting Crono Request Manager daemon version %s\n\n",
         CRONO_VERSION);

  if(!verbose)
  {  if(fork() != 0)
        exit(0);
     fclose(stdout);  
     fclose(stdin);  
     fclose(stderr);  
     if(chdir("/")<0)
     {  perror("chdir");
        exit(1);
     }
     if(setsid()<0)  //release controlling terminal    
     {  perror("setsid");
        exit(1);
     }
     cr_write_pid("crrmd",cluster.cluster,cluster.piddir,cluster.logfile);
  }

  get_nodesfile();
  if( (rm_sd=create_server_tcpsocket(cluster.rmport))  == CR_ERROR)
     ERROR("Cannot create tcp server\n");

  sem_init(&semidsu, NO_SHARE, 1);
  sem_init(&semqueue, NO_SHARE, 1);

  //----setup idsusing vector
  idsusing=(int**)malloc(sizeof(int*)*nnodes);
  for(i=0;i<nnodes;i++)
  {  idsusing[i]=(int*)malloc(sizeof(int)*cluster.share_nodes);
     memset(idsusing[i],NOUSER,sizeof(int)*cluster.share_nodes);
  }
  crvprintf("CLUSTER: %s\nNODES: %d\n",cluster.cluster, nnodes);
  crono_log(cluster.logfile,CR_LOG,"crrmd: RM Server is up. CLUSTER %s"
            " NNODES: %d", cluster.cluster,nnodes);

  if(recover_queue) //recover the queue from disk
     rm_schedule_recover_queue();

  rm_schedule_store_queue();

  if(!cluster.single)
     rm_reset_lock_access_nm();

  rm_reserve_set_alarm_for_reserves();
  update_freenodes();
  crvprintf("RM Server is ready\n");
  while(TRUE)
  {  crvprintf("Waiting conections....\n");
     alen=sizeof(cad); 
     if(  (csd=accept(rm_sd, (struct sockaddr *)&cad, &alen)) < 0)
     {  crvprintf("%s: accept failed\n",argv[0]);
        continue;
     }
     pthread_create(&hand_tid, NULL, rm_handle_client_thread, (void *) csd );
  }
}
//#############################################################################
//## Attend the request 'req'
//#############################################################################
void attend_req(SReq *req)
{
  SReq *user_to_thread=NULL;
  int n,ish;

  crvprintf("# attend_req: Rrequest rid=[%d] uid=[%d]\n",req->rid,req->uid);

  sem_wait(&semidsu);
  //set up the idsusing struct
  for(n=0;n<req->nnodes;n++)
     for(ish=0;ish<cluster.share_nodes;ish++)
        if(idsusing[req->nodes[n]][ish]==NOUSER)
        {  idsusing[req->nodes[n]][ish]=req->uid;
           break;
        }
  sem_post(&semidsu);

  if(req->exec_tpreps)
  {  req->tstart=time(NULL)+cluster.tmpreps;
     req->tfinish=req->tstart+req->time*60;
     req->status=CR_SCHED_STATUS_MPREPS;
  }//else the request was already running

  reqcpy(&user_to_thread,req);

  update_freenodes();
  pthread_create(&user_to_thread->tid,NULL,user_time_thread,
                 (void*)user_to_thread);
  req->tid=user_to_thread->tid;
}
//#############################################################################
//## Parser of command line
//#############################################################################
void crrmd_getargs(int argc,char **argv,char *recover_queue)
{
  char opdir=FALSE,oprecover=FALSE,nclusters=FALSE,opkill=FALSE; //parser
  SConf *conf;

  conf=(SConf*)malloc(sizeof(SConf));
  while (--argc)
  {  argv++;
     if( !strcmp(argv[0],"-d") )
     {  if(nclusters)
           crrmd_usage();
        if( !--argc || opdir )
           crrmd_usage();
        else
        {  argv++;
           strcpy(dirconf,argv[0]);
           opdir=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-v") ) //verbose mode
              verbose=TRUE;
     else if ( !strcmp(argv[0],"--help") ) // show usage of crrmd
               crrmd_usage();
     else if( !strcmp(argv[0],"-k") )
        opkill=TRUE;
     else if( !strcmp(argv[0],"-c") ) //clear queue
     {  *recover_queue=FALSE;
        oprecover=TRUE;
     }
     else
     {  if(nclusters) //A Request Manager manages only one cluster
           crrmd_usage();
        if(!opdir)
        {  crvprintf("Configuration directory wasnt defined. Using default.\n");
           strcpy(dirconf,DIRCONF);
           opdir=TRUE;
        }
        crvprintf("CLUSTER: %s\n",argv[0]);
        crget_config(argv[0],conf,dirconf);
        strcpy(cluster.cluster,argv[0]);
        strcpy(cluster.logfile,conf->logfile);
        strcpy(cluster.queuefile,conf->queuefile);
        strcpy(cluster.piddir,conf->piddir);
        strcpy(cluster.amhost,conf->amhost);
        cluster.amport=conf->amport;
        cluster.rmport=conf->rmport;
        cluster.nmport=conf->nmport;
        cluster.share_nodes=conf->share;
        cluster.single=conf->single;
        cluster.conntimeout=conf->conntimeout;
        cluster.tmpreps=conf->tmpreps;
        cluster.tmpostps=conf->tmpostps;
        nclusters++;
     }
  }
  if(opkill)
  {  if(nclusters==1)
     {  cr_kill_daemon("crrmd",cluster.cluster,cluster.piddir);
        exit(0);
     }
     else
        crrmd_usage();
  }
  
  if(!nclusters) //no cluster specified
     crrmd_usage();
  crvprintf("Dirconf: %s\n",dirconf);
  create_path(cluster.logfile);
  create_path(cluster.queuefile);

  free(conf);
}
//##############################################################################
//## Usage of request manager daemon (crrmd)
//##############################################################################
void crrmd_usage()
{
  printf("Crono v%s\n"
         "Usage: crrmd [-v] [-d <dirconf>] [-c] <cluster>\n"
          "       crrmd -k <cluster>\n"
          "       crrmd --help\n\n"
          "\t-v\t\t: Verbose mode\n"
          "\t-c\t\t: Clear the queue file if it exists\n"
          "\t-d <dirconf>\t: Configuration directory\n"
          "\t-k <cluster>\t: Kill crrmd daemon\n"
          "\t--help\t\t: Display this help and exit\n\n",CRONO_VERSION);
  exit(1);
}
//##############################################################################
//## Check if a request identifier is been used
//##############################################################################
char exists_rid(int reqid)
{
  SReq *req;

  req=firstqueue;
  while(req!=NULL)
  {  if(req->rid==reqid)
        return TRUE;
     req=req->prox;
  }
  return FALSE;
}
//##############################################################################
//## Get a new request identifier
//##############################################################################
int get_newrid()
{
  static int reqids=1;

  do
  {  reqids++;
     if(reqids>=MAX_REQS)
       reqids=1;
  }while(exists_rid(reqids));

  return reqids;
}
//##############################################################################
//## Get the node types and cluster nodes from nodes file and setup 
//## the 'nodes', 'nnodes', 'typenodes' and 'ntypes' variables
//##############################################################################
void get_nodesfile()
{
  #define TAG_GETNODESFILE_WAIT_TYPE_STATEMENT 0
  #define TAG_GETNODESFILE_GET_TYPES 1
  #define TAG_GETNODESFILE_GET_NODES 2
 
  FILE *fp;
  char file[S_PATH], buf[S_LINEF],*type,*arch,*desc,
       tag=TAG_GETNODESFILE_WAIT_TYPE_STATEMENT;
  int i,j,n,nprocs,found;

  snprintf(file,sizeof(file),"%s/%s/nodes",dirconf,cluster.cluster);
  if( (fp=fopen(file,"r")) == NULL)
     crono_log(cluster.logfile,CR_ERROR,"crrmd: Problems to open nodesfile %s",
               file);

  type=(char*)malloc(sizeof(char)*(S_LINEF));
  arch=(char*)malloc(sizeof(char)*(S_LINEF));
  desc=(char*)malloc(sizeof(char)*(S_LINEF));
  while( fgets(buf,sizeof(buf),fp) )
  { 
     buf[strlen(buf)-1]=0;
     i=0;
     //take off spaces, tabs and lines beginning with #
     while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
        i++;
     if( (i==strlen(buf)) || (buf[i]=='#') )
        continue;

     if(!strcasecmp(&buf[i],"types"))
     {  tag=TAG_GETNODESFILE_GET_TYPES;
        continue;
     }
     else if(!strcasecmp(&buf[i],"nodes"))
     {  tag=TAG_GETNODESFILE_GET_NODES;
        //setup missing variables because the use of 'typebase'
        for(n=0;n<ntypes;n++)
        {  if(nodetypes[n].typebase!=NULL)
           {  for(j=0;j<ntypes;j++)
              {  if(!strcmp(nodetypes[n].typebase,nodetypes[j].type))
                 {  free(nodetypes[n].typebase);
                    nodetypes[n].arch=(char*)malloc(sizeof(char)*
                                               (strlen(nodetypes[j].arch)+1));
                    strcpy(nodetypes[n].arch,nodetypes[j].arch);
                    nodetypes[n].nprocs=nodetypes[j].nprocs;
                    break;
                 } 
              }
           }
        }
        if(verbose)  
          for(n=0;n<ntypes;n++)
             printf("# get_nodesfile: %d) type=%s arch=%s nprocs=%d desc=%s\n",
                    n,nodetypes[n].type,nodetypes[n].arch,nodetypes[n].nprocs,
                    nodetypes[n].desc);
        continue;
     } 

     if(tag==TAG_GETNODESFILE_GET_TYPES) //get node types
     {  found=sscanf(&buf[i],"type=%[^,],arch=%[^,],nprocs=%d,desc=%[^\n]",
                            type,arch,&nprocs,desc);
        if(found==4)
        {  nodetypes=(SRMTypenode*)realloc(nodetypes,
                                             sizeof(SRMTypenode)*(ntypes+1));
           nodetypes[ntypes].arch=(char*)malloc(sizeof(char)*
                                                 (strlen(arch)+1));
           strcpy(nodetypes[ntypes].arch,arch);
           nodetypes[ntypes].nprocs=nprocs;
           nodetypes[ntypes].typebase=NULL;
        }
        else
        {  found=sscanf(&buf[i],"type=%[^,],typebase=%[^,],desc=%[^\n]",
                                 type,arch,desc);
           if(found==3)
           {  nodetypes=(SRMTypenode*)realloc(nodetypes,
                                              sizeof(SRMTypenode)*(ntypes+1));
              nodetypes[ntypes].typebase=(char*)malloc(sizeof(char)*
                                                          (strlen(arch)+1));
              strcpy(nodetypes[ntypes].typebase,arch);
           }
        }
        if(found==3 || found==4)
        {  nodetypes[ntypes].type=(char*)malloc(sizeof(char)*
                                                  (strlen(type)+1));
           strcpy(nodetypes[ntypes].type,type);
           nodetypes[ntypes].desc=(char*)malloc(sizeof(char)*
                                                   (strlen(desc)+1));
           strcpy(nodetypes[ntypes].desc,desc);
           ntypes++;
        }
     }
     else if(tag==TAG_GETNODESFILE_GET_NODES) //get the cluster nodes
     {  //'type' and 'arch' are temporary variables
        if(sscanf(&buf[i],"%[^:]:%s",type,arch)==2) //<hostname>:<type>
        {  nodes=(SRMnode*)realloc(nodes,sizeof(SRMnode)*(nnodes+1)); 
           nodes[nnodes].host=(char*)malloc(sizeof(char)*(strlen(type)+1));
           strcpy(nodes[nnodes].host,type);
           nodes[nnodes].ntypes=0;
           nodes[nnodes].idtypes=NULL;
           j=0;
           while(j<strlen(arch))
           {  sscanf(&arch[j],"%[^,]",type);
              nodes[nnodes].idtypes=(int*)realloc(nodes[nnodes].idtypes,
                                         sizeof(int)*(nodes[nnodes].ntypes+1));
              nodes[nnodes].idtypes[nodes[nnodes].ntypes]=
                                                     nodetype2idnodetype(type);
              nodes[nnodes].ntypes++;
              j+=sizeof(char)*(strlen(type)+1);
           }
           nnodes++;
        }
     }
  }
  fclose(fp);
  free(type);
  free(arch);
  free(desc);
  if(verbose)
     for(i=0;i<nnodes;i++)
     {  printf("# get_nodesfile: NODE=%s TYPE=",nodes[i].host);
        for(j=0;j<nodes[i].ntypes;j++)
           printf("%d ",nodes[i].idtypes[j]);
        printf("\n");
     }
  //allocate memory for storing free nodes information
  freenodes=(SFreenodes*)malloc((ntypes+1)*sizeof(SFreenodes));
}
//##############################################################################
//## Return number of nodes of a node type
//##############################################################################
int idnodetype2nnodes(int nodetype)
{
  int i,j,tmp_nnodes=0;

  for(i=0;i<nnodes;i++)
     for(j=0;j<nodes[i].ntypes;j++)
        if(nodes[i].idtypes[j]==nodetype)
           tmp_nnodes++;

  return tmp_nnodes;
}
//##############################################################################
//## Return ID of a node type
//##############################################################################
int nodetype2idnodetype(char *nodetype)
{
  int i;
   
  for(i=0;i<ntypes;i++)
    if(!strcmp(nodetype,nodetypes[i].type))
       return i;
  
  return 0; //never should be reached
}
//##############################################################################
//## copy request (src) to request (dest)
//## the list of the nodes and the node types are not copied
//##############################################################################
void reqcpy(SReq **dest, SReq *src)
{
  *dest=(SReq*)malloc(sizeof(SReq)); 
  memcpy(*dest,src,sizeof(SReq));
 
  (*dest)->prox=NULL;
  (*dest)->bjscript=(char*)malloc(sizeof(char)*(strlen(src->bjscript)+1));
  strcpy((*dest)->bjscript,src->bjscript);
  (*dest)->path=(char*)malloc(sizeof(char)*(strlen(src->path)+1));
  strcpy((*dest)->path,src->path);
}
//##############################################################################
//## Request node manager to modify login.access (and hosts.equiv if 
//## necessary) 
//##############################################################################
void request_nm_lock_node(int uid, int operation)
{
  int n,ish,
      sd;          //socket descriptor
  SPheader packet;
  void *buf;
  
  crvprintf("\n>>> Request NM (UN)LOCK: STARTED: oper=%d\n",operation);
  packet.type=operation;
  packet.size=sizeof(SPheader)+sizeof(int);
  buf=malloc(packet.size);
  memcpy(buf,&packet,sizeof(SPheader));
  memcpy(buf+sizeof(SPheader),&uid,sizeof(int));

  sem_wait(&semidsu);
  for(n=0;n<nnodes;n++)
  {  for(ish=0;ish<cluster.share_nodes;ish++)
        if(idsusing[n][ish]==uid)
        {  dprintf("# request_nm_lock_node: Request NM (UN)LOCK node %s for "
                   "user %d port <%d>\n", nodes[n].host,uid,cluster.nmport);
           if( (sd=create_client_tcpsocket(nodes[n].host,cluster.nmport)) 
                == CR_ERROR)
           {  crono_log(cluster.logfile,CR_LOG,
                        "crrmd: # request_nm_lock_node: Cannot create "
                        "socket to %s",nodes[n].host);
              crvprintf("# request_nm_lock_node: Cannot create socket to"
                        " %s\n",nodes[n].host);
           }
           else
           {  crvprintf("# request_nm_lock_node: send %s\n",nodes[n].host);
              if(send(sd, buf, packet.size, 0) < 0 )
                 crvprintf("Cannot send data(uid)\n");
              close(sd);
           }
        } 
  }

  sem_post(&semidsu);

  free(buf);
  crvprintf("\n>>> Request NM (UN)LOCK: FINISHED\n");
}
//##############################################################################
//## Release the request of a client 
//##############################################################################
void rls_client_using(int uid,int rid)
{
  int n,ish,
      sd;          //socket descriptor
  SPheader packet;
  void *buf;

  crvprintf("# rls_client_using: BEGIN> release user %d\n",uid);

  packet.type=CR_NM_KILL_PROC_LOCK;
  packet.size=sizeof(SPheader)+sizeof(int);
  buf=malloc(packet.size);
  memcpy(buf,&packet,sizeof(SPheader));
  memcpy(buf+sizeof(SPheader),&uid,sizeof(int));

  sem_wait(&semidsu);
  for(n=0;n<nnodes;n++)
     for(ish=0;ish<cluster.share_nodes;ish++)
        if(idsusing[n][ish]==uid)
        {  
           idsusing[n][ish]=NOUSER;
           if(!cluster.single)    
           {  dprintf("# rls_client_using: KILLP_LOCK %s\n",nodes[n].host);
              if( (sd=create_client_tcpsocket(nodes[n].host,cluster.nmport))
                   == CR_ERROR)
              {  crono_log(cluster.logfile,CR_LOG, "crrmd: # rls_client_using: "
                           "Cannot create socket to %s",nodes[n].host);
                 crvprintf("# rls_client_using: Cannot create socket to %s\n",
                           nodes[n].host);
              }
              else
              {  if(send(sd, buf, packet.size, 0) < 0 )
                    crvprintf("Cannot send data(uid)\n");
                 close(sd);
              }
           }
        }
  free(buf);

  update_freenodes();
  sem_post(&semidsu);
  rm_schedule_ret_req_from_queue(&firstqueue,&lastqueue,rid);
  crvprintf("# rls_client_using: END> release user %d\n",uid);
}
//#############################################################################
//## Allocation request of a user 
//##   csd        = client socket descriptor 
//##   packetsize = packet size to receive
//#############################################################################
void rm_alloc_client(int csd,int packetsize)
{
  int answer,i;
  void *buf, *offset;
  SAlloc alloc;
  SReq req;

  buf=malloc(packetsize);
  dprintf("# rm_alloc_client: Packet size=%d\n",packetsize);
  if( recv(csd,buf,packetsize,0) < 0)
  {  free(buf);
     return;
  }
  offset=buf+sizeof(SPheader);
  memcpy(&alloc,offset,S_PACK_SALLOC);

  dprintf("rm_alloc_client: wait semqueue\n");
  sem_wait(&semqueue); 
  if( alloc.when==CR_ALLOC_SOON && user_has_allocated(alloc.uid) )
  {  dprintf("# rm_alloc_client: user has already allocated\n");
     answer=CR_ERM_USER_ALREADY_ALLOC;
     send(csd,&answer,sizeof(int),0);
     free(buf); 
     sem_post(&semqueue);
     return;
  }

  offset+=S_PACK_SALLOC;
  offset+=sizeof(char)*(strlen(offset)+1); //skip cluster
  req.bjscript=(char*)malloc(sizeof(char)*(strlen(offset)+1));
  strcpy(req.bjscript,offset);
  offset+=sizeof(char)*(strlen(offset)+1); //skip bjscript
  req.path=(char*)malloc(sizeof(char)*(strlen(offset)+1));
  strcpy(req.path,offset); 

  req.uid=alloc.uid;
  req.nnodes=alloc.nnodes;
  req.shared=alloc.shared;
  req.when=alloc.when;
  req.exec_tpreps=TRUE;
  req.time=alloc.time;
  req.qualitative=alloc.qualitative;
  req.sid=0;

  offset+=sizeof(char)*(strlen(offset)+1); //skip path
  memcpy(&req.ntypes,offset,sizeof(int));
  offset+=sizeof(int);
  dprintf("# rm_alloc_client: NTYPES=%d NNODES=%d\n",req.ntypes,req.nnodes);

  if(req.ntypes)
  {  req.nodetypes=(SReqnodetype*)malloc(sizeof(SReqnodetype)*req.ntypes);

     for(i=0;i<req.ntypes;i++) //unpack node types requested by the user
     {  req.nodetypes[i].idtype=nodetype2idnodetype((char*)offset);
        offset+=sizeof(char)*(strlen(offset)+1);
        memcpy(&(req.nodetypes)[i].nnodes,offset,sizeof(int));
        offset+=sizeof(int);
        dprintf("# rm_alloc_client: type[%d] nnodes[%d]\n",
                req.nodetypes[i].idtype,req.nodetypes[i].nnodes);
     } 
  }
  else
    req.nodetypes=NULL;
 
  rm_check_requested_resources_exist(&req); //fix the request if needed

  if(alloc.when==CR_ALLOC_SOON) //allocation (resources as soon as possible)
  {  
     req.tstart=0;
     req.tfinish=0;
     req.rid=get_newrid();
     req.nodes=NULL;
     dprintf("# rm_alloc_client: nnodes=%d time=%d\n",req.nnodes, req.time);
     dprintf("# rm_alloc_client: batch script=%s path=%s\n",req.bjscript,
             req.path);
     if(req.qualitative)
        crvprintf("# rm_alloc_client: QUALITATIVE ALLOCATION\n");
     else
        crvprintf("# rm_alloc_client: QUANTITATIVE ALLOCATION\n");

     answer=rm_make_allocation(req,TRUE);
  }
  else  //reserve (resources at a given time)
  {  req.tstart=alloc.tstart;
     req.tfinish=alloc.tfinish;
     answer=rm_reserve_add(req,alloc.nreserves);
  }

  send(csd,&answer,sizeof(int),0);
  free(buf); 
  free(req.bjscript); 
  free(req.path); 
  free(req.nodetypes);
    
  if(answer==CR_REQ_QUEUED)
     rm_schedule_store_queue();

  sem_post(&semqueue); 
}
//#############################################################################
//## Check whether the user requested more nodes than the number
//## of cluster nodes. Or more nodes of a type that are not available.
//##
//## Probably the administrator should configure the Access Manager
//## to take care about this. However, one can make a mistake on the
//## configuration.
//## If the request is not ok, this function will fix it.
//#############################################################################
void rm_check_requested_resources_exist(SReq *req)
{
  int i, tmp_nnodes;

  for(i=0;i<req->ntypes;i++)
  {   
     tmp_nnodes=idnodetype2nnodes(req->nodetypes[i].idtype);

     if(req->nodetypes[i].nnodes > tmp_nnodes)
     {  if(req->qualitative)
           req->nnodes-=(req->nodetypes[i].nnodes-tmp_nnodes);
        req->nodetypes[i].nnodes=tmp_nnodes;
     }
  }
}
//#############################################################################
//## Do nothing - Used for SIGALRM (It's necessary to make a time out in
//## the request_nm_lock_node() function)
//#############################################################################
void rm_conn_timeout()
{
  crvprintf("# rm_conn_timeout: Connection Time Out!!\n");
}
//#############################################################################
//## Handle clients requests from sockets
//#############################################################################
void *rm_handle_client_thread(void *arg)
{
  int csd=(int)arg;
  SPheader packet;
  
  pthread_detach(pthread_self());
  signal(SIGPIPE,SIG_IGN);

  if(recv(csd,&packet,sizeof(SPheader),MSG_PEEK)<0)
  {  close(csd);
     return NULL;
  }  
  switch(packet.type)
  {  case CR_ALLOC:
     {  crvprintf("\n>>> Allocation Operation: STARTED\n");
        rm_alloc_client(csd,packet.size); 
        crvprintf("\n>>> Allocation Operation: FINISHED\n");
        break;
     }
     case CR_QVIEW:
     {  crvprintf("\n>>> Viewer Operation: STARTED\n");
        rm_qview_client(csd,packet.size);
        crvprintf("\n>>> Viewer Operation: FINISHED\n");
        break;
     }
     case CR_RLS:
     {  crvprintf("\n>>> Release Operation: STARTED\n");
        rm_rls_client(csd,packet.size);
        crvprintf("\n>>> Release Operation: FINISHED\n");
        break;
     }
     case CR_NODES:
     {  crvprintf("\n>>> View Nodes Operation: STARTED\n");
        rm_nodes_client(csd,packet.size);
        crvprintf("\n>>> View Nodes Operation: FINISHED\n");
        break;
     }
     case CR_GET_ALLOWED_USERS:
     {  crvprintf("\n>>> Get Allowed users Operation: STARTED\n");
        rm_send_allowed_users_to_nm(csd,packet.size);
        crvprintf("\n>>> Get Allowed users Operation: FINISHED\n");
        break;
     }
     case CR_NMC_EXEC_OPERATION:
     {  crvprintf("\n>>> Node Manager Client Operation: STARTED\n");
        rm_nmc_client(csd,packet.size);
        crvprintf("\n>>> Node Manager Client Operation: FINISHED\n");
        break;
     }
     default:
        crvprintf("\n>>> UNKNOW PACKET!! <%d>\n",packet.type);
  }
  close(csd);
  return NULL;
}
//##############################################################################
//## Make an allocation of a client (newreq)
//## schedule - flag -> TRUE: schedule the request ->FALSE: just queue it
//##############################################################################
int rm_make_allocation(SReq newreq, char schedule)
{
  int answer=CR_REQ_QUEUED;
  SReq *req=NULL;

  crvprintf("# rm_make_allocation: uid: %d time: %d nnodes: %d start: %ld "
            "finish: %ld batch: %s path: %s flag_schedule: %d\n",
            newreq.uid,newreq.time,newreq.nnodes, newreq.tstart, newreq.tfinish,
            newreq.bjscript, newreq.path,schedule);

  //copy the request
  reqcpy(&req,&newreq);

  if(newreq.ntypes)
  {  req->nodetypes=(SReqnodetype*)malloc(newreq.ntypes*sizeof(SReqnodetype));
     memcpy(req->nodetypes,newreq.nodetypes,newreq.ntypes*sizeof(SReqnodetype));
  } 
  else
     req->nodetypes=NULL;

  if(!schedule) //is it necessary to schedule the request?
  {  req->nodes=(int*)malloc(sizeof(int)*(req->nnodes));
     memcpy(req->nodes,newreq.nodes,sizeof(int)*(req->nnodes));
  }

  if(req->when==CR_ALLOC_SOON) 
  {  req->status=CR_SCHED_STATUS_QUEUED;
     if(schedule)
        rm_schedule(req);
     else
     {  rm_schedule_add_req_to_queue(&firstqueue,&lastqueue,req);
        if(req->tstart-cluster.tmpreps<=time(NULL))
        {  attend_req(req);
           return 0;   //probably the queue was recovered
        }  
     }   
  }
  else  //CR_RESERVE
  {  req->status=CR_SCHED_STATUS_RESERVED;
     dprintf("# rm_make_allocation: tstart=%ld\n",req->tstart);
     rm_schedule_add_req_to_queue(&firstqueue,&lastqueue,req);
  }
  if(verbose) 
     rm_schedule_show_queue(firstqueue);
  if(req->when==CR_ALLOC_SOON)
     try_to_attend_new_req();

  return answer;
}
//##############################################################################
//## Execute operations on nodes through the Node Manager  
//##############################################################################
void rm_nmc_client(int csd,int packetsize)
{
  int n,ish;
  char *alloutput, *output,*node;
  void *buf,*offset;
  SNmc nmc;
  SPheader packet;

  dprintf("# rm_nmc_client: Packet size=%d\n",packetsize);
  buf=malloc(packetsize);
  recv(csd,buf,packetsize,0);

  //unpack packet
  offset=buf+sizeof(SPheader);
  memcpy(&nmc,offset,sizeof(int)*3);
  offset+=sizeof(int)*3;
  offset+=sizeof(char)*(strlen(offset)+1);
  nmc.operation=(char*)malloc(sizeof(char)*(strlen(offset)+1));
  strcpy(nmc.operation,offset);
  offset+=sizeof(char)*(strlen(offset)+1);

  alloutput=(char*)malloc(sizeof(char));
  alloutput[0]=0;
  sem_wait(&semidsu);
  //Execute Operation
  if(!nmc.all) //send operation to nodes choosed by the user
  {  node=(char*)malloc(sizeof(char)*MAXHOSTNAMELEN);
     dprintf("# rm_nmc_client: nodes=<%s>\n",(char*)offset);
     while(strlen(offset)>0)
     {  sscanf(offset,"%[^ ]",node);
        if( user_has_node_access(node,nmc.uid) )
        {  rm_send_operation_to_nm(node,nmc,&output);
           crvprintf("# rm_nmc_client: exec output: %s\n",output);
           alloutput=(char*)realloc(alloutput,sizeof(char)*
                                   (strlen(alloutput)+strlen(output)+1)); 
           strcat(alloutput,output);
           free(output);
        }
        offset+=sizeof(char)*(strlen(node)+1);
    }
  }
  else //send operation to all allowed nodes of this user
  {  for(n=0;n<nnodes;n++)
     {  for(ish=0;ish<cluster.share_nodes;ish++)
        {  if(idsusing[n][ish]==nmc.uid)
           {  rm_send_operation_to_nm(nodes[n].host,nmc,&output);
              crvprintf("# rm_nmc_client: exec output: '%s'\n",output);
              alloutput=(char*)realloc(alloutput,sizeof(char)*
                                      (strlen(alloutput)+strlen(output)+1));
              strcat(alloutput,output);
              free(output);
           }
        }
     }
  }
  sem_post(&semidsu);
  crvprintf("# rm_nmc_client: Sending output to the user\n");
  crvprintf("# rm_nmc_client: OUTPUT=<%s>\n",alloutput);
  if(strlen(alloutput)>0)
     packet.type=CR_OK;
  else
     packet.type=CR_ERM_NO_OPERATIONS_EXECUTED;
  packet.size=sizeof(SPheader)+sizeof(char)*(strlen(alloutput)+1);
  free(buf);
  buf=malloc(packet.size);
  memcpy(buf,&packet,sizeof(SPheader));
  memcpy(buf+sizeof(SPheader),alloutput,packet.size-sizeof(SPheader));   
  send(csd, buf, packet.size, 0); //to AM (output of operations)
  free(buf);
  free(nmc.operation);
  free(alloutput);
}
//##############################################################################
//## Send the user nodes to Access Manager
//##############################################################################
void rm_nodes_client(int csd,int packetsize)
{
  void *buf;
  char *usernodes;
  SPheader packet;
  SNodes snodes;

  buf=malloc(packetsize);
  if(recv(csd,buf,packetsize,0)<0)
  {  free(buf);
     return;
  }
  memcpy(&snodes,buf+sizeof(SPheader),S_PACK_SNODES);
  free(buf);
  crvprintf("# rm_nodes_client: UID=<%d> INFO=<%d>\n",snodes.uid,snodes.oper);

  if( (usernodes=rm_uid2nodes(snodes.uid,&usernodes,snodes.oper))==NULL)
     packet.size=sizeof(SPheader);
  else
     packet.size=sizeof(SPheader)+sizeof(char)*(strlen(usernodes)+1);
  packet.type=CR_OK;


  dprintf("# rm_nodes_client: Create and send packet to AM. size=%d\n",packet.size);
  buf=malloc(packet.size);
  memcpy(buf,&packet,sizeof(SPheader));
  if(usernodes!=NULL)
    strcpy(buf+sizeof(SPheader),usernodes);
  send(csd,buf,packet.size,0);

  free(buf);
  if(usernodes!=NULL)
     free(usernodes);
}
//##############################################################################
//## Send the queue to the Access Manager
//##############################################################################
void rm_qview_client(int csd,int packetsize)
{
  int s=sizeof(SPheader)+sizeof(SQview_rminfo), //packet size
      i,t,j,k,
      req_nnodes_original,
      nreqs=0,
      ngapsnow=0;
  void          *buf, *offset;
  char          login[_POSIX_LOGIN_NAME_MAX];
  time_t        currenttime=time(NULL);

  SReq          *req;
  SReqnodetype  *tmp_nodetypes;
  SQview_rminfo qview_rminfo;
  SPheader      packet;
  SQview        qview;
  SGapsnow      *gapsnow=NULL;


  //get the request information (qview structure) from AM
  buf=malloc(packetsize); //necessary to get 'qview.type' flag
  if(recv(csd,buf,packetsize,0)<0)
  {  free(buf);
     return;
  }
  memcpy(&qview,buf+sizeof(SPheader),S_PACK_SQVIEW);
  free(buf);
  
  sem_wait(&semqueue);
  
    //create the packet
  dprintf("# rm_qview_client: Creating queue packet\n");

  //store qview_rminfo information
  req=firstqueue;
  buf=malloc(s);


  if(qview.type) //option to get node types information
  {
     buf=realloc(buf,s+sizeof(int)+sizeof(SFreenodes)*ntypes);
     offset=buf+s;
     s+=sizeof(int)+sizeof(SFreenodes)*ntypes;
 
    //get the number of node types
     memcpy(offset,&ntypes,sizeof(int));
     offset+=sizeof(int);

     //get free nodes of each node type
     memcpy(offset,&freenodes[1],sizeof(SFreenodes)*ntypes);
     offset+=sizeof(SFreenodes)*ntypes;

     //get the names of the node types
     for(i=0;i<ntypes;i++)
     {  
        buf=realloc(buf,s+sizeof(char)*(strlen(nodetypes[i].type)+1));
        strcpy(buf+s,nodetypes[i].type);
        s+=sizeof(char)*(strlen(nodetypes[i].type)+1);
     }
  }

  while(req!=NULL) //for each user request in the queue
  {  
     nreqs++;
     if(!qview.type) // user does not want node types information
     { 
        s+=S_PACK_SREQ;                  //store user request structure
        buf=realloc(buf,s);
        offset=buf+s-S_PACK_SREQ;
        memcpy(offset,req,S_PACK_SREQ);

        //store user login

        uid2login(req->uid,login);
        k=sizeof(char)*(strlen(login)+1);
        buf=realloc(buf,s+k);
        offset=buf+s;
        strcpy(offset,login);

        s+=k;
     }
     else
     {  t=0;
        tmp_nodetypes=NULL;
        //calculate the number of nodes of each type in this request (req)
        for(i=0;i<req->nnodes;i++)  //each requested node
        {  
           for(k=0;k<nodes[req->nodes[i]].ntypes;k++) //each type of this node
           { 
              for(j=0;j<t;j++)
              {  if(tmp_nodetypes[j].idtype == nodes[req->nodes[i]].idtypes[k])
                 {  tmp_nodetypes[j].nnodes++;
                    break;
                 }
              }
              if(j==t) //notfound. add node type to 'tmp_nodetypes'
              {  tmp_nodetypes=(SReqnodetype*)realloc(tmp_nodetypes,
                                                   sizeof(SReqnodetype)*(t+1));
                 tmp_nodetypes[t].idtype=nodes[req->nodes[i]].idtypes[k];
                 tmp_nodetypes[t].nnodes=1;
                 t++;
              }
           }
        }

        req_nnodes_original=req->nnodes;
        for(i=0;i<t;i++) //pack all information
        { 
           buf=realloc(buf,s+S_PACK_SREQ);
           offset=buf+s;
           req->nnodes=tmp_nodetypes[i].nnodes;
           memcpy(offset,req,S_PACK_SREQ);
           s+=S_PACK_SREQ;

           k=sizeof(char)*(strlen(nodetypes[tmp_nodetypes[i].idtype].type)+1);
           buf=realloc(buf,s+k);
           offset=buf+s;
           strcpy(offset,nodetypes[tmp_nodetypes[i].idtype].type);
           offset+=sizeof(char)*
                   (strlen(nodetypes[tmp_nodetypes[i].idtype].type)+1);
           s+=k;
        }
        free(tmp_nodetypes);
        req->nnodes=req_nnodes_original;
     }
     req=req->prox;
  }

  //get additional information of the requests queue and the RM
  qview_rminfo.fshare=freenodes[0].shared;
  qview_rminfo.fexclusive=freenodes[0].exclusive;
  qview_rminfo.nshare=cluster.share_nodes;
  qview_rminfo.nnodes=nnodes;
  qview_rminfo.time=currenttime;
  qview_rminfo.nreqs=nreqs;

  memcpy(buf+sizeof(SPheader),&qview_rminfo,sizeof(SQview_rminfo));

  //get allocation gaps beginning now
  rm_qview_get_gaps_now(nreqs,&gapsnow,&ngapsnow);

  //release the queue
  sem_post(&semqueue);

  //copy allocation gaps to the buffer to be sent to the sockets
  buf=realloc(buf,s+ngapsnow*sizeof(SGapsnow)+sizeof(int));
  memcpy(buf+s,&ngapsnow,sizeof(int));
  s+=sizeof(int);
  if(ngapsnow)
  {   
     memcpy(buf+s,gapsnow,ngapsnow*sizeof(SGapsnow));
     s+=ngapsnow*sizeof(SGapsnow);
  }

  //create the packet

  packet.size=s; 
  packet.type=CR_OK;
  memcpy(buf,&packet,sizeof(SPheader));

  dprintf("# rm_qview_client: Packet created\n");
  //send packet to the Access Manager
  send(csd,buf,packet.size,0);
  
  free(buf);
  dprintf("# rm_qview_client: Finished\n");
}

//##############################################################################
//## Get the allocation gaps beginning now
//## input: nreqs     -> number of all requests in the queue
//## output: gapsnow  -> information about the gaps
//##         ngapsnow -> number of gaps beginning now
//##############################################################################
void rm_qview_get_gaps_now(int nreqs, SGapsnow **gapsnow, int *ngapsnow)
{
  int n,i;
  int timeslot,
      *times=NULL;
  time_t timenow=time(NULL);

  *ngapsnow=0;
  times=(int*)calloc(nnodes,sizeof(int));

  for(n=0;n<nnodes;n++)
  {
     if( !rm_qview_get_gaps_now_nodebusy(n,timenow,&timeslot))
     {
        times[n]=timeslot; 
        for(i=0;i<*ngapsnow;i++)
           if((*gapsnow)[i].timeslot==timeslot)
              break;

        if(i==*ngapsnow) //new gap
        {
           (*gapsnow)=(SGapsnow*)realloc(*gapsnow,sizeof(SGapsnow)*
                                                        (*ngapsnow+1));
           (*gapsnow)[*ngapsnow].timeslot=timeslot;
           (*gapsnow)[*ngapsnow].nnodes=0;
           (*ngapsnow)++;
        }
     }
  }

  //add nodes to each gap
  for(n=0;n<nnodes;n++)
    if( times[n] )
       for(i=0;i<*ngapsnow;i++)
          if( ( (*gapsnow)[i].timeslot!=-1 && 
                (*gapsnow)[i].timeslot<=times[n]) ||
              times[n] == -1)

            (*gapsnow)[i].nnodes++;

  free(times);
  if(verbose)
  {  
     printf("# rm_qview_get_gaps_now: ngaps=%d\n",*ngapsnow);
     for(i=0;i<*ngapsnow;i++)
        printf("# rm_qview_get_gaps_now: timeslot=%d nnodes=%d\n",
                (*gapsnow)[i].timeslot, (*gapsnow)[i].nnodes);
  }

}
//##############################################################################
//## Test if 'node' is busy 'now' and if it is available set 'timeslot' which is
//## the amount of time to available until the 'node'  becomes busy.
//## If it will never get busy 'timeslot' is -1
//##############################################################################
char rm_qview_get_gaps_now_nodebusy(int node, time_t now, int *timeslot)
{

  int i,n;
  SReq *req=firstqueue;

  *timeslot=-1;
  while(req) //for each request in the queue
  {
     //discover if 'node' is busy 'now'
     if( now >= req->tstart && now <= req->tfinish)
        for(n=0;n<req->nnodes;n++)
           if( req->nodes[n] == node )
              return TRUE;

     //discover if this request will use this node 
     for(n=0;n<req->nnodes;n++)
        if(req->nodes[n]==node)
           break;

     if(n!=req->nnodes && now < req->tstart && (req->tstart-now)/60 > 0 )
     { 
        //the node is free and this request will use it 

        if( *timeslot == -1 ) //ftime has never been set
           *timeslot=(req->tstart-now)/60;
        else
           if( *timeslot > (req->tstart-now)/60 )
             *timeslot = (req->tstart-now)/60;
     }
     req=req->prox;
  }
      
  return FALSE;
}
//##############################################################################
//## Reset the access of Node Managers 
//##############################################################################
void rm_reset_lock_access_nm()
{
  int sd, n,ish,i;
  void *buf;
  SPheader packet;

  crvprintf("# rm_reset_lock_access_nm: STARTED\n");
  for(n=0;n<nnodes;n++) //for each node of cluster
  {  i=0;
     buf=malloc(sizeof(SPheader));

     for(ish=0;ish<cluster.share_nodes;ish++)
     {  if(idsusing[n][ish]!=NOUSER)
        {  buf=realloc(buf,sizeof(SPheader)+(i+1)*sizeof(int));
           memcpy(buf+sizeof(SPheader)+i*sizeof(int),&idsusing[n][ish],
                  sizeof(int));
           i++;
        }
     }
     packet.type=CR_NM_RESET_LOCK_ACCESS;
     packet.size=sizeof(SPheader)+i*sizeof(int);
     memcpy(buf,&packet,sizeof(SPheader));
     dprintf("# rm_reset_lock_access_nm: to node=%s packet size=%d\n",
             nodes[n].host,packet.size);

     alarm(cluster.conntimeout);
     signal(SIGALRM,rm_conn_timeout);
     siginterrupt(SIGALRM,1);
     if( (sd=create_client_tcpsocket(nodes[n].host,cluster.nmport)) == CR_ERROR)
     {  crvprintf("crrmd:(rm_reset_lock_access_nm) Cannot create socket "
                  "to %s\n",nodes[n].host);
        crono_log(cluster.logfile,CR_LOG,"crrmd:"
                  "(rm_reset_lock_access_nm) Cannot create socket to %s",
                  nodes[n].host);
     } 
     alarm(0);
     signal(SIGALRM,SIG_DFL);
     siginterrupt(SIGALRM,0);
     if(sd!=CR_ERROR)
     {  send(sd,buf,packet.size,0);
        close(sd);
     }
     free(buf);
  }
  crvprintf("# rm_reset_lock_access_nm: FINISHED\n");
}
//##############################################################################
//## Release request of a user (csd=client socket descriptor)
//##############################################################################
void rm_rls_client(int csd,int packetsize)
{
   int answer,    //answer to the AM
       rid=0,
       usertime;
   char *usernodes=NULL,*usernodesinfo=NULL, login[_POSIX_LOGIN_NAME_MAX];
   void *buf;  
   SReq *req;
   SRls rls;
   SPheader packet;

   buf=malloc(packetsize); 
   dprintf("# rm_rls_client: receiving buffer\n");
   if(recv(csd,buf,packetsize,0)<0) 
   {  free(buf);
      return;
   }
   dprintf("# rm_rls_client: received\n");
   memcpy(&rls,buf+sizeof(SPheader),sizeof(SRls));
   free(buf);

   crvprintf("# rm_rls_client: user '%d' rid '%d'\n",rls.uid,rls.rid);

   sem_wait(&semqueue);

   if(rls.rid!=0) //for reserve (at a given time)
     answer=rm_reserve_del(rls);

   else //for allocation (as soon as possible)
   { 
      answer=CR_ERM_USER_NOT_QUEUED;  
      req=firstqueue;
      while(req!=NULL)
      {  dprintf("# rm_rls_client: RID=%d %d\n",req->rid,req->status);
         if(req->uid==rls.uid && req->when==CR_ALLOC_SOON)
         {  if(req->status==CR_SCHED_STATUS_READY || 
               req->status==CR_SCHED_STATUS_BATCH ||
               req->status==CR_SCHED_STATUS_UPREPS ||
               req->status==CR_SCHED_STATUS_MPREPS)
            {
               crvprintf("# rm_rls_client: CANCEL THREAD %ld\n",req->tid);
               sem_wait(&semidsu);
               pthread_cancel(req->tid);
               pthread_join(req->tid,NULL);
               sem_post(&semidsu);

               if( (req->status==CR_SCHED_STATUS_BATCH || 
                    req->status==CR_SCHED_STATUS_UPREPS) &&
                    req->sid!=0
                 )
                  crem_kill_session(req->sid);

               answer=CR_RESOURCES_RELEASED;
               set_req_status(req->rid,CR_SCHED_STATUS_MPOSTPS);
               usernodes=rm_uid2nodes(req->uid,&usernodes,CRNODES_USERNODES);
               usernodesinfo=rm_uid2nodes(req->uid,&usernodesinfo,
                                          CRNODES_USERNODES_INFO);
               rid=req->rid;
               crono_log(cluster.logfile,CR_LOG, "crrmd: CANCEL USER: %-9s"
                        "RTIME: %-9d " "NNODES: %-3d",uid2login(req->uid,login),
                        (req->tfinish-time(NULL))/60, req->nnodes);


            }
            else  if(req->status==CR_SCHED_STATUS_QUEUED || 
                     req->status==CR_SCHED_STATUS_QUEUED_C  
                    )
            {  
               rm_schedule_ret_req_from_queue(&firstqueue,&lastqueue,req->rid);
               answer=CR_REQUEST_CANCELLED;
               usertime=req->time;
            }
            break;
        }
        else 
          req=req->prox;
      }
   }

   //send answer to the AM
   packet.type=answer;
   packet.size=sizeof(SPheader);
   send(csd,&packet,packet.size,0);

   if(answer==CR_RESOURCES_RELEASED)
   {  //release the queue because the execution of MPOSTPS can take
      //a while
      sem_post(&semqueue);
      crem_ms(rls.uid,rid,cluster,dirconf,usernodes,usernodesinfo,CREM_MPOSTPS,
              usertime);     
      sem_wait(&semqueue);
      rls_client_using(rls.uid,rid);
      crem_us(rls.uid,0,0,cluster,usernodes,usernodesinfo,CREM_UPOSTPS);      
   }

   if(answer==CR_RESOURCES_RELEASED || answer==CR_REQUEST_CANCELLED)
   {  rm_schedule_reschedule_all_requests();
      try_to_attend_new_req();  //try attend new user from the request queue
   }

   free(usernodes);
   free(usernodesinfo);

   sem_post(&semqueue);
}
//##############################################################################
//## Send the users allowed to access a node 
//##############################################################################
void rm_send_allowed_users_to_nm(int csd,int packetsize)
{
   char *node;
   int n,ish,i=0;
   void *buf;
   SPheader packet;

   buf=malloc(packetsize);
   recv(csd,buf,packetsize,0);
   node=(char*)malloc(sizeof(char)*(strlen(buf+sizeof(SPheader))+1));   
   strcpy(node,buf+sizeof(SPheader));
   free(buf);
   buf=malloc(sizeof(SPheader));
   dprintf("# rm_send_allowed_users_to_nm: %s\n",node);
   sem_wait(&semidsu);
   for(n=0;n<nnodes;n++)
   {  if(!strcmp(node,nodes[n].host))
      {  for(ish=0;ish<cluster.share_nodes;ish++)
         {  if(idsusing[n][ish]!=NOUSER )
            {  
               buf=realloc(buf,sizeof(SPheader)+(i+1)*sizeof(int));
               memcpy(buf+sizeof(SPheader)+i*sizeof(int),&idsusing[n][ish],
                      sizeof(int));
               i++;
            }
         }
         break;    
      }
   }
   sem_post(&semidsu);
   packet.size=sizeof(SPheader)+i*sizeof(int);
   dprintf("# rm_send_allowed_users_to_nm: packet size=%d\n",packet.size);
   memcpy(buf,&packet,sizeof(SPheader));
   send(csd,buf,packet.size,0);
   free(buf); 
   free(node);
}
//##############################################################################
//## Sent operation to Node Manager for execution 
//##
//## in  -> node,nmc
//## out -> output
//##############################################################################
void rm_send_operation_to_nm(char *node,SNmc nmc,char **output)
{
  int sd;
  char tmp[CR_MAX_MSG];
  SPheader packet;
  void *buf,*offset;

  if(cluster.single)
  {  snprintf(tmp,CR_MAX_MSG,"%s: Cannot execute operation\n",node);
     *output=(char*)malloc(sizeof(char)*(strlen(tmp)+1));
     strcpy(*output,tmp); 
     return;
  }
  crvprintf("# rm_send_operation_to_nm: Sending operation to %s port %d\n",
             node,cluster.nmport);
  crvprintf("# rm_send_operation_to_nm: operation='%s'\n",nmc.operation);

  //create the packet
  packet.type=CR_NMC_EXEC_OPERATION;
  packet.size=sizeof(SPheader)+sizeof(int)+
              sizeof(char)*(strlen(nmc.operation)+1);
  buf=malloc(packet.size);
  memcpy(buf,&packet,sizeof(SPheader));
  offset=buf+sizeof(SPheader);
  memcpy(offset,&nmc,sizeof(int)); //get just the 'uid' from SNmc struct
  offset+=sizeof(int);
  strcpy(offset,nmc.operation);

  //create connection to Node Manager
  if( (sd=create_client_tcpsocket(node,cluster.nmport)) == CR_ERROR)
  {  crono_log(cluster.logfile,CR_LOG,"crrmd: Cannot create socket to %s",
               node);
     crvprintf("crrmd: send_operation: Cannot create socket to %s",node);
     snprintf(tmp,CR_MAX_MSG,"\n%s: Problems to execute command\n",node);
     (*output)=(char*)malloc(sizeof(char)*(strlen(tmp)+1));
     strcpy(*output,tmp);
     return;
  } 
  //send packet to Node Manager
  if(send(sd, buf, packet.size, 0) < 0 )
  {  close(sd);
     return;
  }
  free(buf);

  //receive packet from Node Manager (execution output)
  recv(sd, &packet, sizeof(SPheader), MSG_PEEK);
  buf=malloc(packet.size); 
  if(recv(sd, buf, packet.size, 0) < 0)
  {  close(sd);
     free(buf);
     return;
  }
  (*output)=(char*)malloc(packet.size-sizeof(SPheader));
  strcpy(*output,buf+sizeof(SPheader));
  free(buf);
  close(sd);
}
//##############################################################################
//## SLEEP in seconds ( linux sleep function has some problems!)
//## (interference with the SIGALRM signal)
//##############################################################################
void rm_sleep (unsigned long secs) 
{
  struct timeval tv;
  tv.tv_sec  = secs;
  tv.tv_usec = 0; 
  (void) select (0, (void *)0, (void *)0, (void *)0, &tv);
} 
//##############################################################################
//## Get the list of nodes of a user
//##############################################################################
char *rm_uid2nodes(int uid, char **usernodes,char oper)
{
  int n,t,s_string,ish=0,bytes=0,s,idtype=0;
  void *offset;
  char snprocs[MAX_SIZE_NPROCS],*stringtype=NULL,*stringwho=NULL;
  char userlogin[_POSIX_LOGIN_NAME_MAX];

  dprintf("# rm_uid2nodes: start\n");
  *usernodes=NULL;
  
  sem_wait(&semidsu);

  for(n=0;n<nnodes;n++)
  {
     if(oper!=CRNODES_ALLNODES_INFO && oper!=CRNODES_ALLNODES)
        for(ish=0;ish<cluster.share_nodes;ish++)
           if(idsusing[n][ish]==uid)
              break;

     if( oper==CRNODES_ALLNODES_INFO || oper==CRNODES_ALLNODES ||
         ish<cluster.share_nodes  //uid is in this node
       )
     {  //A node can have more than one node type.
        //However we get the information of the last node type of this node
        // That is, just the description (desc) should be different
        idtype=nodes[n].idtypes[nodes[n].ntypes-1];

        s=0;
        //get the nodetype names and their information
        if(oper==CRNODES_ALLNODES_INFO || oper==CRNODES_USERNODES_INFO)
        { 
           s_string=0;
           stringtype=NULL;
           for(t=0;t<nodes[n].ntypes;t++)
           {  s_string+=sizeof(char)*
                        (strlen(nodetypes[nodes[n].idtypes[t]].type)+2);
              stringtype=(char*)realloc(stringtype,s_string);
              if(t==0)
              {  strcpy(stringtype,nodetypes[nodes[n].idtypes[t]].type);
                 s_string--;
              }
              else
              {  strcat(stringtype,",");
                 strcat(stringtype,nodetypes[nodes[n].idtypes[t]].type);
              }
           }
           snprintf(snprocs,MAX_SIZE_NPROCS,"%d",nodetypes[idtype].nprocs);
           s+=sizeof(char)*( strlen(stringtype)+
                            strlen(nodetypes[idtype].arch)+
                            strlen(snprocs)+
                            strlen(nodetypes[idtype].desc));
           //s+=sizeof("::::"); //delimiters
           s+=sizeof("::::")-sizeof(char); //delimiters
        }

        s+=sizeof(char)*(strlen(nodes[n].host)+1);

        if(oper==CRNODES_ALLNODES_INFO || oper==CRNODES_ALLNODES)
        {
           s_string=0;
           stringwho=NULL;
           
           for(ish=0,t=0;ish<cluster.share_nodes;ish++)
           {  if(idsusing[n][ish]!=NOUSER) 
              {
                 uid2login(idsusing[n][ish],userlogin);
                 s_string+=sizeof(char)*( strlen(userlogin) +2);
                 stringwho=(char*)realloc(stringwho,s_string);
                 if(t==0)
                 {  strcpy(stringwho,userlogin);
                    s_string--;
                    t=1;
                 }
                 else
                 {  strcat(stringwho,",");
                    strcat(stringwho,userlogin);
                 }
              }
            }
            if(!s_string)
            {  stringwho=(char*)malloc(sizeof(char)*(strlen("empty")+1));
               strcpy(stringwho,"empty");
            }
            s+=sizeof(char)*(strlen(stringwho)+strlen(":") );
        }

        *usernodes=realloc(*usernodes,bytes+s+2);
        switch(oper)
        {  case CRNODES_ALLNODES_INFO:
             snprintf(*usernodes+bytes,s,"%s:%s:%s:%s:%s:%s",nodes[n].host,
                      stringwho, stringtype, nodetypes[idtype].arch,snprocs,
                      nodetypes[idtype].desc);
             break;
           case CRNODES_USERNODES_INFO:
              snprintf(*usernodes+bytes,s,"%s:%s:%s:%s:%s",nodes[n].host,
                       stringtype, nodetypes[idtype].arch,snprocs,
                       nodetypes[idtype].desc);
              break;
           case CRNODES_ALLNODES:
              snprintf(*usernodes+bytes,s,"%s:%s",nodes[n].host,stringwho);
              break;
           case CRNODES_USERNODES:
             memcpy(*usernodes+bytes,nodes[n].host,sizeof(char)*
                                               (strlen(nodes[n].host)));
        }
        bytes+=s;
        offset=*usernodes+bytes;
        offset-=sizeof(char);
        *(char*)offset=' ';
        if(oper==CRNODES_ALLNODES_INFO || oper==CRNODES_USERNODES_INFO)
           free(stringtype);
        if(oper==CRNODES_ALLNODES_INFO || oper==CRNODES_ALLNODES)
           free(stringwho);
     }
  }
  sem_post(&semidsu);

  if(bytes)
  {  offset=*usernodes+bytes-sizeof(char);
     *(char*)offset=0;
  }
  dprintf("# rm_uid2nodes: finish bytes=%d\n",bytes);
  return *usernodes;
}
//##############################################################################
//## Set the status of a request with such "uid" ant "tstart" 
//##############################################################################
void set_req_status(int rid, int status)
{
  SReq *req;

  req=firstqueue;
  while(req!=NULL)
  {  if(req->rid==rid)
     {  crvprintf("# set_req_status: request found\n");
        req->status=status;  
        return;
     }
     req=req->prox;
  }
}
//##############################################################################
//## Set the starting and finishing time of a request
//## time_mpreps-> spent time of the execution of the MPREPS
//## For example, if a user will be attended at 10:00:20, the Timeout of Master
//## pre-processing script is set for 10 seconds, and the execution time of the
//## MPREPS was 2 seconds, the starting time of the user should be 10:00:12
//##############################################################################
time_t set_req_tstart_tfinish(int rid,long time_mpreps)
{
  SReq *req;
  time_t tstart=0; 

  sem_wait(&semqueue);
  req=firstqueue;
  while(req!=NULL)
  {  if(req->rid==rid)
     {  crvprintf("# set_req_tstart_tfinish: request found\n");
        req->tstart-=(cluster.tmpreps-time_mpreps);
        req->tfinish-=(cluster.tmpreps-time_mpreps);
        tstart=req->tstart;
        break;
     }
     req=req->prox;
  }
  rm_schedule_store_queue();
  sem_post(&semqueue);
  return tstart; 
}
//##############################################################################
//## Try to attend a new requisition
//##############################################################################
void try_to_attend_new_req()
{
  SReq *req;
  crvprintf("# try_to_attend_new_req: START\n");

  req=firstqueue;
  if(req==NULL) 
  {  dprintf("# try_to_attend_new_req: FALSE SIGNAL. Someone has released "
             "resources\n");
     return; 
  }

  if(req->when==CR_ALLOC_FUTURE)
  {  dprintf("# try_to_attend_new_req: FALSE SIGNAL. The first one is a "
             "reserve\n");
     return;
  }

  while(req!=NULL)
  {  crvprintf("# try_to_attend_new_req: Get New request: %d %d \n",
               req->rid,req->status);
     if((req->status==CR_SCHED_STATUS_QUEUED || 
         req->status==CR_SCHED_STATUS_QUEUED_C ) && 
         req->when==CR_ALLOC_SOON && req->tstart-cluster.tmpreps<=time(NULL)) 
       
        attend_req(req);
     req=req->prox;
  }
}
//##############################################################################
//## Check if a user (uid) at a give time (currenttime) has an exclusive
//## allocation
//##############################################################################
char uid_time2is_exclusive_allocation(int uid,time_t currenttime)
{
  SReq *req;

  req=firstqueue;
  while(req!=NULL) //get requests that have had started
  {  if(req->uid==uid && req->tstart-cluster.tmpreps<=currenttime)
     {  crvprintf("# uid_time2is_exclusive_allocation: %d %d\n",req->rid,
                  req->shared);
        return !req->shared; 
     }
     req=req->prox;
  }
  return TRUE; //never reach here
}
//##############################################################################
//## Update the freenodes structure
//##############################################################################
void update_freenodes()
{
  int n,s,busy,nodetype;
  time_t currenttime=time(NULL);

  memset(freenodes,0,sizeof(SFreenodes)*(ntypes+1));

  freenodes[0].shared=nnodes; 
  freenodes[0].exclusive=nnodes;

  for(n=0;n<nnodes;n++)
  {  for(s=0;s<nodes[n].ntypes;s++)
     {  nodetype=nodes[n].idtypes[s];
        freenodes[nodetype+1].shared++;
        freenodes[nodetype+1].exclusive++;
     }
  }

  for(n=0;n<nnodes;n++)
  {  busy=cluster.share_nodes;

     for(s=0;s<cluster.share_nodes;s++)
     {  if(idsusing[n][s]!=NOUSER)
        {  if(uid_time2is_exclusive_allocation(idsusing[n][s],currenttime)==
              TRUE
           )
           {  //exclusive nodes cannot have shared requests
              busy=cluster.share_nodes;
              break;
           }
        }
        else
           busy--;
     }

     //nodetype=nodetype2idnodetype(nodes[n].type);
     //shared-- :if it is an exclusive node or all shared places are busy
     if(busy==cluster.share_nodes)
     {  freenodes[0].shared--;
        for(s=0;s<nodes[n].ntypes;s++)
           freenodes[nodes[n].idtypes[s]+1].shared--;
     } 
     //exclusive-- :if someone is on this node
     if(busy>0)
     {  freenodes[0].exclusive--;
        for(s=0;s<nodes[n].ntypes;s++)
          freenodes[nodes[n].idtypes[s]+1].exclusive--;
     }
  }
}
//#############################################################################
//## Check whether the user 'uid' has access on the node 'node'
//#############################################################################
char user_has_node_access(char *node,int uid)
{
  int i,j;

  for(i=0;i<nnodes;i++)
     if( !strcmp(nodes[i].host,node) )
        break;

  if(i==nnodes)
     return FALSE;
  for(j=0;j<cluster.share_nodes;j++) 
     if(idsusing[i][j]==uid)
        return TRUE;
  return FALSE;
}
//##############################################################################
//## User time sleep
//##############################################################################
void *user_time_thread(void *arg)
{
  SReq *user=(SReq*)arg;
  int usrtime,exec_tpreps;
  char login[_POSIX_LOGIN_NAME_MAX],*usernodes,*usernodesinfo;
  struct timeval timeval_start,timeval_finish;
  long time_mpreps;
  pid_t sid=-1;
        
  dprintf("# user_time_thread: START: uid=%d time=%d\n",user->uid,user->time);
  uid2login(user->uid,login);
  pthread_testcancel();

  sem_wait(&semqueue);
  usernodes=rm_uid2nodes(user->uid,&usernodes,CRNODES_USERNODES);
  usernodesinfo=rm_uid2nodes(user->uid,&usernodesinfo,CRNODES_USERNODES_INFO);
  sem_post(&semqueue);
 
  if( strcmp(user->bjscript,"0") )
  {  sid=user->sid;
     crvprintf("# user_time_thread: Batch job mode\n");
  }
  else
      crvprintf("# user_time_thread: Interactive mode\n");

  if(user->exec_tpreps)
     exec_tpreps=TRUE;
  else  //this request was already running
     exec_tpreps=FALSE;
  
  usrtime=(int)(user->tfinish-time(NULL));

  crvprintf(">>> STARTING TIME of user %d\n",user->uid);
        
  if(!cluster.single)
  {  crvprintf(">>> CONTACTING node managers to unlock the access to nodes\n");
     request_nm_lock_node(user->uid,CR_NM_UNLOCK_ACCESS);
  }
  timeval_start.tv_sec=0;
  timeval_finish.tv_sec=0;

  if(exec_tpreps) //execute the UPREPS
  {  crono_log(cluster.logfile,CR_LOG, "crrmd: START USER: %-9s TIME: %-9d "
               "NNODES: %-3d", login,user->time,
               user->nnodes);

     time_mpreps=crem_ms(user->uid,user->rid,cluster,dirconf,usernodes,
                         usernodesinfo,CREM_MPREPS,user->time);
     if(time_mpreps<cluster.tmpreps)
        user->tstart=set_req_tstart_tfinish(user->rid,time_mpreps);
     gettimeofday(&timeval_start,NULL);

     sem_wait(&semqueue);
     set_req_status(user->rid,CR_SCHED_STATUS_UPREPS);
     sem_post(&semqueue);

     crem_send_user_msg(user->uid,CR_USERMSG_TIME_START);
     crem_us(user->uid,usrtime,user->rid,cluster,usernodes,usernodesinfo,
             CREM_UPREPS);
     gettimeofday(&timeval_finish,NULL);
  }

  //we have to taken off the execution time of the upreps from the user time
  usrtime=usrtime-(timeval_finish.tv_sec-timeval_start.tv_sec);
  if(usrtime>0)
  {  if(!strcmp(user->bjscript,"0"))
     {  sem_wait(&semqueue);
        set_req_status(user->rid,CR_SCHED_STATUS_READY);
        sem_post(&semqueue);
        dprintf("# user_time_thread: Sleeping for %d seconds...\n",usrtime);
        rm_sleep(usrtime);
     }
     else
     {  sem_wait(&semqueue);
        set_req_status(user->rid,CR_SCHED_STATUS_BATCH);
        sem_post(&semqueue);
 
        if(exec_tpreps)
           crem_exec_batchjob(user->uid,usrtime,user->rid,user->bjscript,
                              user->path,usernodes,usernodesinfo);
        else
        {  dprintf("# user_time_thread: Waiting end of session execution"
                   " sid=%d\n",sid);
           crem_wait_session_finishes(sid,usrtime);
           dprintf("# user_time_thread: session finished\n");
        }
     }
  }
  pthread_testcancel();  //cancel check point
  crvprintf(">>> END of user time: %s\n",login);

  sem_wait(&semqueue);
  set_req_status(user->rid,CR_SCHED_STATUS_MPOSTPS);
  sem_post(&semqueue);

  crem_send_user_msg(user->uid,CR_USERMSG_TIME_FINISH);
  crem_ms(user->uid,user->rid,cluster,dirconf,usernodes,usernodesinfo,
          CREM_MPOSTPS,user->time);
  crem_us(user->uid,0,0,cluster,usernodes,usernodesinfo,CREM_UPOSTPS);

  sem_wait(&semqueue);
  rls_client_using(user->uid,user->rid);
  rm_schedule_reschedule_all_requests();
  try_to_attend_new_req(); //try to get the next requisition
  sem_post(&semqueue);

  free(user->bjscript);
  free(user->path);
  free(user);
  free(usernodes);
  free(usernodesinfo);
  return NULL;
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
