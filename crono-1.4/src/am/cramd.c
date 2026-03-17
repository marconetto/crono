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
#include "cramd.h"
#include "cram_parsers.h"

SCram *clusters; 
char dirconf[S_PATH];     //configuration directory
int nclusters=0,          //number of cluster AM has access
    am_port,              //daemon listen port
    verbose=FALSE;        //verbose mode
pthread_mutex_t mutex_pw; //to access the passwd struct
//#############################################################################
//##  CRONO ACCESS MANAGER DAEMON (cramd)
//#############################################################################
int main (int argc, char **argv)
{
  struct sockaddr_in cad;       //structure to hold client address 
  int ssd,                      //server socket descriptor
      csd,                      //client socket descriptor
      alen,                     //length of address 
      i;
  pthread_t  tid;               //thread id

  cramd_getargs(argc,argv);
  crvprintf("Verbose mode\n"); 
  am_check_files_syntax();

  if(cr_pid_file_exists("cramd",clusters[0].cluster,clusters[0].piddir))
  {  printf("Verify if you have already executed cramd daemon\n");
     printf("If not, erase the file: %s/%s_%s.pid\n",clusters[0].piddir,"cramd",
            clusters[0].cluster);
     exit(1);
  }
  dprintf("Dir: %s\n",dirconf);
  dprintf("AM Server is up\n");

  for(i=0;i<nclusters;i++)
  {  if(!file_permission_to_write(clusters[i].logfile))
        ERROR_FILE_LOG(clusters[i].logfile);
     crono_log(clusters[i].logfile,CR_LOG,
               "cramd: AM Server is up. CLUSTER: %s",clusters[i].cluster);
  }

  printf("Starting Crono Access Manager daemon version %s\n\n",CRONO_VERSION);  

  //but in background if it is not in verbose mode
  if(!verbose)
  {  if (fork() != 0)
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
     cr_write_pid("cramd",clusters[0].cluster,clusters[0].piddir,
                  clusters[0].logfile);
  }

  if( (ssd=create_server_tcpsocket(clusters[0].amport)) == CR_ERROR) 
     exit(1);

  dprintf("Serving %d server(s)\n",nclusters);
  pthread_mutex_init(&mutex_pw, NULL); 

  while(1)
  {
     alen=sizeof(cad);
     //each client is attended in a thread
     if (  (csd=accept(ssd, (struct sockaddr *)&cad, &alen)) < 0)  
        ERROR("%s: accept failed\n",argv[0]); 
      crvprintf("NEW CLIENT\n");
      pthread_create(&tid, NULL, handle_client_thread, (void *) csd );
  }
}
//#############################################################################
//## Make an allocation 
//#############################################################################
void am_alloc_client(int csd,int packetsize)
{
  int answer, sd, 
      ntypes=0,      //qualitative allocation
      nbytes=0;      //number of bytes of the alloc->nodes that is created when
                     //a quantitative allocation is requested.         
  void *buf, *offset;
  
  SAlloc alloc;

  dprintf("# am_alloc_client: Packet size=%d\n",packetsize);
  buf=malloc(packetsize);

  if( recv(csd,buf,packetsize,0) < 0)
     return;

  //Unpack Header and SAlloc structure
  offset=buf+sizeof(SPheader);
  memcpy(&alloc,offset,S_PACK_SALLOC);
  offset+=S_PACK_SALLOC;
  alloc.cluster=(char*)malloc(sizeof(char)*(strlen(offset)+1));
  strcpy(alloc.cluster,offset);

  if(alloc.qualitative) //qualitative allocation
  {  offset+=sizeof(char)*(strlen(offset)+1);
     //skip batch job
     offset+=sizeof(char)*(strlen(offset)+1);
     //skip path
     offset+=sizeof(char)*(strlen(offset)+1);
     //get node types
     alloc.nodes=malloc(packetsize-(offset-buf));
     memcpy(alloc.nodes,offset,packetsize-(offset-buf));
     memcpy(&ntypes,offset,sizeof(int));
  }

  if(alloc.when==CR_ALLOC_FUTURE && alloc.tstart>=alloc.tfinish)
     answer=CR_RESERVE_TSTART_GREATER_TFINISH;
  else if(alloc.when==CR_ALLOC_FUTURE && alloc.tstart<=time(NULL))
     answer=CR_RESERVE_TSTART_LESSER_NOW;
  else
     answer=check_allocation(&alloc,&nbytes);

  if(answer>=0)
     answer=CR_OK;

  if( answer == CR_OK)
  {  dprintf("# am_alloc_client: Allocation is possible\n");
     answer=(sd=connect_to_rm(alloc.cluster));
     if( answer != CR_EAM_CANNOT_CONNECT_RM)
     {  
        if(alloc.when==CR_ALLOC_FUTURE)
        {  //update the alloc structure in the buf 
           //because the nreserves value has been changed
           memcpy(buf+sizeof(SPheader),&alloc,S_PACK_SALLOC);
        }
        if(!alloc.qualitative) //update alloc->nodes and package size
        {  buf=(char*)realloc(buf,packetsize+nbytes);
           memcpy(buf+packetsize,alloc.nodes,nbytes);
           packetsize+=nbytes; 
           memcpy(buf+sizeof(int),&packetsize,sizeof(int));
        }

        if(send(sd, buf, packetsize, 0) >= 0 )      //to RM
        {  crvprintf("# am_alloc_client: waiting answer\n");
           recv(sd, &answer, sizeof(int), 0);   //from RM
        }
        close(sd);
     }   
  }

  send(csd,&answer,sizeof(int),0); //send to user the answer
  dprintf("# am_alloc_client: finish operation \n");
  dprintf("# am_alloc_client: free memory\n\n");
  free(buf);
  free(alloc.cluster);
  if(alloc.qualitative || nbytes)
     free(alloc.nodes);
}
//##############################################################################
//## Check the syntax of the configuration files 
//##############################################################################
void am_check_files_syntax()
{
  int i;
  char file[S_PATH], group[_POSIX_LOGIN_NAME_MAX]; 

  group[0]=0;
  crvprintf("Checking the syntax of configuration files\n");   
  for(i=0;i<nclusters;i++)
  {  snprintf(file,S_PATH,"%s/%s/%s",dirconf,clusters[i].cluster,
                                      ACCESS_RIGHTS_DEFS);
     crvprintf("Checking %s  ... ",file);
     check_arguments("dummy",CR_CHECK_SYNTAX_FILE,file, 
                     clusters[i].logfile, 0,0,0,0, NULL);
     snprintf(file,S_PATH,"%s/%s/groups",dirconf,clusters[i].cluster);
     crvprintf("OK\nChecking %s ... ",file);
     snprintf(file,S_PATH,"%s/%s/groups",dirconf,clusters[i].cluster);
     get_group("dummy",file,group); 
     crvprintf("OK\n");                                                               
  }
  crvprintf("Checking done\n");
}
//##############################################################################
//## Take care of a crinfo request
//##############################################################################
void am_crinfo(int csd, int packetsize)
{
  int i, last_size,
         nbytes1,      //number of bytes to store the node type specifications
         nbytes2;      //number of bytes to store the access rights information 
  char file[S_PATH], user[_POSIX_LOGIN_NAME_MAX], group[_POSIX_LOGIN_NAME_MAX],
       *specialbuf;
  void *buf, *offset,
       *nodesinfo,
       *arinfo;       //access rights info

  SPheader packet;
  SInforeq inforeq;
  
  //unpack the packet from network
  buf=malloc(packetsize);
  if( recv(csd,buf,packetsize,0) <0)
  {  free(buf);
     return;
  }

  offset=buf+sizeof(SPheader);
  memcpy(&inforeq,offset,sizeof(SInforeq));
  offset+=sizeof(SInforeq);
  inforeq.cluster=(char*)malloc(sizeof(char)*(strlen(offset)+1));
  strcpy(inforeq.cluster,offset);
  free(buf); 

  crvprintf("# am_crinfo: Check the access rights. USER='%d'\n",inforeq.uid);

  if(inforeq.cluster[0]==0) // -all
     crvprintf("# am_crinfo: Get information about all clusters available\n");
  else
  {  crvprintf("# am_crinfo: cluster: '%s'\n",inforeq.cluster);
     packet.type=is_a_cluster(inforeq.cluster);
     packet.size=sizeof(SPheader);
     if( packet.type < 0)  //it is not a valid cluster
     {  send(csd,&packet,sizeof(SPheader),0); //send this information to UI
        free(inforeq.cluster);
        return;
     }
  }

  uid2login(inforeq.uid,user);   //set username (user)
  packet.size=sizeof(SPheader);
  packet.type=CR_OK;
  last_size=packet.size;
  buf=NULL;
  crvprintf("# am_crinfo: Nclusters available=%d\n",nclusters);

  for(i=0;i<nclusters;i++)
  {  //if get information of all clusters or this is the specified cluster
     if(inforeq.cluster[0]==0 || !strcmp(inforeq.cluster,clusters[i].cluster))
     {  crvprintf("# am_crinfo: Checking info of cluster: %s\n",
                   clusters[i].cluster);
      
        snprintf(file,S_PATH,"%s/%s/groups",dirconf,clusters[i].cluster);

        if( !get_group(user,file,group) )
           strcpy(group,"none"); //use the user access rights
        else
           strcpy(user,group);   //use the group access rights

        snprintf(file,S_PATH,"%s/%s/nodes",dirconf,clusters[i].cluster);

        get_nodes_info(file,clusters[i].logfile,&nodesinfo,&nbytes1);

  
        snprintf(file,S_PATH,"%s/%s/%s",dirconf,clusters[i].cluster,
                                        ACCESS_RIGHTS_DEFS);

        check_arguments(user,CR_INFO,file,clusters[i].logfile,0,0,
                        &arinfo,&nbytes2,&specialbuf);

        packet.size+=sizeof(char)*(strlen(clusters[i].cluster)+strlen(group)+
                                   strlen(specialbuf)+3)+nbytes1+nbytes2;

        //buf to send to UI
        buf=realloc(buf,packet.size);
        offset=buf+last_size;
        strcpy(offset,clusters[i].cluster);     //pack cluster
        offset+=sizeof(char)*(strlen(clusters[i].cluster)+1);
        strcpy(offset,group);                   //pack group
        offset+=sizeof(char)*(strlen(group)+1);

        crvprintf("nbytes1=%d nbytes2=%d\n",nbytes1,nbytes2);
        memcpy(offset,nodesinfo,nbytes1);       //pack node types information
        offset+=nbytes1;
        memcpy(offset,arinfo,nbytes2);          //pack access rights
        offset+=nbytes2;
        strcpy(offset,specialbuf);              //pack the special periods
        last_size=packet.size;
        dprintf("# am_crinfo: SPECIAL PERIODS: |%s|\n",specialbuf);
        free(specialbuf);
        free(nodesinfo);
      }
  }

  free(inforeq.cluster);
  if(buf==NULL)
     buf=malloc(packet.size);

  //update the packet size
  memcpy(buf,&packet,sizeof(SPheader));
  dprintf("# am_crinfo: packet created. size=%d\n",packet.size);

  if(send(csd,buf,packet.size,0)<0)
     crvprintf("# am_crinfo: Error sending packet\n");
  free(buf);
}
//#############################################################################
//## Node Manager Client 
//#############################################################################
void am_nmc_client(int csd,int packetsize)
{ 
  int answer;
  void *buf,*bufsend,*offset;
  SNmc nmc;
  SPheader packet;

  dprintf("# am_nmc_client: Packet size = %d\n",packetsize);
  buf=malloc(packetsize);
  if(recv(csd,buf,packetsize,0)<0)
  {  free(buf);
     return;
  }
  memcpy(&packet,buf,sizeof(SPheader));
  memcpy(&nmc,buf+sizeof(SPheader),sizeof(int)*3);
  offset=buf+sizeof(SPheader)+sizeof(int)*3;
  nmc.cluster=(char*)malloc(sizeof(char)*(strlen(offset)+1));
  strcpy(nmc.cluster,offset);
  dprintf("# am_nmc_client: user='%d' cluster='%s'\n",nmc.uid,nmc.cluster);
  packet.size=sizeof(SPheader);

  if( is_a_cluster(nmc.cluster) == CR_EAM_INVALID_CLUSTER )
     answer=CR_EAM_INVALID_CLUSTER;
  else
     answer=CR_OK;

  if(answer>=0) 
  {  if(nmc.oper==CR_NMC_GET_OPERATIONS)
        answer=am_nmc_get_operations(nmc,&bufsend,&packet);

     else //CR_NMC_EXEC_OPERATION
     {  offset+=sizeof(char)*(strlen(nmc.cluster)+1);
        nmc.operation=(char*)malloc(sizeof(char)*(strlen(offset)+1));
        strcpy(nmc.operation,offset);
        dprintf(" am_nmc_client: OPERATION <%s>\n",nmc.operation);
        answer=check_operation(nmc.operation,nmc.cluster);
        if(answer>=0) 
        {  dprintf("# am_nmc_client: check operation OK\n");
           answer=am_nmc_execute_operation(nmc,&bufsend,&packet,buf,packetsize);
           dprintf("# am_nmc_client: answer=%d\n",answer);
        }
     }
  }
  free(buf);
  //Send answer to UI
  if(answer<0)
  {  packet.type=answer;
     send(csd,&packet,sizeof(SPheader),0);
  }
  else
  {  memcpy(bufsend,&packet,sizeof(SPheader)); 
     send(csd,bufsend,packet.size,0);
     free(bufsend);
     dprintf("# am_nmc_client: sent bufsend. SIZE=%d\n",packet.size);
  } 
}
//##############################################################################
//## Execute Operation (crnmc) 
//##############################################################################
int am_nmc_execute_operation(SNmc nmc,void **bufsend,SPheader *packet,void *buf,
                            int packetsize)
{
  int sd;

  *bufsend=NULL;
  packet->size=sizeof(SPheader);

  //create connection to RM
  if ( (sd=connect_to_rm(nmc.cluster)) == CR_EAM_CANNOT_CONNECT_RM)
     return CR_EAM_CANNOT_CONNECT_RM;

  //to RM (send operation)
  if( send(sd, buf, packetsize, 0) < 0 )
  {  close(sd);
     return CR_ERROR_TRANSFER_DATA;
  }

  dprintf("# am_nmc_execute_operation: sent to RM\n");
  //receive packet from RM
  if( recv(sd, packet, sizeof(SPheader), MSG_PEEK) < 0 )
  {  close(sd);
     return CR_ERROR_TRANSFER_DATA;
  }

  dprintf("# am_nmc_execute_operation: recv1 from RM\n");
  *bufsend=malloc(packet->size);
  if( recv(sd, *bufsend, packet->size, 0) < 0 )
  {  close(sd);
     free(*bufsend);
     return CR_ERROR_TRANSFER_DATA;
  }
  dprintf("# am_nmc_execute_operation: recv2 from RM\n");
  dprintf("# am_nmc_execute_operation: END EXECUTION\n");

  close(sd);
  return CR_OK; 
}
//##############################################################################
//## Get operation for execution on node from `commands` file
//##############################################################################
int am_nmc_get_operations(SNmc nmc, void **buf,SPheader *packet)
{
  FILE *fp;
  int last_size; 
  char line[S_LINEF], file[S_PATH];

  dprintf("# am_nmc_get_operations: getting the available operations\n");
  packet->size=sizeof(SPheader);
  *buf=NULL;

  snprintf(file,S_PATH,"%s/%s/commands",dirconf,nmc.cluster);
  if( (fp=fopen(file,"r")) == NULL )
  {  crono_log(clusters[is_a_cluster(nmc.cluster)].logfile,CR_LOG,
               "cramd: Problems to open commands file: %s",file);
     return CR_EAM_NO_OPERATIONS_AVAILABLE;
  }
  else 
  {  packet->type=CR_OK;
     last_size=packet->size;
     while(1)
     {  //read everything, except ' ', '\t' and ':'
        fscanf(fp,"%[^\t :]",line);
        fscanf(fp,"%*s\n");
        if(feof(fp))
           break;
        fscanf(fp,"%*[^\n]\n");
        packet->size+=sizeof(char)*(strlen(line)+1);
        *buf=realloc(*buf,packet->size);
        strcpy(*buf+last_size,line);
        last_size=packet->size;
        dprintf("# am_nmc_get_operations: line =<%s>\n",line);
     }
     fclose(fp);
  } 
  return CR_OK;
}
//##############################################################################
//## Check the access rights of an allocation request
//##############################################################################
int check_allocation(SAlloc *alloc,int *nbytes)
{
  int i;
  char file[S_PATH];
  time_t timet; //get actual time
  char user[_POSIX_LOGIN_NAME_MAX],group[_POSIX_LOGIN_NAME_MAX];

  i=is_a_cluster(alloc->cluster); //get index of cluster
  if(i<0)
      return CR_EAM_INVALID_CLUSTER;
  
  if(alloc->when==CR_ALLOC_FUTURE && alloc->requid==0) //root has free access
  {  if(!alloc->qualitative)
     {  alloc->nodes=(char*)calloc(1,sizeof(int));
        *nbytes=sizeof(int);
     }
     return CR_OK;
  }

  if( is_a_cluster(alloc->cluster) == CR_EAM_INVALID_CLUSTER )
     return CR_EAM_INVALID_CLUSTER;

  if(alloc->when==CR_ALLOC_FUTURE)
     timet=alloc->tstart; //get start time of reserve
  else
     timet=time(NULL); 


  uid2login(alloc->uid,user);

  snprintf(file,S_PATH,"%s/%s/groups",dirconf,alloc->cluster);
  if( get_group(user,file,group) )
    strcpy(user,group);
  
  snprintf(file,S_PATH,"%s/%s/%s",dirconf,alloc->cluster, ACCESS_RIGHTS_DEFS);

  return check_arguments(user,CR_ALLOC,file,clusters[i].logfile,
                            alloc,timet, 0,nbytes,NULL);
}
//##############################################################################
//## Check if 'operation' is valid for execution on 'cluster' nodes
//##############################################################################
int check_operation(char *operation, char *cluster)
{
  FILE *fp;
  char file[S_PATH], buf[S_LINEF];
  int clusterid, answer=CR_EAM_INVALID_OPERATION; 

  dprintf("check_operation(%s)\n",operation);
  clusterid=is_a_cluster(cluster);

  snprintf(file,S_PATH,"%s/%s/commands",dirconf,cluster);
  if( (fp=fopen(file,"r")) == NULL )
  {  crono_log(clusters[clusterid].logfile,CR_LOG,
               "cramd: Problems to open commands file: %s",file);
     dprintf("cramd: Problems to open commands file: %s.\n",file);
     return answer;
  }

  while(1)
  {  fscanf(fp,"%[^\t :]",buf); //read everything, except ' ', '\t' and ':'
     fscanf(fp,"%*s\n");
     if(feof(fp))
        break;
     fscanf(fp,"%*[^\n]\n");
     if( !strcmp(buf,operation) )
     {  answer=CR_OK;
        break;
     } 
  }
  fclose(fp);
  return answer;
}
//#############################################################################
//## Connect to Request Manager (write logs if something goes wrong) 
//## Return a socket descriptor
//#############################################################################
int connect_to_rm(char *cluster)
{
  char host[MAXHOSTNAMELEN];
  int sd,i;
  
  strcpy(host,get_rmserver(cluster)); 
  dprintf("# connect_to_rm: RM host: %s\n",host);

  if( (sd=create_client_tcpsocket(host,get_rmport(cluster)) ) ==
          CR_ERROR)
  {   
     i=is_a_cluster(cluster); //get cluster id

     if(i!=CR_EAM_INVALID_CLUSTER)
        crono_log(clusters[i].logfile,CR_LOG, 
                   "cramd: Cannot connect to %s RM on host %s",cluster,host);
       
     dprintf("cramd: Cannot connect to %s RM on host %s\n",cluster,host);
     return CR_EAM_CANNOT_CONNECT_RM;
  }
  return sd; 
}
//#############################################################################
//## Parser of command line
//#############################################################################
void cramd_getargs(int argc,char **argv)
{
  char opdir=FALSE,opkill=FALSE; //parser
  int  i;
  SConf *conf;

  clusters=(SCram*)malloc(sizeof(SCram*));
  conf=(SConf*)malloc(sizeof(SConf));

  while (--argc)
  {  argv++;
     if( !strcmp(argv[0],"-d") )
     {  if(nclusters>0)  //there is no clusters defined
           cramd_usage();
        if( !--argc || opdir ) //just defined a directory
           cramd_usage();
        else
        {  argv++;
           strcpy(dirconf,argv[0]);
           opdir=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-v") ) //verbose mode
        verbose=TRUE;
     else if ( !strcmp(argv[0],"--help") ) // show usage of cramd
        cramd_usage();
     else if( !strcmp(argv[0],"-k") )
        opkill=TRUE;
     else
     {  if(!opdir)
        {  dprintf("Directory Configuration wasn't defined. Using default.\n");
           strcpy(dirconf,DIRCONF);
           opdir=TRUE;
        }
        //dprintf("CLUSTER: %s\n",argv[0]);
        for(i=0;i<nclusters;i++)
        {  if( !strcmp(argv[0],clusters[i].cluster) )
                ERROR("Cluster %s was already defined\n",argv[0]);
        }
        //add new cluster
        crget_config(argv[0],conf,dirconf);
        clusters=(SCram*)realloc(clusters,sizeof(SCram)*(nclusters+1));
        strcpy(clusters[nclusters].logfile,conf->logfile);
        strcpy(clusters[nclusters].cluster,argv[0]);
        strcpy(clusters[nclusters].rmhost,conf->rmhost);
        strcpy(clusters[nclusters].piddir,conf->piddir);
        clusters[nclusters].amport=conf->amport;
        clusters[nclusters].rmport=conf->rmport;
        create_path(clusters[nclusters].logfile);
        nclusters++;
     }
  }
  if(opkill)
  {  if(nclusters==1)
     {  cr_kill_daemon("cramd",clusters[0].cluster,clusters[0].piddir);
        exit(0);
     }
     else
       cramd_usage();
  }    
  
  if(!nclusters)   //user must define at least one cluster
     cramd_usage();
  free(conf);
}
//##############################################################################
//## Show usage of cramd
//##############################################################################
void cramd_usage()
{
  printf("Crono v%s\n"
         "Usage: cramd [-v] [-d <dirconf> ] <cluster1> ...\n"
         "       cramd -k <cluster>\n"
         "       cramd --help\n\n"
         "\t-v\t\t: Verbose mode\n"
         "\t-d <dirconf>\t: Configuration directory\n"
         "\t-k <cluster>\t: Kill cramd\n"
         "\t--help\t\t: Display this help and exit\n\n",CRONO_VERSION);
  exit(1);
}
//##############################################################################
//## Write the pid of the daemon
//##############################################################################
void cramd_write_pid(char *daemon,char *cluster)
{
  FILE *fp;
  char file[S_PATH];

  snprintf(file,S_PATH,"%s/%s_%s.pid",clusters[0].piddir,daemon,cluster);
  dprintf("Writing pid: %s\n",file);
  if( (fp=fopen(file, "w")) == NULL)
     ERROR("Problems to write the pid file: %s\n",file);
  else
  {  fprintf(fp,"%d",getpid());
     fclose(fp); 
  }
}
//##############################################################################
//## Get Request Manager port of cluster (*cluster)
//##############################################################################
int get_rmport(char *cluster)
{
  int i;

  for(i=0;i<nclusters;i++)
     if(!strcmp(clusters[i].cluster,cluster))
        return clusters[i].rmport;
  
  return 0;
}
//#############################################################################
//## Get Request Manager host of cluster (*cluster)
//#############################################################################
char *get_rmserver(char *cluster)
{
  int i;

  for(i=0;i<nclusters;i++)
     if(!strcmp(clusters[i].cluster,cluster))
        return clusters[i].rmhost;
  
  return 0;
}
//#############################################################################
//## Handle the client requests
//#############################################################################
void *handle_client_thread(void *arg)
{
  int csd=(int)arg;
  SPheader packet;

  pthread_detach(pthread_self());
  signal(SIGPIPE,SIG_IGN);     //ignore broken pipe

  if(recv(csd,&packet,sizeof(SPheader),MSG_PEEK)<0)
  {  close(csd);
     return NULL;
  } 
  dprintf("# handle_client_thread: packet.type=%d\n",packet.type);
  switch(packet.type)
  {  case CR_ALLOC:
     {  crvprintf(">>> Allocation Operation: STARTED\n"); 
         am_alloc_client(csd,packet.size);
         crvprintf(">>> Allocation Operation: FINISHED\n"); 
         break;
     }
     case CR_INFO:
     {  crvprintf(">>> Information Operation: STARTED\n");
        am_crinfo(csd,packet.size);
        crvprintf(">>> Information Operation: FINISHED\n");
        break;
     }
     case CR_QVIEW:
     {  crvprintf(">>> Viewer Operation: STARTED\n");
        simple_protocol_rm_ui(CR_QVIEW,packet.size,csd);
        crvprintf(">>> Viewer Operation: FINISHED\n");
        break;
     }
     case CR_RLS:
     {  crvprintf(">>> Release Operation: STARTED\n");
        simple_protocol_rm_ui(CR_RLS,packet.size,csd);
        crvprintf(">>> Release Operation: FINISHED\n");
        break;
     }
     case CR_NODES:
     {  crvprintf(">>> Nodes Operation: STARTED\n");
        simple_protocol_rm_ui(CR_NODES,packet.size,csd);
        crvprintf(">>> Nodes Operation: FINISHED\n");
        break;
     } 
     case CR_NMC_GET_OPERATIONS:
     case CR_NMC_EXEC_OPERATION:
     {  crvprintf(">>> NMC Operation: STARTED\n");
        am_nmc_client(csd,packet.size);
        crvprintf(">>> NMC Operation: FINISHED\n");
        break;
     }
  }
  close(csd);
  return NULL;
}
//#############################################################################
//## Check if *cluster is a valid cluster name.
//## Return id(index) of cluster
//#############################################################################
int is_a_cluster(char *cluster)
{
  int i;

  for(i=0;i<nclusters;i++)
     if( !strcmp(cluster,clusters[i].cluster) )
        return i;
   
  dprintf("%s: invalid cluster name\n",cluster);
  return CR_EAM_INVALID_CLUSTER;
}
//#############################################################################
//## This function implements a simple protocol between User Interface and 
//## Request Manager (for crqview, crrls, crnodes commands) 
//## 
//## The steps are: 1) receive the packet from UI, 2) check access rights,
//##                3) forward the packet to RM, 4) receive packet from RM
//##                5) forward the packet to UI  
//#############################################################################
void simple_protocol_rm_ui(int type,int packetsize,int csd)
{
  int answer=CR_OK, sd, uid;
  void *buf, *offset;
  SPheader packet;
  char *cluster;

  dprintf("# simple_protocol_rm_ui: Packet size=%d\n",packetsize);

  buf=malloc(packetsize);
  if( recv(csd,buf,packetsize,0) < 0)
     return;

  offset=buf+sizeof(SPheader);

  if(type==CR_QVIEW)
  {  memcpy(&uid,offset,sizeof(int));
     offset+=S_PACK_SQVIEW;
  }
  else if(type==CR_NODES)
  {  memcpy(&uid,offset,sizeof(int));
     offset+=S_PACK_SNODES;
  }
  else if(type==CR_RLS)
  {  memcpy(&uid,offset+sizeof(int),sizeof(int)); //get requester id
     offset+=sizeof(SRls);
  }
  
  crvprintf("# simple_protocol_rm_ui: UID=%d\n",uid);

  cluster=(char*)malloc(sizeof(char)*(strlen(offset)+1));
  strcpy(cluster,offset);

  crvprintf("# simple_protocol_rm_ui: cluster=%s userid=%d\n",cluster,uid);
  crvprintf("# simple_protocol_rm_ui: Packet received from AM\n");
 
  if( is_a_cluster(cluster) == CR_EAM_INVALID_CLUSTER )
     answer=CR_EAM_INVALID_CLUSTER;
 
  if( answer == CR_OK)
  {  dprintf("# simple_protocol_rm_ui: access right OK\n");
     answer=(sd=connect_to_rm(cluster));
     if( answer != CR_EAM_CANNOT_CONNECT_RM)
     {  //transfer the queue from RM to UI

        dprintf("# simple_protocol_rm_ui: sending requisition to RM\n");
        if(send(sd, buf, packetsize, 0) < 0 )
        {  close(sd);
           return;
        }
        dprintf("# simple_protocol_rm_ui: waiting answer from RM\n");
        if(recv(sd,&packet,sizeof(SPheader),MSG_PEEK) < 0)
        {  close(sd);
           return;
        }
        dprintf("# simple_protocol_rm_ui: packetsize = %d\n",packet.size);
        free(buf);
        buf=malloc(packet.size);
        crvprintf("# simple_protocol_rm_ui: Receive queue from RM\n");

        if(recv(sd,buf,packet.size,0)>=0)
           send(csd, buf, packet.size, 0);
        crvprintf("# simple_protocol_rm_ui: Queue sent to UI\n");
        close(sd);
        free(buf);
        return;
     }
  }
  dprintf("# simple_protocol_rm_ui: access right is NOT OK [%d]\n",answer);
  packet.size=sizeof(SPheader);
  packet.type=answer;
  send(csd,&packet,sizeof(SPheader),0); //send the answer to UI
  free(buf);
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
