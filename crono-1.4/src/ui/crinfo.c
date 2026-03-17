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

void crinfo_getargs(int argc,char **argv,char **conffile,SInforeq *inforeq);
void crinfo_show_info(Sinfoshow infoshow);
void crinfo_usage();
//##############################################################################
//## CRONO CLUSTERS INFORMATION  (CRINFO)
//##############################################################################
int main (int argc, char **argv)
{
  int port,        //server port
      answer,      //answer from server
      n,
      ninfoshow;
  char *conffile=NULL, am_server[MAXHOSTNAMELEN],
       ui_msg[MAX_UI_MSG];
  SInforeq inforeq;
  SUIPacket packet;
  Sinfoshow *infoshow;
  
  inforeq.uid=getuid();
  crinfo_getargs(argc,argv,&conffile,&inforeq);

  if(conffile==NULL)
  {  dprintf("Using default file!\n");
     ui_getconf_server(DEFAULT_AMCONF,&port,am_server); 
  }
  else 
  {  dprintf("Using %s file\n",conffile);
     ui_getconf_server(conffile,&port,am_server);
  }

  dprintf("# main: PORT: %d SERVER: %s\n",port,am_server);
  answer=crinfo(inforeq,am_server,port,&packet);

  if(ui_answer2msg(answer,inforeq.cluster,ui_msg))
     printf("%s\n",ui_msg);
 
  if(packet.data!=NULL) //show information if it exists
  { 
      crinfo_set_infoshow(packet.data,packet.size,&infoshow,&ninfoshow);
      for(n=0;n<ninfoshow;n++)
          crinfo_show_info(infoshow[n]);
     
      //release memory 
      crinfo_free_infoshow(infoshow,ninfoshow);
  }

  return 0;
}
//##############################################################################
//## Parser of command line
//##############################################################################
void crinfo_getargs(int argc,char **argv,char **conffile,SInforeq *inforeq)
{
  char opconf=FALSE,opcluster=FALSE,opall=FALSE;
  SDefaults defs;

  while (--argc)
  {  argv++;
     if( !strcmp(argv[0],"-c") )
     {  if( !--argc || opcluster || opall)
          crinfo_usage();
        else
        {  argv++;
           dprintf("Cluster: %s\n",argv[0]);
           inforeq->cluster=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(inforeq->cluster,argv[0]);
           opcluster=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-f") )
     {  if( !--argc || opconf)
           crinfo_usage();
        else
        {  argv++;
           dprintf("Configuration file: %s\n",argv[0]);
           *conffile=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(*conffile,argv[0]);
           opconf=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-all") )
     {  if( opcluster || opall )
            crinfo_usage();
        else
           opall=TRUE;
     }
     else crinfo_usage();
  }
  //----------------Get the missing command line parameters------------------//
  if(opall)
  {  inforeq->cluster=(char*)malloc(sizeof(char));
     inforeq->cluster[0]=0;
  }
  else if(!opcluster)
  {  crget_defaults(&defs,inforeq->uid);

     if(!defs.cluster[0]) //default get information of all clusters
     {  inforeq->cluster=(char*)malloc(sizeof(char));
        inforeq->cluster[0]=0;
     }
     else
     {  inforeq->cluster=(char*)malloc(sizeof(char)*(strlen(defs.cluster)+1));
        strcpy(inforeq->cluster,defs.cluster);
     }
  }
}
//##############################################################################
//## Show information about user
//##############################################################################
void crinfo_show_info(Sinfoshow infoshow)
{
  int i,j,k;
  char buf[sizeof("XXX XX:XX XX:XX ")];

  printf("SHOW-----------------\n");
  printf("CLUSTER: %s\n",infoshow.cluster);
  printf("NUMBER OF NODES: %d\n",infoshow.nnodes);
  printf("GROUP: %s\n",infoshow.group);
  printf("TYPES:\n%s\n",infoshow.types);
  printf("NODES:\n");
  for(i=0;i<infoshow.ntypes;i++)
     printf("%d:%s\n",infoshow.infotypes[i].nnodes,infoshow.infotypes[i].type);

  printf("\nACCESS RIGHTS:\n");
  printf("Type             Period   Nodes  Time     Reserves\n");
  printf("===============  =======  =====  =======  ========\n");

  for(i=0;i<infoshow.ninfo;i++)
  {  printf("%-16s normal   %5d  %6d   %8d\n",infoshow.info[i].nodetype,
                                              infoshow.info[i].n_nnodes,
                                              infoshow.info[i].n_time,
                                              infoshow.info[i].n_nreserves);

     printf("%-16s special  %5d  %6d   %8d\n",infoshow.info[i].nodetype,
                                              infoshow.info[i].s_nnodes,
                                              infoshow.info[i].s_time,
                                              infoshow.info[i].s_nreserves);
  }
  printf("\n");
  printf(">> Maximum time for allocation now=%d\n",infoshow.usermaxtime);
  printf(">> Maximum number of nodes for allocation now=%d\n",
         infoshow.usermaxnnodes);
  printf("\nSpecial Periods\n");
  printf("=================\n");

  for(i=0,j=0,k=0;i<strlen(infoshow.special);i++,j++)
  { 
     if(infoshow.special[i]=='|') 
     {  buf[k]=0;
        printf("%-17s",buf);
        fflush(stdout);
        k=0;
        if(j>=70)  
        {  j=0;    
           printf("\n");
        }
     }           
     else
       buf[k++]=infoshow.special[i];
  }
  printf("\n");

  printf("========================================");
  printf("========================================\n");
}
//##############################################################################
//## Usage of the crinfo command
//##############################################################################
void crinfo_usage()
{
  printf("Crono v%s\n"
         "Usage: crinfo [ -c <cluster> || -all ] [ -f <file> ]\n"
         "   -c <cluster>\t: Cluster name\n"
         "   -all         : Show information about all clusters "
         " available (default)\n"
         "   -f <file>    : Configuration file\n\n%s\n",CRONO_VERSION,
         USAGE_DEF_FILE);
  exit(1);
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
