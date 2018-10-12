#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#define INPUT_FILE_NAME "county-list.csv"

int main(void) {
	FILE* input_file = fopen(INPUT_FILE_NAME, "r");
	if (!input_file) {
		perror("Failed to open input file '" INPUT_FILE_NAME "' for reading");
		exit(EXIT_FAILURE);
	}

	char buf[128];
	while (fgets(buf, sizeof buf, input_file) != NULL) {
		char* comma_start = strstr(buf, ",");
		char county_name[64] = {'\0'};
		char state[8] = {'\0'};

		strncpy(county_name, &comma_start[1], strlen(&comma_start[1]) - 1);
		strncpy(state, buf, strlen(buf) - strlen(comma_start));

		printf("county name = '%s'\n", county_name);
		printf("state = '%s'\n", state);
	}

	fclose(input_file);
}
