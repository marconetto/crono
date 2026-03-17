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

#include <math.h>

//##############################################################################
//##PROTOTYPES

char rm_check_if_try_this_node(int nodeid, int *typeid,
                               SReqnodetype *tmp_nodetypes,int reqntypes,
                               int *possible_nodes,int npossible_nodes);
void rm_schedule(SReq *req);
void rm_schedule_add_event_time(time_t timet);
void rm_schedule_add_req_to_queue(SReq **first,SReq **last, SReq *req);
int  *rm_schedule_get_nodes(SReq *req,time_t basetime,
                            int **available_nodes);
char rm_schedule_is_possible_all_nodesuids(SReq *req,int n, time_t basetime,
                                           char shared);

char rm_schedule_is_possible_one_nodesuids(SReq *req,int n, char shared,
                                           time_t basetime);
int  rm_schedule_is_uid_in_queue(SReq **first,int uid);
void rm_schedule_make_etimes();
void rm_schedule_recover_queue();
void rm_schedule_reschedule_all_requests();
void rm_schedule_ret_req_from_queue(SReq **first,SReq **last,int rid);
char rm_schedule_request_has_node(SReq *req,int n);
void rm_schedule_show_queue(SReq *first);
void rm_schedule_store_queue();
char user_has_allocated(int uid);
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
