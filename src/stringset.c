
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

#include "stringset.h"

#include <string.h>
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>

#include <stdio.h>

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

int stringset_debug = 0;

stringset_t* stringset_debug_root;

stringset_t* stringset_new(void* value) {
   stringset_t* self = malloc(sizeof(stringset_t));
   self->value = value;
   self->min = 0;
   self->max = 0;
   self->u.leafkey = NULL;
   self->count = 0;
   return self;
}

void stringset_delete(stringset_t* self, stringset_delete_fn_t destroy_value) {
   if (self->min == -1) {
      free(self->u.leafkey);
   } else if (self->min > 0) {
      for (int i = self->min; i <= self->max; i++)
         if (self->u.links[i - self->min])
            stringset_delete(self->u.links[i - self->min], destroy_value);
   }
   destroy_value(self->value);
   free(self);
}

#define is_leaf(self) (((self)->min == 1) && ((self)->max == 0))
#define has_links(self) ((self)->max > 0)

#define set_leaf(self) do { (self)->min = 1; (self)->max = 0; } while (0)

void stringset_debug_show(stringset_t* self) {
   fprintf(stderr, "{--------------------------\n");
   if (is_leaf(self))
      fprintf(stderr, "[%p] leaf \"%s\" -> %p\n", self, self->u.leafkey, self->value);
   else
      fprintf(stderr, "[%p] '%c'-'%c' $-> %p\n", self, self->min, self->max, self->value);
   if (has_links(self)) {
      for (int i = self->min; i <= self->max; i++) {
         if (self->u.links[i - self->min]) {
            fprintf(stderr, "%c:%p ", i, self->u.links[i - self->min]);
         }
      }
      fprintf(stderr, "\n");
   }
   fprintf(stderr, "}--------------------------\n");
   if (has_links(self)) {
      for (int i = self->min; i <= self->max; i++) {
         if (self->u.links[i - self->min]) {
            stringset_debug_show(self->u.links[i - self->min]);
         }
      }
   }
}

static bool stringset_put_rec(stringset_t* self, const char* key, void* value, stringset_t* root) {
   assert(self);
   assert(key);
   char ch = key[0];
   if (is_leaf(self)) {
      if (strcmp(self->u.leafkey, key) == 0) {
         //fprintf(stderr, "Already exists.\n");
         return false;
      } else {
         stringset_t* leaf = stringset_new(self->value);
         if (self->u.leafkey[1] != '\0') {
            leaf->u.leafkey = strdup(self->u.leafkey + 1);
            set_leaf(leaf);
         }
         self->min = self->u.leafkey[0];
         self->max = self->u.leafkey[0];
         free(self->u.leafkey);
         self->u.leafkey = NULL;
         self->u.links = calloc(1, sizeof(stringset_t*));
         self->u.links[0] = leaf;
         self->value = NULL;
      }
   }
   assert(!is_leaf(self));
   if (key[0] == '\0') {
      //fprintf(stderr, "Already exists.\n");
      if (self->value)
         return false;
      self->value = value;
      root->count++;
      return true;
   }
   if (!(has_links(self)) && !(self->value)) {
      self->u.leafkey = strdup(key);
      set_leaf(self);
      self->value = value;
      root->count++;
      return true;
   }
   if (ch < self->min || ch > self->max) {
      int newmin = self->min == 0 ? ch : MIN(ch, self->min);
      int newmax = self->max == 0 ? ch : MAX(ch, self->max);
      stringset_t** newlinks = calloc(newmax - newmin + 1, sizeof(stringset_t*));
      if (self->u.links) {
         memcpy(newlinks + (self->min - newmin), self->u.links, sizeof(stringset_t*) * (self->max - self->min + 1));
         free(self->u.links);
      }
      self->min = newmin;
      self->max = newmax;
      self->u.links = newlinks;
   }
   if (self->u.links[ch - self->min] == NULL)
      self->u.links[ch - self->min] = stringset_new( NULL );
   return stringset_put_rec(self->u.links[ch - self->min], key + 1, value, root);
}

bool stringset_put_int(stringset_t* self, int ikey, void* value) {
   char key[32];
   snprintf(key, 31, "%d", ikey);
   return stringset_put_rec(self, key, value, self);
}

