#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>

#define INPUT_FILE_NAME "../data/county-list.csv"
#define OUTPUT_FILE_NAME "../data/zip-codes-list.csv"
#define SQLITE3_DB_NAME "../data/zip_codes_db.sqlite3"
#define BASE_URL "https://www.zip-codes.com/county/"
#define URL_SUFFIX ".asp"

typedef struct CountyNode {
	char state[8];
	char county[64];
	struct CountyNode* next;
} county_node_t;

typedef struct {
  char *memory;
  size_t size;
} memory_t;

typedef struct ZipCodeNode {
	char state[8];
	char county[64];
	char code[8];
	struct ZipCodeNode* next;
} zip_code_node_t;

static FILE* openInputFile() {
	FILE* input_file = fopen(INPUT_FILE_NAME, "r");
	if (!input_file) {
		perror("Failed to open input file '" INPUT_FILE_NAME "' for reading.");
		exit(EXIT_FAILURE);
	}
	return input_file;
}

static FILE* openOutputFile() {
	FILE* output_file = fopen(OUTPUT_FILE_NAME, "w");
	if (!output_file) {
		perror("Failed to open output file '" OUTPUT_FILE_NAME "' for writing.");
	}
	return output_file;
}

static void openDb(sqlite3** db) {
	const int rc = sqlite3_open(SQLITE3_DB_NAME, db);
	if ( rc != SQLITE_OK ) {
		fprintf(stderr, "Failed to open database " SQLITE3_DB_NAME);
		sqlite3_close( *db );
		exit( EXIT_FAILURE );
	} else {
		fprintf(stderr, "Opened database " SQLITE3_DB_NAME " for writing.\n");
	}
}

static void beginTransaction(sqlite3** db) {
	char* err = NULL;
	const int rc = sqlite3_exec(*db, "BEGIN TRANSACTION", NULL, NULL, &err);
	if ( rc != SQLITE_OK ) {
		fprintf(stderr, "Failed to BEGIN TRANSACTION.\n");
		sqlite3_free(err);
	}
}

static void commitTransaction(sqlite3** db) {
	char* err = NULL;
	const int rc = sqlite3_exec(*db, "COMMIT", NULL, NULL, &err);
	if ( rc != SQLITE_OK ) {
		fprintf(stderr, "Failed to COMMIT database transaction.\n");
		sqlite3_free(err);
	}
}

static void initDb(sqlite3** db) {
	beginTransaction(db);
	fprintf(stderr, "About to create the table named 'zip_codes'.\n");
	char *error_message = NULL;
	const char *create_stmt = "CREATE TABLE IF NOT EXISTS zip_codes_by_county ( "
		"zip_code INTEGER PRIMARY KEY, "
		"state TEXT, "
		"county TEXT );";
	const int rc = sqlite3_exec(*db, create_stmt, NULL, NULL, &error_message);
	if (rc != SQLITE_OK ) {
		fputs("Failed to create table.\n", stderr);
		fprintf(stderr, "error message = %s\n", error_message);
		sqlite3_free(error_message);
		sqlite3_free(*db);
		exit( EXIT_FAILURE );
	} else {
		commitTransaction(db);
		fprintf(stderr, "Table created successfully.\n");
	}
}

static void doInsert(sqlite3** db, char stmt[]) {
	char* err = NULL;
	fprintf(stderr, "running insert statement = '%s'\n", stmt);
	const int rc = sqlite3_exec(*db, stmt, NULL, NULL, &err);
	if ( rc != SQLITE_OK ) {
		fprintf(stderr, "Failed to exec insert stmt '%s' with error: %s.\n", stmt, err);
		sqlite3_free(err);
	}
}

static void loadLinkedList(FILE* input_file, county_node_t *head) {
	char buf[128];
	memset(head->state, 0, sizeof head->state);
	memset(head->county, 0, sizeof head->county);

	for (county_node_t *current = head; fgets(buf, sizeof buf, input_file) != NULL;) {
		char* comma_start = strstr(buf, ",");
		strncpy(current->county, &comma_start[1], strlen(&comma_start[1]) - 1);
		strncpy(current->state, buf, strlen(buf) - strlen(comma_start));
		county_node_t *next = (county_node_t*)malloc(sizeof(county_node_t));
                next->next = NULL;
		memset(next->state, 0, sizeof next->state);
		memset(next->county, 0, sizeof next->county);

		fprintf(stderr, "state, county = %s, %s\n", current->state, current->county);

		current->next = next;
		memset(buf, 0, sizeof buf);
		current = next;
	}
}

