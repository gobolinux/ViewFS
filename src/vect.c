
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
#include "vect.h"

#include <malloc.h>
#include <stdint.h>
#include <assert.h>

vect_t* vect_new(int blocksize) {
   vect_t* self = malloc(sizeof(vect_t));
   self->array = NULL;
   self->used = 0;
   self->buffersize = 0;
   self->blocksize = blocksize;
   return self;
}

int64_t vect_add(vect_t* self, void* data) {
   if (self->array) {
      if (self->buffersize - self->used < 2) {
         self->buffersize = self->used + self->blocksize;
         self->array = realloc(self->array, self->buffersize * sizeof(void*));
      }
   } else
      self->array = malloc(self->blocksize * sizeof(void*));
   self->used++;
   int64_t id = self->used-1;
   self->array[id] = data;
   self->array[id+1] = NULL;
   return id;
}

void* vect_get(vect_t* self, int64_t id) {
   assert (id >= 0);
   if (id >= self->used)
      return NULL;
   return self->array[id];
}

void* vect_pop_last(vect_t* self) {
   if (self->used) {
      self->used--;
      return self->array[self->used];
   } else {
      return NULL;
   }
}
