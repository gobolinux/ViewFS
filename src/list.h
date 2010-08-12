
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
#ifndef LIST_H
#define LIST_H

#include <stdint.h>
#include <stdbool.h>

typedef struct listitem listitem_t;

struct listitem {
   int64_t id;
   void* data;
   listitem_t* next;
   listitem_t* prev;
};

typedef struct list list_t;

struct list {
   listitem_t* hd;
   listitem_t* tl;
};

typedef struct list_iter list_iter_t;

struct list_iter {
   list_t* list;
   listitem_t* at;
};

typedef bool (*list_find_fn)(void* item_cast, void* sample_cast);
typedef void (*list_delete_fn)(void* data_cast);

bool list_find_pointer_eq(void* item, void* sample);
bool list_find_string_eq(void* item, void* sample);

list_t* list_new();
void list_put(list_t* self, int id, void* data);
void list_prepend(list_t* self, int id, void* data);
void* list_get(list_t* self, int id);
void* list_find(list_t* self, void* sample, list_find_fn fn);
void list_delete(list_t* self, list_delete_fn fn);
void list_merge_contents_into(list_t* dst, list_t* src);

list_iter_t* list_iter_new(list_t* list);
void list_iter_delete(list_iter_t* self);
void* list_iterate(list_iter_t* iter, void* sample, list_find_fn fn);
void* list_iterate_take(list_iter_t* iter, void* sample, list_find_fn fn);

#define list_foreach(type, item, self) \
   for (type *item ## _ITERATOR = (type*) self->hd, \
             *item = self->hd ? (type*)( self->hd->data ) : NULL \
       ; item ## _ITERATOR \
       ; item ## _ITERATOR = (type *) ( ((listitem_t*) item ## _ITERATOR )->next ), \
         item = item ## _ITERATOR ? (type *) (((listitem_t*) item ## _ITERATOR )->data) : NULL)

#endif
