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
//##############################################################################
//## Get the AM port and hostname(am_server) from the configuration file
//##############################################################################
void ui_getconf_server(char *file,int *port,char *am_server)
{
  FILE *fp;
  char buf[S_LINEF], parser;
  int i;

  if( (fp=fopen(file,"r")) == NULL )
  {  printf("Problems to open AM config file: %s\n",file);
     exit(1);
  }

  while(fgets(buf,S_LINEF,fp))
  {  buf[strlen(buf)-1]=0;
     i=0;
    
     //take off blank lines, spaces, tabs and commented lines 
     while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
        i++;
     if( (i==strlen(buf))|| (buf[i]=='#') )
        continue;
     if( sscanf(&buf[i],"%s %d %c",am_server,port,&parser) != 2)
        ERROR("Error in configure file: %s\n",file);
   }
   fclose(fp);
}
//##############################################################################
//## Get the default parameters of a user 'uid' and put it in 'defs'
//##############################################################################
char crget_defaults(SDefaults *defs, int uid)
{
  int i;
  //parser flags
  char p_shared=FALSE, p_time=FALSE, p_nodes=FALSE, p_cluster=FALSE,
      parser_ok=TRUE, p_bjs=FALSE,p_arch=FALSE;
  char file[S_PATH], buf[S_LINEF], buf1[S_LINEF], buf2[S_LINEF], home[S_PATH];
  FILE *fp;
  
  defs->time=-1;
  defs->shared=-1;
  defs->cluster=(char*)calloc(1,sizeof(char));
  defs->nodes=(char*)calloc(1,sizeof(char));
  defs->bjscript=(char*)calloc(1,sizeof(char));
  defs->arch=(char*)calloc(1,sizeof(char));

  snprintf(file,S_PATH,"%s/.crono/defaults",uid2home(uid,home));
  if( (fp=fopen(file,"r")) == NULL)
  {  printf("Warning: missing command parameters and\n"
            "the defaults file is not available: %s\n",file);
     return FALSE;
  }
  dprintf("Getting missing parameters in default file.\n");

  while( fgets(buf,S_LINEF,fp))
  {  buf[strlen(buf)-1]=0;
     i=0;
     //take off blank lines, spaces, tabs and commented lines 
     while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
        i++;
     if( (i==strlen(buf))|| (buf[i]=='#') )
       continue;

     buf1[0]=0;
     buf2[0]=0;
     sscanf(&buf[i],"%[^=]=%[^NULL]",buf1,buf2);

     if(!strcasecmp(buf1,"shared"))
     {   if(!p_shared)
        {  if(buf2[0]=='n')
              defs->shared=FALSE;
           else if(buf2[0]=='y')
              defs->shared=TRUE;
           else
           {  parser_ok=FALSE;
              printf("%s: Share option must be y|n\n",file);
              break;
           }
           p_shared=TRUE;
        }
        else
           cr_file_error(fp,"%s: Share mode was already defined\n",file);
     }    
     else if(!strcasecmp(buf1,"time"))
     {  if(!p_time)
        {  defs->time=atoi(buf2);
           if(defs->time<=0)
           {  parser_ok=FALSE;
              printf("%s: Time option must be a greater than zero\n",file);
              break;
           }
           p_time=TRUE;
        }
        else
           cr_file_error(fp,"%s: Time was already defined\n",file);
     }   
     else if(!strcasecmp(buf1,"nodes"))
     {  if(!p_nodes)
        {  defs->nodes=(char*)malloc(sizeof(char)*(strlen(buf2)+1));
           strcpy(defs->nodes,buf2);
           p_nodes=TRUE;
        }
        else
           cr_file_error(fp,"%s: Nodes was already defined\n",file);
     }   
     else if(!strcasecmp(buf1,"cluster"))
     {  if(!p_cluster)
        {  defs->cluster=(char*)malloc(sizeof(char)*(strlen(buf2)+1)); 
           strcpy(defs->cluster,buf2);
           p_cluster=TRUE;
        }
        else
           cr_file_error(fp,"%s: Cluster was already defined\n",file);
     }   
     else if(!strcasecmp(buf1,"bjscript"))
     {  if(!p_bjs)
        {  defs->bjscript=(char*)malloc(sizeof(char)*(strlen(buf2)+1)); 
           strcpy(defs->bjscript,buf2);
           p_bjs=TRUE;
        }
        else
          cr_file_error(fp,"%s: Batch job script was already defined\n",file);
     }
     else if(!strcasecmp(buf1,"arch"))
     {  if(!p_arch)
        {  defs->arch=(char*)malloc(sizeof(char)*(strlen(buf2)+1)); 
           strcpy(defs->arch,buf2);
           p_arch=TRUE;
        }
        else
          cr_file_error(fp,"%s: Architecture was already defined\n",file);
     }   
  }
  fclose(fp);

  return parser_ok;
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
