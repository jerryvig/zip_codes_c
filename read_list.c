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

typedef struct ZipCodeRecord {
	char* code;
	char* population;
	char* population2010;
	char* medianHouseholdIncome;
	char* foreignBornPopulation;
	char* medianHomePrice;
	char* landArea;
	char* medianResidentAge;
	char* whitePopulation;
	char* hispanicLatinoPopulation;
} ZipCodeRecord;

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

static void allocateZipCodeRecords(int32_t recordCount, ZipCodeRecord records[]) {
	char nullStr[12] = {'\0'};
	for (int32_t i = 0; i < recordCount; ++i) {
		records[i].code = (char*)malloc(8 * sizeof(char));
		records[i].population = (char*)malloc(10 * sizeof(char));
		strncpy(records[i].population, nullStr, 10);

		records[i].population2010 = (char*)malloc(10 * sizeof(char));
		strncpy(records[i].population2010, nullStr, 10);

		records[i].medianHouseholdIncome = (char*)malloc(12 * sizeof(char));
		strcpy(records[i].medianHouseholdIncome, nullStr);

		records[i].foreignBornPopulation = (char*)malloc(10 * sizeof(char));
		strncpy(records[i].foreignBornPopulation, nullStr, 10);

		records[i].medianHomePrice = (char*)malloc(12 * sizeof(char));
		strcpy(records[i].medianHomePrice, nullStr);

		records[i].landArea = (char*)malloc(10 * sizeof(char));
		strncpy(records[i].landArea, nullStr, 10);

		records[i].medianResidentAge = (char*)malloc(8 * sizeof(char));
		strncpy(records[i].medianResidentAge, nullStr, 10);

		records[i].whitePopulation = (char*)malloc(10 * sizeof(char));
		strncpy(records[i].whitePopulation, nullStr, 10);

		records[i].hispanicLatinoPopulation = (char*)malloc(10 * sizeof(char));
		strncpy(records[i].hispanicLatinoPopulation, nullStr, 10);
	}
}

