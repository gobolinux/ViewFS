
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
#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>

#include "cmdline.h"
#include "vect.h"
#include "entrydata.h"
#include "directfuse.h"
#include "depfile.h"
#include "version.h"
#include "vect.h"

#define LINE_WIDTH (PATH_MAX + 3)
#define MANIFEST_FILE "Manifest"
#define DEPENDENCIES_FILE "Dependencies"

static char* watch_dir;
static vect_t* watches;
static entrydata_t* packages_root_node;
static entrydata_t* tree_root_node;
static vect_t* id_to_v;
static vect_t* id_to_n;
static vect_t* free_ids;
static stringset_t* vn_to_id;
static list_t* depwait_list;
static int forgotten;

static void fill_with_view(entrydata_t* view) {
   char* manifest;
   char* package = view->u.view->package;
   char* version = view->u.view->version;
   asprintf(&manifest, "%s/%s/%s/%s", watch_dir, package, version, MANIFEST_FILE);
   stringset_t* root = tree_root_node->u.dir.entries;
   FILE* file = fopen(manifest, "r");
   if (!file)
      return;
   stringset_t* tree;
   char line[LINE_WIDTH + 1];
   line[LINE_WIDTH] = '\0';
   while (!feof(file)) {
      if (!fgets(line, LINE_WIDTH - 1, file))
         break;
      if (line[0] == '#' || line[0] == '\n' || strlen(line) < 3)
         continue;
      char* newline = strrchr(line, '\n');
      if (newline)
         *(newline) = '\0';
      char type = line[0];
      char* path = &(line[2]);

      tree = root;
      char* word = path;
      entrydata_t* added = NULL;
      char* slash;
      while (slash = strchr(word, '/')) {
         *(slash) = '\0';
         entrydata_t* entry = (entrydata_t*) stringset_get(tree, word);
         if (entry) {
            if (entry->type == ET_DIR) {
               tree = entry->u.dir.entries;
               if (!tree) {
                  tree = stringset_new(NULL);
                  entry->u.dir.entries = tree;
               }
            } else {
               tree = NULL;
               break;
            }
         } else {
            stringset_t* newtree = stringset_new(NULL);
            added = entrydata_new(ET_DIR, newtree);
            if (stringset_put(tree, word, added)) {
               tree = newtree;
            } else {
               tree = NULL;
               break;
            }
         }
         *(slash) = '/';
         word = slash + 1;
      }
      if (!tree)
         continue;
      switch (type) {
      case 'd':
         stringset_put(tree, word, entrydata_new(ET_DIR, NULL));
         break;
      default:
         {
            entrydata_t* entry;
            if (entry = stringset_get(tree, word)) {
               if (entry->type == ET_LINK)
                  entrydata_add_view_to_link(entry, view);
               else
                  fprintf(stderr, "viewfs: warning: %s/%s attempted to add %s as link (already directory)\n", package, version, path);
            } else {
               stringset_put(tree, word, entrydata_new(ET_LINK, view, path));
            }
            break;
         }
      }
   }
   fclose(file);
   free(manifest);
}

static bool match_constraints(char* version, list_t* constraints) {
   list_foreach(constraint_t, constraint, constraints) {
      int cmp = compare_versions(version, constraint->version);
      switch (constraint->kind) {
      case REL_EQ: if (cmp != 0) return false; break;
      case REL_NE: if (cmp == 0) return false; break;
      case REL_LT: if (cmp != 1) return false; break;
      case REL_GE: if (cmp == 1) return false; break;
      case REL_GT: if (cmp != -1) return false; break;
      case REL_LE: if (cmp == -1) return false; break;
      }
   }
   return true;
}

static entrydata_t* find_version(dep_t* dep) {
   entrydata_t* chosen = NULL;
   entrydata_t* package = stringset_get_i(packages_root_node->u.dir.entries, dep->name);
   entrydata_t* version;
   stringset_iter_t* iter = stringset_iter_new(package->u.dir.entries);
   while (version = (entrydata_t*) stringset_iter_next(iter)) {
      if (match_constraints(iter->key, dep->constraints)) {
         chosen = version;
         break;
      }
   }
   stringset_iter_delete(iter);
   return chosen;
}

