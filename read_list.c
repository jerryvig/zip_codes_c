#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_FILE_NAME "zip_code_list_nm_dona_ana.txt"

typedef struct ZipCode {
	char* code;
	struct ZipCode* next;
} ZipCode;

FILE* openFile() {
	FILE* fp = fopen(INPUT_FILE_NAME, "r");
	if (!fp) {
		fprintf(stderr, "File %s not found. Exiting...\n", INPUT_FILE_NAME);
		exit(EXIT_FAILURE);
	}
	return fp;
}

void closeFile(FILE* fp) {
	if (fclose(fp) == EOF) {
 		fprintf(stderr, "Failed to close file %s.\n", INPUT_FILE_NAME);
 		exit(EXIT_FAILURE);
	}
}

ZipCode* loadLinkedList(FILE* fp) {
	char zip_code[8];
	ZipCode* list_head = (ZipCode*)malloc(sizeof(struct ZipCode));
	
	ZipCode* prev = list_head;
	for (; fscanf(fp, "%s", zip_code) != EOF; ) {
		prev->code = (char*)malloc(8*sizeof(char));
		memcpy(prev->code, zip_code, 8);
		ZipCode* next = (ZipCode*)malloc(sizeof(struct ZipCode));
		prev->next = next;
		prev = next;
	}
	prev->next = NULL;

	return list_head;
}

void freeLinkedList(ZipCode* list_head) {
	ZipCode* prevZip = list_head;
	while (1) {
		if (prevZip->next == NULL) {
			free(prevZip->code);
			free(prevZip);
			break;
		}

		free(prevZip->code);
		ZipCode* last = prevZip;	
		prevZip = prevZip->next;
		free(last);
	}
}

int main(void) {
	FILE* fp = openFile();

	ZipCode* list_head = loadLinkedList(fp);

 	closeFile(fp);

	for (ZipCode* prev = list_head; prev->next != NULL; prev = prev->next ) {
		printf("%s \n", prev->code);
	}

	freeLinkedList(list_head);

	return EXIT_SUCCESS;
}
