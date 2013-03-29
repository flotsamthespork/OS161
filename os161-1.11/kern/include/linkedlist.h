#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

struct linkedlist {
	struct listelement *first;
	struct listelement *last;
};

struct listelement {
	struct linkedlist *list;
	struct listelement *next;
	struct listelement *previous;
	void *value;
};


struct linkedlist *ll_create();
struct listelement *ll_add_to_end(struct linkedlist *list, void *value);
struct listelement *ll_add_to_front(struct linkedlist *list, void *value);
struct listelement *ll_remove(struct listelement *el);
void ll_destroy(struct linkedlist *list);

#endif