void scan_dependencies(entrydata_t* scanning, entrydata_t* root_view) {
/*
// Some useful stats for debugging
fprintf(stderr, "scan_dependencies(%s/%s, %s/%s) - forgotten: %d - ids: %d - free_ids: %d\n",
scanning->u.view->package, scanning->u.view->version, root_view->u.view->package, root_view->u.view->version,
forgotten, id_to_v->used, free_ids->used);
*/
   if (!scanning->u.view->dep_rules) {
      char* depfile;
      asprintf(&depfile, "%s/%s/%s/%s", watch_dir, scanning->u.view->package, scanning->u.view->version, DEPENDENCIES_FILE);
      scanning->u.view->dep_rules = depfile_parse(depfile);
      free(depfile);
      if (!scanning->u.view->dep_rules)
         return; // No suitable dependencies file.
   }
   if (!root_view->u.view->priority_views)
      root_view->u.view->priority_views = list_new();
   list_t* chosen_deps = list_new();
   list_t* sub_deps = NULL;
   list_foreach(dep_t, dep_rule, scanning->u.view->dep_rules) {
      if (scanning != root_view && (list_find(root_view->u.view->dep_rules, dep_rule->name, dep_find) || strcasecmp(root_view->u.view->package, dep_rule->name) == 0)) {
         // Avoid redundancy/circular loops:
         // if dependency was already processed in the context of the root_view,
         // don't process it.
         // Note however that this compares based only on the package name,
         // i.e., currently, dependencies that appear lower on the tree
         // cannot enforce further constraints on which versions are accepted.
         continue;
      } else {
         if (!sub_deps)
            sub_deps = list_new();
         list_put(sub_deps, 0, dep_new(dep_rule->name));
      }
      entrydata_t* chosen = find_version(dep_rule);
      if (chosen) {
         list_put(chosen_deps, 0, chosen);
      } else {
         list_put(depwait_list, 0, depwait_new(dep_rule, root_view));
      }
   }
   if (sub_deps) {
      list_merge_contents_into(root_view->u.view->dep_rules, sub_deps);
      free(sub_deps);
   }
   list_merge_contents_into(root_view->u.view->priority_views, chosen_deps);
   list_foreach(entrydata_t, chosen_dep, chosen_deps) {
      scan_dependencies(chosen_dep, root_view);
   }
   free(chosen_deps);
}

static void add_view(entrydata_t* package_node, char* package, char* version) {
   entrydata_t* view = entrydata_new(ET_VIEW, package, version);
   entrydata_add_subentry(package_node, version, view);
   fill_with_view(view);

   depwait_t* depwait;
   list_iter_t* iter = list_iter_new(depwait_list);
   while (depwait = list_iterate_take(iter, package, depwait_find)) {
      if (match_constraints(version, depwait->dep->constraints)) {
         list_put(depwait->view->u.view->priority_views, 0, view);
         scan_dependencies(view, depwait->view);
      } else {
         list_prepend(depwait_list, 0, depwait);
      }
   }
   list_iter_delete(iter);
}

static void create_watch(char* location) {
   inodewatch_t* watch = inodewatch_new(location);
   if (!directfuse_add_watch(watch)) {
      char* pwd = getcwd(NULL, 0);
      fprintf(stderr, "viewfs: at %s, failed creating watch %s\n", pwd, location);
      free(pwd);
   }
   vect_add(watches, watch);
}

static void add_package(char* package) {
   char* dirname;
   asprintf(&dirname, "%s/%s", watch_dir, package);
   create_watch(dirname);

   entrydata_t* package_node = entrydata_new(ET_DIR, NULL);
   entrydata_add_subentry(packages_root_node, package, package_node);

   DIR* d = opendir(dirname);
   if (!d) return; /* ignore non-directories */
   struct dirent* version;
   while ( (version = readdir(d)) ) {
      if (version->d_name[0] == '.')
         continue;
      add_view(package_node, package, version->d_name);
   }
   closedir(d);
   free(dirname);
}

