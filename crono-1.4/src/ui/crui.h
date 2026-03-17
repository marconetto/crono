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

#ifndef _CRUI_H
#define _CRUI_H

#define USAGE_DEF_FILE "The missing parameters will be filled out using the " \
                       "default file. \nTo manipulate the default file use "\
                       "the crsetdef program.\n"
//struct used by the default parameters
typedef struct
{  int  shared,    //share option(exclusive or shared access)
        time;      //time to allocate
   
   char *cluster,  //cluster name
        *nodes,    //number of nodes or nodes description
        *bjscript, //batch job script
        *mpicomp,  //MPI Compilation environment 
        *mpirun,   //MPI Execution environment 
        *arch;     //Architecture
}SDefaults;

extern char crget_defaults(SDefaults *defs, int uid);
extern void ui_getconf_server(char *file,int *port,char *am_server);
#endif
