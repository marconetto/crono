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

//############################################################################## 
//## These are common procedures that are used in all modules of Crono
//##############################################################################
#include "common.h"

//##############################################################################
//## Get some informations about a cluster and put them in "SConf *conf"
//## dirconf= configuration directory
//##############################################################################
void crget_config(char *cluster,SConf *conf,char *dirconf)
{
  int i;
  //parser
  char p_amport=FALSE, p_rmport=FALSE, p_nmport=FALSE, p_amhost=FALSE,
       p_rmhost=FALSE, p_log=FALSE, p_queue=FALSE, p_share=FALSE,
       p_single=FALSE, p_pam=FALSE, p_timeout=FALSE, p_tmpreps=FALSE,
       p_tmpostps=FALSE,p_piddir=FALSE;
   //--
  char file[S_PATH], buf[S_LINEF], buf1[S_LINEF], buf2[S_LINEF], buf3[S_LINEF];
  FILE *fp;

  snprintf(file,S_PATH,"%s/%s/crono.conf",dirconf,cluster);
  if( (fp=fopen(file,"r")) == NULL)
     ERROR("Problems to open configuration file: %s\n",file);

  while(fgets(buf,S_LINEF,fp))
  { 
     buf[strlen(buf)-1]=0;
     i=0;

     //take off spaces, tabs and lines starts with #
     while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
        i++;
     if( (i==strlen(buf))|| (buf[i]=='#') )
        continue;

     buf1[0]=0;
     buf2[0]=0;
     if(sscanf(&buf[i],"%[^=]=%s %s",buf1,buf2,buf3) != 2)
        cr_file_error(fp,"%s: Syntax Error\n",file);
     if(!strcasecmp(buf1,"amport"))
     {  if(!p_amport)
        {  conf->amport=atoi(buf2);
           p_amport=TRUE;
        }
        else
           cr_file_error(fp,"%s: AMPort was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"rmport"))
     {  if(!p_rmport)
        {  conf->rmport=atoi(buf2);
           p_rmport=TRUE;
        }
        else
           cr_file_error(fp,"%s: RMPort was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"nmport"))
     {  if(!p_nmport)
        {  conf->nmport=atoi(buf2);
           p_nmport=TRUE;
        }
        else
           cr_file_error(fp,"%s: NMPort was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"amhost"))
     {  if(!p_amhost)
        {  strcpy(conf->amhost,buf2);
           p_amhost=TRUE;
        }
        else
           cr_file_error(fp,"%s: AMHost was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"rmhost"))
     {  if(!p_rmhost)
        {  strcpy(conf->rmhost,buf2);
           p_rmhost=TRUE;
        }
        else
           cr_file_error(fp,"%s: RMHost was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"logfile"))
     {  if(!p_log)
        {  strcpy(conf->logfile,buf2);
           p_log=TRUE;
        }
        else
           cr_file_error(fp,"%s: Log File was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"queuefile"))
     {  if(!p_queue)
        {  strcpy(conf->queuefile,buf2);
           p_queue=TRUE;
        }
        else
           cr_file_error(fp,"%s: Queue File was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"piddir"))
     {  if(!p_piddir)
        {  strcpy(conf->piddir,buf2);
           p_piddir=TRUE;
        }
        else
           cr_file_error(fp,"%s: Pids Directory was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"share"))
     {  if(!p_share)
        {  conf->share=atoi(buf2);
           if(conf->share<1)
              ERROR("The value of share option must be greater than 1\n");
           p_share=TRUE;
        }
        else
           cr_file_error(fp,"%s: Reserves File was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"single"))
     {  if(!p_single)
        {  if(!strcmp(buf2,"on"))
              conf->single=TRUE;
           else if(!strcmp(buf2,"off"))
              conf->single=FALSE;
           else
              ERROR("The value of single option must be on or off\n");
           p_single=TRUE;
        }
        else
           cr_file_error(fp,"%s: Single option was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"pam"))
     {  if(!p_pam)
        {  if(!strcmp(buf2,"on"))
              conf->pam=TRUE;
           else if(!strcmp(buf2,"off"))
              conf->pam=FALSE;
           else
              ERROR("The value of pam option must be on or off\n");
           p_pam=TRUE;
        }
        else
           cr_file_error(fp,"%s: PAM option was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"conntimeout"))
     {  if(!p_timeout)
        {  conf->conntimeout=atoi(buf2);
           if(conf->conntimeout<=0)
              ERROR("The value of conntimeout option must be greater than "
                    "0\n");
           p_timeout=TRUE;
        }
        else
           cr_file_error(fp,"%s: Connection Timeout option was already "
                            "defined\n",file);
     }
     else if(!strcasecmp(buf1,"tmpreps"))
     {  if(!p_tmpreps)
        {  conf->tmpreps=atoi(buf2);
           if(conf->tmpreps<=-1)
              ERROR("The value of tmpreps option must be positive\n");
           p_tmpreps=TRUE;
        }
        else
           cr_file_error(fp,"%s: Pre-processing time option was already "
                            "defined\n",file);
     }
     else if(!strcasecmp(buf1,"tmpostps"))
     {  if(!p_tmpostps)
        {  conf->tmpostps=atoi(buf2);
           if(conf->tmpostps<=-1)
              ERROR("The value of tmpostps option must be positive\n");
           p_tmpostps=TRUE;
        }
        else
           cr_file_error(fp,"%s: Post-processing time option was already "
                            "defined\n",file);
     }
     else
        ERROR("%s: %s: Invalid keyword\n",file,buf1);
  }//while
  fclose(fp);
  dprintf("END of configuration file\n");
  if(!p_amport)     
     printf("%s: AM Port is not defined\n",file);
  if(!p_rmport) 
     printf("%s: RM Port is not defined\n",file);
  if(!p_nmport) 
     printf("%s: NM Port is not defined\n",file);
  if(!p_amhost)
     printf("%s: AM Host is not defined\n",file);
  if(!p_rmhost)
     printf("%s: RM Host is not defined\n",file);
  if(!p_log)    
     printf("%s: Logfile is not defined\n",file);
  if(!p_queue)
     printf("%s: Queue file is not defined\n",file);
  if(!p_piddir)
     printf("%s: Pids directory is not defined\n",file);
  if(!p_share)
     printf("%s: Share option is not defined\n",file);

  if(!p_single) 
  {  conf->single=FALSE;
     printf("%s: Single option is NOT  defined.\n>>Assuming"
            " \"single=off\"\n",file);
  } 
  if(!p_pam) 
  {  conf->pam=TRUE;
     printf("%s: PAM option is not defined.\n>>Assuming \"pam=on\"\n",file);
  } 
  if(!p_timeout) 
  {  conf->conntimeout=1;
     printf("%s: Connection Timeout option is NOT  defined.\n>>Assuming"
            " \"conntimeout=1\"\n",file); 
  } 
  if(!p_tmpreps) 
  {  conf->tmpreps=CR_DEFAULT_TMPREPS;
     printf("%s: Pre-processing time option is not defined.\n>>Assuming "
            "\"tmpreps=%d\"\n",file,CR_DEFAULT_TMPREPS);
  } 
  if(!p_tmpostps) 
  {  conf->tmpostps=CR_DEFAULT_TMPOSTPS;
     printf("%s: Post-processing time option is not defined.\n>>Assuming "
            "\"tmpostps=%d\"\n",file,CR_DEFAULT_TMPOSTPS);
  } 

  if (!p_amport || !p_nmport ||!p_rmport || !p_amhost || !p_rmhost || 
      !p_log || !p_queue || !p_share || !p_piddir)
     exit(1);
}
//##############################################################################
//## Write a message and close the file descriptor
//##############################################################################
void cr_file_error(FILE *fp,char *fmt,... )
{
  va_list l;

  va_start(l,fmt);
  vprintf(fmt,l);
  fclose(fp);
  printf("\n");
  exit(1);
}
//##############################################################################
//## Kill the daemon (cramd, crrmd or crnmd) of a 'cluster'
//##############################################################################
void cr_kill_daemon(char *daemon,char *cluster,char *piddir)
{
  char buf[S_PATH],pidfile[S_PATH];
  int pid;
  FILE *fp;

  dprintf("Killing %s daemon\n",daemon);
  snprintf(pidfile,S_PATH,"%s/%s_%s.pid",piddir,daemon,cluster);
  dprintf("Pid file: %s\n",pidfile);

  if( (fp=fopen(pidfile,"r")) == NULL )
     ERROR("%s: not found.\nMay be the %s process for \"%s\" cluster is not"
           " running.\n",pidfile,daemon, cluster);
  else
  {  fgets(buf, sizeof(buf), fp);
     fclose(fp);
     pid=atoi(buf);
     if(kill(pid, SIGTERM))
        fprintf(stderr,"%s: it cannot kill the process %d\n",daemon, pid);
     else 
        fprintf(stderr,"%s: process %d is dead.\n",daemon, pid);
     unlink(pidfile);
  }
}
//#############################################################################
//## Check if the pid file exists  ( for this 'daemon' and 'cluster')
//#############################################################################
int cr_pid_file_exists(char *daemon,char *cluster,char *piddir)
{
  char file[S_PATH];

  snprintf(file,S_PATH,"%s/%s_%s.pid",piddir,daemon,cluster);
  if(file_exists(file))
     return TRUE;
  else 
     return FALSE;
}
//#############################################################################
//## Write the pid file of a 'daemon'
//#############################################################################
void cr_write_pid(char *daemon,char *cluster,char *piddir,char *logfile)
{
  FILE *fp;
  char file[S_PATH];

  snprintf(file,S_PATH,"%s/%s_%s.pid",piddir,daemon,cluster);
  dprintf("Writing pid: %s\n",file);

  if( (fp=fopen(file, "w")) == NULL)
     crono_log(logfile,CR_ERROR,"%s: Problems to write the pid file: %s\n",
               daemon,file);
  else
  {  fprintf(fp,"%d",getpid());
     fclose(fp);
  }
}
//##############################################################################
//## Create a tcp client
//##############################################################################
int create_client_tcpsocket(char *host,int port)
{
  int rc,sd;
  struct sockaddr_in localAddr, servAddr;
  struct hostent *h;

  h = gethostbyname(host);
  if(h==NULL)
     ERROR_SOCKET_CREATE_RETURN("Unknown host '%s'\n",host);

  servAddr.sin_family = h->h_addrtype;
  memcpy((char *) &servAddr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
  servAddr.sin_port = htons(port);
  sd = socket(AF_INET, SOCK_STREAM, 0);
  if(sd<0)
     ERROR_SOCKET_CREATE_RETURN("Cannot open socket\n");

  // bind any port number
  localAddr.sin_family = AF_INET;
  localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  localAddr.sin_port = htons(0);
  rc = bind(sd, (struct sockaddr *) &localAddr, sizeof(localAddr));
  if(rc<0)
     ERROR_SOCKET_CREATE_RETURN("Cannot bind port TCP\n");
  // connect to server 
  rc = connect(sd, (struct sockaddr *) &servAddr, sizeof(servAddr));
  if(rc<0)
     return CR_ERROR;
  return sd;
}
//##############################################################################
//## Create the path to put a 'file'
//## Ex.: file='/tmp/dir1/dir2/file  -> Create '/tmp/dir1/dir2'
//##############################################################################
void create_path(char *file)
{
  struct stat stat_buf;
  int i=0,j=0;
  char tmp[S_PATH];

  umask(00000);
  while(j<strlen(file))
  {  if(file[j]=='/')
     {  tmp[i]='/';
        tmp[i+1]=0;
        if(stat(tmp,&stat_buf) == -1) 
           mkdir(tmp,00755);
     }
     else
      tmp[i]=file[j];
     i++;
     j++;
 }
 umask(00022);
}
//##############################################################################
//## Create a tcp server (return a socket descriptor)
//##############################################################################
int create_server_tcpsocket(int port)
{
  int sd;
  //struct   hostent   *phost;     //pointer to hosts table
  struct   protoent  *pproto;    //pointer to protocols table
  struct   sockaddr_in saddr;    //server struct
  //struct   linger li;
  int reuse=1;
   
  dprintf("Port: %d\n",port);
  memset((char  *)&saddr,0,sizeof(saddr)); //clean the saddr structure
  saddr.sin_family = AF_INET;              //set Internet family
  saddr.sin_addr.s_addr = INADDR_ANY;      //set IP local address

  if (port > 0)
      saddr.sin_port = htons((u_short)port); //especify the port
  else
     ERROR_SOCKET_CREATE_RETURN("Invalid port %d\n",port);

  //get the number of protocol
  if ( ((int)(pproto = getprotobyname("tcp"))) == 0)
      ERROR_SOCKET_CREATE_RETURN("Couldnt mapping tcp protocol!\n");

  //create a socket
  sd = socket (PF_INET, SOCK_STREAM, pproto->p_proto);
  if (sd < 0)
     ERROR_SOCKET_CREATE_RETURN("Couldnt create socket!\n");

  if((setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(int)))<0)
     ERROR_SOCKET_CREATE_RETURN("REUSEADDR\n");

  //li.l_onoff = 1;
  //l/i.l_linger = LINGER_SECS;
  //if (setsockopt(sd, SOL_SOCKET, SO_LINGER, (char *) &li,
  //     sizeof(struct linger)) < 0) 
  //   ERROR_SOCKET_CREATE_RETURN("Linger failed\n");

  if (bind(sd, (struct sockaddr *)&saddr, sizeof (saddr)) < 0)
     ERROR_SOCKET_CREATE_RETURN("bind failed\n");

  if (listen(sd, QLEN) < 0)
     ERROR_SOCKET_CREATE_RETURN("Failed to listen\n");

  return sd;
}
//##############################################################################
//## Write the message to log file 
//##############################################################################
void crono_log(char *logfile,int er, char *format,...)
{
  char buf[S_LINEF];
  va_list list;
  FILE *fp;
  char time_buffer[sizeof("XXX XX hh:mm:ss  ")];
  time_t timet;
  struct tm *tmstr;
  
  timet=time(NULL);
  tmstr= localtime(&timet);
  strftime(time_buffer, sizeof(time_buffer), "%b %d %H:%M:%S", tmstr);

  va_start(list,format);
  vsnprintf(buf,S_LINEF,format,list);
  dprintf("%s > %s\n",time_buffer,buf);
 
  if( (fp=fopen(logfile,"a") ) == NULL )
     dprintf("Cannot open logfile: %s\n",logfile);
  else
  {  if(er==CR_ERROR)
        fprintf(fp,"%s > [ERROR] %s\n",time_buffer,buf);
     else
        fprintf(fp,"%s > %s\n",time_buffer,buf);  
     fclose(fp);
  }
  
  if(er==CR_ERROR)
     ERROR(buf);
}
//##############################################################################
//## Check if file exists 
//##############################################################################
char file_exists(char *file)
{
  struct stat stat_buf;

  //EBADF file does not exist
  if(stat(file,&stat_buf)==-1) return FALSE;
  else return TRUE; 
}
//##############################################################################
//## Get user id from user name(login)
//##############################################################################
int login2uid(char *login)
{
  struct passwd* user;

  user=getpwnam(login);
  if(user==NULL)
  {  dprintf("Coundnt find uid from login %s\n",login);
     return -1;
  }
  else
     return user->pw_uid;
}
//##############################################################################
//## Check the permission for writing the *file
//##############################################################################
char file_permission_to_write(char *file)
{
  FILE *fp;
  if( (fp=fopen(file,"a"))==NULL)
     return FALSE;
  else
  {  fclose(fp);
     return TRUE;
  }
}
//##############################################################################
//## Get group from userid
//##############################################################################
int uid2gid(int uid, int *gid)
{
  struct passwd *pw = NULL;
  struct passwd pwd;
  char buffer[PWD_BUFFER];

  if( getpwuid_r(uid, &pwd, buffer, sizeof(buffer), &pw)!=0)
  {  dprintf("Cannot find login for uid %d\n",uid);
     *gid=-1;
  }
  else
     *gid=pwd.pw_gid;

  return *gid;
}
//##############################################################################
//## Get username from userid
//##############################################################################
char *uid2login(int uid, char *login)
{
  struct passwd *pw = NULL;
  struct passwd pwd;
  char buffer[PWD_BUFFER];

  if( getpwuid_r(uid, &pwd, buffer, sizeof(buffer), &pw)!=0)
  {  dprintf("Cannot find login for uid %d\n",uid); 
     strcpy(login,"?");
  }
  else
     strcpy(login,pwd.pw_name);

  return login;
}
//##############################################################################
//## Get home directory from userid
//##############################################################################
char *uid2home(int uid, char *home)
{
  struct passwd *pw = NULL;
  struct passwd pwd;
  char buffer[PWD_BUFFER];

  if( getpwuid_r(uid, &pwd, buffer, sizeof(buffer), &pw)!=0)
  {  dprintf("Cannot find home for uid %d\n",uid);
     strcpy(home,"?");
  }
  else
     strcpy(home,pwd.pw_dir);

  return home;
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