static void scan_watch_dir() {
   DIR* d = opendir(watch_dir);
   if (!d) {
      fprintf(stderr, "viewfs: could not perform initial scan on %s.\n", watch_dir);
      exit(1);
   }
   struct dirent* ent;
   while ( (ent = readdir(d)) ) {
      if (ent->d_name[0] == '.')
         continue;
      add_package(ent->d_name);
   }
   closedir(d);
}

void show(uint64_t i64) {
   char* c = (char*) &i64;
   for (int i = 0; i < 8; i++)
      fprintf(stderr, "%02x ", (unsigned char) *(c++));
   fprintf(stderr, "\n");
}

void id_to_view_node(uint64_t id, entrydata_t** view, entrydata_t** node) {
   *view = vect_get(id_to_v, id);
   *node = vect_get(id_to_n, id);
   if (*node && (*node)->type == ET_VIEW) {
      *view = *node;
      *node = tree_root_node;
   }
}

uint64_t view_node_to_id(entrydata_t* view, entrydata_t* node) {
   stringset_t* nodeset = stringset_get_int(vn_to_id, (int) view);
   if (!nodeset) {
      return 0;
   } else {
      uint64_t id = (uint64_t)(uint32_t) stringset_get_int(nodeset, (int) node);
      return id;
   }
}

uint64_t register_view_node(entrydata_t* view, entrydata_t* node) {
   uint64_t id;
   if (free_ids->used > 0) {
      id = (uint64_t)(uint32_t) vect_pop_last(free_ids);
      vect_set(id_to_v, id, view);
      vect_set(id_to_n, id, node);
   } else {
      vect_add(id_to_v, view);
      id = vect_add(id_to_n, node);
      if (id > 4294967295) {
         fprintf(stderr, "Integer overflow -- 32-bit kernels will truncate id\n");
      }
   }

   stringset_t* nodeset = stringset_get_int(vn_to_id, (int) view);
   if (!nodeset) {
      nodeset = stringset_new(NULL);
      stringset_put_int(vn_to_id, (int) view, nodeset);
   }

   stringset_put_int(nodeset, (int) node, (void*) (uint32_t) id);

   if (node && node->type == ET_VIEW) {
      scan_dependencies(node, node);
   }
   return id;
}

void view_inotify(struct inotify_event* event) {
   char* base = NULL;
   char* name = event->len ? event->name : NULL;
   if (event->wd > 0) {
      for (int i = 1; i < watches->used; i++) {
         inodewatch_t* w = (inodewatch_t*) watches->array[i];
         if (w->wd == event->wd) {
            base = w->name;
            break;
         }
      }
      if (!base) {
         fprintf(stderr, "viewfs: event on unregistered inode.\n");
         return;
      }
      base = strrchr(base, '/');
      assert(base);
      base++;
   }
   if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
      if (event->wd == 0) {
         add_package(name);
      } else {
         entrydata_t* package_node = stringset_get_i(packages_root_node->u.dir.entries, base);
         add_view(package_node, base, event->name);
      }
   } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
      // TODO: handle removal
      /*
      if (event->wd == 0) {
         entrydata_t* entry;
         entry = stringset_get(root_set, event->name);
         if (!entry || entry->type != ET_DIR)
            return;
         stringset_scan(entry->u.dir.entries, remove_view, event->name);
      } else {
         entrydata_t *baseentry, *entry;
         baseentry = stringset_get(root_set, base);
         if (!baseentry || baseentry->type != ET_DIR)
            return;
           entry = stringset_get(baseentry->u.dir.entries, event->name);
         if (!entry)
            return;
         remove_view(base, event->name, entry);
      }
      */
   }
}

