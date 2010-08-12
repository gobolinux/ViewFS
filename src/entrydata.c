
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
#include <stdarg.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include "entrydata.h"
#include "vect.h"

entrydata_t* entrydata_new(entrytype_t type, ...) {
   va_list ap;
   entrydata_t* self;
   va_start(ap, type);
   switch (type) {
      case ET_LINK:
         {
            entrydata_t* view = va_arg(ap, entrydata_t*);
            char* realpath = va_arg(ap, char*);
            self = malloc(sizeof(entrydata_t));
            self->u.link.view = calloc(2, sizeof(entrydata_t*));
            self->u.link.path = strdup(realpath);
            self->u.link.view[0] = view;
            self->u.link.view[1] = NULL;
            strcpy(self->u.link.path, realpath);
            break;
         }
      case ET_DIR:
         {
            self = calloc(1, sizeof(entrydata_t));
            self->u.dir.entries = va_arg(ap, stringset_t*);
            self->u.dir.global = NULL;
            break;
         }
      case ET_VIEW:
         {
            char* package = va_arg(ap, char*);
            char* version = va_arg(ap, char*);
            self = malloc(sizeof(entrydata_t));
            self->u.view = calloc(1, sizeof(viewdata_t));
            self->u.view->package = strdup(package);
            self->u.view->version = strdup(version);
            break;
         }
   }
   va_end(ap);
   self->type = type;
   return self;
}

void entrydata_add_subentry(entrydata_t* self, const char* name, entrydata_t* sub) {
   if (!self->u.dir.entries)
      self->u.dir.entries = stringset_new(NULL);
   stringset_put(self->u.dir.entries, name, sub);
}

void entrydata_add_view_to_link(entrydata_t* self, entrydata_t* view) {
   int count = 0;
   while (self->u.link.view[count] != NULL)
      count++;
   self->u.link.view = realloc(self->u.link.view, sizeof(entrydata_t*) * (count+2));
   self->u.link.view[count] = view;
   self->u.link.view[count+1] = NULL;
}

/*
void entrydata_remove_view_from_link(entrydata_t* self, const char* view_name) {
   int i, j = 0;
   for (i = 0, j = 0; self->u.link.view[i] != NULL; i++) {
      if (j < i)
         self->u.link.view[j] = self->u.link.view[i];
      if (self->u.link.view[i] != view_name)
         j++;
   }
   self->u.link.view[j] = NULL;
   self->u.link.view = realloc(self->u.link.view, sizeof(char*) * (j+1));
}
*/

void entrydata_delete(void* cast) {
   entrydata_t* self = (entrydata_t*) cast;
   switch (self->type) {
      case ET_LINK:
         break;
      case ET_DIR:
         stringset_delete(self->u.dir.entries, entrydata_delete);
         break;
      case ET_VIEW:
         break;
   }
   free(self);
}