bool stringset_put(stringset_t* self, const char* key, void* value) {
   return stringset_put_rec(self, key, value, self);
}

bool stringset_remove_int(stringset_t* self, int ikey, void** removed_value) {
   char key[32];
   snprintf(key, 31, "%d", ikey);
   return stringset_remove(self, key, removed_value);
}

bool stringset_remove(stringset_t* self, const char* key, void** removed_value) {
   if (removed_value)
      *removed_value = NULL;
   bool remove_this = false;
   char ch = key[0];
   if (is_leaf(self)) {
      if (strcmp(self->u.leafkey, key) == 0) {
         if (removed_value)
            *removed_value = self->value;
         free(self->u.leafkey);
         self->min = 0;
         self->max = 0;
         self->value = NULL;
         remove_this = true;
      } else {
         /* Key not in tree. */
      }
   } else {
      int index = ch - self->min;
      if (ch == '\0') {
         if (removed_value)
            removed_value = self->value;
         self->value = NULL;
         if (!has_links(self)) {
            self->min = 0;
            self->max = 0;
            self->value = NULL;
            remove_this = true;
         }
      } else if (!has_links(self) || ch < self->min || ch > self->max) {
         /* Key not in tree. */
      } else if (self->u.links && self->u.links[index] != NULL) {
         bool remove_node = stringset_remove(self->u.links[index], key + 1, removed_value);
         if (remove_node) {
            free(self->u.links[index]);
            self->u.links[index] = NULL;
            if (self->min != self->max) {
               int newcount = 0, newsize = 0;
               if (ch == self->min) {
                  while (self->u.links[index] == NULL)
                     index++;
                  self->min += index;
                  newcount = self->max - self->min + 1;
                  newsize = newcount * sizeof(stringset_t*);
               } else if (ch == self->max) {
                  index = self->max - self->min;
                  while (self->u.links[index] == NULL)
                     index--;
                  self->max = self->min + index;
                  newcount = index + 1;
                  newsize = newcount * sizeof(stringset_t*);
                  index = 0;
               }
               if (newsize) {
                  stringset_t** newlinks = calloc(newcount, sizeof(stringset_t*));
                  memcpy(newlinks, self->u.links + index, newsize);
                  free(self->u.links);
                  self->u.links = newlinks;
               }
            } else {
               free(self->u.links);
               self->u.links = NULL;
               self->min = 0;
               self->max = 0;
               self->value = NULL;
               if (!self->value)
                  remove_this = true;
            }
         }
         if (self->u.links && self->min == self->max && (!self->value) && is_leaf(self->u.links[0])) {
            stringset_t* child = self->u.links[0];
            free(self->u.links);
            self->u.leafkey = malloc(strlen(child->u.leafkey) + 2);
            sprintf(self->u.leafkey, "%c%s", self->min, child->u.leafkey);
            free(child->u.leafkey);
            self->value = child->value;
            set_leaf(self);
            free(child);
         }
      }
   }
   return remove_this;
}

void* stringset_get_int(stringset_t* self, int ikey) {
   char key[32];
   snprintf(key, 31, "%d", ikey);
   return stringset_get(self, key);
}

void* stringset_get(stringset_t* self, const char* key) {
   assert(self);
   assert(key);
   char ch = key[0];
   if (is_leaf(self)) {
      if (strcmp(self->u.leafkey, key) == 0) {
         return self->value;
      } else {
         return NULL;
      }
   } else {
      if (key[0] == '\0') {
         return self->value;
      } else if (ch < self->min || ch > self->max) {
         return NULL;
      } else if (self->u.links[ch - self->min] == NULL) {
         return NULL;
      } else {
         return stringset_get(self->u.links[ch - self->min], key + 1);
      }
   }
}

void* stringset_get_i(stringset_t* self, const char* key) {
   assert(self);
   assert(key);
   if (is_leaf(self)) {
      if (strcasecmp(self->u.leafkey, key) == 0) {
         return self->value;
      } else {
         return NULL;
      }
   } else {
      if (key[0] == '\0') {
         return self->value;
      } 
      char* result = NULL;
      char ch = tolower(key[0]);
      if (ch < self->min || ch > self->max) {
         result = NULL;
      } else if (self->u.links[ch - self->min] == NULL) {
         result = NULL;
      } else {
         result = stringset_get(self->u.links[ch - self->min], key + 1);
      }
      if (!result) {
         ch = toupper(key[0]);
         if (ch < self->min || ch > self->max) {
            result = NULL;
         } else if (self->u.links[ch - self->min] == NULL) {
            result = NULL;
         } else {
            result = stringset_get(self->u.links[ch - self->min], key + 1);
         }
      }
      return result;
   }
}

