
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
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>

#include "depfile.h"

#define LINE_WIDTH (PATH_MAX + 3)

#define ARCHITECTURE "i386"

static inline int str_starts_with(char* a, char* b) {
   return strncmp(a, b, strlen(b)) == 0;
}

static inline void str_strip_space(char* line) {
   int i, j;
   for (i = 0, j = 0; line[i] != '\0'; i++) {
      if (!isspace(line[i])) {
         line[j] = line[i];
         j++;
      }
   }
   line[j] = '\0';
}

// ---------------------------------------------------------------------------

depwait_t* depwait_new(dep_t* dep, entrydata_t* view) {
   depwait_t* self = malloc(sizeof(depwait_t));
   self->dep = dep;
   self->view = view;
   return self;
}

bool depwait_find(void* item_cast, void* sample_cast) {
   depwait_t* item = (depwait_t*) item_cast;
   char* sample = (char*) sample_cast;
   return (strcasecmp(sample, item->dep->name) == 0);
}

void depwait_delete(void* self) {
   free(self);
}

// ---------------------------------------------------------------------------

static inline int depfile_get_token(char* token, char* line, int* x) {
   int start = *x;
   int end = *x;
   int type;
   if (line[end] == '\0' || line[end] == '\n')
      return TK_ERROR;
   while (isalnum(line[end]) || line[end] == '_' || line[end] == '-' || line[end] == '.') {
      type = TK_ID;
      end++;
   }
   if (end == start) {
      if (line[end] == '>' && line[end+1] == '=') {
         type = TK_GE;
         end += 2;
      } else if (line[end] == '>') { // support >> and >
         type = TK_GT;
         end++;
         if (line[end] == '>') end++;
      } else if (line[end] == '<' && line[end+1] == '=') {
         type = TK_LE;
         end += 2;
      } else if (line[end] == '<') { // support << and <
         type = TK_LT;
         end++;
         if (line[end] == '<') end++;
      } else {
         type = line[end];
         end++;
      }
   }
   strncpy(token, line + start, end - start);
   token[end-start] = '\0';
   *x = end;
   return type;
}

static list_t* depfile_lexer(FILE* file) {
   char line[LINE_WIDTH + 1];
   char token[LINE_WIDTH + 1];
   char nextchar;
   line[0] = '\0';
   line[LINE_WIDTH] = '\0';
   int x = 0;
   list_t* tokens = list_new();
   int type;
   while (true) {
      if ( (type = depfile_get_token(token, line, &x)) == TK_ERROR ) {
         x = 0;
         if (!fgets(line, LINE_WIDTH - 1, file))
            break;
         str_strip_space(line);
         if (line[0] == '#')
            line[0] == '\0';
         continue;
      }
      list_put(tokens, type, strdup(token));
   }
   return tokens;
}

// ---------------------------------------------------------------------------

static relation_t relation_invert(relation_t self) {
   switch (self) {
   case REL_EQ: return REL_NE;
   case REL_NE: return REL_EQ;
   case REL_GE: return REL_LT;
   case REL_GT: return REL_LE;
   case REL_LE: return REL_GT;
   case REL_LT: return REL_GE;
   }
}

static void constraint_delete(void* self_cast) {
   constraint_t* self = (constraint_t*) self_cast;
   free(self->version);
   free(self);
}

dep_t* dep_new(char* name) {
   dep_t* self = (dep_t*) malloc(sizeof(dep_t));
   self->name = strdup(name);
   self->constraints = list_new();
   self->chosen = NULL;
   return self;
}

void dep_set_chosen(dep_t* self, entrydata_t* chosen) {
   if (self->chosen)
      return;
   self->chosen = chosen;
}

static void dep_add_constraint(dep_t* self, relation_t kind, char* version) {
   constraint_t* c = malloc(sizeof(constraint_t));
   c->kind = kind;
   c->version = strdup(version);
   list_put(self->constraints, 0, c);
}

static void dep_delete(void* self_cast) {
   dep_t* self = (dep_t*) self_cast;
   free(self->name);
   list_delete(self->constraints, constraint_delete);
   free(self);
}

bool dep_find(void* item_cast, void* sample_cast) {
   dep_t* item = (dep_t*) item_cast;
   char* sample = (char*) sample_cast;
   return (strcmp(item->name, sample) == 0);
}

// ---------------------------------------------------------------------------

static void depfile_process_depends(void* deps_cast, char* name, relation_t kind, char* version) {
   list_t* deps = (list_t*) deps_cast;
   dep_t* dep = list_find(deps, name, dep_find);
   if (!dep) {
      dep = dep_new(name);
      list_put(deps, 0, dep);
   }
   if (version)
      dep_add_constraint(dep, kind, version);
}

