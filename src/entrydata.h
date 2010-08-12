
/*
Copyright (c) 2005, IBM Corporation All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met: Redistributions of source code must retain the above
copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.  Neither the name
of the IBM Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
*/
#ifndef ENTRYDATA_H
#define ENTRYDATA_H

#include <stdint.h>

#include "stringset.h"
#include "list.h"

typedef enum entrytype entrytype_t;

enum entrytype {
   /* A directory */
   ET_DIR,
   /* A symbolic link */
   ET_LINK,
   /* Not-yet-loaded view */
   ET_VIEW
};

typedef struct entrydata entrydata_t;

typedef struct viewdata {
   char* package;
   char* version;
   list_t* dep_rules;
   list_t* priority_views;
} viewdata_t;

struct entrydata {
   entrytype_t type;
   union {
      struct {
         char* path;
         entrydata_t** view;
      } link;
      struct {
         stringset_t* entries;
         entrydata_t* global;
      } dir;
      viewdata_t* view;
   } u;
};

entrydata_t* entrydata_new(entrytype_t type, ...);
void entrydata_delete(void* cast);
void entrydata_add_subentry(entrydata_t* self, const char* name, entrydata_t* sub);
void entrydata_add_view_to_link(entrydata_t* self, entrydata_t* view);

#endif
