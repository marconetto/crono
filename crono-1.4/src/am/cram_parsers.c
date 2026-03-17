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

//##############################################################################
//## Functions responsible for parsing the configuration files
//##############################################################################
#include "common.h"
#include "cramd.h"
#include "cram_parsers.h"
//##############################################################################
//## Add 'nodetype' with 'tmp_nnodes' nodes  to the 'typennodes' structure.
//## '*ntypes' is the number of nodetypes of the 'typennodes' structure.
//##############################################################################
void add_quant_alloc_nodetype(STypennodes **typennodes,int *ntypes,
                              char *nodetype, int tmp_nnodes,int *totalnodes)
{
  int i;
  char *node_type_pointer=NULL,*tmp_node_type,*tmp_pointer,first=TRUE;
  char *ptrptr=NULL;

  dprintf("# add_quant_alloc_nodetype: nodetype = [%s]\n",nodetype);

  tmp_node_type=(char*)malloc(sizeof(char)*(strlen(nodetype)+1));
  strcpy(tmp_node_type,nodetype);
  tmp_pointer=tmp_node_type;

  do //for each type in node_type (ex.: "e800,e60,piii")
  {  if(first)
        node_type_pointer=strtok_r(tmp_node_type,",",&ptrptr);
     else
        node_type_pointer=strtok_r(NULL,",",&ptrptr);
     if(node_type_pointer!=NULL)
     {  for(i=0;i<*ntypes;i++)
          if(!strcmp((*typennodes)[i].type,node_type_pointer))
             break;
        if(i==*ntypes)//not found
        { (*typennodes)=(STypennodes*)realloc(*typennodes,
                                       sizeof(STypennodes)*(*ntypes+1));
           (*typennodes)[*ntypes].type=(char*)malloc(sizeof(char)*
                                             (strlen(node_type_pointer)+1));
           strcpy((*typennodes)[*ntypes].type,node_type_pointer);
           (*typennodes)[*ntypes].nnodes=tmp_nnodes;
           (*ntypes)++;
           (*totalnodes)+=tmp_nnodes;
        }
     }
     first=FALSE;
  }while(node_type_pointer!=NULL);

  free(tmp_pointer);
}
//##############################################################################
//## Add a special period to the list of special periods
//##############################################################################
void am_add_special_info(char *weekday,char *itime,char *ftime,char **buf)
{
  *buf=realloc(*buf,sizeof(char)*(strlen(*buf)+strlen(weekday)+strlen(itime)+
                                 strlen(ftime)+6));
                                 //6 = 4 ' ' + 1 '|' + \0   (below)
  sprintf(*buf+sizeof(char)*strlen(*buf)," %s %s %s |",weekday,itime,ftime);
}
//##############################################################################
//## Check the access right of a particular node type
//## That is, verify if it is possible to allocate X nodes (typennodes)
//## of 'nodetype' based on 'mna' variable
//##
//##############################################################################
char aright_nodetype(char *nodetype,STypennodes **typennodes,int ntypes,int mna)
{
  int i, found=-1;
  char *node_type_pointer=NULL,*tmp_node_type,*tmp_pointer,first;
  char *ptrptr;

  dprintf("# aright_nodetype: node_type <%s> mna=%d\n",nodetype,mna);

  tmp_node_type=(char*)malloc(sizeof(char)*(strlen(nodetype)+1));
  strcpy(tmp_node_type,nodetype);
  tmp_pointer=tmp_node_type;

  for(i=0;i<ntypes;i++) //for each nodetype of the user
  {  first=TRUE;
     tmp_node_type=tmp_pointer;
     strcpy(tmp_node_type,nodetype);
     do //for each type in node_type (ex.: "e800,e60,piii,")
     {  if(first)
           node_type_pointer=strtok_r(tmp_node_type,",",&ptrptr);
        else
           node_type_pointer=strtok_r(NULL,",",&ptrptr);
        if(node_type_pointer!=NULL)
        {  if(!strcmp(node_type_pointer,(*typennodes)[i].type))
           {  found=i;
              dprintf("# aright_nodetype: user nnodes=%d\n",
                      (*typennodes)[i].nnodes);
              if((*typennodes)[i].nnodes<=mna)
              {  (*typennodes)[i].nnodes=-1;
                 break;
              }
              else
              {  free(tmp_pointer);
                 dprintf("# aright_nodetype: no enough nodes\n");
                 return CR_EAM_INVALID_NNODES;
              }
           }
        }
        first=FALSE;
     }while(node_type_pointer!=NULL);
  }
  free(tmp_pointer);
  return CR_OK;
}
//##############################################################################
//## Check the arguments using the ACCESS_RIGHTS_DEFS file
//##
//##  user        = check access right of this user (or user group)
//##  oper        = operation(allocate,info)
//##  file        = file which has the ACCESS_RIGHTS_DEFS and special times
//##  logfile     = log file
//##  alloc       = struct of the request allocation 
//##  timet       = starting time of the request
//##  arinfo      = access rights information
//##  nbytes      = number of bytes to store the arinfo data.
//##                It is also used to store the number of bytes of the
//##                alloc->nodes that is created when a quantitative allocation
//##                is requested.                                    
//##                  
//##  specialbuf  = buffer to be stored the special times
//##############################################################################
int check_arguments(char *user, int oper, char *file, char *logfile,
                    SAlloc *alloc, time_t timet, void **arinfo, int *nbytes,
                    char **specialbuf)
{
  int  i,j,
       is_special=FALSE, //is in special period
       answer=CR_OK, //user has permission to access the cluster
       ninfo=0,
       totalnodes=0, //Used when the user makes a quantitative allocation.
                     //So totalnodes has the sum of all nodes of all types,
                     //the one has access.
       lesser_time=-1,
       tmp_nnodes, tmp_time, tmp_nreserves,
       ntypes=0;  //number of node types

  SInfo *info=NULL;
  STypennodes *typennodes=NULL; //in qualitative allocation = user request
                                //in quantitative allocation = access rights

  crvprintf("\n# check_arguments: Checking the access rights of the user (%s)"
           "\n Access Rights file: %s\n", user,file);

  if(oper==CR_INFO)
    *specialbuf=(char*)calloc(1,sizeof(char));

  //set up 'is_special', 'info' and 'ninfo' variables
  parser_accessrights_defs(file,oper,user,timet,specialbuf,&is_special,
                           &info,&ninfo);

  if(oper==CR_ALLOC)
  { 
     alloc->nreserves=-1;
     if(alloc->qualitative)
     {  dprintf("# check_arguments: qualitative allocation\n");
        //set up 'typennodes' and  'ntypes' variables
        setup_alloc_nodetypes(&typennodes,&ntypes,alloc->nodes);
     }
     else
        dprintf("# check_arguments: quantitative allocation\n");

     for(j=0;j<ninfo;j++) //sweep 'info' structure
     {
        if(is_special) //is in a special period
        {  tmp_nnodes=info[j].s_nnodes;
           tmp_time=info[j].s_time;
           tmp_nreserves=info[j].s_nreserves;
        }
        else
        {  tmp_nnodes=info[j].n_nnodes;
           tmp_time=info[j].n_time;
           tmp_nreserves=info[j].n_nreserves;
        }

        if(alloc->qualitative)
        {  //verify access rights
           answer=aright_nodetype(info[j].nodetype,&typennodes,ntypes,tmp_nnodes);
           if(answer==CR_EAM_INVALID_NNODES)
              break;
        }
        else //Prepare typennodes which is the access rights
             //When a quantitative allocation is done we need to store
             //the access rights 
           add_quant_alloc_nodetype(&typennodes,&ntypes,info[j].nodetype,
                                    tmp_nnodes,&totalnodes);
        
        if(lesser_time == -1)  //first alloc_time
           lesser_time = tmp_time;
        else if(lesser_time > tmp_time) //get the lesser time
           lesser_time = tmp_time;

        if(alloc->nreserves == -1) //first alloc->nreserves
           alloc->nreserves = tmp_nreserves;
        else if(alloc->nreserves > tmp_nreserves) //get the lesser nreserves
           alloc->nreserves = tmp_nreserves;
     }
    
     if(alloc->qualitative) //check access rights
     {  
        for(i=0;i<ntypes;i++)
        {  if(typennodes[i].nnodes!=-1)
           {  dprintf("# check_arguments: nodetype missing or not a valid"
                      " number of nodes\n");
              answer=CR_EAM_INVALID_NODETYPE;
              break;
           }
        }
     }
     else //quantitative allocation
     {  dprintf("# check_arguments: aloc->nnodes=%d totalnodes=%d\n",
                alloc->nnodes,totalnodes); 
        if(alloc->nnodes > totalnodes)
           answer=CR_EAM_INVALID_NNODES;
        //store the access rights(typennodes) in alloc structure 
        *nbytes=typennodes2alloc_nodes(typennodes,ntypes,alloc);
     }

     if(alloc->time > lesser_time)
        answer=CR_EAM_INVALID_TIME;

     free_nodetypes(&typennodes,ntypes);
  }

  if(oper==CR_INFO)
  { 
     //build part of the packet (access rights) to send to UI

     //get the number of bytes to allocate
     //and looks for the maxtime and maxnnodes for allocation now
     *nbytes=sizeof(int)*3;
             //3= 'ninfo'+'maxtime'+'maxnnodes'
     for(i=0;i<ninfo;i++)
     { 
        if(is_special) //is in a special period
        {  totalnodes+=info[i].s_nnodes;
           tmp_time=info[i].s_time;
        }
        else
        {  totalnodes+=info[i].n_nnodes;
           tmp_time=info[i].n_time;
        }

        if(lesser_time == -1)  //first alloc_time
           lesser_time = tmp_time;
        else if(lesser_time > tmp_time) //get the lesser time
           lesser_time = tmp_time;

        *nbytes+=S_PACK_SINFO+sizeof(char)*(strlen(info[i].nodetype)+1);

     }

     //build the part of the packet responsible by the access rights
     *arinfo=malloc(*nbytes);
     memcpy(*arinfo,&ninfo,sizeof(int));
     memcpy(*arinfo+sizeof(int),&lesser_time,sizeof(int));
     memcpy(*arinfo+sizeof(int)*2,&totalnodes,sizeof(int));
    // printf("maxtime=%d maxnnodes=%d\n",lesser_time,totalnodes); 
     j=sizeof(int)*3; //j is the offset
     for(i=0;i<ninfo;i++)
     { 
        memcpy(*arinfo+j,&info[i],S_PACK_SINFO);

        j+=S_PACK_SINFO;
        strcpy(*arinfo+j,info[i].nodetype);
        j+=sizeof(char)*(strlen(info[i].nodetype)+1);
     }
  }

  for(i=0;i<ninfo;i++)
    free(info[i].nodetype);
  free(info);

  dprintf("# check_arguments: ANSWER=%d\n",answer);
  return answer;
}
//##############################################################################
//## Get nodes information from $DIRCONF/<cluster>/nodes and put in 'nodesinfo'
//## nodesinfo = 
//##  <node type specifications><number of types><type><nnodes><type><nnodes>...
//## The <node types specifications> are separed by '\n'  
//## nbytes = number of bytes of the nodesinfo variable
//##############################################################################
void get_nodes_info(char *file,char *logfile,void **nodesinfo,int *nbytes)
{
  FILE *fp;
  char buf[S_LINEF], buf1[S_LINEF],
       tag=0,             //parser
       **tmp_types=NULL,  //node types
       *types;

  int i=0, j, *tmp_nmachtypes=NULL,ntypes=0,nnodes=0,
      b=0;  //bytes to allocate to nodesinfo

  dprintf("# get_nodes_info:  start\n");

  types=(char*)malloc(sizeof(char));
  types[0]=0;
  *nodesinfo=NULL;

  if( (fp=fopen(file,"r")) == NULL)
     crono_log(logfile,CR_ERROR,"Problems to nodes file %s\n", file);

  while(fgets(buf,sizeof(buf),fp))
  { 
     buf[strlen(buf)-1]=0;

     i=0;
     //take off spaces, tabs and lines starting with #
     while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
        i++;

     if( (i==strlen(buf))|| (buf[i]=='#') )
        continue;

     if(!strcasecmp(&buf[i],"types"))
     {  tag=1;
        continue;
     }
     else if(!strcasecmp(&buf[i],"nodes"))
     {  tag=2;
        continue;
     }
     
     //store the node type specifications, separed by '\n'
     if(tag==1) //TYPES (description)
     { 
        types=realloc(types,sizeof(char)*(strlen(types)+strlen(&buf[i])+3));
        sprintf(types+sizeof(char)*strlen(types),"%s\n",&buf[i]);
     }
     //store the node types and the number of nodes of each node type
     else if(tag==2) //NODES (hostname:types)
     { 
        sscanf(&buf[i],"%[^:]",buf1);
        nnodes++;
        for(j=0;j<ntypes;j++)
          if(!strcmp(tmp_types[j],&buf[i+strlen(buf1)+1]))
              break;
        
        if(j==ntypes) //found a new type
        {  tmp_types=(char**)realloc(tmp_types,sizeof(char*)*(ntypes+1));
           tmp_types[ntypes]=(char*)malloc(sizeof(char)*
                                         (strlen(&buf[i+strlen(buf1)+1])+1));
           strcpy(tmp_types[ntypes],&buf[i+strlen(buf1)+1]);

           tmp_nmachtypes=(int*)realloc(tmp_nmachtypes,sizeof(int)*(ntypes+1));
           tmp_nmachtypes[ntypes]=1;

           b+=sizeof(int)+sizeof(char)*(strlen(tmp_types[ntypes])+1);
           ntypes++;
        }
        else
           tmp_nmachtypes[j]++;
     }
  }
  fclose(fp);
 
  dprintf("# get_nodes_info:  allocate and pack the information\n");
  *nbytes=sizeof(char)*(strlen(types)+1)+2*sizeof(int)+b;

  *nodesinfo=malloc(*nbytes);
  strcpy(*nodesinfo,types);
  b=sizeof(char)*(strlen(types)+1);

  memcpy(*nodesinfo+b,&nnodes,sizeof(int));
  b+=sizeof(int);
  memcpy(*nodesinfo+b,&ntypes,sizeof(int));
  b+=sizeof(int);

  for(i=0;i<ntypes;i++)
  {  memcpy(*nodesinfo+b,&tmp_nmachtypes[i],sizeof(int));
     b+=sizeof(int);
     strcpy(*nodesinfo+b,tmp_types[i]);
     b+=sizeof(char)*(strlen(tmp_types[i])+1);
  }
  
  dprintf("# get_nodes_info:  finish\n");
}
//##############################################################################
//## Check if 'timet' is in a special period
//##
//##  weekday  = real weekday
//##  weekdayf = weekday from ACCESS_RIGHTS_DEFS file
//##  itime    = initial time (HH:MM)
//##  ftime    = final time   (HH:MM)
//##  time     = time of request(for reserves it`s used the start time)
//##############################################################################
char check_is_special(char *weekday, char *weekdayf,char *itime, char *ftime,
                    time_t timet)
{
  int b1,b2; 
  time_t timet_i,timet_f;
  struct tm *t;
 
  dprintf("Check is special\n");
  dprintf("Weekday(today)= %s\n",weekday);
  dprintf("Weekday(file)= %s\n",weekdayf);
  if( !strcmp(weekdayf,weekday))
     dprintf("Same weekdays!!!\n");
  else
  {  dprintf("Diferent weekdays, so is not a special time\n");
     return FALSE;
  }

  t = localtime(&timet);

  sscanf(itime,"%d:%d",&b1,&b2);
  dprintf("itime=%s b1=%d b2=%d",itime,b1,b2);
  t->tm_hour=b1;
  t->tm_min=b2;
  timet_i=mktime(t);

  sscanf(ftime,"%d:%d",&b1,&b2);
  dprintf("ftime=%s b1=%d b2=%d",ftime,b1,b2);
  t->tm_hour=b1;
  t->tm_min=b2;
  timet_f=mktime(t);

  dprintf("TIME_NOW=<%ld> TIME_F=<%ld> TIME_I=<%ld>\n",timet,timet_f,timet_i);
  if( (timet<=timet_f) && (timet>=timet_i) )
  {  dprintf("IS A SPECIAL TIME!!\n");
     return TRUE;
  }
  return FALSE;
}
//##############################################################################
//## Check of 'weekday' is a valid weekday in xxx format
//##############################################################################
char check_weekday_name(char *weekday)
{
  if( !( !strcmp(weekday,"sun") || !strcmp(weekday,"mon") || 
      !strcmp(weekday,"tue") ||  !strcmp(weekday,"wed") || 
      !strcmp(weekday,"thu") || !strcmp(weekday,"fri") ||  
      !strcmp(weekday,"sat") ) )
    return FALSE;

  return TRUE;
}
//#######################################################################
//## Free memory of the STypennodes structure
//#######################################################################
void free_nodetypes(STypennodes **typennodes, int ntypes)
{
  int i;

  for(i=0;i<ntypes;i++)
    free((*typennodes)[i].type);

  free((*typennodes));
}
//############################################################################
//## Get the weekday (three letters format)
//############################################################################
void get_weekday(char *weekday,time_t timet )
{
  struct tm *t;

  t = localtime(&timet);

  switch(t->tm_wday)
  {  case 0: { strcpy(weekday,"sun"); break; }
     case 1: { strcpy(weekday,"mon"); break; }
     case 2: { strcpy(weekday,"tue"); break; }
     case 3: { strcpy(weekday,"wed"); break; }
     case 4: { strcpy(weekday,"thu"); break; }
     case 5: { strcpy(weekday,"fri"); break; }
     case 6: { strcpy(weekday,"sat"); break; }
  }
}
//##############################################################################
//## Get the 'group' name of a 'user' from the 'file' (groups file)
//## Return (found -> true | false)
//##############################################################################
char get_group(char *user,char *file,char *group)
{
  FILE *fp;
  char buf[S_LINEF],buf1[S_LINEF],buf2[S_LINEF], buf3[S_LINEF],
       bgroup[S_LINEF],found=FALSE; //buffer group 
  int  i, groups=0, line=0;

  dprintf("\nLooking for a group for this user: %s\n",user);
  if( (fp=fopen(file,"r")) == NULL)
     ERROR("Error opening file %s\n",file);

  while(fgets(buf,sizeof(buf),fp))
  {
     buf[strlen(buf)-1]=0;
     line++;
     i=0;
     while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
        i++;
     if( (i==strlen(buf))|| (buf[i]=='#') )
        continue;

     buf1[0]=0;
     buf2[0]=0;

     if(sscanf(&buf[i],"%s %s %s",buf1,buf2,buf3) > 2 )
        cr_file_error(fp,"\n%s: line %d: syntax error\n",file,line);
     if(groups==0)
     {  if( !strcasecmp("group",buf1) )
        {  if(buf2[0]==0)
              cr_file_error(fp,"\n%s: line %d: syntax error: you have to "
                               " define the group name!\n",file,line);
           strcpy(bgroup,buf2);
           groups=1;
        }
        else 
           cr_file_error(fp,"\n%s: line %d: syntax error \"%s\": expected "
                            "\"group\" statement!\n",file,line,buf1);
     }
     else if( groups==1)
     {  if( !strcasecmp("endgroup",buf1) )
           groups=0;
        else if( !strcasecmp("group",buf1) )
           cr_file_error(fp,"\n%s: line %d: syntax error \"%s\": expected "
                             "\"endgroup\" statement or a group name!\n",
                              file,line,buf1);
        else
        {  if(!strcmp(user,buf1))
           {  strcpy(group,bgroup);
              found=TRUE;
              break;
           }
        } 
     }
  }
  fclose(fp);
  dprintf("# get_group: Parser Done\n");

  return found;
}
//##############################################################################
//## Handle lines of the accessright.defs file
//## input:  'defs' = line of the file
//##         'defaults' = flag -> default definition or node type definition 
//##         'oper' = operation (when 'checksyntax') do not allocate memory
//##         'line' = number of the line (defs) of the accessrights.defs file
//##        
//## output: "info" = put line data to the info structure
//##
//## Example of lines: "defaults:  time=20  nreserves=1"
//##                   "nodetype e800: nnodes=8 time=10"            
//##############################################################################
void handle_defs(char *defs, char defaults, char oper, int line, SInfo *info)
{
  char tmp_defs[S_LINEF], token[S_LINEF], value[S_LINEF],
       *tokenpointer=NULL, first=TRUE, *ptrptr=NULL;
           
  strcpy(tmp_defs,defs);

  //get the tokens and their values (set the info structure)
  do 
  {  if(first)
        tokenpointer=strtok_r(tmp_defs," ",&ptrptr);
     else
        tokenpointer=strtok_r(NULL," ",&ptrptr);
     if(tokenpointer!=NULL)
     {  
        if(strstr(tokenpointer,"="))
        {  
           sscanf(tokenpointer,"%[^=]=%s",token,value);

           if(!strcmp(token,"time"))
              (*info).n_time=atoi(value);
           else if(!strcmp(token,"stime"))
              (*info).s_time=atoi(value);
           else if(!strcmp(token,"nreserves"))
              (*info).n_nreserves=atoi(value);
           else if(!strcmp(token,"snreserves"))
              (*info).s_nreserves=atoi(value);
           else if(!strcmp(token,"nnodes"))
              (*info).n_nnodes=atoi(value);
           else if(!strcmp(token,"snnodes"))
              (*info).s_nnodes=atoi(value);
           else
              crvprintf("Warning: \"%s\": Invalid token. Ignoring token"
                        " in line %d\n",token,line);
        }
     }
     first=FALSE;
  }while(tokenpointer!=NULL); 

  //if special definition is not set, the normal definitions will be used
  if((*info).s_nnodes==-1 && (*info).n_nnodes!= -1)
     (*info).s_nnodes=(*info).n_nnodes;
  if((*info).s_nreserves==-1 && (*info).n_nreserves!= -1)
     (*info).s_nreserves=(*info).n_nreserves;
  if((*info).s_time==-1 && (*info).n_time!= -1)
     (*info).s_time=(*info).n_time;
                             
  if(!defaults && oper!=CR_CHECK_SYNTAX_FILE)
  {   //set the node type 
      sscanf(defs,"nodetype %[^:]",token);
      (*info).nodetype=(char*)malloc(sizeof(char)*(strlen(token)+1));
      strcpy((*info).nodetype,token);

      //zero the non defined fields  
      if((*info).n_nnodes==-1)
        (*info).n_nnodes=0;
      if((*info).s_nnodes==-1)
        (*info).s_nnodes=0;

      if((*info).n_nreserves==-1)
        (*info).n_nreserves=0;
      if((*info).s_nreserves==-1)
        (*info).s_nreserves=0;

      if((*info).n_time==-1)
        (*info).n_time=0;
      if((*info).s_time==-1)
        (*info).s_time=0;
  }
}
//##############################################################################
//## Parser of the accessrights.defs file
//## input  -> file(path of the accessrights.defs), oper (alloc/info),
//##           itime(initial time of the allocation)
//## output -> specialbuf(special periods), is_special(verify the allocation is
//##           in a special period), info (store access rights information),
//##           ninfo (number of info structures)
//##############################################################################
///parser definitions
#define WAIT_SPECIAL_OR_ARIGTHS  0
#define WAIT_ENDSPECIAL_OR_TIMES 1
#define WAIT_ARIGHTS             2
void parser_accessrights_defs(char *file,int oper, char *user, time_t itime,
                              char **specialbuf, int *is_special, SInfo **info,
                              int *ninfo)
{

  FILE *fp;
  int  i,j,
       line=0; //used for parsing
  char buf[S_LINEF],buf1[S_LINEF],buf2[S_LINEF],buf3[S_LINEF],buf4[S_LINEF],
       weekday[4],
       buf_users[S_LINEF],
       get_user_defs=FALSE,    //user(s) profile
       get_default_defs=FALSE, //default profile
       parser_wait=WAIT_SPECIAL_OR_ARIGTHS;

  SInfo infodefaults;
  SInfo info_nodetype;


  crvprintf("#parser_accessrights_defs: BEGIN parser of accessrights.defs\n\n");
  if( (fp=fopen(file,"r")) == NULL)
  {  printf("Problems to open Access Rights file %s\n",file);
     exit(1);
  }
  info_nodetype.nodetype=NULL;
  get_weekday(weekday,itime);
  buf_users[0]=0;

  while( fgets(buf,sizeof(buf),fp) )
  {
     buf[strlen(buf)-1]=0;
     line++;
     i=0;
     //take off spaces, tabs and lines beginning with #
     while( (i!=strlen(buf)) && ((buf[i]==' ')  || (buf[i]=='\t') ))
        i++;
     if( (i==strlen(buf))|| (buf[i]=='#') )
        continue;
     buf1[0]=0;
     buf2[0]=0;
     //-------------- wait "special" or "users:" statement--------------//
     if(parser_wait == WAIT_SPECIAL_OR_ARIGTHS)  
     {  j=sscanf(&buf[i],"%s %s %s",buf1,buf2,buf3);
        if( !strcasecmp("special",buf1) )
        {  parser_wait=WAIT_ENDSPECIAL_OR_TIMES;
           continue;
        }
        else if( strstr("users:",&buf[i]) )
           parser_wait=WAIT_ARIGHTS;
     }
     //--------------- wait "endspecial" or times statements----------//
     else if(parser_wait == WAIT_ENDSPECIAL_OR_TIMES) 
     {  if( (sscanf(&buf[i],"%s %s",buf1,buf2) > 1 ) && 
            (!strcasecmp(buf1,"endspecial") ) 
          )
           cr_file_error(fp,"\n%s: line %d: syntax error \"%s\": expected "
                            "\"endspecial\" statement!\n",file,line,&buf[i]);
        else if( !strcasecmp(buf1,"endspecial") )
        {  parser_wait=WAIT_ARIGHTS;
           continue;
        }
        if(sscanf(&buf[i],"%s %[^-]-%s %s",buf1,buf2,buf3,buf4) != 3)
           cr_file_error(fp,"\n%s: line %d: syntax error \"%s\":\nthe "
                            "special time syntax is \"<weekday> "
                            "<initial time>-<final time>\". Example: "
                            "\"sun 00:00-14:00\"\n",file,line,&buf[i]);

        dprintf("# parser_accessrights_defs: %s %s %s\n",buf1,buf2,buf3);
        if(!check_weekday_name(buf1)) //check if buf1 is a valid weekday
           cr_file_error(fp,"\n%s: line %d: syntax error \"%s\": invalid " 
                            "weekday\n",file,line,buf1);
        if(oper==CR_ALLOC)
           *is_special|=check_is_special(weekday,buf1,buf2,buf3,itime);
        else if(oper==CR_INFO)  
           am_add_special_info(buf1,buf2,buf3,specialbuf); 
    }
    //---------------- wait accessrights--------------------------------------
    if(parser_wait == WAIT_ARIGHTS) 
    { 
       if(strstr(&buf[i],"users:"))
       {  
          if(get_user_defs && oper != CR_CHECK_SYNTAX_FILE )
             break;

          strcpy(buf_users,&buf[i]);
         
          //check for default profile 
          if(strstr(buf_users,"default") || oper == CR_CHECK_SYNTAX_FILE)
          {  get_default_defs=TRUE;
             memset(&infodefaults,-1,S_PACK_SINFO);
          }
          else if(user_in_list(buf_users,user))
          { 
             if(*ninfo) // I have the default definitions
             {  crvprintf("Discard default profile\n");
                for(i=0;i<*ninfo;i++)
                   free((*info)[i].nodetype);
                *ninfo=0;
                free(*info);
                *info=NULL;
             }
             get_user_defs=TRUE;
             memset(&infodefaults,-1,S_PACK_SINFO);
          }
          else
             get_default_defs=FALSE;   
       }
       else if(strstr(&buf[i],"defaults:"))
       {  //check for default values for this profile 
          if(get_user_defs || get_default_defs)
          {  
             handle_defs(&buf[i],TRUE,oper,line,&infodefaults);
             if(verbose && oper!=CR_CHECK_SYNTAX_FILE) 
             {  printf("-----------DEFAULTS---------------\n");
                printf("TIME=%d  ",infodefaults.n_time);
                printf("STIME=%d  ",infodefaults.s_time);
                printf("RESERVES=%d  ",infodefaults.n_nreserves);
                printf("SRESERVES=%d ",infodefaults.s_nreserves);
                printf("NODES=%d ",infodefaults.n_nnodes);
                printf("SNODES=%d\n",infodefaults.s_nnodes);
                printf("--------------------------\n");
             }
          }
       }
       else if(strstr(&buf[i],"nodetype "))
       { 
          if(get_user_defs || get_default_defs) 
          {  
             if(oper!=CR_CHECK_SYNTAX_FILE) 
                memcpy(&info_nodetype,&infodefaults,S_PACK_SINFO);
             handle_defs(&buf[i],FALSE,oper,line,&info_nodetype);
   
             if(oper!=CR_CHECK_SYNTAX_FILE)
             {  *info=(SInfo*)realloc(*info,sizeof(SInfo)*(*ninfo+1));
                memcpy(&(*info)[*ninfo],&info_nodetype,S_PACK_SINFO);
                (*info)[*ninfo].nodetype=(char*)malloc(sizeof(char)*
                                         (strlen(info_nodetype.nodetype)+1));
               
                strcpy((*info)[*ninfo].nodetype,info_nodetype.nodetype);
                free(info_nodetype.nodetype);
                (*ninfo)++;
             }
          }
       }
       else
         crvprintf("Warning: \"%s\" : Syntax Error. Ignoring line %d\n",
                   buf,line);
    }
  }//while
  fclose(fp);
  if(parser_wait != WAIT_ARIGHTS)
     cr_file_error(fp,"%s: Access rigths in accessrights.defs file are missing",
                      file);

  if(verbose && oper!=CR_CHECK_SYNTAX_FILE)
     for(i=0;i<*ninfo;i++)
     {
        printf("NODETYPE=%s\n   ",(*info)[i].nodetype);
        printf("TIME=%d ",(*info)[i].n_time);
        printf("STIME=%d ",(*info)[i].s_time);
        printf("RESERVES=%d ",(*info)[i].n_nreserves);
        printf("SRESERVES=%d ",(*info)[i].s_nreserves);
        printf("NODES=%d ",(*info)[i].n_nnodes);
        printf("SNODES=%d\n",(*info)[i].s_nnodes);
     }
  crvprintf("\n#parser_accessrights_defs: END parser of accessrights.defs\n\n");
}
//##############################################################################
//## Setup the node types from 'allocnodes' string to the STypennodes structure
//## 
//## allocnodes format is: <ntypes><type><nnodes><type><nnodes>
//##############################################################################
void setup_alloc_nodetypes(STypennodes **typennodes, int *ntypes,
                           void *allocnodes)
{
  int i, b;

  dprintf("# setup_alloc_nodetypes: begin\n");
  memcpy(ntypes,allocnodes,sizeof(int));
  *typennodes=(STypennodes*)malloc((*ntypes)*sizeof(STypennodes));
  b=sizeof(int);

  for(i=0;i<*ntypes;i++)
  {  //get node type
     (*typennodes)[i].type=(char*)malloc(sizeof(char)*(strlen(allocnodes+b)+1));
     strcpy( (*typennodes)[i].type, allocnodes+b);
     b+=sizeof(char)*(strlen(allocnodes+b)+1);
     //get number of nodes of this type
     memcpy(&(*typennodes)[i].nnodes,allocnodes+b,sizeof(int));
     b+=sizeof(int); 
     dprintf("# setup_alloc_nodetypes: type=<%s> nnodes=<%d>\n",
              (*typennodes)[i].type,(*typennodes)[i].nnodes);
  }
  dprintf("# setup_alloc_nodetypes: ntypes=%d\n",*ntypes);
  dprintf("# setup_alloc_nodetypes: end\n");
}
//##############################################################################
//## Put the typennodes structure in alloc->nodes variable
//## Return: size of the alloc->nodes buffer
//##############################################################################
int typennodes2alloc_nodes(STypennodes *typennodes, int ntypes, SAlloc *alloc)
{
  int i,j,s=sizeof(int);
  void *offset;

  alloc->nodes=(int*)malloc(s);
  memcpy(alloc->nodes,&ntypes,s);
  crvprintf("# typennodes2alloc_nodes: NTYPES -> %d\n",ntypes);
  for(i=0;i<ntypes;i++)
  {  j=sizeof(char)*(strlen(typennodes[i].type)+1);
     alloc->nodes=(char*)realloc(alloc->nodes,s+j+sizeof(int));
     offset=alloc->nodes+s;
     strcpy(offset,typennodes[i].type);
     offset+=j;
     memcpy(offset,&typennodes[i].nnodes,sizeof(int));
     s+=j+sizeof(int);
  }
  return s;
}
//##############################################################################
//## Verivy if the 'user' is in the list of users 'buf_users'
//## buf_user format: "users: <user1> [ <user2> <user3> ....]"
//##############################################################################
char user_in_list(char *buf_users, char *user)
{

  char buf_users_tmp[S_LINEF];  
  char *tokenpointer=NULL,first=TRUE;
  char *ptrptr=NULL;

  crvprintf("# user_in_list: buf_users=<%s> user=<%s>\n",buf_users,user);
  if(!sscanf(buf_users,"users: %[^NULL]",buf_users_tmp))
    return FALSE;

  do 
  {  if(first)
        tokenpointer=strtok_r(buf_users_tmp," ",&ptrptr);
     else
        tokenpointer=strtok_r(NULL," ",&ptrptr);
     if(tokenpointer!=NULL)
        if(!strcmp(tokenpointer,user))
           return TRUE;

     first=FALSE;

  }while(tokenpointer!=NULL); 

  return FALSE;
}
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
