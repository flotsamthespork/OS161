#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <linkedlist.h>


struct linkedlist *ll_create() {
	struct linkedlist *list = kmalloc(sizeof(struct linkedlist));
	if (list == NULL) {
		return NULL;
	}
	list->first = NULL;
	list->last = NULL;
	return list;
}

struct listelement *ll_add_to_end(struct linkedlist *list, void *value) {
	struct listelement *el = kmalloc(sizeof(struct listelement));
	if (el == NULL) {
		return NULL;
	}
	el->value = value;
	el->next = NULL;
	el->list = list;
	el->previous = list->last;
	if (list->last != NULL) {
		list->last->next = el;
	} else {
		list->first = el;
	}
	list->last = el;
	return el;
}

struct listelement *ll_add_to_front(struct linkedlist *list, void *value) {
	struct listelement *el = kmalloc(sizeof(struct listelement));
	if (el == NULL) {
		return NULL;
	}
	el->value = value;
	el->previous = NULL;
	el->list = list;
	el->next = list->first;
	if (list->first != NULL) {
		list->first->previous = el;
	} else {
		list->last = el;
	}
	list->first = el;
	return el;
}

struct listelement *ll_remove(struct listelement *el) {
	struct linkedlist *list = el->list;
	struct listelement *next = el->next;
	if (list->first == el) {
		list->first = el->next;
	}
	if (list->last == el) {
		list->last = el->previous;
	}

	if (el->previous != NULL) {
		el->previous->next = el->next;
	}
	if (el->next != NULL) {
		el->next->previous = el->previous;
	}

	kfree(el);
	return next;
}

void ll_destroy(struct linkedlist *list) {
	struct listelement *this;
	struct listelement *next;
	next = list->first;
	while (next != NULL) {
		this = next;
		next = next->next;
		kfree(this);
	}
	kfree(list);
}
