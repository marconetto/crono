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
//##############################################################################

#include "common.h"
#include "crui.h"
#include "crui_api.h"
#include <ctype.h>

void add_nodetype(char *buf, char **nodes, int *b,int *alloc_nnodes);
void cralloc_getargs(int argc,char **argv,char **conffile,SAlloc *alloc);
void cralloc_usage();
time_t crstr2timet(char *stime);
char* crtimet2str(time_t timet,char *strtime);
char is_a_number(char *value);
void setup_nodetypes(void **nodes,int *alloc_nnodes,char *file);
//##############################################################################
//## CRONO ALLOCATE NODES (CRALLOC)
//##############################################################################
int main (int argc, char **argv) 
{
  int  port,        //server port
       answer;      //answer from server
  char *conffile=NULL,
       am_server[MAXHOSTNAMELEN],strtime[21],
       ui_msg[MAX_UI_MSG];
       
  SAlloc alloc;

  memset(&alloc,0,sizeof(SAlloc));
  //get user information
  alloc.uid=getuid();
  alloc.requid=getuid();
  alloc.bjscript=NULL;
  alloc.qualitative=FALSE;
  cralloc_getargs(argc,argv,&conffile,&alloc);

  if(alloc.bjscript!=NULL)
  {  if(alloc.bjscript[0]=='/')
     {  alloc.path=(char*)malloc(sizeof(char)*2);
        strcpy(alloc.path,"/");
     }
     else
     {  alloc.path=(char*)malloc(sizeof(char)*(strlen(getenv("PWD"))+1));
        strcpy(alloc.path,getenv("PWD"));
     }
  }  

  if(conffile==NULL)
  {  dprintf("Using default file!\n");
     ui_getconf_server(DEFAULT_AMCONF,&port,am_server); 
  }
  else 
  {  dprintf("Using %s file\n",conffile);
     ui_getconf_server(conffile,&port,am_server);
  }

  answer=cralloc(alloc,am_server,port); //execute the operation

  dprintf("Nnodes=%d Time=%d Cluster=%s bjscript=%s path=%s\n",alloc.nnodes,
           alloc.time, alloc.cluster,alloc.bjscript,alloc.path);
  dprintf("PORT: %d SERVER: %s\n",port,am_server);

  if(ui_answer2msg(answer,alloc.cluster,ui_msg))
      printf("%s\n",ui_msg);
  
  if(answer==CR_RESERVE_TSTART_GREATER_TFINISH)
  {   printf("\nStart time must be greater than final time!\n"
             ">>  Start time : %s\n",crtimet2str(alloc.tstart,strtime));
      printf(">>  Finish time: %s\n\n", crtimet2str(alloc.tfinish,strtime));
      exit(1);
  }

  return 0;
}
//##############################################################################
//## Add the node type (<type>:<nnodes>) in 'buf' to the 'nodes'
//## b-> number of bytes 'nodes' is currently using
//## alloc_nnodes -> amount of nodes the user has requested
//##############################################################################
void add_nodetype(char *buf, char **nodes, int *b,int *alloc_nnodes)
{
  char type[S_LINEF];
  int nnodes;

  if(sscanf(buf,"%[^:]:%d",type,&nnodes)!=2)
  {  printf("Syntax error in node types definitions. Found '%s'. Expected"
            " <type>:<nnodes>\n",buf);
     exit(1);
  }
  dprintf("# add_nodetype: <%s><%d>\n",type,nnodes); 
  *nodes=(char*)realloc((char*)*nodes,*b+sizeof(int)+
                                         sizeof(char)*(strlen(type)+1));
  strcpy(*nodes + *b,type);
  (*b)+=sizeof(char)*(strlen(type)+1);
  
  memcpy(*nodes + *b,&nnodes,sizeof(int));
  (*b)+=sizeof(int); 
  (*alloc_nnodes)+=nnodes;
}
//##############################################################################
//## Parser of command line
//##############################################################################
void cralloc_getargs(int argc,char **argv,char **conffile,SAlloc *alloc)
{
  char opnodes=FALSE,optime=FALSE,opcluster=FALSE,opconf=FALSE,opshare=FALSE,
       opbjs=FALSE,opit=FALSE,opft=FALSE,opuser=FALSE;
  SDefaults defs;

  while (--argc)
  {  argv++;
     if( !strcmp(argv[0],"-n") )
     {  if( !--argc || opnodes )
           cralloc_usage();
        else
        {  argv++;
           dprintf("Nodes: %s\n",argv[0]);
           if(is_a_number(argv[0])) //quantitative allocation
              alloc->nnodes=atoi(argv[0]);
           else //qualitative allocation
           {  setup_nodetypes(&alloc->nodes,&alloc->nnodes,argv[0]);
              alloc->qualitative=TRUE;
           }
           opnodes=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-t") )
     {  if( !--argc || optime || opft)
           cralloc_usage();
        else
        {  argv++;
           dprintf("Time: %s\n",argv[0]);
           alloc->time=atoi(argv[0]);
           if(alloc->time<1)
              ERROR("Invalid time: %s\n",argv[0]);
            optime=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-c") )
     {  if( !--argc || opcluster)
           cralloc_usage();
        else
        {  argv++;
           dprintf("Cluster: %s\n",argv[0]);
           alloc->cluster=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(alloc->cluster,argv[0]);
           opcluster=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-f") )
     {  if( !--argc || opconf)
           cralloc_usage();
        else
        {  argv++;
           dprintf("Configuration file: %s\n",argv[0]);
           *conffile=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(*conffile,argv[0]);
           opconf=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-s") )
     {  if(opshare)
           cralloc_usage();
        else
        {  dprintf("Shared option: %s\n",argv[0]);
           alloc->shared=TRUE;
           opshare=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-e") )
     {  if(opshare)
           cralloc_usage();
        else
        {  dprintf("Shared option: %s\n",argv[0]);
           alloc->shared=FALSE;
           opshare=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-b") )
     {  if( !--argc || opbjs)
           cralloc_usage();
        else
        {  argv++;
           dprintf("Bjscript: %s\n",argv[0]);
           alloc->bjscript=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(alloc->bjscript,argv[0]);
           opbjs=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-it") )
     {  if(!--argc || opit)
           cralloc_usage();
        else
        {  argv++;
           dprintf("Initial Time: %s\n",argv[0]);
           opit=TRUE; 
           alloc->tstart=crstr2timet(argv[0]);
        }
     }    
     else if( !strcmp(argv[0],"-ft") )
     {  if(!--argc || opft || optime)
          cralloc_usage();
        else
        {  argv++;
           dprintf("Final Time: %s\n",argv[0]);
           opft=TRUE; 
           alloc->tfinish=crstr2timet(argv[0]);
        }   
     }
     else if( !strcmp(argv[0],"-u") )
     {  if(!--argc || opuser)
             cralloc_usage();
        else
        {  argv++;
           dprintf("User Name: %s\n",argv[0]);
           if(getuid()!=0)
           {  printf("cralloc: [-u <user>] option is only for root!!!\n");
              cralloc_usage();
           }
           opuser=1;
           if( (alloc->uid=login2uid(argv[0])) == -1)
              ERROR("Invalid user name\n");
        }
    }
    else cralloc_usage();
  }
  //----------------Get the missing parameters-------------------//
  if( !opnodes || !optime || !opcluster || !opshare || !opbjs)
  {  
     if( !crget_defaults(&defs,alloc->requid) && 
         (!opnodes || !opcluster || ( !optime && !opft))
       )
        cralloc_usage();
     if(!opnodes)
     {  if(!defs.nodes[0])
           cralloc_usage();
        else
        {
           if(is_a_number(defs.nodes)) //quantitative allocation
              alloc->nnodes=atoi(defs.nodes);
           else //qualitative allocation
           {  setup_nodetypes(&alloc->nodes,&alloc->nnodes,defs.nodes);
              alloc->qualitative=TRUE;
           }
        }
     }
     if(!optime)
     {  if(defs.time==-1 && (opit==FALSE || opft==FALSE))
           cralloc_usage();
         else
           alloc->time=defs.time;
         optime=TRUE;
     }
     if(!opcluster)
     {  if(!defs.cluster[0])
           cralloc_usage();
        else
        {  alloc->cluster=(char*)malloc(sizeof(char)*
                          (strlen(defs.cluster)+1));
           strcpy(alloc->cluster,defs.cluster);
        }
     }
     if(!opshare)
     {  if(defs.shared==-1)
          alloc->shared=TRUE;
        else
           alloc->shared=defs.shared;
     }
     if(!opbjs)
     {  if(defs.bjscript[0])
        {  alloc->bjscript=(char*)malloc(sizeof(char)*
                           (strlen(defs.bjscript)+1));
           strcpy(alloc->bjscript,defs.bjscript);
        }
     }
  }
  if( opit && (!optime && !opft)) 
     cralloc_usage();

}
//##############################################################################
//## Usage of the cralloc command
//##############################################################################
void cralloc_usage()
{
  printf("Crono v%s\n"
         "Usage:\n" 
             "As soon as possible:\n"
            "\tcralloc -c <cluster> -n <nodes> -t <time> [-s|-e]"
            " [ -b <bjscript> ]\n\t  [ -f <file> ]\n"
            "\nAt a given time:\n"
         "\tcralloc -c <cluster> -n <nodes> -it <itime>"
         " [ -t <time> | -ft <ftime> ]\n\t  [-s|-e] [ -b <bjscript> ]"
         " [ -u <user> ] [ -f <file> ]\n\n"
         "       cralloc --help\n\n"
         "\t-c <cluster>\t\t: Cluster name\n"
         "\t-n <nodes>\t\t: Number of nodes or\n\t\t\t\t"
                          "  a description to make qualitative allocation\n"
         "\t-t <time>\t\t: Time of allocation in minutes\n"
         "\t-s \t\t\t: Shared access\n"
         "\t-e \t\t\t: Exclusive access\n"
         "\t-b <bjscript>\t\t: Batch job script\n"
         "\t-it <itime>\t\t: Initial Time (HH:MM[-dd[/mm[/yyyy]]])\n"
         "\t-ft <ftime>\t\t: Final Time (HH:MM[-dd[/mm[/yyyy]]])\n"
         "\t-u <user>\t\t: User Name (option only for root)\n"
         "\t-f <file>\t\t: Configuration file\n"
         "\t--help\t\t\t: Display this help and exit\n\n"
         "%sThe 'itime' and 'ftime' parameters are exceptions.\n \n",
         CRONO_VERSION,USAGE_DEF_FILE);
  exit(1);
}
//##############################################################################
//## Time(string) to time in seconds
//##############################################################################
time_t crstr2timet(char *stime)
{
  time_t timet;
  struct tm t;
  int n;

  dprintf("# crstr2timet: Changing time format str2time_t\n");
  
  timet=time(NULL);

  memcpy(&t,localtime(&timet),sizeof(struct tm));
  t.tm_sec=0;
  n=sscanf(stime,"%d:%d-%d/%d/%d",&t.tm_hour,&t.tm_min,&t.tm_mday, &t.tm_mon,
                                  &t.tm_year);

  if(n<2 || n>5)
     ERROR("Time %s has a wrong format. Use: HH:MM[-dd[/mm[/yyyy]]]\n",stime);

  if(n>4)
     t.tm_year-=1900;
  if(n>3)
     t.tm_mon--;

  timet=mktime(&t);

  return timet;
}
//##############################################################################
//## Time in seconds (timet) to string (strtime)
//##############################################################################
char* crtimet2str(time_t timet,char *strtime)
{
  struct tm *t;

  t = localtime(&timet);
  sprintf(strtime,"%02d:%02d:%02d %02d/%02d/%04d",t->tm_hour,
             t->tm_min,t->tm_sec,t->tm_mday,t->tm_mon+1,t->tm_year+1900);
  return strtime;
}
//##############################################################################
//## Check whether a string (value) is number
//##############################################################################
char is_a_number(char *value)
{
  int i=0;

  for(i=0;i<strlen(value);i++)
     if(!isdigit(value[i]))
        return FALSE;
  
  return TRUE;
}
//##############################################################################
//## Put the content of the file (which has the lines with <node type>:<nnodes>
//## format) to the 'nodes' variable.
//## The node types can also be passed directally in the command line.
//## The format of nodes is: <n types><node type><nnodes><node type><nnodes>.... 
//## Put the number of the nodes in 'alloc_nnodes' variable.
//## 'input' can be either a file name or a list of node types separed by space
//##     (e.g., "p4:2 p3:4")
//##############################################################################
void setup_nodetypes(void **nodes,int *alloc_nnodes,char *input)
{
  FILE *fp;
  char buf[S_LINEF],*backup_string, *pointer_token;
  int i, ntypes=0,
      b=sizeof(int);   //to control de **nodes size

  dprintf("# setup_nodetypes: start\n");
  *alloc_nnodes=0;       
  *nodes=malloc(sizeof(int));

  //verify if 'input' is a file
  if( (fp=fopen(input,"r"))!=NULL ) 
  {  while( fgets(buf,sizeof(buf),fp) )
     {  i=0;
        //take off spaces, tabs and lines starts with #
        while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
           i++;
        if( (i==strlen(buf))|| (buf[i]=='#') )
           continue;
        add_nodetype(&buf[i],(void*)nodes,&b,alloc_nnodes);
        ntypes++;
     }
     fclose(fp);
  }
  else //user passed the node types in command line instead of using a file
  { 
     //backup original string
     backup_string=(char*)malloc(sizeof(char)*(strlen(input)+1));
     strcpy(backup_string,input);
     pointer_token=strtok(backup_string," ");
     while(pointer_token!=NULL)
     {  
        add_nodetype(pointer_token,(void*)nodes,&b,alloc_nnodes);
        ntypes++;
        pointer_token=strtok(NULL," ");
     }
     free(backup_string);
  }

  if(ntypes<1)
  {  printf("No nodes were define in nodes file (or in command line)\n");
     exit(1);
  }
  memcpy(*nodes,&ntypes,sizeof(int));   

  dprintf("# setup_nodetypes: end\n");
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
