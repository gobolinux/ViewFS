
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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#define MIN(a,b) ((a)<(b)?(a):(b))

int* str_to_version_number(char* str) {
   int start = 0;
   int end = 0;
   int* values = calloc(sizeof(int), 4);
   values[0] = 3;
   int v = 1;
   while (str[start] != '\0') {
      if (v == values[0]) {
         values[0] *= 2;
         values = realloc(values, sizeof(int) * values[0]);
         for (int i = v; i < values[0]; i++)
            values[i] = 0;
      }
      while (isdigit(str[end]))
         end++;
      if (end != start) {
         int mag, c;
         unsigned int val = 0;
         for (c = end - 1, mag = 1; c >= start; c--, mag *= 10)
            val += (str[c] - '0') * mag;
         values[v] = val;
         start = end;
         v++;
         continue;
      }
      while (isalpha(str[end]))
         end++;
      if (end == start + 1) {
         values[v] = 0xff - str[start];
         start = end;
         v++;
         continue;
      }
      if (end != start) {
         int i;
         int val = 0;
         for (i = 0; i < 6; i++) {
            int n = i < (end - start)
                  ? toupper(str[start+i]) & 0x1f
                  : 0;
            val |= n;
            if (i < 5) val <<= 5;
         }
         values[v] = val - 0x7fffffff;
         start = end;
         v++;
         continue;
      }
      end++;
      start = end;
   }
   return values;
}

/*
Compares two version strings and applies some heuristics
to decide which refers to the most recent package.
Returns -1, 0, 1, respectively: whether v1 is more recent;
they are considered equivalent; or v2 is more recent. 
*/ 
int compare_versions(char* v1, char* v2) {
fprintf(stderr, "compare_versions(\"%s\", \"%s\")\n", v1, v2);
   int result = 0;
   int* a = str_to_version_number(v1);
   int* b = str_to_version_number(v2);
   for (int i = 1; i < MIN(a[0], b[0]); i++) {
      if (a[i] > b[i]) {
         result = -1;
         break;
      } else if (a[i] < b[i]) {
         result = 1;
         break;
      }
   }
   if (result == 0) {
      if (a[0] > b[0])
         result = -1;
      else if (a[0] < b[0])
         result = 1;
   }
   free(a);
   free(b);
   return result;
}
