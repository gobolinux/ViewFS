
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
#include "list.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

void* list_get(list_t* self, int id) {
   listitem_t* item = self->hd;
   while (item) {
      if (item->id == id)
         return item->data;
      item = item->next;
   }
   return NULL;
}

void list_put(list_t* self, int id, void* data) {
   listitem_t* item = malloc(sizeof(listitem_t));
   item->id = id;
   item->data = data;
   item->prev = self->tl;
   item->next = NULL;
   if (!self->hd) {
      self->hd = item;
      self->tl = item;
   } else {
      self->tl->next = item;
      self->tl = item;
   }
}

void list_prepend(list_t* self, int id, void* data) {
   listitem_t* item = malloc(sizeof(listitem_t));
   item->id = id;
   item->data = data;
   item->prev = NULL;
   item->next = self->hd;
   if (!self->hd) {
      self->hd = item;
      self->tl = item;
   } else {
      self->hd->prev = item;
      self->hd = item;
   }
}

list_t* list_new() {
   list_t* self = malloc(sizeof(list_t));
   self->hd = NULL;
   self->tl = NULL;
   return self;
}

void list_delete(list_t* self, list_delete_fn fn) {
   listitem_t* item = self->hd;
   while (item) {
      listitem_t* curr = item;
      item = item->next;
      fn(curr->data);
      free(curr);
   }
   free(self);
}

bool list_find_pointer_eq(void* item, void* sample) {
   return item == sample;
}

bool list_find_string_eq(void* item, void* sample) {
   return strcmp((char*)item, (char*)sample) == 0;
}

list_iter_t* list_iter_new(list_t* list) {
   assert(list);
   list_iter_t* iter = malloc(sizeof(list_iter_t));
   iter->list = list;
   iter->at = list->hd;
   return iter;
}

void list_iter_delete(list_iter_t* self) {
   free(self);
}

void* list_iterate(list_iter_t* iter, void* sample, list_find_fn fn) {
   listitem_t* item = iter->at;
   while (item) {
      if (fn(item->data, sample)) {
         iter->at = item->next;
         return item->data;
      }
      item = item->next;
   }
   return NULL;
}

void* list_iterate_take(list_iter_t* iter, void* sample, list_find_fn fn) {
   listitem_t* item = iter->at;
   void* result = NULL;
   while (item) {
      if (fn(item->data, sample)) {
         if (item->prev)
            item->prev->next = item->next;
         else
            iter->list->hd = item->next;
         if (item->next)
            item->next->prev = item->prev;
         else
            iter->list->tl = item->prev;
         result = item->data;
         iter->at = item->next;
         free(item);
         break;
      }
      item = item->next;
   }
   return result;
}

void* list_find(list_t* self, void* sample, list_find_fn fn) {
   list_iter_t iter;
   iter.list = self;
   assert(self);
   iter.at = self->hd;
   void* result = list_iterate(&iter, sample, fn);
   return result;
}

void* list_find_take(list_t* self, void* sample, list_find_fn fn) {
   list_iter_t iter;
   iter.list = self;
   iter.at = self->hd;
   void* result = list_iterate_take(&iter, sample, fn);
   return result;
}

/*
Notice that the listitem_t elements of src are now linked into dst.
Therefore, you should not list_delete(src); just free() it when
you don't need the pointers to hd and tl of src anymore.
*/
void list_merge_contents_into(list_t* dst, list_t* src) {
   if (dst->hd && src->hd) {
      dst->tl->next = src->hd;
      src->hd->prev = dst->tl;
      dst->tl = src->tl;
   } else if (src->hd) {
      dst->hd = src->hd;
      dst->tl = src->tl;
   }
}


