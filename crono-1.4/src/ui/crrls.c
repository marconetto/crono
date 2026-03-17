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

void crrls_getargs(int argc,char **argv,char **conffile,SRls *rls );
void crrls_usage();
//##############################################################################
//## CRONO RELEASE NODES (CRRLS)
//##############################################################################
int main (int argc, char **argv) 
{
  int port,        //server port
      answer;      //answer from server
  char *conffile=NULL, am_server[MAXHOSTNAMELEN],
       ui_msg[MAX_UI_MSG];
  SRls rls;

  //get user information
  rls.uid=getuid();
  rls.requid=getuid();
  rls.rid=0;
  crrls_getargs(argc,argv,&conffile,&rls);

  if(conffile==NULL)
  {  dprintf("Using default file!\n");
     ui_getconf_server(DEFAULT_AMCONF,&port,am_server); 
  }
  else 
  {  dprintf("Using %s file\n",conffile);
     ui_getconf_server(conffile,&port,am_server);
  }
  
  answer=crrls(rls,am_server,port);
  if(ui_answer2msg(answer,rls.cluster,ui_msg)) //print the answer message
     printf("%s\n",ui_msg);
  
  return 0;
}
//##############################################################################
//## Parser of command line
//##############################################################################
void crrls_getargs(int argc,char **argv,char **conffile,SRls *rls)
{
  char opcluster=FALSE,opconf=FALSE,oprid=FALSE,opuser=FALSE;
  SDefaults defs;

  conffile=NULL;
  while (--argc)
  {  argv++;
     if( !strcmp(argv[0],"-c") )
     {  if( !--argc || opcluster)
               crrls_usage();
        else
        {  argv++;
           dprintf("Cluster: %s\n",argv[0]);
           rls->cluster=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(rls->cluster,argv[0]);
           opcluster=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-f") )
     {  if( !--argc || opconf)
           crrls_usage();
        else
        {  argv++;
           dprintf("Configuration file: %s\n",argv[0]);
           *conffile=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
           strcpy(*conffile,argv[0]);
           opconf=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-rid") )
     {  if( !--argc || oprid)
           crrls_usage();
        else
        {  argv++;
           dprintf("Request id: %s\n",argv[0]);
           rls->rid=atoi(argv[0]);
           oprid=TRUE;
        }
     }
     else if( !strcmp(argv[0],"-u") )
     {  if(!--argc || opuser)
           crrls_usage();
        else
        {  argv++;        
           dprintf("User Name: %s\n",argv[0]);
           if(getuid()!=0)
           {  printf("crrls: [-u <user>] option is only for root!!!\n");
              crrls_usage();
           }
           opuser=TRUE;
           if( (rls->uid=login2uid(argv[0])) == -1)
              ERROR("Invalid user name\n");
        }
    }
    else crrls_usage();
  }
  //----------------Get the missing parameters-------------------//

  if( !opcluster)
  {  if(!crget_defaults(&defs,rls->uid))
        crrls_usage();
     if(!defs.cluster[0])
        crrls_usage();
     else
     {  rls->cluster=(char*)malloc(sizeof(char)*(strlen(defs.cluster)+1));
        strcpy(rls->cluster,defs.cluster);
     }
  }
}
//##############################################################################
//## Usage of the crrls command
//##############################################################################
void crrls_usage()
{
  printf("Crono v%s\n"
         "Usage: crrls -c <cluster> [ -rid <rid> ] [ -u <user> ] "
                 "[ -f <file> ] \n"
         "       crrls --help\n\n"
         "\t-c <cluster>\t\t: Cluster name\n"
         "\t-rid <rid>\t\t: Request id (cancel a reserve)\n"
         "\t-u <user>\t\t: User Name (option only for root)\n"
         "\t-f <file>\t\t: Configuration file\n"
         "\t--help\t\t\t: Display this help and exit\n\n"
         "%s\n",CRONO_VERSION,USAGE_DEF_FILE);   
  exit(1);
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