static void freeLinkedList(county_node_t *head) {
	county_node_t *next;
        county_node_t *current = head;
	for (; current->next != NULL; current = next) {
		next = current->next;
		free(current);
	}
        free(current);
}

static char* buildUrl(char* state, char* county) {
	char *dest = (char*)calloc(128, sizeof(char));
	strcpy(dest, BASE_URL);
	strcat(dest, state);
	strcat(dest, "-");

	for (size_t i = 0; i < strlen(county); ++i) {
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
	memory_t *memory = (memory_t*)userp;

	char *ptr = realloc(memory->memory, memory->size + realsize + 1);
	if (ptr == NULL) {
		fprintf(stderr, "Insufficient memory to reallocate. realloc() returned NULL.\n");
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
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "br, gzip");
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 180L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
    curl_easy_setopt(curl, CURLOPT_TCP_FASTOPEN, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 0);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
	return curl;
}

static void initZipCodeNode(zip_code_node_t *node) {
	node->next = NULL;
	memset(node->state, 0, sizeof node->state);
	memset(node->county, 0, sizeof node->county);
	memset(node->code, 0, sizeof node->code);
}

static void processChunk(char* memory, char state[], char county[], zip_code_node_t *zipHead) {
	char* token;
	char* rest = memory;

	zip_code_node_t *current = zipHead;

	while ((token = strtok_r(rest, "\n", &rest))) {
		char* zipCodeStr = strstr(token, "class=\"statTable\"");
		char* zipCodeTitle = NULL;

		if (zipCodeStr != NULL) {
			while((zipCodeTitle = strstr(zipCodeStr, "title=\"ZIP Code "))) {
				char code[8] = {'\0'};
				strncpy(code, &zipCodeTitle[16], 5);
				strcpy(current->state, state);
				strcpy(current->county, county);
				strcpy(current->code, code);

				zip_code_node_t *next = (zip_code_node_t*)malloc(sizeof(zip_code_node_t));
				initZipCodeNode(next);
				current->next = next;
				current = next;

				zipCodeStr = &zipCodeTitle[21];
			}
		}
	}
}

static void getUrl(CURL* curl, const char* url, char state[], char county[], zip_code_node_t *zipHead) {
	CURLcode res;
	memory_t *chunk = (memory_t*)malloc(sizeof(memory_t));
	chunk->memory = (char*)malloc(1);
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

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	}

	processChunk(chunk->memory, state, county, zipHead);

	free(chunk->memory);
	free(chunk);
}

int main(void) {
	FILE * input_file = openInputFile();
	FILE * output_file = openOutputFile();
	sqlite3* db = NULL;
	openDb(&db);
	initDb(&db);

	county_node_t *head = (county_node_t*)malloc(sizeof(county_node_t));
	loadLinkedList(input_file, head);
	CURL* curl = initCurl();

	for (county_node_t *current = head; current->next != NULL; current = current->next) {
		zip_code_node_t *zipCodesHead = (zip_code_node_t*)malloc(sizeof(zip_code_node_t));
		initZipCodeNode(zipCodesHead);

		fprintf(stderr, "state = \"%s\"\n", current->state);
		fprintf(stderr, "county = \"%s\"\n", current->county);

                if (strlen(current->state) < 1 || strlen(current->county) < 1) {
                    printf("breaking out of the loop\n");
                    break;
                }

		char* url = buildUrl(current->state, current->county);
		getUrl(curl, url, current->state, current->county, zipCodesHead);
		free(url);

		beginTransaction(&db);
		const char insert_fmt[] = "INSERT INTO zip_codes_by_county VALUES ( %s, \"%s\", \"%s\" );";

		zip_code_node_t *curZip;
		for (curZip = zipCodesHead; curZip != NULL;) {
			if (strlen(curZip->code) > 0) {
				fprintf(output_file, "\"%s\",\"%s\",\"%s\"\n",
					curZip->state, curZip->county, curZip->code);

                                char insert_stmt[64] = {'\0'};
				sprintf(insert_stmt, insert_fmt, curZip->code, curZip->state, curZip->county );
				doInsert(&db, insert_stmt);
			}
			zip_code_node_t *last = curZip;
			curZip = curZip->next;
			free(last);
		}

		free(curZip);
		commitTransaction(&db);

		if (current->next->next == NULL) {
			break;
		}

		usleep(1000000);
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();
	freeLinkedList(head);

	sqlite3_close(db);
	fclose(input_file);
	fclose(output_file);
}
