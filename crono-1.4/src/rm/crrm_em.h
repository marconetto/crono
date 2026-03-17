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
#include <utmp.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <grp.h>
#include <dirent.h> 
#include <ctype.h>
#include <errno.h>
 
#define CR_USERMSG_TIME_START "\n\aYour time has started"
#define CR_USERMSG_TIME_FINISH "\n\aYour time has finished"
#define CR_USERMSG_CONFLICT_WITH_A_RESERVE "\n\aThere is a conflict with a" \
                     " reserve. You can cancel it to solve this problem."
#define CREM_MPREPS   1
#define CREM_MPOSTPS  2
#define CREM_UPREPS   3
#define CREM_UPOSTPS  4
#define CREM_BATCHJOB 5
#define PATH_SHELL    "/bin/sh"
#define SLEEP_WAIT_SESSION_FINISHES 2

void crem_exec_batchjob(int uid,int timeout,int rid,char *bjscript,char *path,
                        char *usernodesc,char *usernodesinfo);
time_t crem_get_idle_tty (char *tty);
void crem_kill_session(pid_t sid);
void crem_kill_timeout();
long crem_ms(int uid,int rid,SCrm cluster,char *dirconf,char *usernodes,
             char *usernodesinfo, int type, int usertime);
void crem_send_user_msg(int uid, char *msg);
void crem_set_environ_var(char ***envp,char *cluster,char *usernodes,
                          char *usernodesinfo,int uid, int rid, int type, 
                          int usertime);
void crem_us(int uid,int timeout,int rid,SCrm cluster,char *usernodes,
             char *usernodesinfo,int type);
void crem_wait_session_finishes(pid_t sid,long timeout);
void set_sessionid_job(int rid, pid_t sid);

//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
