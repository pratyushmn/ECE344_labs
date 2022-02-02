#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"
#include "wc.h"

char* substringCopy(char* inputString, long start, long end) {
	/* 
	create a copy of a substring of inputString based on start and end indexes.
	start is the index before the slice
	end is the index after the slice 
	*/
	char* substr = (char*) malloc(sizeof(char) * (end - start)); // dynamically allocate memory for substring copy

	strncpy(substr, inputString + start + 1, end - start); // copy the substring over

	substr[end - start - 1] = '\0'; // ensure it's a null-terminating string

	return substr;
}

typedef struct node {
	/* linked list node structure for hash table. */
	char* word;
	unsigned long count;
	struct node* link;
} Node;

Node* newNode(char* word, long count, Node* link) {
	/* create a new linked list node. */
	Node* t = (Node*) malloc(sizeof(Node));

	if (t != NULL) {
		t -> word = word;
		t -> count = count;
		t -> link = link;
	}
	
	return t;
}

Node* insertWord(Node* head, char* word) {
	/* insert word at the end of linked list, or increment count if it's already there. */
	Node* curr = head;

	if (curr == NULL) { 
		// insert as head if the list is empty
		return newNode(word, 1, NULL);
	} else {
		// traverse list
		while (curr -> link != NULL) { 
			// if word is found, then increment count and return early
			if (strcmp(curr -> word, word) == 0) { 
				curr -> count += 1; 
				return head;
			}

			curr = curr -> link;
		}
		
		// if last node of list is the word we're looking for then increment the count there
		// if not, create new node for the word and add it at the end of the list
		if (strcmp(curr -> word, word) == 0) {
			curr -> count += 1;
		} else {
			curr -> link = newNode(word, 1, NULL);
		}
	}

	return head;
}

void printLinkedList(Node* head) {
	/* print out all word counts in a specific linked list. */
	Node* curr = head;

	// go through list and print word:count in the specified format
	while (curr != NULL) {
		printf("%s:%ld\n", curr -> word, curr -> count);
		curr = curr -> link;
	}
}

void freeLinkedList(Node* head) {
	/* free all allocated memory in a specific linked list. */
	Node* curr = head;

	// traverse list and free current node while saving reference to next node
	// need to free allocated space for the word before freeing the node
	while (curr != NULL) {
		Node* secondNode = curr->link;
		free(curr->word);
		free(curr);
		curr = secondNode;
	}
}

struct wc {
	/* you can define this struct to have whatever fields you want. */
	Node** hashTable; // an array of pointers to Linked List nodes
	int length;
};

void allocateHashTableSize(struct wc* wc, long wordCount) {
	/* 
	allocate hash table (array of pointers to linked lists) size based on rule of thumb
	(ie. size should be roughly twice the total number of elements expected)
	*/
	wc -> length = (unsigned long) 2 * wordCount;
	wc -> hashTable = (Node**) malloc(2 * wordCount * sizeof(Node*));

	// ensure all indexes are set to NULL
	for (int i = 0; i < wc -> length; i++) {
		wc -> hashTable[i] = NULL;
	}
}

unsigned long hashFunction(char* word) {
	/* 
	based on cyclic shift hash code
	source: https://www.cpp.edu/~ftang/courses/CS240/lectures/hashing.htm
	*/
	unsigned long hash = 0;

	for (int i = 0; i < strlen(word); i++) {
		hash = (((unsigned int) hash << 5) | ((unsigned int) hash >> 27)); // 5 bit cyclic shift of the running sum
		hash += (unsigned int) word[i];
	}

	return hash;
}

struct wc* wc_init(char *word_array, long size) {
	/* initialize the word counter structure. */
	struct wc* wc;
	wc = (struct wc*) malloc(sizeof(struct wc));
	assert(wc);

	long wordCount = 0; // running count of number of unique words in the text

	long firstSpace = -1;
	long secondSpace = -1;

	// count all words in the input text
	for (long i = 0; i < size; i++) {
		if ((isspace(word_array[i]) || word_array[i] == '\0')) {
			secondSpace = i;
			
			// check to make sure it's not just consecutive whitespace characters
			if (secondSpace > (firstSpace + 1)) wordCount++;
			
			firstSpace = secondSpace;
		}
	}

	// allocate hash table size based on total word count
	allocateHashTableSize(wc, wordCount);

	// loop through the text again
	// but this time, add words into the hash table as they occur
	firstSpace = -1;
	secondSpace = -1;

	for (long i = 0; i < size; i++) {
		if ((isspace(word_array[i]) || word_array[i] == '\0')) {
			secondSpace = i;
			
			// check to make sure it's not just consecutive whitespace characters
			// if not, add the word into the hash table
			if (secondSpace > (firstSpace + 1)) {
				char* substring = substringCopy(word_array, firstSpace, secondSpace);
				unsigned long idx = (hashFunction(substring)) % (wc -> length);
				(wc -> hashTable)[idx] = insertWord((wc -> hashTable)[idx], substring);
			}
			
			firstSpace = secondSpace;
		}
	}

	return wc;
}

void wc_output(struct wc *wc) {
	/* print word count in the specified output. */
	for (int i = 0; i < wc -> length; i++) {
		printLinkedList(wc -> hashTable[i]);
	}
}

void wc_destroy(struct wc *wc) {
	/* free all dynamically allocated memory in the word counter structure. */
	for (int i = 0; i < wc -> length; i++) {
		freeLinkedList(wc -> hashTable[i]);
	}

	free(wc -> hashTable);
	free(wc);
}