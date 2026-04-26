#include <stdio.h>
#include <stdlib.h>

typedef struct ListNode {
	struct ListNode *next;
	int value;
} node_t;

// we store a pointer to the first element of our list as global variable
node_t *list_head = NULL;

static int insertElement(int value) {
	// if value is negative we return because we only store positive integer values
	if (value < 0) {
		return -1;
	}

	// iterate through our list from list_head to the end of the list (marked by the curr pointer becoming NULL)
	node_t *curr = list_head;
	while (curr != NULL) {
		// if we find a list element that already stores the current value we return with -1 since inserting was not successfull
		if (curr->value == value) {
			return -1;
		}

		// otherwise we go to the next element
		curr = curr->next;
	}

	// value is not in the list so we add it
	// first we allocate bytes to store one ListNode and check if allocation worked
	// if allocation failed we return -1 since inserting was not successful
	node_t *new_node = malloc(sizeof(node_t));
	if (new_node == NULL) {
		return -1;
	}

	// we make our new node the new current node and set its value and next variable
	// the new node then becomes the new list_head so that we have a FiLo structure and can easily remove the last added element in our remove method
	new_node->next = list_head;
	new_node->value = value;
	list_head = new_node;

	return value;
}

static int removeElement(void) {
	// if the list is empty there is nothing to do, and we return the error code -1
	if (list_head == NULL) {
		return -1;
	}

	// we store the pointer to the node that we want to delete and also safe the result value which is the value of the head of our list
	// we need to store this pointer because we first have to set the list_head to the node following the current list_head and then free the current list_head
	// otherwise we would lose access to the node we try to delete (memory leak) or we would access memory that was freed (use after free)
	// both is bad so we just don't do it :)))
	node_t *old_head = list_head;
	int result = old_head->value;

	// we make the element following the list head the new list head because the old list head will be removed
	list_head = old_head->next;

	// we give back the memory for the old list head to not have memory leaks
	free(old_head);

	return result;
}

int main(int argc, char *argv[]) {
	printf("insert 47: %d\n", insertElement(47));
	printf("insert 11: %d\n", insertElement(11));
	printf("insert 23: %d\n", insertElement(23));
	printf("insert 11: %d\n", insertElement(11));

	printf("remove: %d\n", removeElement());
	printf("remove: %d\n", removeElement());

	exit(EXIT_SUCCESS);
}