int view_readlink(uint64_t id, char* buf, size_t bufsiz) {
   entrydata_t *view, *node;
   id_to_view_node(id, &view, &node);
   // TODO: detect a dead node
   if (node->type != ET_LINK) {
      return -EINVAL;
   }
   entrydata_t* chosen = NULL;
   if (view->u.view->priority_views) {
      list_foreach(entrydata_t, priority_view, view->u.view->priority_views) {
         for (int i = 0; node->u.link.view[i]; i++) {
            if (node->u.link.view[i] == priority_view) {
               chosen = node->u.link.view[i];
               break;
            }
         }
         if (chosen)
            break;
      }
   }
   if (!chosen)
      chosen = node->u.link.view[0];
   snprintf(buf, bufsiz, "%s/%s/%s/%s", watch_dir, chosen->u.view->package, chosen->u.view->version, node->u.link.path);

   return 0;
}

void view_forget(uint64_t id) {
   entrydata_t *view, *node;
   id_to_view_node(id, &view, &node);
   if (view == 0)
      return;
   id_to_v->array[id] = NULL;
   id_to_n->array[id] = NULL;
   vect_add(free_ids, (void*)(uint32_t) id);
   stringset_t* nodeset = stringset_get_int(vn_to_id, (int) view);
   stringset_remove_int(nodeset, (int) node, NULL);
   forgotten++;
}

int view_getdir(uint64_t id, dirbuffer_t* h, getdir_fn_t filler) {
   entrydata_t *view, *node;
   id_to_view_node(id, &view, &node);
   // TODO: detect a dead node
   if (node->type != ET_DIR) {
      return -EINVAL;
   }
   if (node->u.dir.entries) {
      stringset_iter_t* iter = stringset_iter_new(node->u.dir.entries);
      entrydata_t* item;
      while (item = (entrydata_t*) stringset_iter_next(iter)) {
         filler(h, iter->key, (item->type == ET_LINK ? DT_LNK : DT_DIR), -1);
      }
      stringset_iter_delete(iter);
   }
   return 0;
}

int view_getattr(uint64_t id, struct stat *stbuf) {
//fprintf(stderr, "GETATTR %lld.\n", id);
   entrydata_t *view, *node;
   id_to_view_node(id, &view, &node);
   memset(stbuf, 0, sizeof(struct stat));
   if (node->type == ET_LINK) {
      stbuf->st_mode = S_IFLNK | 0755;
      stbuf->st_nlink = 1;
   } else {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 1; // get_dir_link_count(id);
   }
   stbuf->st_ino = id;
   return 0;
}

#define is_root(path) ((path)[1] == '\0' && (path)[0] == '/')
int view_lookup(uint64_t id, char* name, uint64_t* result) {
   entrydata_t *view, *node;
   id_to_view_node(id, &view, &node);
   if (node->type != ET_DIR) {
      return -ENOENT;
   }
   entrydata_t* child = NULL;
   if (node->u.dir.entries)
      child = stringset_get(node->u.dir.entries, name);
   if (!child)
      return -ENOENT;

   *result = view_node_to_id(view, child);
   if (!*result)
      *result = register_view_node(view, child);

   return 0;
}

static directfuse_ops_t view_operations = {
   .lookup       = view_lookup,
   .getattr      = view_getattr,
   .getdir       = view_getdir,
   .readlink     = view_readlink,
   .inotify      = view_inotify,
   .forget       = view_forget
};

int main(int argc, char *argv[]) {
   char* mountpoint = NULL;
   int foreground = 0;
   parse_cmdline(argc, argv, &watch_dir, &mountpoint, &foreground);

   packages_root_node = entrydata_new(ET_DIR, stringset_new(NULL));
   tree_root_node = entrydata_new(ET_DIR, stringset_new(NULL));

   forgotten = 0;
   watches = vect_new(100);
   id_to_v = vect_new(10000);
   id_to_n = vect_new(10000);
   free_ids = vect_new(1000);
   vn_to_id = stringset_new();
   depwait_list = list_new();
   vect_add(watches, inodewatch_new(watch_dir));

   register_view_node(NULL, NULL); // id 0
   register_view_node(NULL, packages_root_node); // id 1
   directfuse_init(mountpoint, view_operations, (inodewatch_t**) watches->array);

   scan_watch_dir();

   directfuse_run(foreground);
   return 0;
}
