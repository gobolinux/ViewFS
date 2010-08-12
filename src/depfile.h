
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
#ifndef DEPFILE_H
#define DEPFILE_H

#include "list.h"
#include "stringset.h"
#include "entrydata.h"

typedef enum {
   REL_EQ,
   REL_NE,
   REL_GE,
   REL_GT,
   REL_LE,
   REL_LT
} relation_t;

typedef enum {
   TK_ERROR = 255,
   TK_ID,
   TK_GE,
   TK_LE,
   TK_LT,
   TK_GT,
} token_t;

// A version constraint
typedef struct constraint {
   // Relation to be applied to the version
   // (<, >, >=, ...).
   relation_t kind;
   // A version "number", as a string.
   // Example: "1.10k"
   char* version;
} constraint_t;

// Data about a specific program in a dependencies file.
typedef struct dep {
   // Dependency package name. Case is irrelevant.
   // Example: "ncurses"
   char* name;
   // List of constraint_t structures, specifying
   // version constraints to this particular package.
   // For a dependency match, all constraints must apply.
   list_t* constraints;
   // View chosen to fulfill this dependency.
   // Initially NULL. After a successful dependency resolution,
   // contains a reference to an item from views_list.
   entrydata_t* chosen;
} dep_t;

// Items for a list of dependencies that are waiting
// to be fulfilled.
typedef struct depwait {
   // Reference to the dependency data
   dep_t* dep;
   // Tree to be populated with items from this
   // dependency.
   entrydata_t* view;
} depwait_t;

typedef void (*depfile_parse_app_fn)(void*, char*, relation_t, char*);

depwait_t* depwait_new(dep_t* dep, entrydata_t* view);

bool depwait_find(void* item_cast, void* sample_cast);

void depwait_delete(void* self_cast);

dep_t* dep_new(char* name);

void dep_set_chosen(dep_t* self, entrydata_t* chosen);

bool dep_find(void* item_cast, void* sample_cast);

list_t* depfile_parse(char* filename);

#endif
