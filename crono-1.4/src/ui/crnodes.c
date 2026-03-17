//##############################################################################
//# Copyright (C) 2002-2004 Marco Aurelio Stelmar Netto <stelmar@cpad.pucrs.br>
//# Copyright (C) 2002 Alex de Vargas Barcelos <alexb@pucrs.br>
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
#include "common.h"
#include "crui.h"
#include "crui_api.h"

#define SEL_TYPE   1  //node type
#define SEL_ARCH   2  //architecture
#define SEL_NPROCS 3  //number of processes
#define SEL_DESC   4  //description
#define SEL_ALL    5  //all information
#define NSEL       5  //number of selections

void crnodes_getargs(int argc,char **argv,char **conffile,SNodes *nodes,
                     int **sel,int* nsel);
void crnodes_shownodes(void *usernodes,int packetsize,char oper,
                       int *sel,int nsel);
void crnodes_usage();
int  info2id(char *info);
int setup_selection(int **sel, char *selections);
//##############################################################################
//## CRONO LIST NODES (CRNODES)
//##############################################################################
int main (int argc, char **argv) 
{
  int port,        //server port
      answer,      //answer from server
      nsel=0,      //number of selections
      *sel=NULL;   //the selections
  char *conffile=NULL, am_server[MAXHOSTNAMELEN], ui_msg[MAX_UI_MSG];
  SNodes nodes;
  SUIPacket packet;

  //get user information
  nodes.uid=getuid();
  crnodes_getargs(argc,argv,&conffile,&nodes,&sel,&nsel);
  
  if(conffile==NULL)
  {  dprintf("Using default file!\n");
     ui_getconf_server(DEFAULT_AMCONF,&port,am_server); 
  }
  else 
  {  dprintf("Using %s file\n",conffile);
     ui_getconf_server(conffile,&port,am_server);
  }

  answer=crnodes(nodes,am_server,port,&packet);
  if(ui_answer2msg(answer,nodes.cluster,ui_msg))  //print the answer message
     printf("%s\n",ui_msg);

  if(packet.data!=NULL) //show the nodes if they are available
     crnodes_shownodes(packet.data,packet.size,nodes.oper,sel,nsel); 

  printf("\n\n");
  return 0;
}
//##############################################################################
//## Parser of command line
//##############################################################################
void crnodes_getargs(int argc,char **argv,char **conffile,SNodes *nodes,
                    int **sel,int* nsel)
{
  char opcluster=FALSE,opconf=FALSE,opsel=FALSE,opwho=FALSE;
  SDefaults defs;

  while (--argc)
  {  argv++;
     if( !strcmp(argv[0],"-c") )
     {  if( !--argc || opcluster)
           crnodes_usage();
        else
        {  argv++;
           dprintf("Cluster: %s\n",argv[0]);
           nodes->cluster=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(nodes->cluster,argv[0]);
           opcluster=1;
        }
     }
     else if( !strcmp(argv[0],"-f") )
     {  if( !--argc || opconf)
           crnodes_usage();
        else
        {  argv++;
           dprintf("Configuration file: %s\n",argv[0]);
           *conffile=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(*conffile,argv[0]);
           opconf=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-s") )
     {  if( !--argc || opsel)
             crnodes_usage();
        else
        {  argv++;
           *nsel=setup_selection(sel,argv[0]);
           opsel=TRUE; 
        }
     }
     else if( !strcmp(argv[0],"-w") )
        opwho=TRUE; 
     else 
        crnodes_usage();
  }
  if(!opcluster)
  {  if(!crget_defaults(&defs,nodes->uid))
        crnodes_usage();
     if(defs.cluster[0]==0)
        crnodes_usage();
     else
     {  nodes->cluster=(char*)malloc(sizeof(char)*(strlen(defs.cluster)+1));
        strcpy(nodes->cluster,defs.cluster);
     }
  }

  if(opsel && opwho)
     nodes->oper=CRNODES_ALLNODES_INFO;
  else if(!opsel && opwho)
     nodes->oper=CRNODES_ALLNODES;
  else if(opsel && !opwho)
     nodes->oper=CRNODES_USERNODES_INFO;
  else
     nodes->oper=CRNODES_USERNODES; 
}
//##############################################################################
//## Show the list of the nodes
//##############################################################################
void crnodes_shownodes(void *usernodes,int packetsize,char oper,int *sel,
                       int nsel)
{
  void *offset;
  char host[S_LINEF],type[S_LINEF],arch[S_LINEF],nprocs[S_LINEF],desc[S_LINEF],
       who[S_LINEF];
  int i=0;

  offset=usernodes;
  while(offset<usernodes-sizeof(SPheader)+packetsize-sizeof(char))
  {  
     if(oper==CRNODES_USERNODES_INFO)
     {  sscanf(offset,"%[^:]:%[^:]:%[^:]:%[^:]:\"%[^\"]",host,type,arch,
                      nprocs, desc);
        printf("%s",host);
     }
     else if(oper==CRNODES_ALLNODES_INFO)
     {  sscanf(offset,"%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:\"%[^\"]",host,who,type,
                      arch,nprocs,desc);
        printf("%s:%s",host,who);
     }
     if(oper==CRNODES_USERNODES_INFO || oper==CRNODES_ALLNODES_INFO)
     { 
        for(i=0;i<nsel;i++)
        {  if(sel[i]==SEL_ALL)
              printf(":type=%s:arch=%s:nprocs=%s:desc=\"%s\"",type,arch,nprocs,
                                                              desc);
           else if(sel[i]==SEL_TYPE)
              printf(":%s",type);
           else if(sel[i]==SEL_ARCH)
              printf(":%s",arch);
           else if(sel[i]==SEL_NPROCS)
              printf(":%s",nprocs);
           else if(sel[i]==SEL_DESC)
              printf(":%s",desc);
        }
        if(oper==CRNODES_USERNODES_INFO)
           offset+=sizeof(char)*(strlen(host)+strlen(type)+strlen(arch)+
                                 strlen(nprocs)+strlen(desc)+7);
        else
           offset+=sizeof(char)*(strlen(host)+strlen(who)+strlen(type)+
                                 strlen(arch)+strlen(nprocs)+strlen(desc)+8);
        printf("\n");
     }
     else
     {  if(*(char*)offset!=' ')  
           printf("%c",*(char*)offset);
        else
           printf("\n");
        offset+=sizeof(char);
     }
  }
}
//##############################################################################
//## Usage of crnodes
//##############################################################################
void crnodes_usage()
{
  printf("Crono v%s\n"
         "Usage: crnodes -c <cluster> [ -w ] [ -s \"<sel> <sel> ... <sel>\" ]"
         " [ -f <file> ]\n"
         "       crnodes --help\n\n"
         "\t-c <cluster>\t\t: Cluster name\n"
         "\t-w\t\t\t: Show all cluster nodes and who is using\n"
                      "\t\t\t\t  each of them\n"
         "\t-s <sel>...\t\t: Select nodes information\n"
                     "\t\t\t\t  (sel='nprocs','type','arch','desc' or 'all')\n"
         "\t-f <file>\t\t: Configuration file\n"
         "\t--help\t\t\t: Display this help and exit\n\n"
         "%s\n",CRONO_VERSION,USAGE_DEF_FILE);    
  exit(1);
}
//##############################################################################
//## Information to id
//##############################################################################
int info2id(char *info)
{
  int i;

  for(i=0;i<NSEL;i++)
  {  if(!strcmp(info,"type"))
       return SEL_TYPE; 
     if(!strcmp(info,"arch"))
        return SEL_ARCH;
     if(!strcmp(info,"nprocs"))
        return SEL_NPROCS; 
     if(!strcmp(info,"desc"))
        return SEL_DESC;
     if(!strcmp(info,"all"))
        return SEL_ALL; 
  }
  printf("%s: invalid information selection\n",info);
  exit(1);
}
//##############################################################################
//## Setup vector with selections 'sel' using the string 'selections', which
//## has the "<sel> <sel>..." format. Ex.: crnodes -s "arch nprocs"
//##############################################################################
int setup_selection(int **sel, char *selections)
{
  int i=0,
      nsel=0; //number of selections 
  char buf[S_LINEF];
  
  while(i<strlen(selections))
  {  sscanf(&selections[i],"%s",buf);
     i+=strlen(buf)+1;
     *sel=(int*)realloc(*sel,sizeof(int)*(nsel+1));
     (*sel)[nsel]=info2id(buf);
     nsel++;
  }
  return nsel;
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
