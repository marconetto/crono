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
//##############################################################################
//## Receive alarm to get a reserve
//##############################################################################
void recv_alarm_to_get_reserve()
{
  time_t now=time(NULL);
  SReq *req;

  crvprintf("Received alarm signal. Starting a reserve...\n");
  sem_wait(&semqueue);

  req=firstqueue;
  while(req!=NULL)
  {  if( req->when==CR_ALLOC_FUTURE && req->tstart<=now ) //this is the reserve
     {  req->when=CR_ALLOC_SOON;
        set_req_status(req->rid,CR_SCHED_STATUS_QUEUED);
        try_to_attend_new_req(); 
        break; 
     }
     req=req->prox;
  }
  rm_schedule_store_queue();
  rm_reserve_set_alarm_for_reserves();
  pthread_testcancel();

  sem_post(&semqueue);
  dprintf("# recv_alarm_to_get_reserve: finished\n");
}
//##############################################################################
//## Add a new reserve.
//##   'req'-> user request.
//##   'nreserves'-> max number of reserves allowed for this user
//##############################################################################
int rm_reserve_add(SReq req,int nreserves)
{
  int answer, i, *available_nodes=NULL, cont_nreserves=0;
  SReq *tmpreq;

  dprintf("# rm_reserve_add: START\n");

  //Check if this user already has a reserve in this period and
  //if he/she has exceeded the maximum number of reserves
  tmpreq=firstqueue;
  while(tmpreq!=NULL)
  {  if(tmpreq->uid==req.uid)
     {  if(  (req.tstart>=tmpreq->tstart && req.tstart<=tmpreq->tfinish)  ||
             (req.tfinish>=tmpreq->tstart && req.tfinish<=tmpreq->tfinish)
          )
           return CR_ERM_RESERVE_UID_EXISTS_THIS_PERIOD;
        else if(tmpreq->when==CR_ALLOC_FUTURE)
        {  cont_nreserves++;
           if(cont_nreserves==nreserves)
              return CR_ERM_NRESERVES_EXCEEDED;
        }
     }  
     tmpreq=tmpreq->prox;
  } 

  //Try to get nodes at tstart time
  available_nodes=rm_schedule_get_nodes(&req,req.tstart-cluster.tmpreps,
                                        &available_nodes);
  if(available_nodes)
  {  answer=CR_REQ_QUEUED;
     req.nodes=(int*)malloc(sizeof(int)*req.nnodes);
     memcpy(req.nodes,available_nodes,sizeof(int)*req.nnodes);
     for(i=0;i<req.nnodes;i++)
        dprintf("node available reserve-> %d\n",req.nodes[i]);
     req.rid=get_newrid();
     rm_make_allocation(req,FALSE);
     free(req.nodes);
     rm_reserve_set_alarm_for_reserves();
     free(available_nodes);
     dprintf("# rm_reserve_add: Reserve added\n");
  }
  else
  {  answer=CR_ERM_RESERVE_COLLISION;
     dprintf("# rm_reserve_add: Reserve NOT added. Collision detected\n");
  }
  
  return answer;
}
//##############################################################################
//## Execute recv_alarm_to_get_reserve after a specific time
//##############################################################################
void *rm_reserve_alarm(void *arg)
{
  time_t time_alarm=(long)arg;

  reserve_tid=pthread_self();
  dprintf("# rm_reserve_alarm: reserve_tid=%ld\n",reserve_tid);
  crvprintf("# rm_reserve_alarm: Check the reserves after %ld seconds\n",
            time_alarm);
  sleep(time_alarm);
  reserve_tid=-1;
  pthread_testcancel();  //cancel point!!
  recv_alarm_to_get_reserve();
  return NULL;
}
//##############################################################################
//##  Delete a reserve
//##############################################################################
int rm_reserve_del(SRls rls)
{
  int answer=CR_RESERVE_NOT_DELETED;
  SReq *req;

  req=firstqueue;
  while(req!=NULL)
  {  
     if(req->rid==rls.rid && req->when==CR_ALLOC_FUTURE && 
        (req->uid==rls.uid || rls.requid==0) )
     {  answer=CR_REQUEST_CANCELLED;
        rm_schedule_ret_req_from_queue(&firstqueue,&lastqueue,req->rid);
     }
     req=req->prox; 
  }
  rm_schedule_store_queue();
  rm_reserve_set_alarm_for_reserves();
  
  return answer;
}
//##############################################################################
//## Set the alarm using the earliest time starting of reserves
//##############################################################################
void rm_reserve_set_alarm_for_reserves()
{
  long talarm=-1,now=time(NULL);
  pthread_t old_reserve_tid=-1; //to kill the rm_reserve_alarm thread
  SReq *node;

  crvprintf("# rm_reserve_set_alarm_for_reserves: Setting alarm for "
             "reserves\n");
  node=firstqueue;
  while(node!=NULL)
  {  if(node->when==CR_ALLOC_FUTURE)
     {  dprintf("# rm_reserve_set_alarm_for_reserves: found a reserve\n");
        if(talarm!=-1)
        {  if(talarm>node->tstart) 
              talarm=node->tstart;
        }
        else
           talarm=node->tstart; 
     }
     node=node->prox;
  }
  if(reserve_tid!=-1)
  {  old_reserve_tid=reserve_tid; 
     reserve_tid=-1;
  }
  if(talarm==-1)
     dprintf("# rm_reserve_set_alarm_for_reserves: There aren't any "
             " reserves\n");
  else
  {  crvprintf("# set_alarm_for_reserves: talarm=%ld now=%ld alarm(%ld)\n",
               talarm,now,talarm-now);
     crvprintf("Creating rm_reserve_alarm thread\n");
     pthread_create(&reserve_tid, NULL, rm_reserve_alarm,(void*)(talarm-now));
  }
  if(old_reserve_tid!=-1)
  {  dprintf("# set_alarm_for_reserves: cancel %ld\n",old_reserve_tid);
     pthread_cancel(old_reserve_tid);
  }
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
