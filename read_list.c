#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <sds.h>
#include <unistd.h>

#define INPUT_FILE_NAME "zip_code_list_nm_dona_ana.txt"
#define BASE_URL "http://www.city-data.com/zips/"

typedef struct ZipCode {
	char* code;
	struct ZipCode* next;
} ZipCode;

typedef struct MemoryBuffer {
	char* memory;
	size_t size;
} MemoryBuffer;

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
		strcpy(prev->code, zip_code);
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

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t rsz = size * nmemb;
	MemoryBuffer* mem = (MemoryBuffer*)userp;
	char* ptr = realloc(mem->memory, mem->size + rsz + 1);

	if (ptr == NULL) {
		fprintf(stderr, "Insufficient memory (realloc returned NULL)...\n");
		return 0;
	}

	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, rsz);
	mem->size += rsz;
	mem->memory[mem->size] = 0;
	return rsz;
}

CURL* initCurl(void) {
	CURL* curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "Failed to initialize curl.\n");
		exit(EXIT_FAILURE);
	}
	return curl;
}

static void processLines(char* memory) {
	char* token = strtok(memory, "\n");
	while (token) {
		char* zip_pop = strstr(token, "Estimated zip code population in 2016:");
		char* zip_median_income = strstr(token, "Estimated median household income in 2016:");
		char* foreign_born_pop = strstr(token, "Foreign born population:"); 

		if (zip_pop != NULL) {
			char* zip_pop_b = strstr(zip_pop, "</b>");
			char* zip_pop_end = strstr(zip_pop_b, " ");
			char zip_pop_end_copy[10] = {'\0'};
			strncpy(zip_pop_end_copy, zip_pop_end, strlen(zip_pop_end) - 5);
			sds sds_zip_pop = sdsnew(zip_pop_end_copy);
			sdstrim(sds_zip_pop, " ");
			printf("zip population = %s\n", sds_zip_pop);
		}
		if (zip_median_income != NULL) {
			char* this_zip_code = strstr(zip_median_income, "This zip code:");
			char* this_zip_code_p = strstr(this_zip_code, "</p>");
			char* this_zip_code_p4 = &this_zip_code_p[4];
			char* p4_td = strstr(this_zip_code_p4, "</td>");
			char median_income_str[16] = {'\0'};
			strncpy(median_income_str, this_zip_code_p4, strlen(this_zip_code_p4) - strlen(p4_td));
			printf("median household income = %s\n", median_income_str);
		}
		if (foreign_born_pop != NULL) {
			char* fb_b = strstr(foreign_born_pop, "</b>");
			char* fb_b_open_parens = strstr(fb_b, "(");
			char* fb_b_close_parens = strstr(fb_b_open_parens, ")");
			char fb_out[10] = {'\0'};
			strncpy(fb_out, fb_b_open_parens, strlen(fb_b_open_parens) - strlen(fb_b_close_parens));
			char* fb_population = &fb_out[1];
			printf("foreign born population = %s\n", fb_population);
		}

		token = strtok(NULL, "\n");
	}
}

int main(void) {
	CURL *curl = initCurl();
	FILE* fp = openFile();

	ZipCode *list_head = loadLinkedList(fp);

 	closeFile(fp);

	for (ZipCode *prev = list_head; prev->next != NULL; prev = prev->next ) {
		printf("%s \n", prev->code);
	}

	MemoryBuffer* chunk;

	for (ZipCode *prev = list_head; prev->next != NULL; prev = prev->next ) {
		chunk = (MemoryBuffer*)malloc(sizeof(MemoryBuffer));
		chunk->memory = malloc(1);
		chunk->size = 0;

		char url[64] = BASE_URL;
		strcat(url, prev->code);
		strcat(url, ".html");
		printf("Fetching url %s \n", url);

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)chunk);

		CURLcode res = curl_easy_perform(curl);

		processLines(chunk->memory);

		if (res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		free(chunk->memory);
		free(chunk);

		usleep(1700000);
	}

	freeLinkedList(list_head);
	curl_easy_cleanup(curl);

	return EXIT_SUCCESS;
}
