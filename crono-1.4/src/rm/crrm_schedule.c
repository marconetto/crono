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

//##############################################################################
// These functions are responsible for scheduling of the user requests
//##############################################################################
#include "common.h"
#include "crrmd.h"
#include "crrm_schedule.h"
#include "crrm_em.h"
//##############################################################################
//## Check if this node(nodeid) must be tested
//## if it is a qualitative allocation it is verified the user request
//## if it is a quantitative allocation it is verified the access rights
//##    These information are in tmp_nodetypes variable
//## This function also verifies if the 'nodeid' node is already in
//## possible_nodes list.
//##
//## Remember a node can have more than one type
//##############################################################################
char rm_check_if_try_this_node(int nodeid, int *typeid,
                               SReqnodetype *tmp_nodetypes,int reqntypes,
                               int *possible_nodes,int npossible_nodes)
{
  int i,this_nodetype;   

  for(i=0;i< npossible_nodes;i++)
     if(possible_nodes[i]==nodeid)
        return FALSE;
 
  //free access, probably root has made a reserve (quantitative)
  if(!reqntypes)
     return TRUE; 

  //For each node type of this node...
  //Note, the base nodetype is the last to be tested
  //Ex.: node1:p4,p4v   -> p4v is tested before p4
  for(i=nodes[nodeid].ntypes-1; i >=0;i--)
  { 
     this_nodetype=nodes[nodeid].idtypes[i];
     for(*typeid=0; *typeid < reqntypes; (*typeid)++ )
     {
        if( tmp_nodetypes[*typeid].idtype == this_nodetype && 
           tmp_nodetypes[*typeid].nnodes > 0)
              //Node type OK and this type needs (qualitative)
              //or is allowed (quantitative) to have more nodes
          return TRUE;
     }
  }
 
  return FALSE;
}
//##############################################################################
//## Schedule a request 
//##############################################################################
void rm_schedule(SReq *req)
{
  time_t *temp_etimes=NULL;
  int i,*available_nodes=NULL;//possible nodes for allocation
  int n;
  
  crvprintf(">>> START SCHEDULE: uid=%d shared=%d\n",req->uid,req->shared);

  //Get the event times
  rm_schedule_make_etimes();

  //The current time is also considered an event time (the first event time)
  temp_etimes=(time_t*)malloc(sizeof(time_t)*(netimes));
  dprintf("# rm_schedule: netimes=%d\n",netimes);
  memcpy(temp_etimes,etimes,sizeof(time_t)*netimes);

  crvprintf("+++++++++++++++++\n");
  rm_schedule_show_queue(firstqueue);

  for(i=0;i<netimes;i++)//for each event time
    crvprintf("# rm_schedule: ETIME[%d]=%ld\n",i,temp_etimes[i]);
  crvprintf("+++++++++++++++++\n");

  for(i=0;i<netimes;i++)//for each event time
  {  dprintf("# rm_schedule: BEGIN temp_etimes[%d]=%ld\n",i,temp_etimes[i]);

     //+1 second
     available_nodes=rm_schedule_get_nodes(req,temp_etimes[i]+1,
                                           &available_nodes);
     dprintf("# rm_schedule: END temp_etimes[%d]=%ld\n",i,temp_etimes[i]);
     if(available_nodes)
        break;
  }//for each event time

  if(available_nodes)
  {  dprintf("# rm_schedule: iet possible=%d req->nnodes=%d\n",i,req->nnodes);
     for(n=0;n<req->nnodes;n++)
       dprintf("# rm_schedule: N=%d\n",available_nodes[n]);

     req->nodes=(int*)malloc(sizeof(int)*req->nnodes);
     memcpy(req->nodes,available_nodes,sizeof(int)*req->nnodes);

     req->tstart=temp_etimes[i]+cluster.tmpreps;
     req->tfinish=req->tstart+req->time*60;
     rm_schedule_add_req_to_queue(&firstqueue,&lastqueue,req);

     free(temp_etimes);
     if(available_nodes!=NULL)
        free(available_nodes);
  }
  crvprintf("*****************\n");
  rm_schedule_show_queue(firstqueue);
  crvprintf("*****************\n");
  crvprintf(">>> END SCHEDULE\n");
}
//#############################################################################
//## Add an Event time
//#############################################################################
void rm_schedule_add_event_time(time_t timet)
{
  int i;

  for(i=0;i<netimes;i++)
     if( etimes[i] == timet) //time is already set
        return;

  etimes=(time_t*)realloc(etimes,sizeof(time_t)*(netimes+1));
  etimes[netimes++]=timet;
}
//#############################################################################
//## Add a new request (req) to the queue. Sort by starting time
//#############################################################################
void rm_schedule_add_req_to_queue(SReq **first,SReq **last,SReq *req)
{
  SReq *aux,*aux2;

  req->prox=NULL;
  if((*first)==NULL)
  {  (*first)=req;
     (*last)=req;
  }
  else
  {  aux=*first;
     aux2=NULL;
     while(aux!=NULL)
     {  if( (req->tstart <= aux->tstart)  && aux==*first )
        {  req->prox=aux;
           (*first)=req;
            return;
        }
        else if( (req->tstart >= aux->tstart) && (aux->prox==NULL) )
        {  aux->prox=req;
           (*last)=req;
           return;
        }
        else
        {  if(aux->prox!=NULL)
           {  aux2=aux->prox;
              if( (req->tstart >= aux->tstart) && 
                  (req->tstart <= aux2->tstart) )
              {  aux->prox=req;
                 req->prox=aux2;
                 return;
              }
           }
        }
        aux=aux->prox;
     }
  }
}
//#############################################################################
//## Get nodes to a request(req) at a givent time(time)
//#############################################################################
int *rm_schedule_get_nodes(SReq *req,time_t basetime, int **available_nodes)
{                                        
  char possible,
       shared_req, 
       try_this,
       attempt;
  int p,nodeid,
      typeid, // counter for tmp_nodetypes
      *possible_nodes; //possible nodes for allocation
  SReq *tmpreq;   
  time_t usertimeslot=(req->time*60+cluster.tmpreps+cluster.tmpostps);
  SReqnodetype *tmp_nodetypes=NULL;

  possible_nodes=NULL;
  *available_nodes=NULL;
  possible=FALSE;

  p=0; // counter for 'possible_nodes'
  if(req->ntypes)
  {  tmp_nodetypes=(SReqnodetype*)malloc(req->ntypes*sizeof(SReqnodetype));
     memcpy(tmp_nodetypes,req->nodetypes,req->ntypes*sizeof(SReqnodetype));
  }

  //attempt<2
  //SHARE REQ: First, it tries to get free shared nodes
  //           Second, it tries to get free exclusive nodes
  //EXCLUSIVE REQ: First: it tries to get free exclusive nodes
  //               There is no Second attempt
  for(attempt=0;attempt<2;attempt++)
  {  dprintf("# rm_schedule_get_nodes: ATTEMPT=%d\n",attempt);

     if(!req->shared) //exclusive allocation
        shared_req=FALSE;
     else
        shared_req=TRUE; 
        //First, shared allocation tries to get free shared nodes

     if(attempt==1)  //Now, shared allocation tries to get empty nodes
        shared_req=FALSE;
     
     for(nodeid=0;nodeid<nnodes;nodeid++) //look for each cluster node
     {  
        try_this=rm_check_if_try_this_node(nodeid,&typeid,tmp_nodetypes,
                                           req->ntypes, possible_nodes,p);
        dprintf("NODEID[%d]=try_this=%d\n",nodeid,try_this);
        // If this node must be checked and it is available, add this node
        // to the list of possible nodes ('possible_nodes')
        if(try_this && 
           rm_schedule_is_possible_all_nodesuids(req,nodeid,basetime,shared_req))
        { 
           possible_nodes=(int*)realloc(possible_nodes,sizeof(int)*(p+1));
           possible_nodes[p++]=nodeid;
           dprintf("# rm_schedule_get_nodes: Node %d is possible.\n",nodeid);

           if(req->ntypes) //update number of nodes for alloc.
              tmp_nodetypes[typeid].nnodes--; 

           dprintf("# rm_schedule_get_nodes: Possible nodes=%d/%d\n",p,
                   req->nnodes);
           if(p==req->nnodes)
           {  possible=TRUE;
              break;
           }
        }
     }
     //if the allocation is possible: OK
     //if it is an exclusive request, there is no second attempt
     if(possible || !req->shared)
        break;
  }
  free(tmp_nodetypes);

  if(possible)
  {  //Check if there is a reserve in this period
     // CRONO does not allow a user has more the one request
     // attended at same time because the creation of the machine files
     // If you don't care about this, comment this part of the code
     //----begin:  checking conflict      
     tmpreq=firstqueue;
     while(tmpreq!=NULL)
     {  if( (tmpreq->uid==req->uid) &&
            !( basetime >= tmpreq->tfinish+cluster.tmpostps || 
               basetime+usertimeslot <= tmpreq->tstart-cluster.tmpreps
             )
           )
        { 
           free(possible_nodes);
           if(req->status!=CR_SCHED_STATUS_QUEUED_C)
           {  req->status=CR_SCHED_STATUS_QUEUED_C;
              crem_send_user_msg(req->uid,CR_USERMSG_CONFLICT_WITH_A_RESERVE);
           }
           return NULL;
        }
        tmpreq=tmpreq->prox;        
     }
     //----end:  checking conflict      
     *available_nodes=(int*)malloc(sizeof(int)*req->nnodes);
     memcpy(*available_nodes,possible_nodes,sizeof(int)*req->nnodes);
  }

  free(possible_nodes);
  
  return *available_nodes;
}
//##############################################################################
//## Check if a request(req/node n) is possible for the needed event times 
//##############################################################################
char rm_schedule_is_possible_all_nodesuids(SReq *req,int n,time_t basetime,
                                           char shared)
{
  int iet=0;
  time_t usertimeslot=(req->time*60+cluster.tmpreps+cluster.tmpostps);

  dprintf("# rm_schedule_is_possible_all_nodesuids: start NODE=%d\n",n);
  if(rm_schedule_is_possible_one_nodesuids(req,n,shared,basetime))
  {  dprintf("base OK: testing for the rest of the event times: NODE=%d\n",n);
     for(iet=0;iet<netimes;iet++)
       if( etimes[iet]>=basetime && etimes[iet]< (basetime+usertimeslot) )
         {  dprintf("etimes[%d]=%ld\n",iet,etimes[iet]);
            if(!rm_schedule_is_possible_one_nodesuids(req,n, shared,etimes[iet]))
               return FALSE;
         }
  }
  else 
     return FALSE;

  return TRUE;
}
//##############################################################################
//## Check if a request(req/node n) is possible at a given 
//## event time (basetime)
//##############################################################################
char rm_schedule_is_possible_one_nodesuids(SReq *req,int n, char shared,
                                           time_t basetime)
{
  SReq *reqtmp;
  int nshared=0;
  char exclusive_alloc=FALSE;

  dprintf("# rm_schedule_is_possible_one_nodesuids: start\n");
  dprintf("base time= %ld, shared=%d\n",basetime,shared);
  reqtmp=firstqueue;
  while(reqtmp!=NULL)
  {  if( (basetime>=reqtmp->tstart-cluster.tmpreps) &&
        (basetime<=reqtmp->tfinish+cluster.tmpostps) && 
        rm_schedule_request_has_node(reqtmp,n)                     
       )
     {  //this user (will be)/is using the node n in this period
        if(reqtmp->shared)
           nshared++;   
        else
        {  exclusive_alloc=TRUE;
           break;
        }
     }
     reqtmp=reqtmp->prox;
  }

  if(!shared)
  {  if(nshared || exclusive_alloc)
        return FALSE;
  }
  else //shared allocation
  {  //if this node has an exclusive request or 
     //this node has reached maximum user to share a node
     if(exclusive_alloc || nshared==cluster.share_nodes)
        return FALSE;
  }

  dprintf("# rm_schedule_is_possible_one_nodesuids: finish. NODE %d ok\n",n);
  return TRUE;
}
//##############################################################################
//## Check if a user id is in the queue
//##############################################################################
int rm_schedule_is_uid_in_queue(SReq **first,int uid)
{
  SReq *req;

  req=(*first);
  while(req!=NULL)
  {  if(req->uid==uid) 
       return TRUE;
     req=req->prox;
  }
  return FALSE;
}
//##############################################################################
//## Make the array with the event times(etimes) of the schedule 
//## Event time is when a user time starts or finishes
//##############################################################################
void rm_schedule_make_etimes()
{  
  SReq *req;
  time_t currenttime=time(NULL);
  int i,j;
 
  //clear the event times
  free(etimes);
  netimes=0;
  etimes=NULL;

  rm_schedule_add_event_time(currenttime);
  req=firstqueue;
  while(req!=NULL)
  {  if(req->tstart-cluster.tmpreps>currenttime)
        rm_schedule_add_event_time(req->tstart-cluster.tmpreps);
     if(req->tfinish+cluster.tmpostps>currenttime)
        rm_schedule_add_event_time(req->tfinish+cluster.tmpostps);
     req=req->prox;
  }
  //Sort the event times
  //For this, 'currenttime' is a temporary variable
  for(i=0;i<netimes;i++)
     for(j=0;j<netimes-1-i;j++)
     {  if(etimes[j]>etimes[j+1])
        {  currenttime=etimes[j];
           etimes[j]=etimes[j+1];
           etimes[j+1]=currenttime;
        }
    }
}
//##############################################################################
//## Recover the queue from queue file (cluster.queuefile) 
//##############################################################################
void rm_schedule_recover_queue()
{
  FILE *fp;
  long now=time(NULL);
  char *line,*buf,*buf2,*token;
  int nsyntax,i,reqntypes;
  SReq req;

  line=(char*)malloc(sizeof(char)*(S_LINEF+BUFNODES));
  buf=(char*)malloc(sizeof(char)*BUFNODES);
  buf2=(char*)malloc(sizeof(char)*BUFNODES);

  req.bjscript=(char*)malloc(sizeof(char)*S_PATH);
  req.path=(char*)malloc(sizeof(char)*S_PATH);
  crvprintf("# get_queue_from_file: Recover the queue from file \"%s\"\n",
            cluster.queuefile);
  if( (fp=fopen(cluster.queuefile,"r")) != NULL)
  {  while(fgets(line,S_LINEF,fp))
     {  
        line[strlen(line)-1]=0; 
        dprintf("LINE=<%s>\n",line);
        nsyntax=sscanf(line,
                   "%ld|%ld|%d|%d|%d|%d|%c|%c|%c|%d|%[^|]|%[^|]|%[^|]|%d|%[^|]",
                    &req.tstart,&req.tfinish,&req.rid,&req.uid,&req.nnodes,
                    &req.time,&req.shared,&req.when,&req.qualitative,&req.sid,
                    req.bjscript,req.path,buf,&reqntypes,buf2);

        req.shared-='0'; //get integer value instead of char value
        req.when-='0'; 
        req.qualitative-='0'; 

        if( nsyntax!=14 && nsyntax!=15)
        {  printf("Queue file has syntax error.\n"
                  "Ignoring the rest of this file.\n");
           break;
        }
        
        if(req.tfinish-60>=now) // consider this request
        {  req.nodes=NULL;
           i=0;
           token=strtok(buf,".");
           while(token) //get the nodes list
           {  req.nodes=(int*)realloc(req.nodes,sizeof(int)*(i+1));
              req.nodes[i++]=atoi(token);
              token=strtok(NULL,".");
           }
           req.ntypes=reqntypes;
           req.nodetypes=NULL;
           if(reqntypes)
           {  i=0;
              token=strtok(buf2,":");
              while(token) //get the nodetypes list
              {  req.nodetypes=(SReqnodetype*)realloc(req.nodetypes,
                                                    sizeof(SReqnodetype)*(i+1));
                 req.nodetypes[i].idtype=atoi(token);
                 token=strtok(NULL,".");
                 req.nodetypes[i++].nnodes=atoi(token);
                 token=strtok(NULL,":");
              }
           }

           if(req.tstart<now)  //request was running
           {  crvprintf("# get_queue_from_file: request was running\n");
              req.exec_tpreps=FALSE;
           } 
           else
              req.exec_tpreps=TRUE;
           //Check if the user requested more nodes than the number
           //of cluster nodes.
           rm_check_requested_resources_exist(&req); 
           rm_make_allocation(req,FALSE);

           free(req.nodes);
           if(reqntypes)
              free(req.nodetypes);
        }
        else
          dprintf("# get_queue_from_file: DO NOT consider this request\n");
     }
     fclose(fp);
  }
  free(line);
  free(buf);
  free(buf2);
  free(req.bjscript);
  free(req.path);
}
//##############################################################################
//## Reschedule all requests 
//##############################################################################
void rm_schedule_reschedule_all_requests()
{
  SReq *aux,*aux2,*firstaux=NULL,*lastaux=NULL,*reqaux;
  int i;
  char add_req;

  crvprintf("# rm_schedule_reschedule_all_requests: START\n");
  aux=firstqueue;
  dprintf("# rm_schedule_reschedule_all_requests: copy queue to auxqueue\n");
  while(aux!=NULL)  // copy the queue to auxqueue
  {  reqaux=(SReq*)malloc(sizeof(SReq));
     memcpy(reqaux, aux, sizeof(SReq));
     reqaux->bjscript=(char*)malloc(sizeof(char)*(strlen(aux->bjscript)+1));
     strcpy(reqaux->bjscript,aux->bjscript);
     reqaux->path=(char*)malloc(sizeof(char)*(strlen(aux->path)+1));
     strcpy(reqaux->path,aux->path);
     reqaux->nodes=(int*)malloc(sizeof(int)*(aux->nnodes));
     memcpy(reqaux->nodes,aux->nodes,sizeof(int)*(aux->nnodes));
     if(aux->ntypes)
     {  reqaux->nodetypes=(SReqnodetype*)malloc(sizeof(SReqnodetype)*
                                                     (aux->ntypes));
        memcpy(reqaux->nodetypes,aux->nodetypes,sizeof(SReqnodetype)*
                                                      (aux->ntypes));
     }
     else
        reqaux->nodetypes=NULL;

     rm_schedule_add_req_to_queue(&firstaux,&lastaux,reqaux);
     aux=aux->prox;
  }
  rm_schedule_show_queue(firstaux);
  aux=firstqueue;
  while(aux!=NULL) // delete queue
  {  aux2=aux->prox;
     rm_schedule_ret_req_from_queue(&firstqueue,&lastqueue,aux->rid);
     aux=aux2;
  }
  firstqueue=NULL;
  lastqueue=NULL;

  //rebuild the queue 
  //i=1(ready requests) i=2(reserves) i=3(queued)
  for(i=1;i<=3;i++)
  {  aux=firstaux;
     while(aux!=NULL)
     {  add_req=FALSE;
        //first -> put the ready users to the queue   
        if( (i==1) && ( aux->status==CR_SCHED_STATUS_READY   ||
                        aux->status==CR_SCHED_STATUS_MPREPS  ||
                        aux->status==CR_SCHED_STATUS_UPREPS  ||
                        aux->status==CR_SCHED_STATUS_MPOSTPS ||
                        aux->status==CR_SCHED_STATUS_BATCH )
          )
             add_req=TRUE;
        //second -> put the reserves to the queue
        else if( (i==2) && (aux->status==CR_SCHED_STATUS_RESERVED))
           add_req=TRUE;
        //third -> put the queued users to the queue
        else if( (i==3) && (aux->status==CR_SCHED_STATUS_QUEUED || 
                            aux->status==CR_SCHED_STATUS_QUEUED_C )
               )
           add_req=TRUE;

        if(add_req)
        {  reqaux=(SReq*)malloc(sizeof(SReq));
           memcpy(reqaux, aux, sizeof(SReq));
           reqaux->bjscript=(char*)malloc(sizeof(char)*(strlen(aux->bjscript)
                                                       +1));
           strcpy(reqaux->bjscript,aux->bjscript);
           reqaux->path=(char*)malloc(sizeof(char)*(strlen(aux->path)+1));
           strcpy(reqaux->path,aux->path);
           if(i==1 || i==2) //ready or reserves 
           {  reqaux->nodes=(int*)malloc(sizeof(int)*(aux->nnodes));
              memcpy(reqaux->nodes,aux->nodes,sizeof(int)*(aux->nnodes));
           }

           if(aux->ntypes)
           {  reqaux->nodetypes=(SReqnodetype*)malloc(sizeof(SReqnodetype)*
                                                     (aux->ntypes));
              memcpy(reqaux->nodetypes,aux->nodetypes,sizeof(SReqnodetype)*
                                                     (aux->ntypes));
           }
           else
              reqaux->nodetypes=NULL;

           if(i==3) //queued
              rm_schedule(reqaux);
           else 
              rm_schedule_add_req_to_queue(&firstqueue,&lastqueue,reqaux);
        }
        aux=aux->prox;
     }
  }
  aux=firstaux;
  while(aux!=NULL) // delete aux queue
  {  aux2=aux->prox;
     rm_schedule_ret_req_from_queue(&firstaux,&lastaux,aux->rid);
     aux=aux2;
  }
  rm_schedule_store_queue();
  crvprintf("# rm_schedule_reschedule_all_requests: FINISH\n");
}
//##############################################################################
//## Retrieve a request from the queue
//##############################################################################
void rm_schedule_ret_req_from_queue(SReq **first,SReq **last,int rid)
{
  SReq *req=NULL,*prior;
  char found=FALSE;

  dprintf("# rm_schedule_ret_req_from_queue: STARTED\n");
  dprintf("# rm_schedule_ret_req_from_queue: RID=%d\n",rid);
  if((*first)!=NULL)
  {  req=(*first);
     prior=NULL;
     while(req!=NULL)
     {  if( req->rid==rid)
        {
           if(req==(*first)) //first of queue
           {  (*first)=(*first)->prox; 
              if (req==(*last))
                 (*last)=prior;
              found=TRUE;
              break;
           }
           else  
           {  if (req==(*last))
                 (*last)=prior;
              prior->prox = req->prox;  
              found=TRUE;
              break;
           }
        } 
        else //go to the next req
        {  prior=req;
           req=req->prox;
        }     
     }//while
  }//if
  if(found) //free memory
  {  free(req->bjscript);
     free(req->path);
     free(req->nodes);
     free(req->nodetypes);
     free(req);
     req=NULL; 
  }
}
//##############################################################################
//## Check if the request 'req' has the node 'n'
//##############################################################################
char rm_schedule_request_has_node(SReq *req,int n)
{
  int i;
   
  for(i=0;i<req->nnodes;i++)
     if(req->nodes[i]==n)
        return TRUE; 
  
  return FALSE;
}
//##############################################################################
//## Show the requests queue
//##############################################################################
void rm_schedule_show_queue(SReq *first)
{
  SReq *req;

  dprintf("# rm_schedule_show_queue: INI-------show-------\n");
  req=first;   
  if(first==NULL) 
     dprintf("# rm_schedule_show_queue: queue empty!!\n");
  while(req!=NULL)
  {  crvprintf("# rm_schedule_show_queue: show: uid <%d> when <%d> Node[0]=%d "
               "shared=%d tstart=%ld tfinish=%ld\n,time=%d", req->uid,req->when,
               req->nodes[0], req->shared,req->tstart,req->tfinish,req->time);
     req=req->prox;
  }
  dprintf("# rm_schedule_show_queue: END-------show-------\n");
}
//##############################################################################
//## Store queue on a file
//##############################################################################
void rm_schedule_store_queue()
{        
  FILE *fp;
  SReq *req;
  char *buf,*offset;
  int i;
  
  buf=(char*)malloc(sizeof(char)*BUFNODES);
  crvprintf("# rm_schedule_store_queue: <%s>\n",cluster.queuefile);
  if ( (fp=fopen(cluster.queuefile,"w+") ) == NULL)
     crono_log(cluster.logfile,CR_ERROR,
               "crrmd: Problems to open queuefile: %s",cluster.queuefile);
  req=firstqueue;
  while(req!=NULL)
  {  memset(buf,0,sizeof(char)*BUFNODES);
     offset=buf;
     for(i=0;i<req->nnodes;i++)
     {  sprintf(offset,"%d.%c",req->nodes[i],0);
        offset=offset+sizeof(char)*(strlen(offset));
     }
     buf[strlen(buf)-1]=0;
     fprintf(fp,"%ld|%ld|%d|%d|%d|%d|%d|%d|%d|%d|%s|%s|%s|",req->tstart,
             req->tfinish,req->rid, req->uid, req->nnodes,req->time,req->shared,
             req->when,req->qualitative,req->sid,req->bjscript,req->path,buf);

     fprintf(fp,"%d|",req->ntypes); 
     for(i=0;i<req->ntypes;i++)
       fprintf(fp,"%d:%d.",req->nodetypes[i].idtype,req->nodetypes[i].nnodes);
     fprintf(fp,"\n");

     req=req->prox;
  }
  fclose(fp);
  free(buf);
}
//#############################################################################
//## Check if user is already in the queue and he/she wants resources as soon
//## as possible 
//#############################################################################
char user_has_allocated(int uid)
{
  SReq *req;
 
  crvprintf("# user_has_allocated: STARTED uid=%d\n",uid);
  //look for in queue
  req=firstqueue;
  while(req!=NULL)
  {  if(req->uid==uid && req->when==CR_ALLOC_SOON)
     {  dprintf("# user_has_allocated: YES status=%d\n",req->status);
        return TRUE;
     }
     req=req->prox;
  }
  dprintf("# user_has_allocated: NO\n");
  return FALSE;
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
