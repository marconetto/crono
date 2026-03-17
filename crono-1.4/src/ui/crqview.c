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

void crqview_getargs(int argc,char **argv,char **conffile,SQview *qview);
void crqview_show_qviewshow(char type, char *cluster,Sqviewshow *qviewshow);
char *crtimet2str(time_t timet);
char *show_share(char share);
char *show_status(int status);
void crqview_usage();
//##############################################################################
//## CRONO QUEUE VIEW
//##############################################################################
int main (int argc, char **argv) 
{
  int port,        //server port
      answer;      //answer from server
  char *conffile=NULL, am_server[MAXHOSTNAMELEN], ui_msg[MAX_UI_MSG];
  SUIPacket packet;
  SQview qview;
  Sqviewshow *qviewshow;

  qview.uid=getuid();
  qview.type=FALSE;
  crqview_getargs(argc,argv,&conffile,&qview);

  if(conffile==NULL)
  {  dprintf("Using default file!: %s\n",DEFAULT_AMCONF);
     ui_getconf_server(DEFAULT_AMCONF,&port,am_server); 
  }
  else 
  {  dprintf("Using %s file\n",conffile);
     ui_getconf_server(conffile,&port,am_server);
  }

  answer=crqview(qview,am_server,port,&packet);
  if(ui_answer2msg(answer,qview.cluster,ui_msg)) //print the answer message
     printf("%s\n",ui_msg);

  if(packet.data!=NULL)
  {
    crqview_set_qviewshow(packet.data,packet.size,qview.type,&qviewshow);
    crqview_show_qviewshow(qview.type,qview.cluster,qviewshow);
    crqview_free_qviewshow(qviewshow);
  }

  return 0;
}
//##############################################################################
//## Parser of command line
//##############################################################################
void crqview_getargs(int argc,char **argv,char **conffile,SQview *qview)
{
  char opcluster=FALSE,opconf=FALSE,optype=FALSE;
  SDefaults defs;

  while (--argc)
  {  argv++;
     if( !strcmp(argv[0],"-c") )
     {  if( !--argc || opcluster)
           crqview_usage();
        else
        {  argv++;
           dprintf("Cluster: %s\n",argv[0]);
           qview->cluster=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(qview->cluster,argv[0]);
           opcluster=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-t") )
     {  if(optype==1)
           crqview_usage();
        else
        {  
           qview->type=TRUE;
           optype=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-f") )
     {  if( !--argc || opconf)
             crqview_usage();
        else
        {  argv++;
           dprintf("Configuration file: %s\n",argv[0]);
           *conffile=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(*conffile,argv[0]);
           opconf=TRUE;
        }
     }
     else crqview_usage();
  }

  //----------------Get the missing parameters-------------------//
  if( !opcluster)
  {  
     if(!crget_defaults(&defs,qview->uid))
         crqview_usage();

     if(defs.cluster[0]==0)
        crqview_usage(); 
     else
     {  qview->cluster=(char*)malloc(sizeof(char)*(strlen(defs.cluster)+1));
        strcpy(qview->cluster,defs.cluster);
     }
  }
}
//##############################################################################
//## Show 'qviewshow' data
//## 'type' -> show or not show the node types
//## 'cluster' -> requested cluster
//##############################################################################
void crqview_show_qviewshow(char type, char *cluster,Sqviewshow *qviewshow)
{
//  void *offset;
  struct tm *tmstr;
  int i;
  char    time_buffer[sizeof("XXX XX hh:mm:ss  ")];

  tmstr= localtime(&(qviewshow)->qview_rminfo.time);
  strftime(time_buffer, sizeof(time_buffer), "%b %d %H:%M:%S", tmstr);

  printf("###################################################################"
         "#############\n");
  printf("## SYSTEM TIME: %-20s                                           #\n",
         time_buffer);
  printf("## CLUSTER: %-20s                                               #\n",
         cluster); 
  printf("###################################################################"
         "#############\n");
  if(type)
    printf("NODETYPE NODES TIME  START               FINISH              STATUS "
         "   SHARE RID\n");
  else
    printf("USER     NODES TIME  START               FINISH              STATUS "
         "   SHARE RID\n");
  printf("======== ===== ===== =================== =================== ======="
         "== ===== ===\n");
  //show the queue
  if((qviewshow)->qview_rminfo.nreqs)
     for(i=0;i<(qviewshow)->qview_rminfo.nreqs;i++)
      printf("%-8s %5d %5d %s %s %-9s %-5s %03d\n",
             (qviewshow)->reqs[i].additinfo, (qviewshow)->reqs[i].nnodes,
             (qviewshow)->reqs[i].time,
             crtimet2str((qviewshow)->reqs[i].tstart),
             crtimet2str((qviewshow)->reqs[i].tfinish),
             show_status((qviewshow)->reqs[i].status),
             show_share((qviewshow)->reqs[i].shared),(qviewshow)->reqs[i].rid);
  //queue has no users
  else
     printf("******** ***** ***** ******************* *******************"
            " ********* ***** ***\n");
  if(type)
  {  printf("\nFREE NODES:\n");
     for(i=0;i<(qviewshow)->ntypes;i++)
       printf("%10s: %3d  exclusive %3d  shared\n",(qviewshow)->nodetypes[i],
              (qviewshow)->freenodes[i].exclusive,
             (qviewshow)->freenodes[i].shared);
  }
  else
     printf("\nFREE NODES: %-3d exclusive\n%12s%-3d shared\n",
            (qviewshow)->qview_rminfo.fexclusive," ",
            (qviewshow)->qview_rminfo.fshare); 
  printf("\n\n");

}
//##############################################################################
//## Time in seconds to time in string
//##############################################################################
char *crtimet2str(time_t timet)
{
  struct tm *t;
  char *buf;

  buf=(char*)malloc(sizeof(char)*20);
  t = localtime(&timet);
  sprintf(buf,"%02d:%02d:%02d %02d/%02d/%04d",t->tm_hour,t->tm_min,t->tm_sec,
               t->tm_mday,t->tm_mon+1,t->tm_year+1900);

  return buf;
}
//##############################################################################
//## Transform status from int to char* form
//##############################################################################
char *show_share(char shared)
{
  if(shared==TRUE) return "on";
  else return "off";
}
//##############################################################################
//## Transform status from int to char* form
//##############################################################################
char *show_status(int status)
{
  switch(status)
  {  case CR_SCHED_STATUS_QUEUED    : return "queued";
     case CR_SCHED_STATUS_READY     : return "ready";
     case CR_SCHED_STATUS_RESERVED  : return "reserved";
     case CR_SCHED_STATUS_MPREPS    : return "mpreproc";
     case CR_SCHED_STATUS_MPOSTPS   : return "mpostproc";
     case CR_SCHED_STATUS_UPREPS    : return "upreproc";
     case CR_SCHED_STATUS_UPOSTPS   : return "upostproc";
     case CR_SCHED_STATUS_BATCH     : return "batchjob";
     case CR_SCHED_STATUS_QUEUED_C  : return "*queued_c";
  }
  return "???";
}
//##############################################################################
//## Usage of the crqview command
//##############################################################################
void crqview_usage()
{
  printf("Crono v%s\n"
          "Usage: crqview -c <cluster> [ -t ] [ -f <file> ]\n"
          "       crqview --help\n\n"
          "\t-c <cluster>\t\t: Cluster name\n"
          "\t-t \t\t\t: Show node types information\n"
          "\t-f <file>\t\t: Configuration file\n"
          "\t--help\t\t\t: Display this help and exit\n\n"
          "%s\n",CRONO_VERSION,USAGE_DEF_FILE);
  exit(1);
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
