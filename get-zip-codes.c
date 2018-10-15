#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#define INPUT_FILE_NAME "county-list.csv"
#define BASE_URL "https://www.zip-codes.com/county/"
#define URL_SUFFIX ".asp"

typedef struct CountyNode {
	char state[8];
	char county[64];
	struct CountyNode* next;
} CountyNode;

typedef struct Memory {
  char* memory;
  size_t size;
} Memory;

static FILE* openInputFile() {
	FILE* input_file = fopen(INPUT_FILE_NAME, "r");
	if (!input_file) {
		perror("Failed to open input file '" INPUT_FILE_NAME "' for reading");
		exit(EXIT_FAILURE);
	}
	return input_file;
}

static CountyNode* loadLinkedList(FILE* input_file) {
	CountyNode* head = (CountyNode*)malloc(sizeof(struct CountyNode));
	char nullStr[128] = {'\0'};
	char buf[128];
	for (CountyNode* current = head; fgets(buf, sizeof buf, input_file) != NULL;) {
		char* comma_start = strstr(buf, ",");
		strncpy(current->county, &comma_start[1], strlen(&comma_start[1]) - 1);
		strncpy(current->state, buf, strlen(buf) - strlen(comma_start));
		CountyNode* next = (CountyNode*)malloc(sizeof(struct CountyNode));

		printf("current->county = %s\n", current->county);

		current->next = next;
		strcpy(buf, nullStr);
		current = next;
	}

	return head;
}

static void freeLinkedList(CountyNode* head) {
	CountyNode* next;
	for (CountyNode* current = head; current->next != NULL; current = next) {
		next = current->next;
		free(current);
	}
}

static char* buildUrl(char* state, char* county) {
	char nullStr[128] = {'\0'};
	char* dest = (char*)malloc(128 * sizeof(char));
	strcpy(dest, nullStr);
	strcpy(dest, BASE_URL);
	strcat(dest, state);
	strcat(dest, "-");

	for (size_t i=0; i<strlen(county); ++i) {
		if (county[i] == ' ') {
			county[i] = '-';
		}
	}
	strcat(dest, county);
	strcat(dest, URL_SUFFIX);
	return dest;
}

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void* userp) {
	size_t realsize = size * nmemb;
	Memory* memory = (Memory*)userp;

	char *ptr = realloc(memory->memory, memory->size + realsize + 1);
	if (ptr == NULL) {
		printf("Insufficient memory to reallocate. realloc() returned NULL.\n");
		return 0;
	}

	memory->memory = ptr;
	memcpy(&(memory->memory[memory->size]), contents, realsize);
	memory->size += realsize;
	memory->memory[memory->size] = 0;
	return realsize;
}

static CURL* initCurl() {
	curl_global_init(CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init();

	if (!curl) {
		fprintf(stderr, "Failed initialize curl.\n");
		exit( EXIT_FAILURE );
	}

	return curl;
}

int main(void) {
	FILE * input_file = openInputFile();
	CountyNode* head = loadLinkedList(input_file);
	CURL* curl = initCurl();
	CURLcode res;

	for (CountyNode* current = head; current->next != NULL; current = current->next) {
		printf("state = %s\n", current->state);
		printf("county = %s\n", current->county);
		char* url = buildUrl(current->state, current->county);

		Memory* chunk = (Memory*)malloc(sizeof(Memory));
		chunk->memory = malloc(1);
		chunk->size = 0;
		curl_easy_setopt(curl, CURLOPT_URL, url);

#ifdef SKIP_PEER_VERIFICATION
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif	
#ifdef SKIP_HOSTNAME_VERIFICATION
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)chunk);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
		}

		printf("chunk = %s\n", chunk->memory);

		if (current->next->next == NULL) {
			break;
		}

		free(chunk->memory);
		free(chunk);
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();	
	freeLinkedList(head);
	fclose(input_file);
}
