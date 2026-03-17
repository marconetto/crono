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
#ifndef _CRAM_PARSERS_H
#define _CRAM_PARSERS_H

void add_quant_alloc_nodetype(STypennodes **typennodes,int *ntypes,
                              char *nodetype, int tmp_nnodes,int *totalnodes);
void am_add_special_info(char *weekday,char *itime,char *ftime,char **buf);
int  check_arguments(char *user,int oper,char *file, char *logfile,
                    SAlloc *alloc,time_t timet,void **arinfo,int *nbytes,
                    char **specialbuf);
char check_is_special(char *weekday, char *weekdayf,char *itime, char *ftime,
                      time_t timet);
char check_weekday_name(char *weekday);
void free_nodetypes(STypennodes **typennodes, int ntypes);
void get_weekday(char *weekday,time_t timet);
char get_group(char *user,char *file,char *group);
void get_nodes_info(char *file,char *logfile,void **nodesinfo, int *nbytes);
void handle_defs(char *defs, char defaults,char oper,int line, SInfo *info);
void parser_accessrights_defs(char *file,int oper,char *user, time_t itime,
                              char **specialbuf, int *is_special, SInfo **info,
                              int *ninfo);
void setup_alloc_nodetypes(STypennodes **typennodes,int *ntypes,
                           void *allocnodes);
int  typennodes2alloc_nodes(STypennodes *typennodes,int ntypes,SAlloc *alloc);
char user_in_list(char *buf_users, char *user);
#endif
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
