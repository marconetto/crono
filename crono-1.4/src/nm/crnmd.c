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
#include "crnmd.h"

int *usersallowed=NULL,   //vector with ids of users allowed to access this node
   nusersallowed=0,      //number of users allowed to access this node
   verbose=FALSE;        //verbose mode
char dirconf[S_PATH];     //configuration directory
SCnm cluster;
//#############################################################################
//## CRONO NODE MANAGER DAEMON
//#############################################################################
int main (int argc, char **argv)
{
  struct   sockaddr_in cad;     //structure to hold client address 
  int ssd,                      //server socket descriptor
      csd,                      //client socket descriptor
      alen;                     //length of address 
  pthread_t  tid;               //thread id

  crnmd_getargs(argc,argv);
  if(cr_pid_file_exists("crnmd",cluster.cluster,cluster.piddir))
  {  printf("Verify if you already executed crnmd daemon\n");
     printf("If not, erase the file: %s/%s_%s.pid\n",cluster.piddir,"crnmd",
             cluster.cluster);
     exit(1);
  }
  if(!file_permission_to_write(cluster.logfile))
     ERROR("%s: Impossible for writing this file.\n"
           "Try to modify the path of the logfile.\n",cluster.logfile);

  printf("Starting Crono Node Manager daemon version %s\n\n",CRONO_VERSION);

  if(!verbose)
  {  if(fork() != 0) 
       exit(0);
     fclose(stdout);
     fclose(stdin);
     fclose(stderr);
     if(chdir("/")<0)
     {  perror("chdir");
        exit(1);
     }
     if(setsid()<0)  //release controlling terminal    
     {  perror("setsid");
        exit(1);
     }
     cr_write_pid("crnmd",cluster.cluster,cluster.piddir,cluster.logfile);
  }

  if( (ssd=create_server_tcpsocket(cluster.nmport)) == CR_ERROR)
     exit(1);

  request_rm_allowed_user();
  dprintf("NM Server is up\n");
  while(TRUE)
  {  alen=sizeof(cad);
     if (  (csd=accept(ssd, (struct sockaddr *)&cad, &alen)) < 0)  
         ERROR("%s: accept failed\n",argv[0]); 
     //VERSION
     pthread_create(&tid, NULL, crnmd_handle_client_thread, (void *) &csd );
  }
}
//##############################################################################
//## Add a user to the list of allowed users 
//##############################################################################
void add_user_allowed(int uid)
{
  int i;

  //dprintf("# add_user_allowed: user='%s'\n",uid2login(uid));
  //if user has already the access, return
  crvprintf("# add_user_allowed: UID=%d\n",uid);
  for(i=0;i<nusersallowed;i++)
     if( usersallowed[i] == uid)
        return; 

  usersallowed=(int*)realloc(usersallowed,sizeof(int)*(nusersallowed+1));
  usersallowed[nusersallowed++]=uid;
}
//##############################################################################
//## Parser of command line 
//##############################################################################
void crnmd_getargs(int argc,char **argv)
{
  char opdir=FALSE,opkill=FALSE, //used in parser for command line
       nclusters=0;              //used in parser for command line
  SConf *conf;                   //used to get configuration 

  conf=(SConf*)malloc(sizeof(SConf));

  while (--argc)
  {  argv++;
     if( !strcmp(argv[0],"-d") )
     {  if(nclusters)
           crnmd_usage();
        if( !--argc || opdir )
           crnmd_usage();
        else
        {  argv++;
           strcpy(dirconf,argv[0]);
           opdir=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-v") )      //verbose mode
           verbose=TRUE;
     else if ( !strcmp(argv[0],"--help") ) // show usage of crnmd
           crnmd_usage();
     else if( !strcmp(argv[0],"-k") )
       opkill=TRUE;
     else
     {  if(nclusters) //A Node Manager just manages one cluster
           crnmd_usage();
        if(!opdir)
        {  dprintf("Configuration directory wasnt defined. Using default.\n");
           strcpy(dirconf,DIRCONF);
           opdir=TRUE;
        }
        dprintf("CLUSTER: %s\n",argv[0]);
        crget_config(argv[0],conf,dirconf);
        strcpy(cluster.cluster,argv[0]);
        strcpy(cluster.logfile,conf->logfile);
        strcpy(cluster.piddir,conf->piddir);
        strcpy(cluster.rmhost,conf->rmhost);
        cluster.rmport=conf->rmport;
        cluster.nmport=conf->nmport;
        cluster.pam=conf->pam;
        nclusters=1;
     }
  }
  if(opkill)
  {  if(nclusters==1)
     {  cr_kill_daemon("crnmd",cluster.cluster,cluster.piddir);
        exit(0);
     }
     else
        crnmd_usage();
  }
  
  if(!nclusters)
     crnmd_usage();
  free(conf);
  create_path(cluster.logfile);
}
//##############################################################################
//## Handle the client requests
//##############################################################################
void *crnmd_handle_client_thread(void *arg)
{
  int csd=*((int*)arg);
  SPheader packet;
  char *buf;
  int uid;

  pthread_detach(pthread_self());
  signal(SIGPIPE,SIG_IGN); //ignore broken pipe

  crvprintf("# crnmd_handle_client_thread: NEW CLIENT\n");
  recv(csd,&packet,sizeof(SPheader),MSG_PEEK);

  switch(packet.type)
  {  case CR_NM_LOCK_ACCESS:
     case CR_NM_UNLOCK_ACCESS:
     case CR_NM_KILL_PROC:
     case CR_NM_KILL_PROC_LOCK:
     {   buf=malloc(packet.size);
         recv(csd,buf,packet.size,0);
         memcpy(&uid,buf+sizeof(SPheader),sizeof(int));
         nm_exec_request(uid,packet.type); 
         free(buf);
         break;
     }
     case CR_NM_RESET_LOCK_ACCESS:
     {  crvprintf(">>> Reset Lock Access: STARTED\n");
        nm_reset_lock_access(csd,packet.size);
        crvprintf(">>> Reset Lock Access: FINISHED\n");
        break;
     }
     case CR_NMC_EXEC_OPERATION:
     {  crvprintf(">>> Execute Operation: STARTED\n"); 
        nm_exec_operation(csd,packet.size); 
        crvprintf(">>> Execute Operation: FINISHED\n");
        break;
     }
  }
  close(csd);
  return NULL;
}
//#############################################################################
//## Show usage of crnmd
//#############################################################################
void crnmd_usage()
{
  printf("Crono v%s\n"
         "Usage: crnmd [-v] [-d <dirconf> ] <cluster>\n"
         "       crnmd -k <cluster>\n"
         "       crnmd --help\n\n"
         "\t-v\t\t: Verbose mode\n"
         "\t-d <dirconf>\t: Configuration directory\n"
         "\t-k <cluster>\t: Kill crnmd\n"
         "\t--help\t\t: Display this help and exit\n\n",CRONO_VERSION);
   exit(1);
}
//##############################################################################
//## Remove a user from the list of allowed users
//##############################################################################
void del_user_allowed(int uid)
{
  int i,j;

 // crvprintf("# del_user_allowed: user='%s'\n",uid2login(uid));
  for(i=0;i<nusersallowed;i++)
  {  if(usersallowed[i]==uid)
     {  crvprintf("# del_user_allowed: found\n");
        for(j=i;j<nusersallowed-1;j++)
           usersallowed[j]=usersallowed[j+1];
        nusersallowed--;
        usersallowed=(int*)realloc(usersallowed,sizeof(int)*(nusersallowed)+1);
        crvprintf("# del_user_allowed: user removed\n");
        return; 
     }
  }
}
//#############################################################################
//## Flush file login.access 
//#############################################################################
void flush_loginaccess()
{
  FILE *fp1, // temp file
       *fp2; // login.access (original)
  char tmp_path[S_PATH],buf[S_LINEF],login[_POSIX_LOGIN_NAME_MAX];
  int tfd; 
  int i;
  struct stat stat_buf;

  crvprintf("# flush_loginaccess: update access right file\n");
  strcpy(tmp_path,"/tmp/tmp_crono.XXXXXX");
  if ( (tfd = mkstemp(tmp_path)) < 0 )
     ERROR("%s: Cannot be created.\n",tmp_path);
  dprintf("Temporary file created : %s\n",tmp_path);
  fp1=fdopen(tfd,"w");

  sprintf(buf,"%s/%s/login.access",dirconf,cluster.cluster);
  if( (fp2=fopen(buf,"r")) == NULL)
     ERROR("Error opening file %s\n",buf);

  for(i=0;i<nusersallowed;i++)
  {  sprintf(buf,"+:%s:ALL\n",uid2login(usersallowed[i],login));
     fputs(buf,fp1);
     //dprintf("PUT: <%s>\n",buf);
  }

  while(fgets(buf,S_LINEF,fp2))
     fputs(buf,fp1);
  
  fclose(fp1);
  fclose(fp2);
  crvprintf("rename <%s> to <%s>\n",tmp_path,LOGIN_ACCESS_FILE);

  stat(LOGIN_ACCESS_FILE,&stat_buf);
  rename(tmp_path,LOGIN_ACCESS_FILE);


  chmod(LOGIN_ACCESS_FILE,stat_buf.st_mode);
  chown(LOGIN_ACCESS_FILE,stat_buf.st_uid,stat_buf.st_gid);
}
//#############################################################################
//## Flush file hosts.equiv 
//#############################################################################
void flush_hostsequiv()
{
  FILE *fp1, // temp file
       *fp2; // login.access (original)
  char tmp_path[S_PATH],buf[S_LINEF],login[_POSIX_LOGIN_NAME_MAX];
  int tfd; 
  int i;
  struct stat stat_buf;

  crvprintf("# flush_hostsequiv: update access right file\n");
  strcpy(tmp_path,"/tmp/tmp_crono.XXXXXX");
  if ( (tfd = mkstemp(tmp_path)) < 0 )
     ERROR("%s: Cannot be created.\n",tmp_path);
  dprintf("Temporary file created : %s\n",tmp_path);
  fp1=fdopen(tfd,"w");

  sprintf(buf,"%s/%s/hosts.equiv",dirconf,cluster.cluster);
  if( (fp2=fopen(buf,"r")) == NULL)
     ERROR("Error opening file %s\n",buf);

  for(i=0;i<nusersallowed;i++)
  {  snprintf(buf,S_LINEF,"+ %s\n",uid2login(usersallowed[i],login));
     fputs(buf,fp1);
     dprintf("PUT: <%s>\n",buf);
  }

  while(fgets(buf,S_LINEF,fp2))
     fputs(buf,fp1);
 
  fclose(fp1);
  fclose(fp2);
  //dprintf("rename <%s> to <%s>\n",tmp_path,HOSTS_EQUIV_FILE);
  stat(LOGIN_ACCESS_FILE,&stat_buf);
  rename(tmp_path,HOSTS_EQUIV_FILE);

  chmod(LOGIN_ACCESS_FILE,stat_buf.st_mode);
  chown(LOGIN_ACCESS_FILE,stat_buf.st_uid,stat_buf.st_gid);
}
//##############################################################################
//## Generate command. 
//##   Ex.: command="skill -u $CR_USERNAME" realcommand="skill -u stelmar"
//##
//## in  -> command, nmc //command will be lost!! (strtok)
//## out -> realcommand
//##############################################################################
void generate_command(char *command,char *realcommand,SNmc nmc,char *output)
{
  char *sep = " ", *token,login[_POSIX_LOGIN_NAME_MAX];

  crvprintf("# generate_command: COMMAND= %s\n",command);
  (realcommand)[0]=0;
  for (token = strtok (command, sep); token != NULL; token = strtok(NULL, sep))
  {  if( !strcmp(token,"$CR_USERNAME") )
     {  snprintf(&(realcommand)[strlen(realcommand)],CR_MAX_COMMAND_SIZE," %s",
                 uid2login(nmc.uid,login));
     }
     else if( !strcmp(token,"$CR_USERID") )
        snprintf(&(realcommand)[strlen(realcommand)],CR_MAX_COMMAND_SIZE," %s",
                 nmc.uid);
     else if( !strcmp(token,"$CR_CLUSTER") )
        snprintf(&(realcommand)[strlen(realcommand)],CR_MAX_COMMAND_SIZE," %s",
                 nmc.cluster);
     else
     {  snprintf(&(realcommand)[strlen(realcommand)],CR_MAX_COMMAND_SIZE,
                 " %s",token);
     }
  }
  snprintf(&(realcommand)[strlen(realcommand)],CR_MAX_COMMAND_SIZE,
           " > %s 2>&1",output);

  crvprintf("# generate_command: REALCOMMAND= %s\n",realcommand);
}
//##############################################################################
//## Kill user process 
//##############################################################################
void kill_user_process(int uid)
{
  char buf[CR_MAX_COMMAND_SIZE],login[_POSIX_LOGIN_NAME_MAX];

  crvprintf(">>> Kill Processes: BEGIN\n");
  snprintf(buf,CR_MAX_COMMAND_SIZE,"/usr/bin/pkill -KILL -u %s",
           uid2login(uid,login));
  dprintf("# kill_user_process: %s\n",buf);
  system(buf);  
  crvprintf(">>> Kill Processes: END\n");
}
//##############################################################################
//## Execute Operation 
//##############################################################################
void nm_exec_operation(int csd,int packetsize)
{
  char found=FALSE; //operation found , operation not found
  char file[S_PATH], path_out[S_PATH],operation[CR_MAX_COMMAND_SIZE], 
       command[CR_MAX_COMMAND_SIZE], *realcommand, *output;
  int i,fd;
  FILE *fp;
  void *buf,*offset;
  struct stat stat_buf;
  SNmc nmc;
  SPheader packet;

  //--------------RECEIVE AND UNPACK THE SOCKET PACKET-------------------
  buf=malloc(packetsize);
  recv(csd,buf,packetsize,0); 
  dprintf("# nm_exec_operation: packet received\n");
  offset=buf+sizeof(SPheader); 
  memcpy(&nmc,offset,sizeof(int)); //uid
  offset+=sizeof(int);
  nmc.operation=(char*)malloc(sizeof(char)*(strlen(offset)+1));
  strcpy(nmc.operation,offset);
  // dprintf("# nm_exec_operation: user='%s'\n",uid2login(nmc.uid));
  dprintf("# nm_exec_operation: operation='%s'\n",nmc.operation);
  free(buf);
  //--------------------------------------------------------------------
  //---------------GET COMMAND------------------------------------------
  snprintf(file,S_PATH,"%s/%s/commands",dirconf,cluster.cluster);
  if( (fp=fopen(file,"r")) == NULL)
  {  crono_log(cluster.logfile,CR_LOG,"crnmd: Cannot open commands file: %s",
               file);
     crvprintf("# nm_exec_operation:  Cannot open commands file: %s", file);
     free(nmc.operation);
     return ;
  }

  dprintf("# nm_exec_operation: getting command\n");
  //get the command of respective operation
  while(1)
  {  fscanf(fp,"%[^\t :]",operation);  //read everything until ' ', '\t' or ':'
     fscanf(fp,"%*[ \t:]"); //read every ' ', '\t' and ':'
     if(feof(fp))
        break;
     fgets(command,sizeof(command),fp);
     command[strlen(command)-1]=0;
     dprintf("nmc.operation |%s| operation |%s|\n",nmc.operation,operation);
     if( !strcmp(operation,nmc.operation) )
     {  found=TRUE;
        break;
     } 
  }
  fclose(fp);
  if(!found)  
  {  free(nmc.operation);
     return;
  }   

  strcpy(path_out,"/tmp/tmp_crono.XXXXXX");
  if ( (fd = mkstemp(path_out)) < 0 )               //generate temp file name
     ERROR("%s: Cannot be created.\n",path_out);
  close(fd);

  if( (realcommand=(char*)malloc(sizeof(char)*CR_MAX_COMMAND_SIZE))==NULL)
  {  free(nmc.operation);
     return;
  }
  generate_command(command,realcommand,nmc,path_out);
  dprintf("# nm_exec_operation: REALCOMMAND: %s\n",realcommand);
  //-----------------------------------------------------------------------
  //---------------EXECUTE COMMAND AND GET STDOUT AND STDERROR-------------
  system(realcommand);
  free(realcommand);
  free(nmc.operation);
  //now 'command' is the hostname
  gethostname(command,sizeof(command));
  //i=> \n<hostname>:' '
  i=strlen(command)+3; 

  //now 'found' is a flag
  found=stat(path_out,&stat_buf);

  //get the size of output
  if(found!=-1 && stat_buf.st_size!=0)
     i+=stat_buf.st_size;
  else if (found!= -1 && stat_buf.st_size==0)
     i+=strlen("Command executedi\n ");          
  else if(found==-1)
     i+=strlen("Command not executed\n ");    

  if( (output=(char*)malloc(sizeof(char)*i)) ==NULL)
     return;

  dprintf("hostname: %s\n",command);
  snprintf(output,sizeof(char)*i,"%s: ",command);
  
  //get the output
  if(found!=-1 && stat_buf.st_size!=0)
  {  fp=fopen(path_out,"r");
     fread(output+sizeof(char)*(strlen(command)+2),i,sizeof(char),fp);
     fclose(fp);
  }
  else if(found!= -1 && stat_buf.st_size==0)
     strcat(output,"Command executed\n");
  else if(found==-1)
     strcat(output,"Command not executed\n");
  //output[strlen(command)+i+3]=0; 
  //-----------------------------------------------------------------------
  //---------------RETURN OUTPUT-------------------------------------------
  dprintf("# nm_exec_operation: Return output: <%s>\n",output);
  packet.size=sizeof(SPheader)+sizeof(char)*i;
  dprintf("# nm_exec_operation: Packetsize=%d\n",packet.size);
  if( (buf=malloc(packet.size)) == NULL)
  {  free(output);
     return;
  }   
  memcpy(buf,&packet,sizeof(SPheader));
  strcpy(buf+sizeof(SPheader),output);
  send(csd,buf,packet.size,0);
  free(buf);
  free(output);
  unlink(path_out);
  dprintf("# nm_exec_operation: output returned\n");
}
//#############################################################################
//## (Un)lock a user 
//#############################################################################
void nm_lock_unlock_user(int uid, int type)
{
  crvprintf(">>> (UN)LOCK: BEGIN\n");

  if(type==CR_NM_UNLOCK_ACCESS)
     add_user_allowed(uid);
  else if(type==CR_NM_LOCK_ACCESS)
     del_user_allowed(uid);

  flush_loginaccess();

 if(cluster.pam==FALSE)
     flush_hostsequiv();

 crvprintf(">>> (UN)LOCK: END\n");
}
//#############################################################################
//## Execute lock,unlock, kill process and kill_lock requests
//#############################################################################
void nm_exec_request(int uid,int type)
{
  if(type==CR_NM_LOCK_ACCESS || type==CR_NM_UNLOCK_ACCESS)
     nm_lock_unlock_user(uid,type);
  else if(type==CR_NM_KILL_PROC)
     kill_user_process(uid); 
  else if(type==CR_NM_KILL_PROC_LOCK)
  {  nm_lock_unlock_user(uid,CR_NM_LOCK_ACCESS);
     kill_user_process(uid);
  }
}
//#############################################################################
//## Receive the users that have access to this node from Request Manager 
//#############################################################################
void nm_reset_lock_access(int csd,int packetsize)
{
  int uid;
  void *buf,*offset; 

  if(nusersallowed!=0)
  {  nusersallowed=0;
     free(usersallowed);
     usersallowed=NULL;
  } 
  if( (buf=malloc(packetsize))==NULL)
     return;

  recv(csd,buf,packetsize,0);
  offset=buf+sizeof(SPheader);
  if(packetsize==sizeof(SPheader))
     crvprintf("# nm_reset_lock_access: no users to add\n");
  else
  {  while(offset<buf+packetsize)
     {  memcpy(&uid,offset,sizeof(int)); 
        offset+=sizeof(int);
        add_user_allowed(uid);
     }
  }
  flush_loginaccess();
  if(cluster.pam==FALSE)
     flush_hostsequiv();
  free(buf);
}
//#############################################################################
//## Request allowed users from Request Manager
//## This is useful, if for example, the nm crash, or the node is rebooted,
//## the login.access and hosts.equiv(if PAM is on) files have to be flushed 
//## correctly. 
//#############################################################################
void request_rm_allowed_user()
{
  int sd;          //socket descriptor
  char node[MAXHOSTNAMELEN];  
  SPheader packet;
  void *buf,*offset;

  crvprintf(">>> Request RM for allowed users: STARTED\n");

  gethostname(node,MAXHOSTNAMELEN);

  packet.size=sizeof(SPheader)+sizeof(char)*(strlen(node)+1);
  packet.type=CR_GET_ALLOWED_USERS;
  buf=malloc(packet.size);
  memcpy(buf,&packet,sizeof(SPheader));
  strcpy(buf+sizeof(SPheader),node);

  if( (sd=create_client_tcpsocket(cluster.rmhost,cluster.rmport)) == CR_ERROR)
  {  crono_log(cluster.logfile,CR_LOG,"crnmd: Cannot request crrmd to get "
                                      "allowed users");
  }
  else
  {  send(sd, buf, packet.size, 0);
     free(buf);
     recv(sd,&packet,sizeof(SPheader),MSG_PEEK);
     buf=malloc(packet.size);
     recv(sd,buf,packet.size,0);
     close(sd);
     dprintf("# request_rm_allowed_user: received packet from RM size=%d\n",
             packet.size);
     offset=buf+sizeof(SPheader);
     dprintf("# request_rm_allowed_user: %p %p\n",offset,buf+packet.size);
     //'sd' used as 'user id' 
     while(offset<buf+packet.size)
     {  memcpy(&sd,offset,sizeof(int)); 
        offset+=sizeof(int);
        add_user_allowed(sd);
     }
  }
  free(buf);
  crvprintf(">>> Request RM for allowed users: FINISHED\n"); 
  flush_loginaccess();
  if(cluster.pam==FALSE)
     flush_hostsequiv();
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