static void processLines(char* memory, char* code, ZipCodeRecord* record) {
	char* token = strtok(memory, "\n");
	printf("zip code = %s\n", code);
	strcpy(record->code, code);

	while (token) {
		char* zip_pop = strstr(token, "Estimated zip code population in 2016:");
		char* zip_pop_2010 = strstr(token, "Zip code population in 2010:");
		char* zip_median_income = strstr(token, "Estimated median household income in 2016:");
		char* foreign_born_pop = strstr(token, "Foreign born population:");
		char* median_home_price = strstr(token, "Estimated median house or condo value in 2016:");
		char* land_area_base = strstr(token, "Land area:");
		char* median_age_base = strstr(token, "Median resident age:");
		char* white_pop_base = strstr(token, "White population");
		char* hispanic_pop_base = strstr(token, "Hispanic or Latino population");

		if (zip_pop != NULL) {
			char* zip_pop_b = strstr(zip_pop, "</b>");
			char* zip_pop_end = strstr(zip_pop_b, " ");
			char zip_pop_end_copy[10] = {'\0'};
			strncpy(zip_pop_end_copy, zip_pop_end, strlen(zip_pop_end) - 5);
			sds sds_zip_pop = sdsnew(zip_pop_end_copy);
			sdstrim(sds_zip_pop, " ");
			strcpy(record->population, sds_zip_pop);
			printf("zip population = %s\n", record->population);
		}

		if (zip_pop_2010 != NULL) {
			char* zp_2010_b = strstr(zip_pop_2010, "</b>");
			char* zp_2010_b_end = strstr(&zp_2010_b[4], "<");
			char zip_population_2010[10] = {'\0'};
			strncpy(zip_population_2010, &zp_2010_b[4], strlen(&zp_2010_b[4]) - strlen(zp_2010_b_end));
			strcpy(record->population2010, zip_population_2010);
			printf("zip population 2010 = %s\n", record->population2010);
		}
		
		if (zip_median_income != NULL) {
			char* this_zip_code = strstr(zip_median_income, "This zip code:");
			char* this_zip_code_p = strstr(this_zip_code, "</p>");
			char* this_zip_code_p4 = &this_zip_code_p[4];
			char* p4_td = strstr(this_zip_code_p4, "</td>");
			strncpy(record->medianHouseholdIncome, this_zip_code_p4, strlen(this_zip_code_p4) - strlen(p4_td));
			printf("median household income = %s\n", record->medianHouseholdIncome);
		}

		if (foreign_born_pop != NULL) {
			char* fb_b = strstr(foreign_born_pop, "</b>");
			char* fb_b_open_parens = strstr(fb_b, "(");
			char* fb_b_close_parens = strstr(fb_b_open_parens, ")");
			char fb_out[10] = {'\0'};
			strncpy(fb_out, fb_b_open_parens, strlen(fb_b_open_parens) - strlen(fb_b_close_parens));
			strcpy(record->foreignBornPopulation, &fb_out[1]);
			printf("foreign born population = %s\n", record->foreignBornPopulation);
		}

		if (median_home_price != NULL) {
			char* med_home_price_b = strstr(median_home_price, "</b>");
			sds med_home_price = sdsnew(&med_home_price_b[4]);
			sdstrim(med_home_price, "\r");
			strcpy(record->medianHomePrice, med_home_price);
			printf("med home price = %s\n", record->medianHomePrice);
		}

		if (land_area_base != NULL) {
			char* land_area_b = strstr(land_area_base, "</b>");
			char* sqmi_start = &land_area_b[5];
			char* sqmi_end = strstr(sqmi_start, " ");
			char sqmi[10] = {'\0'};
			strncpy(sqmi, sqmi_start, strlen(sqmi_start) - strlen(sqmi_end));
			strcpy(record->landArea, sqmi);
			printf("zip land area = %s\n", record->landArea);
		}

		if (median_age_base != NULL) {
			char* med_age_p = strstr(median_age_base, "</p>");
			if (med_age_p) {
				char* med_age_start = &med_age_p[4];
				char* med_age_space = strstr(med_age_start, " ");
				char median_age[8] = {'\0'};
				strncpy(median_age, med_age_start, strlen(med_age_start) - strlen(med_age_space));
				strcpy(record->medianResidentAge, median_age);
				printf("med age = %s\n", record->medianResidentAge);
			}
		}

		if (white_pop_base != NULL) {
			char* white_pop_start_1 = strstr(token, "'badge'>");
			char* gt = strstr(white_pop_start_1, ">");
			char* lt = strstr(white_pop_start_1, "<");
			char white_population[10] = {'\0'};
			strncpy(white_population, &gt[1], strlen(&gt[1]) - strlen(lt));
			strcpy(record->whitePopulation, white_population);
			printf("white population = %s\n", record->whitePopulation);
		}

		if (hispanic_pop_base != NULL) {
			char* h_pop_start_1 = strstr(token, "'badge'>");
			char* gt = strstr(h_pop_start_1, ">");
			char* lt = strstr(h_pop_start_1, "<");
			char hispanic_population[10] = {'\0'};
			strncpy(hispanic_population, &gt[1], strlen(&gt[1]) - strlen(lt));
			strcpy(record->hispanicLatinoPopulation, hispanic_population);
			printf("hispanic/latino population = %s\n", record->hispanicLatinoPopulation);
		}

		token = strtok(NULL, "\n");
	}
}

int main(void) {
	CURL *curl = initCurl();
	FILE* fp = openFile();

	ZipCode *list_head = loadLinkedList(fp);

 	closeFile(fp);

 	int32_t zip_code_count = 0;
	for (ZipCode *prev = list_head; prev->next != NULL; prev = prev->next ) {
		printf("%s \n", prev->code);
		zip_code_count++;
	}

	MemoryBuffer* chunk;
	ZipCodeRecord zipCodeRecords[zip_code_count];

	allocateZipCodeRecords(zip_code_count, zipCodeRecords);
	int32_t recordIndex = 0;

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

		processLines(chunk->memory, prev->code, &zipCodeRecords[recordIndex]);

		if (res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		free(chunk->memory);
		free(chunk);

		usleep(1700000);
		recordIndex++;
	}

	for (recordIndex = 0; recordIndex < zip_code_count; ++recordIndex) {
		printf("record.code = %s\n", zipCodeRecords[recordIndex].code);
		printf("record.population = %s\n", zipCodeRecords[recordIndex].population);
		printf("record.medianHouseholdIncome = %s\n", zipCodeRecords[recordIndex].medianHouseholdIncome);
	}

	freeLinkedList(list_head);
	curl_easy_cleanup(curl);

	return EXIT_SUCCESS;
}
