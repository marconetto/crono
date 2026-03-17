//##############################################################################
//# Copyright (C) 2002-2004 Marco Aurelio Stelmar Netto <stelmar@cpad.pucrs.br>
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
#include "crui.h"
#include "crui_api.h"

//##############################################################################
//## CRONO ALLOCATE NODES (CRALLOC)
//##############################################################################
int cralloc(SAlloc alloc,char *am_server,int port)
{
  int sd,          //socket descriptor
      answer,      //answer from server
      bnodes=0,    //bytes in alloc.nodes
      i,ntypes;
  SPheader packet;
  void *buf,*offset;

  dprintf("PORT: %d SERVER: %s\n",port,am_server);

  if(alloc.bjscript==NULL)
  {  alloc.bjscript=(char*)malloc(sizeof(char)*2);
     strcpy(alloc.bjscript,"0");
     alloc.path=(char*)malloc(sizeof(char)*2);
     strcpy(alloc.path,"0");
  }

  if(alloc.tstart!=0)
  {  alloc.when=CR_ALLOC_FUTURE;
     alloc.requid=getuid();  //get the requester id
     if(alloc.tfinish!=0)
        alloc.time=(int)((int)(alloc.tfinish-alloc.tstart)/60);
     else
        alloc.tfinish=alloc.tstart+alloc.time*60;
  }
  else
    alloc.when=CR_ALLOC_SOON;

  //qualitative allocation
  if(alloc.qualitative && alloc.nodes!=NULL) //set the bnodes variable
  {  memcpy(&ntypes,alloc.nodes,sizeof(int));
     bnodes=sizeof(int);
     for(i=0;i<ntypes;i++)
     {  bnodes+=sizeof(char)*(strlen(alloc.nodes+bnodes)+1); 
        bnodes+=sizeof(int);
     }
  }

  packet.type=CR_ALLOC;
  packet.size=sizeof(SPheader) + S_PACK_SALLOC + bnodes+
              sizeof(char)*(strlen(alloc.cluster)+
                             strlen(alloc.bjscript)+
                             strlen(alloc.path)+3);

  if( (buf=malloc(packet.size))==NULL)
     return CR_ERROR_ALLOC_MEMORY;

  offset=buf;
  memcpy(offset,&packet,sizeof(SPheader));
  offset+=sizeof(SPheader);
  memcpy(offset,&alloc,S_PACK_SALLOC);
  offset+=S_PACK_SALLOC;
  strcpy(offset,alloc.cluster);
  offset+=sizeof(char)*(strlen(alloc.cluster)+1);
  strcpy(offset,alloc.bjscript);
  offset+=sizeof(char)*(strlen(alloc.bjscript)+1);
  strcpy(offset,alloc.path);

  if(bnodes>0) //qualitative allocation
  {  offset+=sizeof(char)*(strlen(alloc.path)+1);
     memcpy(offset,alloc.nodes,bnodes);
  }

  if( (sd=create_client_tcpsocket(am_server,port)) == CR_ERROR)
  {  free(buf);
     return CR_EAM_CANNOT_CONNECT_AM;
  }

  if( send(sd, buf, packet.size, 0) < 0 )
  {  free(buf);
     close(sd);
     return CR_ERROR_TRANSFER_DATA;
  }
  recv(sd,&answer,sizeof(int),0);

  close(sd);
  free(buf);
  return answer;
}
//##############################################################################
//## CRONO INFORMATION (CRINFO)
//##############################################################################
int crinfo(SInforeq inforeq,char *am_server,int port,SUIPacket *uipacket)
{
  int sd;          //socket descriptor
  SPheader packet;
  void *buf;

  uipacket->data=NULL;
  uipacket->size=0;
 
  dprintf("PORT: %d SERVER: %s\n",port,am_server);
  dprintf("Cluster=%s\n", inforeq.cluster);
  packet.type=CR_INFO;
  packet.size=(sizeof(SPheader)+sizeof(SInforeq)+
               sizeof(char)*(strlen(inforeq.cluster)+1));
  dprintf("main: packet size to send=%d\n",packet.size);
  dprintf("main: creating packet\n");

  //create packet
  if( (buf=malloc(packet.size))==NULL)
     return CR_ERROR_ALLOC_MEMORY;

  memcpy(buf,&packet,sizeof(SPheader));
  memcpy(buf+sizeof(SPheader),&inforeq,sizeof(SInforeq));
  strcpy(buf+sizeof(SPheader)+sizeof(SInforeq),inforeq.cluster);

  //create connectio to AM
  if( (sd=create_client_tcpsocket(am_server,port)) == CR_ERROR)
  {  free(buf);
     return CR_EAM_CANNOT_CONNECT_AM;
  }

  //send packet
  dprintf("main: sending packet to AM\n");
  if(send(sd, buf, packet.size, 0) < 0 )
  {  close(sd);
     free(buf);
     return CR_ERROR_TRANSFER_DATA;
  }
  free(buf);

  //receive packet header from AM 
  if( recv(sd,uipacket,sizeof(SPheader),0) < 0) 
  {  close(sd);
     return CR_ERROR_TRANSFER_DATA;
  }
 
  if(uipacket->type==CR_OK)
  {  //receive list of nodes from AM
     if(uipacket->size-sizeof(SPheader)>0)
     {  uipacket->data=malloc(uipacket->size);
        if( recv(sd,uipacket->data,uipacket->size-sizeof(SPheader),0) < 0)
        {  close(sd);
           free(uipacket->data);
           return CR_ERROR_TRANSFER_DATA; 
        }
     }
  }
  close(sd);
  return uipacket->type;
}
//##############################################################################
//## Get the maximum number of nodes allowed for allocation now
//##############################################################################
int crinfo_getmaxnnodes(SInforeq inforeq, char *am_server,int port,
                        int *maxnnodes)
{
  SUIPacket uipacket;
  Sinfoshow *infoshow;
  int ninfoshow, answer;
 
  *maxnnodes=0; 
  answer=crinfo(inforeq,am_server,port,&uipacket);
  if(uipacket.data!=NULL) 
  {
    crinfo_set_infoshow(uipacket.data,uipacket.size,&infoshow,&ninfoshow);
    //if there are more clusters, it gets only the first one
    *maxnnodes=infoshow[0].usermaxnnodes; 
    crinfo_free_infoshow(infoshow,ninfoshow);
  }

  return answer;
}
//##############################################################################
//## Get the maximum time allowed for allocation now
//##############################################################################
int crinfo_getmaxtime(SInforeq inforeq,char *am_server,int port, int *maxtime)
{
  SUIPacket uipacket;
  Sinfoshow *infoshow;
  int ninfoshow, answer;
  
  *maxtime=0;
  answer=crinfo(inforeq,am_server,port,&uipacket);
  if(uipacket.data!=NULL) 
  {
    crinfo_set_infoshow(uipacket.data,uipacket.size,&infoshow,&ninfoshow);
    //if there are more clusters, it gets only the first one
    *maxtime=infoshow[0].usermaxtime; 
    crinfo_free_infoshow(infoshow,ninfoshow);
  }

  return answer;
}
//##############################################################################
//## Release memory of the infoshow structure
//##############################################################################
void crinfo_free_infoshow(Sinfoshow *infoshow, int ninfoshow)
{
  int i,n;

  for(n=0;n<ninfoshow;n++)
  {
     free(infoshow[n].cluster);
     free(infoshow[n].group);
     free(infoshow[n].types);
     free(infoshow[n].special);

     for(i=0;i<infoshow[n].ntypes;i++)
        free(infoshow[n].infotypes[i].type);
     free(infoshow[n].infotypes);

     for(i=0;i<infoshow[n].ninfo;i++)
        free(infoshow[n].info[i].nodetype);
     free(infoshow[n].info);
    
  }
  free(infoshow);
}
//##############################################################################
//## Unpack data from AM and put in Sinfoshow structure
//##############################################################################
void crinfo_set_infoshow(void *data, int size, Sinfoshow **infoshow,
                         int *ninfoshow)
{
  void *offset;
  int i;
 
  *infoshow=NULL;
  *ninfoshow=0;
  size-=sizeof(SPheader); //take off the packet header
  offset=data; 

  //Unpack data (for each cluster the AM has access to)
  while( offset < data+size )
  {
     *infoshow=(Sinfoshow*)realloc(*infoshow,sizeof(Sinfoshow)*(*ninfoshow+1));

     //Unpack cluster 
     (*infoshow)[*ninfoshow].cluster=(char*)malloc(sizeof(char)*
                                                   (strlen((char*)offset)+1));
     strcpy((*infoshow)[*ninfoshow].cluster,offset);
     offset+=sizeof(char)*(strlen(offset)+1);

     //Unpack group
     (*infoshow)[*ninfoshow].group=(char*)malloc(sizeof(char)*
                                           (strlen((char*)offset)+1));
     strcpy((*infoshow)[*ninfoshow].group,offset);
     offset+=sizeof(char)*(strlen(offset)+1);

     //Unpack node type specifications (type,arch,nprocs,desc..)
     (*infoshow)[*ninfoshow].types=(char*)malloc(sizeof(char)*
                                                 (strlen(offset)+1));
     strcpy((*infoshow)[*ninfoshow].types,offset);
     offset+=sizeof(char)*( (strlen(offset)+1));

     //Unpack number of nodes in this cluster
     memcpy(&(*infoshow)[*ninfoshow].nnodes,offset,sizeof(int));
     offset+=sizeof(int);

     //Unpack number of node types
     memcpy(&(*infoshow)[*ninfoshow].ntypes,offset,sizeof(int));
     offset+=sizeof(int);

     (*infoshow)[*ninfoshow].infotypes=(STypennodes*)malloc(sizeof(STypennodes)*
                                    (*infoshow)[*ninfoshow].ntypes);

     //Unpack the number of nodes of each node type and the node type
     for(i=0;i<(*infoshow)[*ninfoshow].ntypes;i++)
     {
        memcpy(&(*infoshow)[*ninfoshow].infotypes[i].nnodes,offset,sizeof(int));
        offset+=sizeof(int);
        (*infoshow)[*ninfoshow].infotypes[i].type=(char*)malloc(sizeof(char)*
                                                            (strlen(offset)+1));
        strcpy((*infoshow)[*ninfoshow].infotypes[i].type,offset); 
        offset+=sizeof(char)*(strlen(offset)+1);
     }
 
     //Unpack the number of SInfo structures, usermaxtime and usermaxnnodes
     memcpy(&(*infoshow)[*ninfoshow].ninfo,offset,sizeof(int));
     offset+=sizeof(int);
     memcpy(&(*infoshow)[*ninfoshow].usermaxtime,offset,sizeof(int));
     offset+=sizeof(int);
     memcpy(&(*infoshow)[*ninfoshow].usermaxnnodes,offset,sizeof(int));
     offset+=sizeof(int);

     (*infoshow)[*ninfoshow].info=(SInfo*)malloc(sizeof(SInfo)*
                                  ((*infoshow)[*ninfoshow].ninfo));

     //Unpack the SInfo structures
     for(i=0;i<(*infoshow)[*ninfoshow].ninfo;i++)
     { 
        memcpy(&(*infoshow)[*ninfoshow].info[i],offset,S_PACK_SINFO);
        offset+=S_PACK_SINFO;
        (*infoshow)[*ninfoshow].info[i].nodetype=(char*)malloc(sizeof(char)*
                                                            (strlen(offset)+1));
        strcpy((*infoshow)[*ninfoshow].info[i].nodetype,offset);
        offset+=sizeof(char)*(strlen(offset)+1);
     }
 
     //Unpack the special periods
     (*infoshow)[*ninfoshow].special=(char*)malloc(sizeof(char)*
                                                   (strlen(offset)+1));
     strcpy((*infoshow)[*ninfoshow].special,offset);
     offset+=sizeof(char)*(strlen(offset)+1);
     (*ninfoshow)++;
  }
}
//##############################################################################
//## CRONO NODE MANAGER CLIENT (CRNMC)
//##############################################################################
int crnmc(SNmc nmc,char *am_server,int port,SUIPacket *uipacket)
{
  int sd;          //socket descriptor
  SPheader packet;
  void *buf,*offset;

  uipacket->data=NULL;
  uipacket->size=0;

  //create packet to send to Access Manager
  packet.type=nmc.oper;
  if(nmc.oper==CR_NMC_EXEC_OPERATION)
    packet.size=(sizeof(SPheader)+sizeof(int)*3+
                  sizeof(char)*(strlen(nmc.cluster)+strlen(nmc.operation)+
                                strlen(nmc.nodes)+3));
  else
    packet.size=(sizeof(SPheader)+sizeof(int)*3+
                  sizeof(char)*(strlen(nmc.cluster)+1));
  
  if( (buf=malloc(packet.size))==NULL)
     return CR_ERROR_ALLOC_MEMORY;

  memcpy(buf,&packet,sizeof(SPheader));
  memcpy(buf+sizeof(SPheader),&nmc,sizeof(int)*3); //uid,oper,all
  strcpy(buf+sizeof(SPheader)+sizeof(int)*3,nmc.cluster);

  if(nmc.oper==CR_NMC_EXEC_OPERATION)
  {  offset=buf+sizeof(SPheader)+sizeof(int)*3+
            sizeof(char)*(strlen(nmc.cluster)+1); 
     strcpy(offset,nmc.operation);
     offset+=sizeof(char)*(strlen(nmc.operation)+1);
     strcpy(offset,nmc.nodes);
  }

  //create connection to AM
  if( (sd=create_client_tcpsocket(am_server,port)) == CR_ERROR)
  {  free(buf);
     return CR_EAM_CANNOT_CONNECT_AM;
  }

  //send packet to AM
  dprintf("main: sending packet to AM. Size=%d\n",packet.size);
  if(send(sd, buf, packet.size, 0) < 0 )
  {  close(sd);
     free(buf);
     return CR_ERROR_TRANSFER_DATA;
  }
  free(buf);

  //receive packet header from AM 
  if( recv(sd,uipacket,sizeof(SPheader),0) < 0) 
  {  close(sd);
     return CR_ERROR_TRANSFER_DATA;
  }

  if(uipacket->type==CR_OK)
  {  //receive data
     if(uipacket->size-sizeof(SPheader)>0)
     {  uipacket->data=malloc(uipacket->size);
        if( recv(sd,uipacket->data,uipacket->size-sizeof(SPheader),0) < 0)
        {  close(sd);
           free(uipacket->data);
           return CR_ERROR_TRANSFER_DATA; 
        }
     }
  }
  close(sd);
  return uipacket->type;
}
//##############################################################################
//## CRONO SHOW NODES (CRNODES)
//##############################################################################
int crnodes(SNodes nodes,char *am_server,int port,SUIPacket *uipacket)
{
  int sd;          //socket descriptor
  SPheader packet;
  void *buf;

  uipacket->data=NULL;
  uipacket->size=0;

  //create packet to send to Access Manager
  packet.type=CR_NODES;
  packet.size=(sizeof(SPheader)+S_PACK_SNODES+
               sizeof(char)*(strlen(nodes.cluster)+1));

  if( (buf=malloc(packet.size))==NULL)
     return CR_ERROR_ALLOC_MEMORY;

  memcpy(buf,&packet,sizeof(SPheader));
  memcpy(buf+sizeof(SPheader),&nodes,S_PACK_SNODES);
  strcpy(buf+sizeof(SPheader)+S_PACK_SNODES,nodes.cluster);

  //create connection to AM
  if( (sd=create_client_tcpsocket(am_server,port)) == CR_ERROR)
  {  free(buf);
     return CR_EAM_CANNOT_CONNECT_AM;
  }

  //send packet to AM
  dprintf("main: sending packet to AM\n");
  if(send(sd, buf, packet.size, 0) < 0 )
  {  close(sd);
     free(buf);
     return CR_ERROR_TRANSFER_DATA;
  }
  free(buf);

  //receive packet header from AM 
  if( recv(sd,uipacket,sizeof(SPheader),0) < 0) 
  {  close(sd);
     return CR_ERROR_TRANSFER_DATA;
  }

  if(uipacket->type==CR_OK)
  {  //receive list of nodes from AM
     if(uipacket->size-sizeof(SPheader)>0)
     {  uipacket->data=malloc(uipacket->size);
        if( recv(sd,uipacket->data,uipacket->size-sizeof(SPheader),0) < 0)
        {  close(sd);
           free(uipacket->data);
           return CR_ERROR_TRANSFER_DATA; 
        }
     }
  }
  close(sd);
  return uipacket->type;
}
//##############################################################################
//## CRONO SHOW QUEUE (CRQVIEW)
//##############################################################################
int crqview(SQview qview,char *am_server,int port,SUIPacket *uipacket)
{
  int sd;          //socket descriptor
  SPheader packet;
  void *buf;

  uipacket->data=NULL;
  uipacket->size=0;

  //create packet to send to Access Manager
  packet.type=CR_QVIEW;
  packet.size=(sizeof(SPheader)+S_PACK_SQVIEW+
               sizeof(char)*(strlen(qview.cluster)+1));

  if( (buf=malloc(packet.size))==NULL)
     return CR_ERROR_ALLOC_MEMORY;

  memcpy(buf,&packet,sizeof(SPheader));
  memcpy(buf+sizeof(SPheader),&qview,S_PACK_SQVIEW);
  strcpy(buf+sizeof(SPheader)+S_PACK_SQVIEW,qview.cluster);

  //create connection to AM
  if( (sd=create_client_tcpsocket(am_server,port)) == CR_ERROR)
  {  free(buf);
     return CR_EAM_CANNOT_CONNECT_AM;
  }

  //send packet to AM
  dprintf("main: sending packet to AM\n");
  if(send(sd, buf, packet.size, 0) < 0 )
  {  close(sd);
     free(buf);
     return CR_ERROR_TRANSFER_DATA;
  }
  free(buf);
  //receive packet header from AM 
  if( recv(sd,uipacket,sizeof(SPheader),0) < 0)
  {  close(sd);
     return CR_ERROR_TRANSFER_DATA;
  }

  if(uipacket->type==CR_OK)
  {  //receive the queue from AM
     if(uipacket->size-sizeof(SPheader)>0)
     {  uipacket->data=malloc(uipacket->size);
        if( recv(sd,uipacket->data,uipacket->size-sizeof(SPheader),0) < 0)
        {  close(sd);
           free(uipacket->data);
           return CR_ERROR_TRANSFER_DATA;
        }
     }
 }

 close(sd);
 return uipacket->type;
}
//##############################################################################
//## Release memory allocated to store 'qviewshow' data
//##############################################################################
void crqview_free_qviewshow(Sqviewshow *qviewshow)
{
  int i;

  for(i=0;i<qviewshow->qview_rminfo.nreqs;i++)
     free(qviewshow->reqs[i].additinfo);
  free(qviewshow->reqs);
  
  free(qviewshow->gaps);
  
  for(i=0;i<qviewshow->ntypes;i++)
  {
     free(qviewshow->nodetypes[i]);
  }

  free(qviewshow->nodetypes);
  free(qviewshow);
}
//##############################################################################
//## Get the number of user requests in RM's queue
//##############################################################################
int crqview_getnreqs(SQview qview,char *am_server,int port, int *nreqs)
{

  SUIPacket uipacket;
  Sqviewshow *qviewshow;
  int answer;

  *nreqs=0;
  answer=crqview(qview,am_server,port,&uipacket);
  if(uipacket.data!=NULL)
  {
    crqview_set_qviewshow(uipacket.data,uipacket.size,qview.type,&qviewshow);
    *nreqs=qviewshow->qview_rminfo.nreqs;
    crqview_free_qviewshow(qviewshow);
  }

  return answer;
}
//##############################################################################
//## Get available gaps that can be allocated now
//##############################################################################
int crqview_getgaps(SQview qview,char *am_server,int port, int *ngaps,
                    SGapsnow **gaps)
{

  SUIPacket uipacket;
  Sqviewshow *qviewshow;
  int answer;

  *ngaps=0;
  answer=crqview(qview,am_server,port,&uipacket);

  if(uipacket.data!=NULL)
  {
    crqview_set_qviewshow(uipacket.data,uipacket.size,qview.type,&qviewshow);
    *ngaps=qviewshow->ngaps;
    *gaps=(SGapsnow*)malloc(sizeof(SGapsnow)*(*ngaps));
    memcpy(*gaps,qviewshow->gaps,sizeof(SGapsnow)*(*ngaps));
    crqview_free_qviewshow(qviewshow);
  }

  return answer;
}
//##############################################################################
//## Set qviewshow structure
//##############################################################################
void crqview_set_qviewshow(void *data, int size, char type,
                           Sqviewshow **qviewshow)
{
  void *offset;
  int i;
  
  *qviewshow=(Sqviewshow*)malloc(sizeof(Sqviewshow));
       
  size-=sizeof(SPheader); //take off the packet header
  offset=data; 
     

  size-=sizeof(SPheader); //take off the packet header
  //copy the Request Manager information
  memcpy(&(*qviewshow)->qview_rminfo,offset,sizeof(SQview_rminfo));
  offset+=sizeof(SQview_rminfo);


  if(type) //get free nodes of each node type
  {  memcpy(&(*qviewshow)->ntypes,offset,sizeof(int));
     (*qviewshow)->freenodes=(SFreenodes*)malloc(sizeof(SFreenodes)*
                                          (*qviewshow)->ntypes);
     offset+=sizeof(int);
     memcpy((*qviewshow)->freenodes,offset,sizeof(SFreenodes)* 
                                           (*qviewshow)->ntypes);
     offset+=sizeof(SFreenodes)*(*qviewshow)->ntypes;
     (*qviewshow)->nodetypes=(char**)malloc(sizeof(char*)*
                                            ((*qviewshow)->ntypes));

     //get the nodetype names 
     for(i=0;i<(*qviewshow)->ntypes;i++) //get node types
     {  (*qviewshow)->nodetypes[i]=(char*)malloc(sizeof(char)*
                                                 (strlen(offset)+1));
        strcpy((*qviewshow)->nodetypes[i],offset);
        offset+=sizeof(char)*(strlen(offset)+1);
     }
  }
  else
  {
     (*qviewshow)->ntypes=0; 
     (*qviewshow)->nodetypes=NULL;
  }

  (*qviewshow)->reqs=(SReq*)malloc(sizeof(SReq)*
                                   (*qviewshow)->qview_rminfo.nreqs);
  //get the user requests in the queue
  for(i=0;i<(*qviewshow)->qview_rminfo.nreqs;i++)
  { 
     memcpy(&(*qviewshow)->reqs[i],offset,S_PACK_SREQ);
     offset+=S_PACK_SREQ;
     (*qviewshow)->reqs[i].additinfo=(char*)malloc(sizeof(char)*
                                                  (strlen(offset)+1));
     strcpy((*qviewshow)->reqs[i].additinfo,offset);
     offset+=sizeof(char)*(strlen(offset)+1);
  }

  //get the gaps (possible allocations starting now)
  memcpy(&(*qviewshow)->ngaps,offset,sizeof(int));
  offset+=sizeof(int);
  (*qviewshow)->gaps=(SGapsnow*)malloc(sizeof(SGapsnow)*(*qviewshow)->ngaps);
  memcpy((*qviewshow)->gaps,offset,sizeof(SGapsnow)*(*qviewshow)->ngaps);

}
//##############################################################################
//## CRONO RELEASE NODES (CRRLS)
//##############################################################################
int crrls(SRls rls,char *am_server,int port)
{
  int sd;          //socket descriptor
  SPheader packet;
  void *buf;
 
  dprintf("PORT: %d SERVER: %s\n",port,am_server);
  dprintf("Cluster=%s\n",rls.cluster);
  packet.type=CR_RLS;
  packet.size=(sizeof(SPheader)+sizeof(SRls)+
               sizeof(char)*(strlen(rls.cluster)+1));
  dprintf("main: packet size to send=%d\n",packet.size);
  dprintf("main: creating packet\n");

  //create packet
  if( (buf=malloc(packet.size))==NULL)
     return CR_ERROR_ALLOC_MEMORY;

  memcpy(buf,&packet,sizeof(SPheader));
  memcpy(buf+sizeof(SPheader),&rls,sizeof(SRls));
  strcpy(buf+sizeof(SPheader)+sizeof(SRls),rls.cluster);

  //create connectio to AM
  if( (sd=create_client_tcpsocket(am_server,port)) == CR_ERROR)
  {  free(buf);
     return CR_EAM_CANNOT_CONNECT_AM;
  }

  //send packet
  dprintf("main: sending packet to AM\n");
  if(send(sd, buf, packet.size, 0) < 0 )
  {  close(sd);
     free(buf);
     return CR_ERROR_TRANSFER_DATA;
  }
  free(buf);

  //receive answer
  if( recv(sd,&packet,sizeof(SPheader),0) <0 )
  {  close(sd);
     return CR_ERROR_TRANSFER_DATA;
  }
  close(sd);
  return packet.type;
}
//##############################################################################
//## Return a message for this 'answer' of this 'cluster'
//## ui_msg -> the message will be stored in ui_msg
//##############################################################################
char ui_answer2msg(int answer,char *cluster,char *ui_msg)
{
  switch(answer)
  {  case CR_REQ_QUEUED:
        strncpy(ui_msg,UI_MSG_REQ_QUEUED,MAX_UI_MSG);
        return TRUE;
     case CR_EAM_NOT_ACCESS: 
        strncpy(ui_msg,UI_MSG_EAM_NOT_ACCESS,MAX_UI_MSG);
        return TRUE;
     case CR_EAM_INVALID_CLUSTER:
        snprintf(ui_msg,MAX_UI_MSG,UI_MSG_EAM_INVALID_CLUSTER,cluster);
        return TRUE; 
     case CR_EAM_INVALID_TIME:
        strncpy(ui_msg,UI_MSG_EAM_INVALID_TIME,MAX_UI_MSG);
        return TRUE;
     case CR_EAM_INVALID_NNODES:
        strncpy(ui_msg,UI_MSG_EAM_INVALID_NNODES,MAX_UI_MSG);
        return TRUE;
     case CR_EAM_INVALID_NODETYPE:
        strncpy(ui_msg,UI_MSG_EAM_INVALID_NODETYPE,MAX_UI_MSG);
        return TRUE;
     case CR_ERM_USER_ALREADY_ALLOC:
        strncpy(ui_msg,UI_MSG_ERM_USER_ALREADY_ALLOC,MAX_UI_MSG);
        return TRUE;
     case CR_RESOURCES_RELEASED:
        strncpy(ui_msg,UI_MSG_RESOURCES_RELEASED,MAX_UI_MSG);
        return TRUE;
     case CR_REQUEST_CANCELLED:
        strncpy(ui_msg,UI_MSG_REQUEST_CANCELLED,MAX_UI_MSG);
        return TRUE;
     case CR_ERM_USER_NOT_QUEUED:
        strncpy(ui_msg,UI_MSG_ERM_USER_NOT_QUEUED,MAX_UI_MSG);
        return TRUE;
     case CR_ERM_NO_OPERATIONS_EXECUTED:
        strncpy(ui_msg,UI_MSG_ERM_NO_OPERATIONS_EXECUTED,MAX_UI_MSG);
        return TRUE;
     case CR_RESERVE_NOT_DELETED:
        strncpy(ui_msg,UI_MSG_RESERVE_NOT_DELETED,MAX_UI_MSG);
        return TRUE;
     case CR_ERM_RESERVE_COLLISION:
        strncpy(ui_msg,UI_MSG_ERM_RESERVE_COLLISION,MAX_UI_MSG);
        return TRUE;
     case CR_ERM_RESERVE_UID_EXISTS_THIS_PERIOD:
        strncpy(ui_msg,UI_MSG_ERM_RESERVE_UID_EXISTS_THIS_PERIOD,MAX_UI_MSG);
        return TRUE;
     case CR_ERM_NRESERVES_EXCEEDED:
         strncpy(ui_msg,UI_MSG_ERM_NRESERVES_EXCEEDED,MAX_UI_MSG);
        return TRUE;
     case CR_RESERVE_TSTART_LESSER_NOW:
        strncpy(ui_msg,UI_MSG_RESERVE_TSTART_LESSER_NOW,MAX_UI_MSG);
        return TRUE;
     case CR_EAM_CANNOT_CONNECT_AM:
        snprintf(ui_msg,MAX_UI_MSG,UI_MSG_CANNOT_CONNECT_AM,cluster);
        return TRUE;
     case CR_EAM_CANNOT_CONNECT_RM:
        snprintf(ui_msg,MAX_UI_MSG,UI_MSG_CANNOT_CONNECT_RM,cluster);
        return TRUE;
     case CR_ERROR_TRANSFER_DATA:
        strncpy(ui_msg,UI_MSG_ERROR_TRANSFER_DATA,MAX_UI_MSG);
        return TRUE;
     case CR_ERROR_ALLOC_MEMORY:
        strncpy(ui_msg,UI_MSG_ERROR_ALLOC_MEMORY,MAX_UI_MSG);
        return TRUE;
  }
  return FALSE;
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
