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
//#############################################################################
#ifndef _CRUI_API_H
#define _CRUI_API_H
 
#include "common.h"
#define MAX_UI_MSG 200

#define UI_MSG_REQ_QUEUED "Request has been queued."
#define UI_MSG_EAM_NOT_ACCESS "You don't have access to use this cluster"
#define UI_MSG_EAM_INVALID_CLUSTER "%s: Invalid cluster name."
#define UI_MSG_EAM_INVALID_TIME "Invalid time. Use crinfo to check."
#define UI_MSG_EAM_INVALID_NNODES "Invalid number of nodes. " \
                                   "Use crinfo to check."
#define UI_MSG_EAM_INVALID_NODETYPE "Check the types and the number of nodes"\
                                    " of each type\n"\
                                    "in your qualitative allocation.\n"\
                                    "Use crinfo to check your access rights."
#define UI_MSG_ERM_USER_ALREADY_ALLOC "You've already allocated some nodes. "\
                                "Use crqview to check."
#define UI_MSG_RESOURCES_RELEASED "Resources have been released."
#define UI_MSG_REQUEST_CANCELLED "Request has been cancelled."
#define UI_MSG_ERM_USER_NOT_QUEUED "You are not in the queue. If you have a "\
                                    "reserve, use the -rid option."
#define UI_MSG_ERM_NO_OPERATIONS_EXECUTED "No operations were executed"
#define UI_MSG_RESERVE_NOT_DELETED "Reserve NOT deleted."
#define UI_MSG_ERM_RESERVE_COLLISION "Reserve NOT created. Collision time was "\
                                      "detected."
#define UI_MSG_ERM_RESERVE_UID_EXISTS_THIS_PERIOD "User already has a request "\
                                                   "in this period."
#define UI_MSG_ERM_NRESERVES_EXCEEDED "Number of reserves exceeded. "\
                                      "Use crinfo to check."
#define UI_MSG_RESERVE_TSTART_LESSER_NOW "Reserve NOT created. The starting "\
                                       "time is not correct."

#define UI_MSG_CANNOT_CONNECT_AM "Access Manager of %s cluster is down.\n" \
                                 "Please advise the administrator.\n"

#define UI_MSG_CANNOT_CONNECT_RM "Request Manager of %s cluster is down.\n" \
                                 "Please advise the administrator.\n"
                                                                  
#define UI_MSG_ERROR_TRANSFER_DATA "Transfer data error."
#define UI_MSG_ERROR_ALLOC_MEMORY "Error alloc memory."

typedef struct
{
   char *cluster,  //cluster name
        *group,    //user's group
        *types,    //node types' description
        *special;  //special periods

   int nnodes,        //number of nodes in the cluster
       ntypes,        //number of nodetypes in the cluster
       usermaxtime,   //maximum amount of time the user can allocate 'now'
       usermaxnnodes, //maximum number of nodes the user can allocate 'now'
       ninfo;         //number of 'info' structures received from AM

   STypennodes *infotypes; //number of nodes of each nodetype
   SInfo       *info;      //Information about the user access rigths

}Sinfoshow; //structure used in crinfo* functions


typedef struct
{
   SQview_rminfo qview_rminfo; //Some information about the Request Manager
   int ntypes;                 //number of nodetypes 
   SFreenodes *freenodes;      //number of free nodes of each nodetype
   char **nodetypes;           //nodetypes names
   SReq *reqs;                 //RM's queue requests
   int ngaps;                  //number of gaps that can be allocated 'now'
   SGapsnow *gaps;             //gaps that can be allocated 'now'

}Sqviewshow; //structure used in crqview* functions


typedef struct
{
   int type,     //packet type
       size;     //packet size
   void *data;   //packet data
}SUIPacket;      // Packet received from Access Manager


extern int  cralloc(SAlloc alloc,char *am_server,int port);
extern int  crinfo(SInforeq inforeq,char *am_server,int port,
                   SUIPacket *uipacket);
extern void crinfo_free_infoshow(Sinfoshow *infoshow, int ninfoshow);
extern void crinfo_set_infoshow(void *data, int size, Sinfoshow **infoshow,
                                int *ninfoshow);
extern int  crinfo_getmaxnnodes(SInforeq inforeq, char *am_server,int port, 
                                int *maxnnodes);
extern int  crinfo_getmaxtime(SInforeq inforeq, char *am_server,int port,
                              int *maxtime);
extern int  crnmc(SNmc nmc,char *am_server,int port,SUIPacket *uipacket);
extern int  crnodes(SNodes nodes,char *am_server,int port,SUIPacket *uipacket);
extern int  crqview(SQview qview,char *am_server,int port,SUIPacket *uipacket);
extern int  crqview_getgaps(SQview qview,char *am_server,int port, int *ngaps,
                            SGapsnow **gaps);
extern int  crqview_getnreqs(SQview qview,char *am_server,int port, int *nreqs);
extern void crqview_free_qviewshow(Sqviewshow *qviewshow);
extern void crqview_set_qviewshow(void *data, int size, char type,
                                  Sqviewshow **qviewshow);
extern int  crrls(SRls rls,char *am_server,int port);
extern char ui_answer2msg(int answer,char *cluster,char *ui_msg);
#endif
