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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_BUF1 256
#define FALSE 0
#define TRUE 1

typedef struct
{
   char *arch;
   char *type;
   int nprocs;
   int nhosts;
   char **hosts;
}SData;

SData *data=NULL;
int ndata=0;

void add_to_data(char *host,char *type, char * arch, int nprocs);
void crnodesinfofilter_usage();
char type_exists(char *type);
//##############################################################################
//## Create machine files
//##############################################################################
int main (int argc, char **argv) 
{
  int i,j,nprocs; 
  char archbuf[STR_BUF1];
  char typebuf[STR_BUF1];
  char hostbuf[STR_BUF1];
 
  if(argc<2)
    crnodesinfofilter_usage();
  
  for(i=1;i<argc;i++)
  {  sscanf(argv[i],"%[^:]:%[^:]:%[^:]:%d:%*s",hostbuf,typebuf,archbuf,&nprocs);
     add_to_data(hostbuf,typebuf,archbuf,nprocs);
  } 

  for(i=0;i<ndata;i++) //show data
  {  printf("%s:%s=%d ",data[i].type,data[i].arch,data[i].nprocs);
     for(j=0;j<data[i].nhosts;j++)
        printf("%s ",data[i].hosts[j]);
  }
  
  return 0;
}
//#############################################################################
//## Add data to the 'data' strcuture
//#############################################################################
void add_to_data(char *host,char *type, char * arch, int nprocs)
{
  int i;

  for(i=0;i<strlen(type);i++)
     if(type[i]==',')
     {  type[i]=0;
         break;
     }

  if(!type_exists(type))
  {  data=(SData*)realloc(data,sizeof(SData)*(ndata+1));
     data[ndata].type=(char*)malloc(sizeof(char)*(strlen(type)+1));
     strcpy(data[ndata].type,type);
     data[ndata].arch=(char*)malloc(sizeof(char)*(strlen(arch)+1));
     strcpy(data[ndata].arch,arch);
     data[ndata].nprocs=nprocs;
     data[ndata].hosts=NULL;
     data[ndata].nhosts=0;
     ndata++;
  }
  //add node
  for(i=0;i<ndata;i++)
  {  if(!strcmp(data[i].type,type))
       break;
  } 
  data[i].hosts=(char**)realloc(data[i].hosts,sizeof(char*)*(data[i].nhosts+1));
  data[i].hosts[data[i].nhosts]=(char*)malloc(sizeof(char)*(strlen(host)+1));
  strcpy(data[i].hosts[data[i].nhosts],host);
  data[i].nhosts++;
  
}
//#############################################################################
//## Show the crnodesinfofilter usage
//#############################################################################
void crnodesinfofilter_usage()
{
   printf("Usage: crnodesinfofilter <cr_nodesinfo var>\n"
          "\t<cr_nodesinfo var>\t: $CR_NODESINFO variable\n\n");
   exit(1);
}
//##############################################################################
//## Check of 'type' exists
//##############################################################################
char type_exists(char *type)
{
   int i;

   for(i=0;i<ndata;i++)
     if(!strcmp(type,data[i].type))
        return TRUE;
   return FALSE;
} 
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$


