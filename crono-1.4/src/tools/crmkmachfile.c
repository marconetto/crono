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


//#############################################################################
// This is a simple program to create machine files 
//
// You can modify the crmkmachfile_exec() function and 
// *environments[NENVIRONMENTS] variable to add a new environment
//
//##############################################################################
#include "common.h"
#include "crui.h"
#include <dirent.h>
#include <ctype.h>

#define NENVIRONMENTS 3

#define ENV_SIMPLE   0
#define ENV_MPIETHER 1
#define ENV_MPIGM    2

char *environments[NENVIRONMENTS]={"simple","mpiether","mpigm"};

struct
{
  char *cluster,   //cluster name
       *env;       //environment name
  int uid,         //user id
      nprocs,      //number of processes on each machine
      idenv;       //environment id
}Scrmkmachfile;

typedef struct
{
   char *type;   //node type
   char *arch;   //architecture
   int nprocs;   //number of processes 
}SType;

SType *usertypes=NULL;
int nusertypes=0;

void crmkmachfile_delfiles();
void crmkmachfile_exec(int argc,char **argv);
void crmkmachfile_getargs(int argc,char **argv);
void crmkmachfile_getuserdefs();

void show_environments();
char supported_environment(char *env);
//##############################################################################
//## Create machine files
//##############################################################################
int main (int argc, char **argv) 
{
   crmkmachfile_getargs(argc,argv);
   crmkmachfile_getuserdefs();
   crmkmachfile_exec(argc,argv);
   return 0;
}
//#############################################################################
//## Show the crmkmachfile usage
//#############################################################################
void crmkmachfile_usage()
{
  printf("Usage: crmkmachfile -c <cluster> -env <environment>\n"
         "\t\t   [-u <uid> ] -n <type>:<arch>:<nprocs> <node1> <node2> ...\n "
         "\t\t                  <type>:<arch>:<nprocs> <node3> <node4> ...\n"
         "       crmkmachfile -l\n"
         "       crmkmachfile --help\n\n"
         "\t-c <cluster>\t\t: Cluster name\n"
         "\t-env <environment>\t: Environment name\n"
         "\t-u <uid>\t\t: User identification (option only for root)\n"
         "\t-n\t\t\t: List of nodes\n"
         "\t-l\t\t\t: Show the supported environments\n"
         "\t--help\t\t\t: Display this help and exit\n\n");
  exit(1);
}
//##############################################################################
//## Delete machine files of a cluster
//##############################################################################
void crmkmachfile_delfiles()
{
  char file[S_PATH],template[S_PATH],dir[S_PATH],home[S_PATH];
  DIR *dirp;
  struct dirent *direntp;
  char *p;
  
  uid2home(Scrmkmachfile.uid,home);
  sprintf(dir,"%s/.crono/",home);
  sprintf(template,"machfile.%s",Scrmkmachfile.cluster);

  dirp = opendir(dir);
  if(dirp==NULL)
  {   printf("Cannot open /proc directory\n");
      return;
  }

  //delete files in $HOME/.crono that match the template
  //machfile.<cluster>.*.<env>
  while ( (direntp = readdir( dirp )) != NULL )
  {  if(strstr(direntp->d_name,template))
     {  if( (p=strstr(direntp->d_name,Scrmkmachfile.env))!=NULL)
        { if( strlen(p)==strlen(Scrmkmachfile.env))
          {  sprintf(file,"%s%s",dir,direntp->d_name); 
             remove(file);
          }   
        }    
     }
  }
  closedir( dirp );
}
//##############################################################################
//## Parser of command line
//##############################################################################
void crmkmachfile_getargs(int argc,char **argv)
{
  char opcluster=0,opuid=0,opnodes=0,openv=0;

  while (--argc)
  {  argv++;
      
     if( !strcmp(argv[0],"-c") )
     {   argv[0]=NULL;
         if( !--argc || opcluster || opnodes)
             crmkmachfile_usage();
         else
         {  argv++;
            dprintf("Cluster: %s\n",argv[0]);
            Scrmkmachfile.cluster=(char*)malloc(sizeof(char)*
                                               (strlen(argv[0])+1));
            strcpy(Scrmkmachfile.cluster,argv[0]);
            opcluster=1;
            argv[0]=NULL;
         }
     }
     else if( !strcmp(argv[0],"-env") )
     {   argv[0]=NULL;
         if( !--argc || openv || opnodes)
             crmkmachfile_usage();
         else
         {  argv++;
            dprintf("Env: %s\n",argv[0]);
            Scrmkmachfile.idenv=supported_environment(argv[0]);
            if(Scrmkmachfile.idenv==-1)
               exit(1);
            Scrmkmachfile.env=(char*)malloc(sizeof(char)*
                                               (strlen(argv[0])+1));
            strcpy(Scrmkmachfile.env,argv[0]);
            openv=1;
            argv[0]=NULL;
         }
     }
     else if( !strcmp(argv[0],"-u") )
     {   argv[0]=NULL;
         if( !--argc || opuid || opnodes)
             crmkmachfile_usage();
         else
         {  argv++;
            dprintf("Uid: %s\n",argv[0]);
            if(getuid()!=0)
            {   printf("-u: Option available only for root\n");
                exit(1);
            }
            Scrmkmachfile.uid=atoi(argv[0]);
            opuid=1;
            argv[0]=NULL;
         }
     }
     else if( !strcmp(argv[0],"-n") )
     {   argv[0]=NULL;
         if( opnodes)
            crmkmachfile_usage();
         else
           opnodes=1;
     }
     else if( !strcmp(argv[0],"-l") )
     {   show_environments(); 
         exit(1);
     }
  }
  
  //check parameters used
  if( (!opcluster  || !opnodes || !openv) )
     crmkmachfile_usage();

  if(getuid()!=0)
     Scrmkmachfile.uid=getuid();
}
//##############################################################################
//## Create the machine file
//##############################################################################
void crmkmachfile_exec(int argc,char **argv)
{
  FILE *fp;
  char file[S_PATH],home[S_PATH],type[MAXHOSTNAMELEN],arch[S_PATH];
  struct stat stat_buf;
  int i,k,gid,nprocs;

  crmkmachfile_delfiles();  //delete last machine files

  uid2home(Scrmkmachfile.uid,home);
  uid2gid(Scrmkmachfile.uid,&gid);


  //create $HOME/.crono directory if it does not exist
  snprintf(file,S_PATH,"%s/.crono",home);
  if(stat(file,&stat_buf) == -1)   //create the $HOME/.crono 
  {   umask(00000);   //here we should get umask, and set again
      mkdir(file,00755); 
      umask(00022);
      chown(file,Scrmkmachfile.uid,gid);        
  }

  fp=NULL;
  for(i=1;i<argc;i++)
  {  if(argv[i]!=NULL)
     {   if(sscanf(argv[i],"%[^:]:%[^=]=%d",type,arch,&nprocs)==3)
         {   //found a new type of node <type>:<arch>:<nprocs>
            if(fp!=NULL)
            {  fclose(fp); //close current machine file
               chown(file,Scrmkmachfile.uid,gid);
            }
            snprintf(file,S_PATH,"%s/.crono/machfile.%s.%s.%s",home,
                     Scrmkmachfile.cluster,arch,Scrmkmachfile.env);
            //try to get the user definition
            for(k=0;k<nusertypes;k++)
               if(!strcmp(usertypes[k].type,type))
               {   nprocs=usertypes[k].nprocs;
                   break;
               }

            //create a new machine file
            if( (fp=fopen(file,"a+")) == NULL )
                 ERROR("Coundn't create file: %s\n",file);
            dprintf("Creating file %s\n",file);
         }
         else
         {  //put this node nprocs times in machine file
            for(k=0;k<nprocs;k++)
               fprintf(fp,"%s\n",argv[i]);  
         }
     }
  }
  fclose(fp);
  chown(file,Scrmkmachfile.uid,gid);
}
//##############################################################################
//## Get user definitions of the number of processes for each node type 
//##############################################################################
void crmkmachfile_getuserdefs()
{
  char home[S_PATH],file[S_PATH],buf[S_LINEF];
  FILE *fp;
  int nprocs;

  uid2home(Scrmkmachfile.uid,home);
  sprintf(file,"%s/.crono/nodetypes",home);

  if( (fp=fopen(file,"r"))!=NULL)
  {  while(fscanf(fp,"%[^=]=%d",buf,&nprocs)>0)
     {  usertypes=(SType*)realloc(usertypes,sizeof(SType)*(nusertypes+1));
        usertypes[nusertypes].type=(char*)malloc(sizeof(char)*(strlen(buf)+1));
        strcpy(usertypes[nusertypes].type,buf);
        usertypes[nusertypes].nprocs=nprocs;
        nusertypes++;
     }
     fclose(fp);
  }
}
//##############################################################################
//## Show the supported environments
//##############################################################################
void show_environments()
{
  int i;

  for(i=0;i<NENVIRONMENTS;i++)
     printf("%s\n",environments[i]);
  printf("\n");
}
//##############################################################################
//## Check if "env" is a supported environment
//##############################################################################
char supported_environment(char *env)
{
  int i;

  for(i=0;i<NENVIRONMENTS;i++)
  {   if(!strcmp(env,environments[i]))
         return i;
  }
  printf("%s: environment not supported\n",env);
  return -1;
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
