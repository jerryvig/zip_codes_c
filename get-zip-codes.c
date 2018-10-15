#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#define INPUT_FILE_NAME "county-list.csv"

typedef struct CountyNode {
	char state[8];
	char county[64];
	struct CountyNode* next;
} CountyNode;

int main(void) {
	FILE* input_file = fopen(INPUT_FILE_NAME, "r");
	if (!input_file) {
		perror("Failed to open input file '" INPUT_FILE_NAME "' for reading");
		exit(EXIT_FAILURE);
	}

	CountyNode* head = (CountyNode*)malloc(sizeof(struct CountyNode));
	CountyNode* current = head;

	char nullStr[128] = {'\0'};
	char buf[128];
	while (fgets(buf, sizeof buf, input_file) != NULL) {
		char* comma_start = strstr(buf, ",");
		strncpy(current->county, &comma_start[1], strlen(&comma_start[1]) - 1);
		strncpy(current->state, buf, strlen(buf) - strlen(comma_start));
		CountyNode* next = (CountyNode*)malloc(sizeof(struct CountyNode));

		printf("current->county = %s\n", current->county);

		current->next = next;
		current = next;
		strcpy(buf, nullStr);
	}

	current = head;
	while (1) {
		printf("state = %s\n", current->state);
		printf("county = %s\n", current->county);
		if (current->next == NULL) {
			break;
		}
		current = current->next;
	}

	fclose(input_file);
}