static void stringset_scan_rec(stringset_t* self, stringset_scan_fn_t fn, void* param, char* name, int len) {
   if (is_leaf(self)) {
      strcpy(name + len, self->u.leafkey);
      len += strlen(self->u.leafkey);
      fn(param, name, self->value);
      return;
   }
   if (self->value)
      fn(param, name, self->value);
   if (!has_links(self))
      return;
   for (int i = self->min; i <= self->max; i++) {
      if (self->u.links[i - self->min]) {
         name[len] = i;
         name[len+1] = '\0';
         stringset_scan_rec(self->u.links[i - self->min], fn, param, name, len+1);
      }
   }
}

void stringset_scan(stringset_t* self, stringset_scan_fn_t fn, void* param) {
   char buffer[PATH_MAX];
   buffer[0] = '\0';
   stringset_scan_rec(self, fn, param, buffer, 0);
}

void stringset_draw_rec(stringset_t* self, int depth) {
   if (self->value) {
      for (int i = 0; i < depth; i++)
         printf("  ");
      printf("[%p]\n", self->value);
   }
   if (is_leaf(self)) {
      for (int i = 0; i < depth; i++)
         printf("  ");
      printf("'%s' (leaf)\n", self->u.leafkey);
   } else {
      if (self->u.links) {
         for (int i = self->min; i <= self->max; i++) {
            int idx = i - self->min;
            if (self->u.links[idx]) {
               for (int i = 0; i < depth; i++)
                  printf("  ");
               printf("'%c' (link)\n", i);
               stringset_draw_rec(self->u.links[idx], depth+1);
            }
         }
      }
   }
}

void stringset_draw(stringset_t* self) {
   printf("------------------------------------\n");
   stringset_draw_rec(self, 0);
   printf("------------------------------------\n");
}

stringset_iter_t* stringset_iter_new(stringset_t* set) {
   stringset_iter_t* self = malloc(sizeof(stringset_iter_t));
   self->stack_size = 20;
   self->set_stack = malloc(sizeof(stringset_t*) * self->stack_size);
   self->at_stack = malloc(sizeof(stringset_t*) * self->stack_size);
   self->stack_level = 0;
   self->key[0] = '\0';
   self->set_stack[0] = set;
   self->at_stack[0] = set->min;
   return self;
}

void stringset_iter_delete(stringset_iter_t* self) {
   free(self->set_stack);
   free(self->at_stack);
   free(self);
}

void* stringset_iter_next(stringset_iter_t* self) {
   void* result = NULL;
   while (!result) {
      if (self->stack_level == -1) {
         result = NULL;
         break;
      }
      int level = self->stack_level;
      stringset_t* set = self->set_stack[level];
      int at = self->at_stack[level];
      if (is_leaf(set)) {
         strcpy(self->key + level, set->u.leafkey);
         self->stack_level--;
         result = set->value;
         break;
      }
      self->key[level] = at;
      self->key[level + 1] = '\0';
      if (has_links(set) && at <= set->max) {
         int next_at = at + 1;
         while (next_at <= set->max && !set->u.links[next_at - set->min])
            next_at++;
         self->at_stack[level] = next_at;
         self->stack_level++;
         if (self->stack_level == self->stack_size) {
            self->stack_size *= 2;
            self->set_stack = realloc(self->set_stack, sizeof(stringset_t*) * self->stack_size);
            self->at_stack = realloc(self->at_stack, sizeof(stringset_t*) * self->stack_size);
         }
         self->set_stack[self->stack_level] = set->u.links[at - set->min];
         self->at_stack[self->stack_level] = self->set_stack[self->stack_level]->min;
      } else {
         self->key[level] = '\0';
         self->stack_level--;
         if (set->value) {
            result = set->value;
         }
      }
   }
   return result;
}
