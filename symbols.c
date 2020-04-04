/* Emulator for LEGO RCX Brick, Copyright (C) 2003 Jochen Hoenicke.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; see the file COPYING.LESSER.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: symbols.c 86 2004-05-13 11:54:37Z hoenicke $
 */

#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "symbols.h"

struct symbol {
  uint16 addr;
  int16  type;
  char * name;
  struct symbol* left;
  struct symbol* right;
};


static struct symbol *root;

/*
// top down splay routine.
//
// If name is in the tree with root t, it will be moved to the
// top of the tree and the element is returned.
//
// If name is not in the tree a neighbour of name is moved to the
// top of the tree and NULL is returned.
//
// If tree is empty NULL is returned.
*/
static struct symbol *splay (uint16 addr, int16 type, struct symbol ** root) {
    struct symbol N, *t, *l, *r, *result;
    
    t = *root;
    if (t == NULL)
        return t;

    /* Fast path */
    if (t->addr == addr && t->type == type)
        return t;

    /* Slow path: Need to reorder tree */
    result = NULL;
    N.left = N.right = NULL;
    l = r = &N;

    for (;;) {
        if (addr < t->addr || (addr == t->addr && type < t->type)) {
            if (t->left == NULL) 
                break;
            
            if (addr < t->left->addr || 
                (addr == t->left->addr && type < t->left->type)) {
                struct symbol *y;
                y = t->left;                           /* rotate right */
                t->left = y->right;
                y->right = t;
                t = y;
                if (t->left == NULL) 
                    break;
            }

            r->left = t;                               /* link right */
            r = t;
            t = t->left;
        } else if (addr > t->addr || (addr == t->addr && type > t->type)) {
            if (t->right == NULL)
                break;

            if (addr > t->right->addr || 
                (addr == t->right->addr && type > t->right->type)) {
                struct symbol *y;
                y = t->right;                          /* rotate left */
                t->right = y->left;
                y->left = t;
                t = y;
                if (t->right == NULL) 
                    break;
            }
            l->right = t;                              /* link left */
            l = t;
            t = t->right;

        } else {
            /* we found the address */
            result = t;
            break;
        }
    }
    l->right = t->left;                                /* assemble */
    r->left = t->right;
    t->left = N.right;
    t->right = N.left;
    *root = t;
    return result;
}

void symbols_add(uint16 addr, int16 type, char* name) {
    struct symbol *sym;
    splay(addr, type, &root);

    sym = malloc(sizeof(struct symbol));
    sym->addr = addr;
    sym->type = type;
    
    if (root == NULL) {
        sym->left = sym->right = NULL;
    } else if (addr < root->addr || 
               (addr == root->addr && type <= root->type)) {
        sym->left  = root->left;
        sym->right = root;
        root->left = NULL;
    } else {
        sym->right  = root->right;
        sym->left = root;
        root->right = NULL;
    }
    root = sym;

    sym->name = name;
}
          
char *symbols_get(uint16 addr, int16 type) {
    struct symbol *sym = splay(addr, type, &root);
    if (sym)
        return sym->name;
    else
        return NULL;
}

static uint16 symbols_getaddr_subtree(char *name, struct symbol *sym) {
    uint16 addr;

    if (!sym)
        return 0;

    addr = symbols_getaddr_subtree(name, sym->left);
    if (addr)
        return addr;

    if (strcmp(name, sym->name) == 0)
        return sym->addr;

    return symbols_getaddr_subtree(name, sym->right);
}

uint16 symbols_getaddr(char *name) {
    return symbols_getaddr_subtree(name, root);
}

int symbols_remove(uint16 addr, int16 type) {
    struct symbol *sym = splay(addr, type, &root);
    if (sym) {

        if (!sym->left) {
            root = sym->right;
        } else {
            splay(addr, type, &sym->left);
            root = sym->left;
            root->right = sym->right;
        }

        free(sym->name);
        free(sym);
        return 1;
    }
    return 0;
}

void symbols_removeall() {
    struct symbol * sym = root;
    if (sym) {
        root = sym->right;
        symbols_removeall();
        root = sym->left;
        symbols_removeall();
        free(sym->name);
        free(sym);
        root = NULL;
    }
}

void symbols_iterate(symbols_iterate_func f, void *info) {
    struct symbol * sym = root;
    if (sym) {
        root = sym->left;
        symbols_iterate(f, info);

        f(info, sym->addr, sym->type, sym->name);

        root = sym->right;
        symbols_iterate(f, info);

        root = sym;
    }
}