static void depfile_process_conflicts(void* deps_cast, char* name, relation_t kind, char* version) {
   list_t* deps = (list_t*) deps_cast;
   dep_t* dep = list_find(deps, name, dep_find);
   if (!dep) {
      dep = dep_new(name);
      list_put(deps, 0, dep);
   }
   if (version)
      dep_add_constraint(dep, relation_invert(kind), version);
}

// ---------------------------------------------------------------------------

#define TRY(x) if (!(x)) { return false; }

#define IS_NEXT_STR(at, s) ((*at) && (strcmp((s), (char*)((*at)->data)) == 0))

#define IS_NEXT(at, t) ((*at) && ((*at)->id == (t)))

#define MATCH_STR(at, s) if (IS_NEXT_STR(at, s)) { (*at) = (*at)->next; } else { return false; }

#define MATCH_OR_BREAK(at, t) if (IS_NEXT(at, t)) { (*at) = (*at)->next; } else { break; }

#define MATCH_ID(at, v) if (IS_NEXT(at, TK_ID)) { v = (*at)->data; (*at) = (*at)->next; } else { return false; }

#define MATCH(at, t) if (IS_NEXT(at, t)) { (*at) = (*at)->next; } else { return false; }

static bool depfile_parse_archs(listitem_t** at, bool* arch_ok) {
   *arch_ok = true;
   if (IS_NEXT(at, '[')) {
      bool neg_arch = false;
      MATCH(at, '[');
      if (IS_NEXT(at, '!')) {
         neg_arch = true;
         *arch_ok = true;
      } else {
         neg_arch = false;
         *arch_ok = false;
      }
      while (!IS_NEXT(at,']')) {
         char* arch = NULL;
         if (neg_arch) MATCH(at, '!');
         MATCH_ID(at, arch);
         if (strcmp(arch, ARCHITECTURE) == 0)
            *arch_ok = !neg_arch;
      }
      MATCH(at, ']');
   }
   return true;
}

static bool depfile_parse_app(listitem_t** at, depfile_parse_app_fn fn, void* data) {
   while (true) {
      char* name = NULL;
      char* version = NULL;
      bool relevant = true;
      relation_t kind;
      MATCH_ID(at, name);
      if (IS_NEXT(at, '(')) {
         MATCH(at, '(');
         if      (IS_NEXT(at, TK_LT)) { MATCH(at, TK_LT); kind = REL_LT; }
         else if (IS_NEXT(at, TK_GT)) { MATCH(at, TK_GT); kind = REL_GT; }
         else if (IS_NEXT(at, TK_GE)) { MATCH(at, TK_GE); kind = REL_GE; }
         else if (IS_NEXT(at, TK_LE)) { MATCH(at, TK_LE); kind = REL_LE; }
         else if (IS_NEXT(at, '='  )) { MATCH(at, '='); kind = REL_EQ; }
         MATCH_ID(at, version);
         MATCH(at, ')');
      }
      TRY(depfile_parse_archs(at, &relevant));
      if (relevant)
         fn(data, name, kind, version);
      MATCH_OR_BREAK(at, '|');
   }
   return true;
}

static bool depfile_parse_applist(listitem_t** at, list_t* deps, depfile_parse_app_fn fn) {
   while (true) {
      TRY(depfile_parse_app(at, fn, deps));
      MATCH_OR_BREAK(at, ',');
   }
   return true;
}

static bool depfile_parse_directive(listitem_t** at, list_t* deps, char* name, depfile_parse_app_fn fn) {
   MATCH_STR(at, name);
   MATCH(at, ':');
   TRY(depfile_parse_applist(at, deps, fn));
   return true;
}

static bool depfile_parse_file(list_t* tokens, list_t* deps) {
   listitem_t* aat = tokens->hd;
   listitem_t** at = &aat;
   while (aat) {
      if (IS_NEXT_STR(at, "Depends")) {
         TRY(depfile_parse_directive(at, deps, "Depends", depfile_process_depends));
      } else if (IS_NEXT_STR(at, "Conflicts")) {
         TRY(depfile_parse_directive(at, deps, "Conflicts", depfile_process_conflicts));
      } else
         return false;
   }
   return true;
}

list_t* depfile_parse(char* filename) {
   list_t* deps = list_new();
   FILE* file = fopen(filename, "r");
   if (!file) {
      fprintf(stderr, "viewfs: file not found: %s\n", filename);
      return NULL;
   }
   list_t* tokens = depfile_lexer(file);
   fclose(file);
   bool ok = depfile_parse_file(tokens, deps);
   if (!ok)
      fprintf(stderr, "viewfs: parse error: %s\n", filename);
   // TODO: test for errors
   list_delete(tokens, free);
   return deps;
}
