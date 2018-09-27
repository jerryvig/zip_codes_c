#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INPUT_FILE_NAME "zip_code_list_nm_dona_ana.txt"
#define BASE_URL "http://www.city-data.com/zips/"

typedef struct ZipCode {
	char* code;
	struct ZipCode* next;
} ZipCode;

static FILE* openFile() {
	FILE* fp = fopen(INPUT_FILE_NAME, "r");
	if (!fp) {
		fprintf(stderr, "File %s not found. Exiting...\n", INPUT_FILE_NAME);
		exit(EXIT_FAILURE);
	}
	return fp;
}

static void closeFile(FILE* fp) {
	if (fclose(fp) == EOF) {
 		fprintf(stderr, "Failed to close file %s.\n", INPUT_FILE_NAME);
 		exit(EXIT_FAILURE);
	}
}

static ZipCode* loadLinkedList(FILE* fp) {
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
	prev->code = NULL;
	prev->next = NULL;

	return list_head;
}

static void freeLinkedList(ZipCode* list_head) {
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
	CURL *curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "Failed to initialize curl.\n");
		exit(EXIT_FAILURE);
	}

	FILE* fp = openFile();

	ZipCode *list_head = loadLinkedList(fp);

 	closeFile(fp);

	for (ZipCode *prev = list_head; prev->next != NULL; prev = prev->next ) {
		printf("%s \n", prev->code);
	}

	for (ZipCode *prev = list_head; prev->next != NULL; prev = prev->next ) {
		char url[64] = BASE_URL;
		strcat(url, prev->code);
		strcat(url, ".html");
		printf("Fetching url %s \n", url);

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		sleep(1);
	}

	freeLinkedList(list_head);

	curl_easy_cleanup(curl);

	return EXIT_SUCCESS;
}
