#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "list.h"

list_t *list_init() {

    list_t *list = calloc(1, sizeof(list_t));

    return list;
}

struct list_elem *list_insert_after(list_t *list, struct list_elem *elem, void *data) {
    if(elem == NULL){
        return list_insert(list, data);
    }
    struct list_elem *list_elem1 = malloc(sizeof(struct list_elem));
    if (list_elem1 == NULL) {
        return NULL;
    }

    list_elem1->next = elem->next;
    list_elem1->data = data;
    elem->next = list_elem1;
    if (list_elem1->next == NULL) {
        list->last = list_elem1;
    }
    ++list->count;
    return list_elem1;
}

struct list_elem *list_insert(list_t *list, void *data) {
    struct list_elem *list_elem1 = malloc(sizeof(struct list_elem));
    if (list_elem1 == NULL) {
        return NULL;
    }

    list_elem1->next = list->first;
    list_elem1->data = data;
    list->first = list_elem1;
    if (list->last == NULL) {
        list->last = list_elem1;
    }
    ++list->count;
    return list_elem1;
}

struct list_elem *list_append(list_t *list, void *data) {
    struct list_elem *list_elem1 = malloc(sizeof(struct list_elem));
    if (list_elem1 == NULL) {
        return NULL;
    }

    list_elem1->data = data;
    list_elem1->next = NULL;

    if (list->last != NULL) {
        list->last->next = list_elem1;
    } else {
        list->first = list_elem1;
    }

    list->last = list_elem1;

    ++list->count;
    return list_elem1;
}

int list_remove(list_t *list, struct list_elem *elem) {
    for (struct list_elem *prev = NULL, *curr = list->first; curr; prev = curr, curr = curr->next) {
        if (curr == elem) {
            if (curr == list->first) {
                list->first = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (curr == list->last) {
                list->last = prev;
            }
            free(curr);
            --list->count;
            return 0;
        }
    }
    return -1;
}

void list_removeAll(list_t *list) {
    while (list->first != NULL) {
        list_remove(list, list->last);
    }
}

void list_finit(list_t *list) {
    if (list == NULL) return;
    list_removeAll(list);
    free(list);
}

struct list_elem *list_find(list_t *list, void *data, int (*cmp_elem)(const void *, const void *)) {
    struct list_elem *current_list_elem = list->first;
    while (current_list_elem != NULL && (*cmp_elem)(current_list_elem->data, data) != 0) {
        current_list_elem = current_list_elem->next;
    }
    return current_list_elem;
}

void list_print(list_t *list, void (*print_elem)(char *)) {
    struct list_elem *curr = list->first;
    for (int i = 1; curr != NULL; i++) {
        printf("%d:", i);
        (*print_elem)(curr->data);
        curr = curr->next;
    }
}

char *list_toString(list_t *list) {
    char *res = calloc(list->count + 1, sizeof(char));

    int i = 0;
    struct list_elem *current_list_elem = list->first;
    while (current_list_elem != NULL) {
        res[i] = *((char *) current_list_elem->data);
        //printf("%c", res[i]);
        current_list_elem = current_list_elem->next;
        i++;
    }

    res[i] = '\0';

    return res;
}

int list_length(list_t *list) {
    return list == NULL ? -1 : list->count;
}

void list_to_array(list_t *list, void *dataArray[]) {
    struct list_elem *curr = list->first;
    for (int i = 0; curr != NULL; i++) {
        dataArray[i] = curr->data;
        curr = curr->next;
    }
}

int list_updateCount(list_t *list) {
    list->count = 0;
    struct list_elem *curr = list->first;
    while (curr != NULL) {
        list->count++;
        curr = curr->next;
    }
    return list->count;
}