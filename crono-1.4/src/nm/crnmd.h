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
#include <fcntl.h>
typedef struct
{
    char logfile[S_PATH],           //log file
         rmhost[MAXHOSTNAMELEN],    //Request Manager hostname
         cluster[MAXHOSTNAMELEN],   //Cluster name
         piddir[MAXHOSTNAMELEN];   //Cluster name
    int rmport,                     //Request Manager port number
        nmport,                     //Node Manager port number
        pam;                        //PAM support
}SCnm;
void add_user_allowed(int uid);
void crnmd_getargs(int argc,char **argv);
void *crnmd_handle_client_thread(void *arg);
void crnmd_usage();
void del_user_allowed(int uid);
void flush_loginaccess();
void flush_hostsequiv();
void generate_command(char *command,char *realcommand,SNmc nmc,char *output);
void kill_user_process(int uid);
void nm_exec_request(int uid,int type);
void nm_reset_lock_access(int csd,int packetsize);
void nm_lock_unlock_user(int uid,int type);
void nm_exec_operation(int csd,int packetsize);
void request_rm_allowed_user();
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
