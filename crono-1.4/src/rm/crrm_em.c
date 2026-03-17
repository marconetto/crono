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
#include "crrm_em.h"
//##############################################################################
//## Execution of a batch job 
//##############################################################################
void crem_exec_batchjob(int uid,int timeout,int rid,char *bjscript, char *path,
                        char *usernodes,char *usernodesinfo)
{
  char login[_POSIX_LOGIN_NAME_MAX],script[S_PATH];
  char **envp; //used by child process
  pid_t cpid;
  int s,gid;

  snprintf(script,sizeof(script),"%s/%s",path,bjscript);

  crvprintf(">>> EXECUTION of Batch job: uid=%d batch=%s\n",uid,script);
  if(file_exists(script))
  {  cpid=fork();
     if(!cpid)
     {  setsid(); //create a new session
        crvprintf("# crem_exec_batchjob: session created\n");
        close(rm_sd); //close de RM socket descriptor for this process
        //become the user (user->uid) with right gid and home directory
        clearenv();
        initgroups(uid2login(uid,login),uid2gid(uid,&gid));
        crvprintf("# crem_exec_batchjob: initgroups ok gid=%d\n",gid);
        if(setgid(gid) == -1)
        {  crvprintf("Cannot set gid\n");
           return;
        }
        if(setuid(uid)==-1)
        {  crvprintf("Cannot set uid\n");
           return;  
        }

        crem_set_environ_var(&envp,cluster.cluster,usernodes,usernodesinfo,uid,
                             rid, CREM_BATCHJOB,timeout/60);
        chdir(path);
        crvprintf("# crem_exec_batchjob: Execution of Batch Job: STARTED\n");
        crvprintf("# crem_exec_batchjob: script=%s\n",script);
        execve(script,NULL,envp);
        if (errno == ENOEXEC || errno == EACCES)
        {  crvprintf("# crem_exec_batchjob: failed. Try to execute with sh\n");
           snprintf(script,sizeof(script),"%s %s/%s",PATH_SHELL,path,bjscript);
           execle(PATH_SHELL,"sh", "-c", script,NULL,envp);
        }
        _exit(0);
    }
    else
    {  set_sessionid_job(rid,cpid);
       crvprintf("Waiting child\n");
       alarm(timeout);
       signal(SIGALRM,crem_kill_timeout);
       siginterrupt(SIGALRM,1);
       s=waitpid(cpid,NULL,WUNTRACED);
       alarm(0);
       signal(SIGALRM,SIG_DFL);
       siginterrupt(SIGALRM,0);
       if(s!=cpid)
       {  crem_kill_session(cpid);  
          waitpid(cpid,NULL,WUNTRACED);
       }
    }
  }
  else
     crvprintf("# crem_exec_batchjob: %s: Script not found\n",script);
}
//#############################################################################
//## Get idle time of a terminal
//#############################################################################
time_t crem_get_idle_tty (char *tty)
{
  struct stat st;
  char buf[32];

  snprintf(buf,sizeof(buf), "/dev/%s",tty);
  if (stat(buf,&st) == -1) return -1;
  return (time(0) - st.st_atime);
}
//#############################################################################
//## Kill a session (sid=session id)
//#############################################################################
void crem_kill_session(pid_t sid)
{
  DIR *dirp;
  struct dirent *direntp;
  FILE *fp;
  int cur_pid, //current pid
      cur_sid, //current sid
      found;
  char file[S_PATH];

  dprintf("# crem_kill_session: SID=%d STARTED\n",sid);
  kill(sid,SIGKILL);
  //kill processes of this session
  do 
  {  found=FALSE;
     dirp = opendir("/proc");
     if(dirp==NULL)
     {  dprintf("Cannot open /proc directory\n");
        return;
     }
     while ( (direntp = readdir( dirp )) != NULL )
     {  if(isdigit(direntp->d_name[0]))
        {  cur_pid=atoi(direntp->d_name);
           snprintf(file,S_PATH,"/proc/%s/stat",direntp->d_name);
           //dprintf("# crem_kill_session: Opening file %s\n",file);
           if( (fp=fopen(file,"r") )!=NULL)
           {  fscanf(fp,"%*d %*s %*c %*d %*d %d",&cur_sid);
              fclose(fp);
                   
              if(cur_sid==sid && cur_pid!=sid)
              {  dprintf("# crem_kill_session: FOUND(PROC=%d)\n",cur_pid);
                 kill(cur_pid,SIGKILL);
                 found=TRUE;
              }
           }
        }
     }
     closedir( dirp );
  }while(found);
  dprintf("# crem_kill_session: FINISHED\n");
}
//#############################################################################
//## Function executed when a timeout is achieved 
//#############################################################################
void crem_kill_timeout()
{
  dprintf("# crem_kill_timeout: TIMEOUT!!!\n");
}
//##############################################################################
//## Execution of Master Pre and Post Processing Script
//##############################################################################
long crem_ms(int uid, int rid, SCrm cluster,char *dirconf,char *usernodes,
             char *usernodesinfo, int type, int usertime)
{

  char script[S_PATH],*tmp;
  pid_t cpid;
  int s;
  struct timeval timeval_start,timeval_finish;
  char **envp; //used by child process
    
  if(type==CREM_MPREPS)
  {  crvprintf(">>> EXECUTION of Master Pre-Processing Script\n");
     snprintf(script,sizeof(script),"%s/%s/mpreps",dirconf,cluster.cluster);
     gettimeofday(&timeval_start,NULL);
  }
  else
  {  crvprintf(">>> EXECUTION of Master Post-Processing Script\n");
     snprintf(script,sizeof(script),"%s/%s/mpostps",dirconf,cluster.cluster);
  }

  if(file_exists(script))
  {  cpid=fork();
     if(!cpid)
     {   setsid(); //create a new session
        close(rm_sd); //close de RM socket descriptor for this process

        crem_set_environ_var(&envp,cluster.cluster,usernodes,usernodesinfo,uid,
                             rid,type,usertime);

        crvprintf("# crem_ms: Execution of MS: STARTED\n");
        crvprintf("# crem_ms: script=%s\n",script);
        execve(script,NULL,envp);
        
        if (errno == ENOEXEC || errno == EACCES)
        {  crvprintf("# crem_ms: failed. Try to execute with sh\n");
           tmp=(char*)malloc(sizeof(char)*(strlen(script)+
                                           strlen(PATH_SHELL)+2));
           sprintf(tmp,"%s %s",PATH_SHELL,script);
           crvprintf("# crem_ms: %s\n",tmp);
           execle(PATH_SHELL,"sh", "-c", tmp,NULL,envp);
        }
        _exit(0);
     }
     else
     {  dprintf("# crem_ms: waiting end of MS execution\n");   
        if(type==CREM_MPREPS)
           alarm(cluster.tmpreps);
        else
           alarm(cluster.tmpostps);
        signal(SIGALRM,crem_kill_timeout);
        siginterrupt(SIGALRM,1);
        s=waitpid(cpid,NULL,WUNTRACED);
        alarm(0);
        siginterrupt(SIGALRM,0);
        if(s!=cpid)
        {  crem_kill_session(cpid);
           dprintf("WAIT PID again..\n");
           waitpid(cpid,NULL,WUNTRACED);
           dprintf("WAIT PID again:DONE\n");
        }
        dprintf("# crem_ms: end of MS execution\n");
        if(type==CREM_MPREPS)
        {  gettimeofday(&timeval_finish,NULL);
           dprintf("# crem_ms: Time spent : %lds\n",
                   (timeval_finish.tv_sec-timeval_start.tv_sec));

           return (timeval_finish.tv_sec-timeval_start.tv_sec);
        }
     }        
  }
  else
     crvprintf("# crem_ms: %s: Script not found\n",script);

  return 0;
}
//#############################################################################
//## Send a message to user (using terminals)
//## Send this message to terminal with lesser idle time
//#############################################################################
void crem_send_user_msg(int uid, char *msg)
{
  int fd, i,cpid;
  static struct utmp entry;
  time_t timet=-1;
  char term[UT_LINESIZE], user[_POSIX_LOGIN_NAME_MAX],
       command[CR_MAX_COMMAND_SIZE];

  uid2login(uid,user);

  if ((fd = open(UTMP_FILE ,O_RDONLY)) == -1)
  {   crvprintf("Cannot open file %s\n",UTMP_FILE);
      crono_log(cluster.logfile,CR_LOG, "crrmd: Cannot open file %s", 
                UTMP_FILE);
      return;
  }
  while((i = read(fd, &entry,sizeof entry)) > 0) 
  {  if(i != sizeof (entry) ) 
         exit(1);
     if(entry.ut_type != USER_PROCESS) continue;

     if( !strcmp(entry.ut_name,user) )
     {   if(timet==-1)
         {   timet=crem_get_idle_tty(entry.ut_line);
             strcpy(term,entry.ut_line);
          }
         else if(timet>crem_get_idle_tty(entry.ut_line))
         {    timet=crem_get_idle_tty(entry.ut_line);
              strcpy(term,entry.ut_line);
         }
     }
  }
  close(fd);
  if(timet==-1)  //do not send message
     dprintf("# crem_send_user_msg: User %s is not logged\n",user);
  else
  {  cpid=fork();
     if(cpid==0)
     {  close(rm_sd); //close de RM socket descriptor for this process
        dprintf("# crem_send_user_msg: Sending a message to user %s using "
                "the /dev/%s terminal\n",user,term);
        snprintf(command,sizeof(command),"echo -e  \"%s\" > /dev/%s\n",msg,
                 term);
        dprintf(" MSG: '%s'\n",msg);
        execl(PATH_SHELL,"sh","-c",command,NULL);
        _exit(0);
    }
    else
       waitpid(cpid,NULL,WUNTRACED);
  }
}
//#############################################################################
//## Set the session id to a job
//#############################################################################
void set_sessionid_job(int rid, pid_t sid)
{
  SReq *req;

  sem_wait(&semqueue);
  req=firstqueue;
  while(req!=NULL)
  {  if(req->rid==rid)
     {  crvprintf("# set_sessionid_job: job found\n");
        req->sid=sid;
        break;
     }
     req=req->prox;
  }
  rm_schedule_store_queue();  //sid was changed
  sem_post(&semqueue);
}
//##############################################################################
//## Set environment variables
//##############################################################################
void crem_set_environ_var(char ***envp,char *cluster,char *usernodes,
                          char *usernodesinfo,int uid, int rid, int type,
                          int usertime)
{
  int i,execuid;
  char login[_POSIX_LOGIN_NAME_MAX],buf[S_PATH],nodeslist=FALSE;
  struct passwd *pas;

  if(type==CREM_MPREPS || type==CREM_MPOSTPS)
     execuid=getuid();
  else
     execuid=uid;

  if ((pas = getpwuid(execuid)) == 0)
  {  crvprintf("failed to get passwd struct from user %d\n", execuid);
     exit(1);
  }

  *envp=(char**)malloc(sizeof(char*)*(12));
  for(i=0;i<11;i++)
  {  nodeslist=FALSE;
     if(i==0) 
        snprintf(buf,sizeof(buf),"CR_USERID=%d",uid);
     else if(i==1)
        snprintf(buf,sizeof(buf),"CR_CLUSTER=%s",cluster);
     else if(i==2)
        snprintf(buf,sizeof(buf),"CR_USERNAME=%s",uid2login(uid,login));
     else if(i==3)
        snprintf(buf,sizeof(buf),"CR_USERHOME=%s",uid2home(uid,login));
     else if(i==4)
        snprintf(buf,sizeof(buf),"CR_RID=%d",rid);
     else if(i==5)
        snprintf(buf,sizeof(buf),"CR_USERTIME=%d",usertime);
     else if(i==6)
     { 
        if( usernodes==NULL)
          strcpy(buf,"CR_NODES=");
        else    
        {  (*envp)[i]=(char*)malloc(sizeof(char)*(strlen("CR_NODES=")+
                                                  strlen(usernodes)+1));
            strcpy((*envp)[i],"CR_NODES=");
            strcat((*envp)[i],usernodes);
            nodeslist=TRUE;
        }
     }
     else if(i==7)
     { 
        if( usernodesinfo==NULL)
          strcpy(buf,"CR_NODESINFO=");
        else    
        {  
            (*envp)[i]=(char*)malloc(sizeof(char)*(strlen("CR_NODESINFO=")+
                                                  strlen(usernodesinfo)+1));
            strcpy((*envp)[i],"CR_NODESINFO=");
            strcat((*envp)[i],usernodesinfo);
            nodeslist=TRUE;
        }
     }
     else if(i==8)
        snprintf(buf,sizeof(buf),"USER=%s",pas->pw_name);
     else if(i==9)
        snprintf(buf,sizeof(buf),"HOME=%s",pas->pw_dir);
     else if(i==10)
        snprintf(buf,sizeof(buf),"SHELL=%s",pas->pw_shell);

     if(nodeslist==FALSE)
     {  (*envp)[i]=(char*)malloc(sizeof(char)*(strlen(buf)+1));
        strcpy((*envp)[i],buf);
     }
  }
  (*envp)[11]=NULL;
}
//##############################################################################
//## Execution of User Pre and Post Processing Script
//##############################################################################
void crem_us(int uid, int timeout, int rid, SCrm cluster, char *usernodes,
             char *usernodesinfo, int type)
{

  char script[S_PATH],login[_POSIX_LOGIN_NAME_MAX],home[S_PATH],*tmp;
  pid_t cpid;
  int s,gid=0;
  char **envp; //used by child process
  uid2home(uid,home);

  if(type==CREM_UPREPS)
  {  crvprintf(">>> EXECUTION of the User Pre-Processing Script\n");
     snprintf(script,sizeof(script),"%s/.crono/%s.upreps",home,
              cluster.cluster);
  }
  else
  {  crvprintf(">>> EXECUTION of the User Post-Processing Script\n");
     snprintf(script,sizeof(script),"%s/.crono/%s.upostps",home,
              cluster.cluster);
  }

  if(file_exists(script))
  {  cpid=fork();
     if(!cpid)
     {  setsid(); //create a new session
        close(rm_sd); //close de RM socket descriptor for this process
        //become the user (user->uid) with right gid and home directory
        clearenv();
        initgroups(uid2login(uid,login),uid2gid(uid,&gid));
        if(setgid(gid) == -1)
        {  crvprintf("Cannot set gid\n");
           exit(1);
        }
        if(setuid(uid)==-1)
        {  crvprintf("Cannot set uid\n");
           exit(1);
        }

        crem_set_environ_var(&envp,cluster.cluster,usernodes,usernodesinfo,uid,
                             rid,type,timeout/60);
        chdir(home);

        crvprintf("# crem_us: Execution of US: STARTED\n");
        crvprintf("# crem_us: script=%s\n",script);
        execve(script,NULL,envp);
        if (errno == ENOEXEC || errno == EACCES)
        {  crvprintf("# crem_us: failed. Try to execute with sh\n");
           tmp=(char*)malloc(sizeof(char)*(strlen(script)+
                                           strlen(PATH_SHELL)+2));
           sprintf(tmp,"%s %s",PATH_SHELL,script);
           crvprintf("# crem_us: %s\n",tmp);
           execle(PATH_SHELL,"sh", "-c", tmp,NULL,envp);
        }
        _exit(0);
     }
     else
     {  if(type==CREM_UPREPS)
        {  set_sessionid_job(rid,cpid);
           crvprintf("# crem_us: Waiting child: timeout=%d\n",timeout);
           alarm(timeout);

           signal(SIGALRM,crem_kill_timeout);
           siginterrupt(SIGALRM,1);
           s=waitpid(cpid,NULL,WUNTRACED);
           alarm(0);
           signal(SIGALRM,SIG_DFL);
           siginterrupt(SIGALRM,0);
           if(s!=cpid)
           {  crvprintf("# crem_us: Killing child process PID=%d\n",cpid);
              crono_log(cluster.logfile,CR_LOG, "crrmd: # crem_us: "
                        "Killing child process PID=%d\n",cpid);
              crem_kill_session(cpid);
              waitpid(cpid,NULL,WUNTRACED);
           }
           dprintf("# crem_us: FINISHED\n");
        }
     }
  }
  else
     crvprintf("# crem_us: %s: Script not found\n",script);
}
//#############################################################################
//## Wait until a session with id finishes
//## In this function we have a polling (ergh!!)
//## This function should be improved, but for now I don't have a better idea.
//## Marco Jan 31 2003
//#############################################################################
void crem_wait_session_finishes(pid_t sid,long timeout)
{
  DIR *dirp;
  struct dirent *direntp;
  char found=FALSE;

  timeout-=SLEEP_WAIT_SESSION_FINISHES;
  do
  {  found=FALSE;
     dirp = opendir("/proc");
     if(dirp==NULL)
     {  dprintf("# crem_wait_session_finishes: Cannot open /proc directory\n");
        return;
     }
     while ( (direntp = readdir( dirp )) != NULL )
     {  if(isdigit(direntp->d_name[0]))
        {  if(atoi(direntp->d_name)==sid)
           {  found=TRUE;
              dprintf("# crem_wait_session_finishes: found\n");
              break;
           }
        }
     }
     closedir(dirp);
     sleep(SLEEP_WAIT_SESSION_FINISHES);
     timeout-=SLEEP_WAIT_SESSION_FINISHES;
  }while(found && timeout>0);

  dprintf("# crem_wait_session_finishes: TIMEOUT!!!\n");
  dprintf("# crem_wait_session_finishes: Killing the session!!!\n");
  crem_kill_session(sid);
  dprintf("# crem_wait_session_finishes: Session killed\n");
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
