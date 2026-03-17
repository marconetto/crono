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
#ifndef _CRAMD_H
#define _CRAMD_H

#define ACCESS_RIGHTS_DEFS   "accessrights.defs"
#define AM_MSG_NO_OPERATIONS_AVAILABLE "No operations available"

typedef struct
{
    char logfile[S_PATH],
         piddir[S_PATH],
         rmhost[MAXHOSTNAMELEN],
         cluster[MAXHOSTNAMELEN];
    int amport,
        rmport;
}SCram;

extern SCram *clusters;
extern int nclusters;
extern sem_t semreserve;
extern char dirconf[S_PATH];
extern int verbose;

void am_alloc_client(int csd,int packetsize);
void am_check_files_syntax();
void am_crinfo(int csd, int packetsize);
void am_nmc_client(int csd,int packetsize);
int  am_nmc_execute_operation(SNmc nmc,void **bufsend,SPheader *packet,
                              void *buf, int packetsize);

int  am_nmc_get_operations(SNmc nmc, void **buf,SPheader *packet);
void am_nodes_client(int csd,int oper);
void am_qview_client(int csd,int packetsize);
void am_rls_client(int csd,int packetsize);
int  check_allocation(SAlloc *alloc, int *nbytes);
int  check_operation(char *operation, char *cluster);
int  connect_to_rm(char *cluster);
void cramd_getargs(int argc,char **argv);
void cramd_usage();
void cramd_write_pid(char *daemon,char *cluster);
int  get_rmport(char *cluster);
char *get_rmserver(char *cluster);
void *handle_client_thread(void *arg);
int  is_a_cluster(char *cluster);
void simple_protocol_rm_ui(int type,int packetsize,int csd);
#endif
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
