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

int  crsetdef_delkey(char *keyword, int uid);
void crsetdef_getargs(int argc,char **argv,char **keyword,char **value,int uid);
int  crsetdef_getvalue(char *keyword, char **pvalue, int uid);
void crsetdef_showfile(int uid);
void crsetdef_usage();
void crsetdef_writefile(char *keyword,char *value,int uid);
//##############################################################################
//## CRONO SET DEFAULTS PARAMETERS  (CRSETDEF)
//##############################################################################
int main (int argc, char **argv) 
{
  char *keyword,*value, *am_server,file[S_PATH],home[S_PATH];
  struct stat stat_buf;
  int uid=0,gid;

  am_server=(char*)malloc(sizeof(char)*MAXHOSTNAMELEN);
  uid=getuid();

  snprintf(file,S_PATH,"%s/.crono",uid2home(uid,home));
  if(stat(file,&stat_buf) == -1)  //it has to create the $HOME/.crono directory 
  {  umask(00000);   //here we should get umask, and set again
     mkdir(file,00755);
     umask(00022);
     chown(file,uid,uid2gid(uid,&gid));
  }

  if(argc==1)
  {  crsetdef_showfile(uid);
     exit(1);
  }
  if(argc>3)
      crsetdef_usage();

  crsetdef_getargs(argc,argv,&keyword,&value,uid);
  crsetdef_writefile(keyword,value,uid);
  return 0;
}
//##############################################################################
//## Delete a keywork
//##############################################################################
int crsetdef_delkey(char *keyword, int uid)
{
  FILE *fp1, // $HOME/.crono/defaults (original)
       *fp2; // temp file
  char tmp_path[S_PATH],buf[S_LINEF],buf1[S_LINEF],buf2[S_LINEF];
  char file[S_PATH],home[S_PATH];
  int removed=FALSE,tfile,i;

  uid2home(uid,home);
  snprintf(tmp_path,S_PATH,"%s/.crono/defaults.XXXXXX",home);
  if ( (tfile = mkstemp(tmp_path)) < 0 )
     ERROR("%s: Cannot be created.\n",tmp_path);
  fp2=fdopen(tfile,"w");

  sprintf(file,"%s/.crono/defaults",home);
  if( (fp1=fopen(file,"r")) == NULL)
     ERROR("Problems to open defaults file: %s\n",file);

  while(fgets(buf,S_LINEF,fp1))
  {  buf[strlen(buf)-1]=0;
     i=0;
     while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
        i++;

     if( (i==strlen(buf))|| (buf[i]=='#') )
     {  fputs(&buf[i],fp2);
        continue;
     }
     buf1[0]=0;
     buf2[0]=0;

     sscanf(buf,"%[^=]=%[^NULL]",buf1,buf2);

     if( !strcmp(buf1,keyword))
        removed=TRUE; // does not bypass this line to temp file
     else
     {  buf[strlen(buf)]='\n';
        fputs(buf,fp2);
     }
  }
  fclose(fp1);
  fclose(fp2);
  if(removed==FALSE)
     unlink(tmp_path);
  else
  {  sprintf(file,"%s/.crono/defaults",home);
     rename(tmp_path,file);
  }
  return removed;
}
//##############################################################################
//## Parser of command line
//##############################################################################
void crsetdef_getargs(int argc,char **argv,char **keyword,char **value,int uid)
{
  char *pvalue; //value of a keyword

  if(argc>=2)
  {  if(!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h") ||
       (!strcmp(argv[1],"-d") && argv[2]==NULL) )
         crsetdef_usage();
  }

  if(argc==2)
  {  if(crsetdef_getvalue(argv[1],&pvalue,uid)==TRUE)
        printf("%s\n",pvalue);
     else
        printf("%s: Keyword not defined\n",argv[1]);
      exit(1);
  } 
  else //argc=3
  {  if( !strcmp(argv[1],"-d") )
     {  if(crsetdef_delkey(argv[2],uid)==FALSE)
           printf("%s: Keyword not defined\n",argv[2]);
        exit(1);
     }
     //add or change keyword value
     *keyword=(char*)malloc(sizeof(char)*(strlen(argv[1])+1));
     *value=(char*)malloc(sizeof(char)*(strlen(argv[2])+1));
     strcpy(*keyword,argv[1]);
     strcpy(*value,argv[2]);
  }
}
//##############################################################################
//## Get the value of a keyword
//##############################################################################
int crsetdef_getvalue(char *keyword, char **pvalue, int uid)
{
  int i;
  char file[S_PATH], buf[S_LINEF], buf1[S_LINEF], buf2[S_LINEF], home[S_PATH];
  FILE *fp;

  snprintf(file,S_PATH,"%s/.crono/defaults",uid2home(uid,home));
  if( (fp=fopen(file,"r")) == NULL)
  {  printf("Problems to open file: %s\n",file);
     return FALSE;
  }

  while(fgets(buf,S_LINEF,fp))
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
     if(!strcmp(keyword,buf1))
     {  *pvalue=(char*)malloc(sizeof(char)*(strlen(buf2)+1)); 
        strcpy(*pvalue,buf2);
        fclose(fp);
        return TRUE;
     }
  }
  fclose(fp);
  return FALSE;
}
//##############################################################################
//## Show the keywords and values of ~/.crono/crsetdef 
//##############################################################################
void crsetdef_showfile(int uid)
{
  char buf[S_LINEF], **keywords, file[S_PATH], home[S_PATH];
  FILE *fp;
  int *printed, nprinted=0,nkeywords=0,i;
  char found=0;
  char d_cluster=FALSE, d_time=FALSE, d_shared=FALSE, d_nodes=FALSE,
       d_bjs=FALSE, d_mpirun=FALSE,d_mpicomp=FALSE,d_arch=FALSE; 

  printf("Crono definitions\n===== ===========\n");
  snprintf(file,S_PATH,"%s/.crono/defaults",uid2home(uid,home));
  if( (fp=fopen(file,"r")) == NULL)
  {   printf("cluster=<empty>\n");
      printf("time=<empty>\n");
      printf("nodes=<empty>\n");
      printf("shared=<empty>\n");
      printf("bjscript=<empty>\n");
      printf("mpirun=<empty>\n");
      printf("mpicomp=<empty>\n");
      printf("arch=<empty>\n");
      return;
  }

  keywords=(char**)malloc(sizeof(char*));
  printed=(int*)malloc(sizeof(int));

  while(fgets(buf,sizeof(buf),fp))
  {  keywords=(char**)realloc(keywords,sizeof(char*)*(nkeywords+1));
     printed=(int*)realloc(printed,sizeof(int)*(nkeywords+1));
     keywords[nkeywords]=(char*) malloc(sizeof(char)*(strlen(buf)+1));
     strcpy(keywords[nkeywords++],buf);
     printed[nkeywords-1]=FALSE;
  }
  fclose(fp);

  for(i=0;i<nkeywords;i++) // discovers the available keywords
  {  found=FALSE;
     sscanf(keywords[i],"%[^=]",buf);
     if( !strcmp(buf,"cluster"))
     {  d_cluster=TRUE;
        found=TRUE;
     }
     else if( !strcmp(buf,"time"))
     {  d_time=TRUE;
        found=TRUE;
     }
     else if( !strcmp(buf,"nodes"))
     {  d_nodes=TRUE;
        found=TRUE;
     }
     else if( !strcmp(buf,"shared"))
     {  d_shared=TRUE;
        found=TRUE;
     }
     else if( !strcmp(buf,"bjscript"))
     {  d_bjs=TRUE;
        found=TRUE;
     }
     else if( !strcmp(buf,"mpirun"))
     {  d_mpirun=TRUE;
        found=TRUE;
     }
     else if( !strcmp(buf,"mpicomp"))
     {  d_mpicomp=TRUE;
        found=TRUE;
     }
     else if( !strcmp(buf,"arch"))
     {  d_arch=TRUE;
        found=TRUE;
     }

     if(found)
     {  printed[i]=TRUE;
        printf("%s",keywords[i]);
        nprinted++;
     }
  }//for
  if(!d_cluster) printf("cluster=<empty>\n");
  if(!d_time)    printf("time=<empty>\n");
  if(!d_nodes)   printf("nodes=<empty>\n");
  if(!d_shared)  printf("shared=<empty>\n");
  if(!d_bjs)     printf("bjscript=<empty>\n");
  if(!d_mpirun)  printf("mpirun=<empty>\n");
  if(!d_mpicomp) printf("mpicomp=<empty>\n");
  if(!d_arch)    printf("arch=<empty>\n");

  if(nprinted<nkeywords)
  {  printf("\nUser definitions\n==== ===========\n");
     for(i=0;i<nkeywords;i++)
       if(!printed[i])
           printf("%s",keywords[i]);
  }
}
//##############################################################################
//## Usage of the crsetdef command
//##############################################################################
void crsetdef_usage()
{
  printf("Crono v%s\n"
         "Usage: crsetdef [ <keyword> [ <value> ] ] | -d <keyword> | "
         "--help\n\n"
         "\t<keyword>         : Shows the keyword from defaults file\n"
         "\t<keyword> <value> : If keyword exists, set a new value. If not,\n"
         "                            add a new entry in file\n"
         "\t-d <keyword>      : Remove the line that contains keyword\n\n",
         CRONO_VERSION);
  exit(1);
}
//##############################################################################
//## Put 'keyword' and 'value' to the defaults file
//##############################################################################
void crsetdef_writefile(char *keyword,char *value,int uid)
{
  FILE *fp1, // $HOME/.crono/defaults (original)
       *fp2; // temp file
  char tmp_path[S_PATH],buf[S_LINEF],buf1[S_LINEF],buf2[S_LINEF];
  char file[S_PATH],home[S_PATH];
  int tfile, i, replaced=FALSE;

  uid2home(uid,home);
  snprintf(tmp_path,S_PATH,"%s/.crono/defaults.XXXXXX",home);
  if ( (tfile = mkstemp(tmp_path)) < 0 )
     ERROR("%s: Cannot be created.\n",tmp_path);

  dprintf("Temporary file created : %s\n",tmp_path);
  fp2=fdopen(tfile,"w");

  sprintf(file,"%s/.crono/defaults",home);
  if( (fp1=fopen(file,"a+")) == NULL)
     ERROR("Error opening file %s\n",file);

  rewind(fp1);
  while(fgets(buf,sizeof(buf),fp1))
  {  buf[strlen(buf)-1]=0; // take off CR+LF
     i=0;

     while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
        i++;
     if( (i==strlen(buf))|| (buf[i]=='#') )
     {  fputs(&buf[i],fp2);  
        continue;
     }
     buf1[0]=0;
     buf2[0]=0;

     sscanf(&buf[i],"%[^=]=%[^NULL]",buf1,buf2);

     if( !strcmp(buf1,keyword)) //replace value of a keyword
     {  snprintf(buf,S_LINEF,"%s=%s\n",keyword,value);
        fputs(buf,fp2);
        replaced=TRUE;
     }
     else
     {  buf[strlen(buf)]='\n'; // CR+LF back again
        fputs(buf,fp2);
     }
  }
  if(replaced==FALSE)
  {  snprintf(buf,S_LINEF,"%s=%s\n",keyword,value);
     fputs(buf,fp2);
  }
  fclose(fp1);
  fclose(fp2);
  sprintf(file,"%s/.crono/defaults",home);
  rename(tmp_path,file);
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
