
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
/*
View filesystem module for Fuse
Hisham Muhammad, 2005/04/07.
*/
#ifndef STRINGTREE_H
#define STRINGTREE_H

#include <stdbool.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct stringset stringset_t;

struct stringset {
   int count;
   void* value;
   union {
      stringset_t** links;
      char* leafkey;
   } u;
   char min;
   char max;
};

typedef void(*stringset_delete_fn_t)(void*);

stringset_t* stringset_new();

void stringset_delete(stringset_t* self, stringset_delete_fn_t destroy_value);

bool stringset_put(stringset_t* self, const char* key, void* value);

void* stringset_get(stringset_t* self, const char* key);

bool stringset_remove(stringset_t* self, const char* key, void** removed_value);

bool stringset_put_int(stringset_t* self, int ikey, void* value);

void* stringset_get_int(stringset_t* self, int ikey);

bool stringset_remove_int(stringset_t* self, int ikey, void** removed_value);

void* stringset_get_i(stringset_t* self, const char* key);

typedef void(*stringset_scan_fn_t)(void* param, char* key, void* value);

void stringset_scan(stringset_t* self, stringset_scan_fn_t fn, void* param);

extern int stringset_debug;

extern stringset_t* stringset_debug_root;

void stringset_draw(stringset_t* self);

typedef struct stringset_iter {
   char key[PATH_MAX];
   stringset_t** set_stack;
   int* at_stack;
   int stack_level;
   int stack_size;
} stringset_iter_t;

stringset_iter_t* stringset_iter_new(stringset_t* set);
void stringset_iter_delete(stringset_iter_t* self);
void* stringset_iter_next(stringset_iter_t* self);

#define stringset_begin_iterate(type, item, set) \
   do { \
      stringset_iter_t* item ## _ITERATOR = stringset_iter_new(set); \
      char* item ## _key = item ## _ITERATOR -> key; \
      type* item; \
      while (item = stringset_iter_next( item ## _ITERATOR )) { \
      do {} while (0)

#define stringset_end_iterate(item) \
      } \
      stringset_iter_delete( item ## _ITERATOR ); \
   } while (0)

#endif
