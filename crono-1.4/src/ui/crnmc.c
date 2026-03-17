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

void crnmc_add_node(char *node,SNmc *nmc,int *sizenodes);
void crnmc_add_list_nodes (char *nodes,SNmc *nmc);
void crnmc_getargs(int argc,char **argv,char **conffile,SNmc *nmc);
void crnmc_load_machine_file(char *file,SNmc *nmc);
void crnmc_show_commands(void *data,int size);
void crnmc_show_exec(void *data,int size);   
void crnmc_usage();
//##############################################################################
//## CRONO NODE MANAGER CLIENT
//##############################################################################
int main (int argc, char **argv) 
{
  int port,        //server port
      answer;      //answer from server
  char *conffile=NULL, am_server[MAXHOSTNAMELEN], ui_msg[MAX_UI_MSG];
  SUIPacket packet;
  SNmc nmc;

  nmc.nodes=(char*)malloc(sizeof(char));
  nmc.nodes[0]=0;

  nmc.operation=NULL;
  nmc.uid=getuid();
  crnmc_getargs(argc,argv,&conffile,&nmc);

  if(conffile==NULL)
  {  dprintf("Using default file!\n");
     ui_getconf_server(DEFAULT_AMCONF,&port,am_server); 
  }
  else 
  {  dprintf("Using %s file\n",conffile);
     ui_getconf_server(conffile,&port,am_server);
  }

  answer=crnmc(nmc,am_server,port,&packet);
  if(answer==CR_EAM_INVALID_OPERATION)
     ERROR("%s: Invalid operation\n",nmc.operation);
  else
  {  if(ui_answer2msg(answer,nmc.cluster,ui_msg))
       printf("%s\n",ui_msg);
  }     

  if(nmc.oper==CR_NMC_GET_OPERATIONS && packet.data!=NULL)
     crnmc_show_commands(packet.data,packet.size);

  if(nmc.oper==CR_NMC_EXEC_OPERATION && packet.data!=NULL)
     crnmc_show_exec(packet.data,packet.size);

  return 0;
}
//##############################################################################
//## Add node from command line or from crnmc_load_machine_file(...) to 
//## nmc->nodes
//##############################################################################
void crnmc_add_node(char *node,SNmc *nmc,int *sizenodes)
{
  dprintf("# crnmc_add_node: '%s' sizenodes=%d\n",node,*sizenodes);
  nmc->nodes=(char*)realloc(nmc->nodes,*sizenodes+
                                        sizeof(char)*(strlen(node)+2));
  sprintf(nmc->nodes+*sizenodes,"%s|",node);
  *sizenodes+=sizeof(char)*(strlen(node)+1);
  dprintf("# crnmc_add_node:NODES=\"%s\" sizenodes=%d\n",nmc->nodes,*sizenodes);
}
//##############################################################################
//## Parser of command line
//##############################################################################
void crnmc_getargs(int argc,char **argv,char **conffile,SNmc *nmc)
{
  char opnodes=FALSE, opcluster=FALSE, opconf=FALSE, opmfile=FALSE, opall=FALSE,
       opop=FALSE, opv=FALSE;
  SDefaults defs;

  while (--argc)
  {  argv++;
     if( !strcmp(argv[0],"-all") )
     {  if( opall || opnodes || opmfile || opv)
           crnmc_usage();
        else
        {  dprintf("All nodes: %s\n",argv[0]);
           opall=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-n") )
     {  if(!--argc || opall || opnodes || opmfile || opv)
           crnmc_usage();
        else
        { argv++;
          dprintf("Nodes: %s\n",argv[0]);
           crnmc_add_list_nodes(argv[0],nmc);
           opnodes=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-mf") )
     {  if( !--argc || opall || opnodes || opmfile || opv)
           crnmc_usage();
        else
        {  argv++;
           dprintf("Machine file: %s\n",argv[0]);
           crnmc_load_machine_file(argv[0],nmc);            
           opmfile=TRUE;

        }
     }
     else if( !strcmp(argv[0],"-c") )
     {  if( !--argc || opcluster)
           crnmc_usage();
        else
        {  argv++;
           dprintf("Cluster: %s\n",argv[0]);
           nmc->cluster=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(nmc->cluster,argv[0]);
           opcluster=TRUE;

        }
     }
     else if( !strcmp(argv[0],"-f") )
     {  if( !--argc || opconf)
           crnmc_usage();
        else
        {  argv++;
           dprintf("Configuration file: %s\n",argv[0]);
           *conffile=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(*conffile,argv[0]);
           opconf=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-op") )
     {  if(!--argc || opop)
           crnmc_usage();
        else
        {  argv++;
           dprintf("Operation: %s\n",argv[0]);
           nmc->operation=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(nmc->operation,argv[0]);
           nmc->oper=CR_NMC_EXEC_OPERATION;
           opop=TRUE;

        }
     }
     else if( !strcmp(argv[0],"-v") )
     {  if(opv || opall || opnodes || opmfile )
           crnmc_usage();
        else
        {  dprintf("Operation: %s\n",argv[0]);
           opv=TRUE;
           nmc->oper=CR_NMC_GET_OPERATIONS;
        }
     }
     else
       crnmc_usage();
             
  }//while

  //----------------Get the missing parameters-------------------//
  if(!opcluster)
  {  if(!crget_defaults(&defs,nmc->uid))
        crnmc_usage();
  
     if(defs.cluster[0]==0)
        crnmc_usage();
     else
     {  nmc->cluster=(char*)malloc(sizeof(char)*(strlen(defs.cluster)+1));
        strcpy(nmc->cluster,defs.cluster);
        opcluster=TRUE;
     }
  }
  if( !( opcluster && (opnodes || opall || opmfile ) && opop ) &&
      !( opcluster && opv) )
     crnmc_usage();
  nmc->all=opall;
}
//##############################################################################
//## Add the list of the nodes ("node1 node2 ...noden" format) to nmc->nodes
//##############################################################################
void crnmc_add_list_nodes (char *nodes,SNmc *nmc)
{
   int i=0, sizenodes=0; 
   char buf[MAXHOSTNAMELEN];

   while(i<strlen(nodes))
   {  sscanf(&nodes[i],"%s",buf);
      i+=strlen(buf)+1;
      crnmc_add_node(buf,nmc,&sizenodes);
   }
}
//##############################################################################
//## Add nodes to nmc->nodes from machine file
//##############################################################################
void crnmc_load_machine_file(char *file,SNmc *nmc)
{
  FILE *fp;
  char buf[S_LINEF];
  int sizenodes=0;

  if( (fp=fopen(file,"r")) == NULL ) 
    ERROR("%s: Cannot open machine file\n",file); 

  while( fscanf(fp,"%s",buf) > 0 )
    crnmc_add_node(buf,nmc,&sizenodes);
  fclose(fp);
}
//##############################################################################
//## Receive available operations 
//##############################################################################
void crnmc_show_commands(void *data,int size)
{
  void *offset;

  offset=data;
  size-=sizeof(SPheader);

  while(offset<data+size)
  {  printf("Operation: %s\n",(char*)offset);
     offset+=sizeof(char)*(strlen(offset)+1);
  }
}
//##############################################################################
//## Execute command on nodes 
//##############################################################################
void crnmc_show_exec(void *data,int size)
{
  dprintf("Execute Command\n");
  printf("%s\n",(char*)data);  
}
//##############################################################################
//## Usage of crnmc
//##############################################################################
void crnmc_usage()
{
  printf("Crono v%s\n"
         "Usage:\n\tcrnmc -c <cluster> <-all | -mf <mach> | -n \"<node1> "
          "<node2> ...\" > -op <operation> [-f <file>]\n"
          "\tcrnmc -c <cluster> -v [-f <file> ]\n\n"
          "\t-c <cluster>\t\t: Cluster name\n"
          "\t-all\t\t\t: All allowed nodes\n"
          "\t-mf\t\t\t: Machine file\n"
          "\t-n\t\t\t: Nodes\n"
          "\t-op <operation>\t\t: Operation to execute\n"
          "\t-v\t\t\t: View operations\n"
          "\t-f <file>\t\t: Configuration file\n\n",CRONO_VERSION);
  exit(1);
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
