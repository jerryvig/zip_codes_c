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

	char buf[128];
	while (fgets(buf, sizeof buf, input_file) != NULL) {
		char* comma_start = strstr(buf, ",");
		char county_name[64] = {'\0'};
		char state[8] = {'\0'};

		
		strncpy(head->county, &comma_start[1], strlen(&comma_start[1]) - 1);
		strncpy(head->state, buf, strlen(buf) - strlen(comma_start));

		printf("county name = '%s'\n", head->county);
		printf("state = '%s'\n", head->state);
	}

	fclose(input_file);
}
