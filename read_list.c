#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <sds.h>
#include <unistd.h>
#include <sqlite3.h>

#define INPUT_FILE_NAME "zip_code_list_nm_chaves.txt"
#define OUTPUT_FILE_NAME "zip_code_data_nm_chaves.csv"
#define BASE_URL "http://www.city-data.com/zips/"
#define SQLITE3_DB_NAME "zip_codes_db.sqlite3"

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
	char* population2000;
	char* medianHouseholdIncome;
	char* foreignBornPopulation;
	char* medianHomePrice;
	char* landArea;
	char* medianResidentAge;
	char* malePercent;
	char* femalePercent;
	char* whitePopulation;
	char* hispanicLatinoPopulation;
	char* blackPopulation;
	char* asianPopulation;
	char* americanIndianPopulation;
	char* highSchool;
	char* bachelorsDegree;
	char* graduateDegree;
	char* averageHouseholdSize;
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

static void openDb(sqlite3** db) {
	int rc = sqlite3_open(SQLITE3_DB_NAME, db);
	if ( rc ) {
		fprintf(stderr, "Failed to open database " SQLITE3_DB_NAME);
		sqlite3_close( *db );
		exit( EXIT_FAILURE );
	} else {
		fprintf(stderr, "Opened database " SQLITE3_DB_NAME " for writing.\n");
	}
}

static void initDb(sqlite3** db) {
	fprintf(stderr, "About to create the table named 'zip_codes'.\n");
	char *error_message = NULL;
	char *create_stmt = "CREATE TABLE IF NOT EXISTS zip_codes ( "
		"zip_code INTEGER PRIMARY KEY, "
		"population INTEGER, "
		"population_2010 INTEGER, "
		"population_2000 INTEGER, "
		"land_area REAL, "
		"foreign_born_population REAL, "
		"median_household_income INTEGER, "
		"median_home_price INTEGER, "
		"median_resident_age REAL, "
		"white_population INTEGER, "
		"hispanic_population INTEGER, "
		"black_population INTEGER, "
		"asian_population INTEGER, "
		"american_indian_population INTEGER, "
		"high_school REAL, "
		"bachelors_degree REAL, "
		"graduate_degree REAL, "
		"male_percent REAL, "
		"female_percent REAL, "
		"average_household_size REAL );";
	int rc = sqlite3_exec(*db, create_stmt, NULL, NULL, &error_message);
	if ( rc != SQLITE_OK ) {
		fputs("SOME SQL ERROR OCURRED.\n", stderr);
		fprintf(stderr, "error message = %s\n", error_message);
		sqlite3_free(error_message);
		sqlite3_close( *db );
		exit( EXIT_FAILURE );
	} else {
		fprintf(stderr, "Table 'zip_codes' created successfully.\n");
	}
}

static void beginTransaction(sqlite3* db) {
	char *error_message = NULL;
	int rc = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &error_message);
	if ( rc != SQLITE_OK ) {
		fprintf(stderr, "Failed to 'BEGIN TRANSACTION' with error: %s\n", error_message);
		sqlite3_free(error_message);
	}
}

static void commitTransaction(sqlite3* db) {
	char *error_message = NULL;
	int rc = sqlite3_exec(db, "COMMIT", NULL, NULL, &error_message);
	if ( rc != SQLITE_OK ) {
		fprintf(stderr, "Failed to commit transaction with error: %s\n", error_message);
		sqlite3_free(error_message);
	}
}

static void doInsert(sqlite3* db, char* stmt) {
	char *error_message = NULL;
	printf("insert stmt = %s\n", stmt);
	int rc = sqlite3_exec(db, stmt, NULL, NULL, &error_message);
	if ( rc != SQLITE_OK ) {
		fprintf(stderr, "Failed to execute insert stmt with error: %s\n", error_message);
		sqlite3_free(error_message);
	}
}

static void removeCommasFromNumber(char* output, char* input) {
	char *rest = input;
	char *token = strtok_r(rest, ",", &rest);
	while (token) {
		strcat(output, token);
		token = strtok_r(rest, ",", &rest);
	} 
	if (strlen(output) == 0) {
		strcat(output, "0");
	}
}

static void percentToFraction(char* output, char* input) {
	if (strlen(input) == 0) {
		output = "0.0";
		return;
	}

	char intermediate[8] = {'\0'};
	char* end;
	strncpy(intermediate, input, strlen(input) - 1);
	float intermediateFloat = strtof(intermediate, &end);
	intermediateFloat /= 100.0f;
	sprintf(output, "%.4f", intermediateFloat);
}

static void allocateRecordField(char** field, size_t size, char initialValue[]) {
	char nullStr[12] = {'\0'};
	*field = (char*)malloc(size * sizeof(char));
	strncpy(*field, nullStr, size);
	if (initialValue != NULL) {
		strcpy(*field, initialValue);
	}
}

static void allocateZipCodeRecords(int32_t recordCount, ZipCodeRecord records[]) {
	for (int32_t i = 0; i < recordCount; ++i) {
		allocateRecordField(&records[i].code, 8, NULL);
		allocateRecordField(&records[i].population, 10, "0");
		allocateRecordField(&records[i].population2010, 10, "0");
		allocateRecordField(&records[i].population2000, 10, "0");
		allocateRecordField(&records[i].medianHouseholdIncome, 12, "0");
		allocateRecordField(&records[i].foreignBornPopulation, 10, "0.0");
		allocateRecordField(&records[i].medianHomePrice, 12, "0");
		allocateRecordField(&records[i].landArea, 10, "0");
		allocateRecordField(&records[i].medianResidentAge, 8, "0.0");
		allocateRecordField(&records[i].whitePopulation, 10, "0");
		allocateRecordField(&records[i].hispanicLatinoPopulation, 10, "0");
		allocateRecordField(&records[i].blackPopulation, 10, "0");
		allocateRecordField(&records[i].asianPopulation, 10, "0");
		allocateRecordField(&records[i].americanIndianPopulation, 10, "0");
		allocateRecordField(&records[i].highSchool, 8, "0.0");
		allocateRecordField(&records[i].bachelorsDegree, 8, "0.0");
		allocateRecordField(&records[i].graduateDegree, 8, "0.0");
		allocateRecordField(&records[i].malePercent, 8, "0.0");
		allocateRecordField(&records[i].femalePercent, 8, "0.0");
		allocateRecordField(&records[i].averageHouseholdSize, 8, "0.0");
	}
}

static void processLines(char* memory, char* code, ZipCodeRecord* record) {
	char* token = strtok(memory, "\n");
	char nullStr[24] = {'\0'};
	printf("zip code = %s\n", code);
	strcpy(record->code, code);

	while (token) {
		char* zip_pop = strstr(token, "Estimated zip code population in 2016:");
		char* zip_pop_2010 = strstr(token, "Zip code population in 2010:");
		char* zip_pop_2000 = strstr(token, "Zip code population in 2000:");
		char* zip_median_income = strstr(token, "Estimated median household income in 2016:");
		char* foreign_born_pop = strstr(token, "Foreign born population:");
		char* median_home_price = strstr(token, "Estimated median house or condo value in 2016:");
		char* land_area_base = strstr(token, "Land area:");
		char* median_age_base = strstr(token, "Median resident age:");
		char* white_pop_base = strstr(token, "White population");
		char* hispanic_pop_base = strstr(token, "Hispanic or Latino population");
		char* black_pop_base = strstr(token, "Black population");
		char* asian_pop_base = strstr(token, "Asian population");
		char* american_indian_pop_base = strstr(token, "American Indian population");
		char* high_school_base = strstr(token, "High school or higher:");
		char* bachelors_degree_base = strstr(token, "Bachelor's degree or higher:");
		char* graduate_degree_base = strstr(token, "Graduate or professional degree:");
		char* male_base = strstr(token, "Males:");
		char* female_base = strstr(token, "Females:");
		char* avg_household_base = strstr(token, "Average household size:");

		if (zip_pop != NULL) {
			char* zip_pop_b = strstr(zip_pop, "</b>");
			char* zip_pop_end = strstr(zip_pop_b, " ");
			char zip_pop_end_copy[10] = {'\0'};
			strncpy(zip_pop_end_copy, zip_pop_end, strlen(zip_pop_end) - 5);
			sds sds_zip_pop = sdsnew(zip_pop_end_copy);
			sdstrim(sds_zip_pop, " ");

			char *noComma = (char*)malloc((strlen(sds_zip_pop) + 1) * sizeof(char));
			strncpy(noComma, nullStr, strlen(sds_zip_pop) + 1);
			removeCommasFromNumber(noComma, sds_zip_pop);

			strcpy(record->population, noComma);
			printf("zip population = %s\n", record->population);
			sdsfree(sds_zip_pop);
			free(noComma);
		}

		if (zip_pop_2010 != NULL) {
			char* zp_2010_b = strstr(zip_pop_2010, "</b>");
			char* zp_2010_b_end = strstr(&zp_2010_b[4], "<");
			char zip_population_2010[10] = {'\0'};
			strncpy(zip_population_2010, &zp_2010_b[4], strlen(&zp_2010_b[4]) - strlen(zp_2010_b_end));
			sds sds_zp_2010 = sdsnew(zip_population_2010);
			sdstrim(sds_zp_2010, " ");

			char *noComma = (char*)malloc((strlen(sds_zp_2010) + 1) * sizeof(char));
			strncpy(noComma, nullStr, strlen(sds_zp_2010) + 1);
			removeCommasFromNumber(noComma, sds_zp_2010);

			strcpy(record->population2010, noComma);
			printf("zip population 2010 = %s\n", record->population2010);
			sdsfree(sds_zp_2010);
			free(noComma);
		}

		if (zip_pop_2000 != NULL) {
			char* zp_2000_b = strstr(zip_pop_2000, "</b>");
			char* zp_2000_b_end = strstr(&zp_2000_b[4], "<");
			char zip_population_2000[10] = {'\0'};
			strncpy(zip_population_2000, &zp_2000_b[4], strlen(&zp_2000_b[4]) - strlen(zp_2000_b_end));
			sds sds_zp_2000 = sdsnew(zip_population_2000);
			sdstrim(sds_zp_2000, " ");

			char *noComma = (char*)malloc((strlen(sds_zp_2000) + 1) * sizeof(char));
			strncpy(noComma, nullStr, strlen(sds_zp_2000) + 1);
			removeCommasFromNumber(noComma, sds_zp_2000);

			strcpy(record->population2000, noComma);
			printf("zip population 2000 = %s\n", record->population2000);
			sdsfree(sds_zp_2000);
			free(noComma);
		}
		
		if (zip_median_income != NULL) {
			char* this_zip_code = strstr(zip_median_income, "This zip code:");
			char* this_zip_code_p = strstr(this_zip_code, "</p>");
			char* this_zip_code_p4 = &this_zip_code_p[4];
			char* p4_td = strstr(this_zip_code_p4, "</td>");
			char mhi[12] = {'\0'};
			strncpy(mhi, this_zip_code_p4, strlen(this_zip_code_p4) - strlen(p4_td));
			sds median_income_trim = sdsnew(mhi);
			sdstrim(median_income_trim, "\r");

			char *noComma = (char*)malloc((strlen(median_income_trim) + 1) * sizeof(char));
			strncpy(noComma, nullStr, strlen(median_income_trim) + 1);
			removeCommasFromNumber(noComma, &median_income_trim[1]);

			strcpy(record->medianHouseholdIncome, noComma);
			printf("median household income = %s\n", record->medianHouseholdIncome);
			sdsfree(median_income_trim);
			free(noComma);
		}

		if (foreign_born_pop != NULL) {
			char* fb_b = strstr(foreign_born_pop, "</b>");
			char* fb_b_open_parens = strstr(fb_b, "(");
			char* fb_b_close_parens = strstr(fb_b_open_parens, ")");
			char fb_out[10] = {'\0'};
			strncpy(fb_out, fb_b_open_parens, strlen(fb_b_open_parens) - strlen(fb_b_close_parens));
			char fb_fraction[10] = {'\0'};
			percentToFraction(fb_fraction, &fb_out[1]);
			strcpy(record->foreignBornPopulation, fb_fraction);
			printf("foreign born population = %s\n", record->foreignBornPopulation);
		}

		if (median_home_price != NULL) {
			char* med_home_price_b = strstr(median_home_price, "</b>");
			sds med_home_price = sdsnew(&med_home_price_b[4]);
			sdstrim(med_home_price, "\r");

			char *noComma = (char*)malloc((strlen(med_home_price) + 1) * sizeof(char));
			strncpy(noComma, nullStr, strlen(med_home_price) + 1);
			removeCommasFromNumber(noComma, &med_home_price[1]);

			strcpy(record->medianHomePrice, noComma);
			printf("med home price = %s\n", record->medianHomePrice);
			sdsfree(med_home_price);
			free(noComma);
		}

		if (land_area_base != NULL) {
			char* land_area_b = strstr(land_area_base, "</b>");
			char* sqmi_start = &land_area_b[5];
			char* sqmi_end = strstr(sqmi_start, " ");
			char sqmi[10] = {'\0'};
			strncpy(sqmi, sqmi_start, strlen(sqmi_start) - strlen(sqmi_end));

			char *noComma = (char*)malloc((strlen(sqmi) + 1) * sizeof(char));
			strncpy(noComma, nullStr, strlen(sqmi) + 1);
			removeCommasFromNumber(noComma, sqmi);

			strcpy(record->landArea, noComma);
			printf("zip land area = %s\n", record->landArea);
			free(noComma);
		}

		if (median_age_base != NULL) {
			char* med_age_p = strstr(median_age_base, "</p>");
			if (med_age_p) {
				char* med_age_start = &med_age_p[4];
				char* med_age_space = strstr(med_age_start, " ");
				char median_age[8] = {'\0'};
				strncpy(median_age, med_age_start, strlen(med_age_start) - strlen(med_age_space));
				strcpy(record->medianResidentAge, median_age);
				printf("median age = %s\n", record->medianResidentAge);
			}
		}

		if (white_pop_base != NULL) {
			char* white_pop_start_1 = strstr(token, "'badge'>");
			if (white_pop_start_1) {
				char* gt = strstr(white_pop_start_1, ">");
				char* lt = strstr(white_pop_start_1, "<");
				char white_population[10] = {'\0'};
				strncpy(white_population, &gt[1], strlen(&gt[1]) - strlen(lt));

				char *noComma = (char*)malloc((strlen(white_population) + 1) * sizeof(char));
				strncpy(noComma, nullStr, strlen(white_population) + 1);
				removeCommasFromNumber(noComma, white_population);

				strcpy(record->whitePopulation, noComma);
				printf("white population = %s\n", record->whitePopulation);
				free(noComma);
			}
		}

		if (hispanic_pop_base != NULL) {
			char* h_pop_start_1 = strstr(token, "'badge'>");
			if (h_pop_start_1) {
				char* gt = strstr(h_pop_start_1, ">");
				char* lt = strstr(h_pop_start_1, "<");
				char hispanic_population[10] = {'\0'};
				strncpy(hispanic_population, &gt[1], strlen(&gt[1]) - strlen(lt));

				char *noComma = (char*)malloc((strlen(hispanic_population) + 1) * sizeof(char));
				strncpy(noComma, nullStr, strlen(hispanic_population) + 1);
				removeCommasFromNumber(noComma, hispanic_population);

				strcpy(record->hispanicLatinoPopulation, noComma);
				printf("hispanic/latino population = %s\n", record->hispanicLatinoPopulation);
				free(noComma);
			}
		}

		if (black_pop_base != NULL) {
			char* b_pop_start = strstr(token, "'badge'>");
			if (b_pop_start) {
				char* gt = strstr(b_pop_start, ">");
				char* lt = strstr(b_pop_start, "<");
				char black_population[10] = {'\0'};
				strncpy(black_population, &gt[1], strlen(&gt[1]) - strlen(lt));

				char *noComma = (char*)malloc((strlen(black_population) + 1) * sizeof(char));
				strncpy(noComma, nullStr, strlen(black_population) + 1);
				removeCommasFromNumber(noComma, black_population);

				strcpy(record->blackPopulation, noComma);
				printf("black population = %s\n", record->blackPopulation);
				free(noComma);
			}
		}

		if (asian_pop_base != NULL) {
			char* asian_pop_start = strstr(token, "'badge'>");
			if (asian_pop_start) {
				char* gt = strstr(asian_pop_start, ">");
				char* lt = strstr(&gt[1], "<");
				char asian_population[10] = {'\0'};
				strncpy(asian_population, &gt[1], strlen(&gt[1]) - strlen(lt));

				char *noComma = (char*)malloc((strlen(asian_population) + 1) * sizeof(char));
				strncpy(noComma, nullStr, strlen(asian_population) + 1);
				removeCommasFromNumber(noComma, asian_population);

				strcpy(record->asianPopulation, noComma);
				printf("asian population = %s\n", record->asianPopulation);
				free(noComma);
			}
		}

		if (american_indian_pop_base != NULL) {
			char* american_indian_start = strstr(token, "'badge'>");
			if (american_indian_start) {
				char* gt = strstr(american_indian_start, ">");
				char* lt = strstr(&gt[1], "<");
				char americanIndianPop[10] = {'\0'};
				strncpy(americanIndianPop, &gt[1], strlen(&gt[1]) - strlen(lt));

				char *noComma = (char*)malloc((strlen(americanIndianPop) + 1) * sizeof(char));
				strncpy(noComma, nullStr, strlen(americanIndianPop) + 1);
				removeCommasFromNumber(noComma, americanIndianPop);

				strcpy(record->americanIndianPopulation, noComma);
				printf("american indian population = %s\n", record->americanIndianPopulation);
				free(noComma);
			}
		}

		if (high_school_base != NULL) {
			char* hs_b = strstr(high_school_base, "</b>");
			char* lt = strstr(&hs_b[5], "<");
			char hs_val[8] = {'\0'};
			strncpy(hs_val, &hs_b[5], strlen(&hs_b[5]) - strlen(lt));

			char hs_fraction[8] = {'\0'};
			percentToFraction(hs_fraction, hs_val);

			strcpy(record->highSchool, hs_fraction);
			printf("high school = %s\n", record->highSchool);
		}

		if (bachelors_degree_base != NULL) {
			char* bs_degree_b = strstr(bachelors_degree_base, "</b>");
			char* lt = strstr(&bs_degree_b[5], "<");
			char bs_degree_val[8] = {'\0'};
			strncpy(bs_degree_val, &bs_degree_b[5], strlen(&bs_degree_b[5]) - strlen(lt));

			char bs_fraction[8] = {'\0'};
			percentToFraction(bs_fraction, bs_degree_val);

			strcpy(record->bachelorsDegree, bs_fraction);
			printf("bachelors degree pct = %s\n", record->bachelorsDegree);
		}

		if (graduate_degree_base != NULL) {
			char* graduate_degree_b = strstr(graduate_degree_base, "</b>");
			char* lt = strstr(&graduate_degree_b[5], "<");
			char graduate_degree_val[8] = {'\0'};
			strncpy(graduate_degree_val, &graduate_degree_b[5], strlen(&graduate_degree_b[5]) - strlen(lt));

			char graduate_degree_fraction[8] = {'\0'};
			percentToFraction(graduate_degree_fraction, graduate_degree_val);

			strcpy(record->graduateDegree, graduate_degree_fraction);
			printf("graduate degree = %s\n", record->graduateDegree);
		}

		if (male_base != NULL) {
			char* male_parens = strstr(male_base, "&nbsp;(");
			char* close_parens = strstr(&male_parens[7], ")");
			char male_val[8] = {'\0'};
			strncpy(male_val, &male_parens[7], strlen(&male_parens[7]) -  strlen(close_parens));
			
			char male_fraction[8] = {'\0'};
			percentToFraction(male_fraction, male_val);

			strcpy(record->malePercent, male_fraction);
			printf("male percent = %s\n", record->malePercent);
		}

		if (female_base != NULL) {
			char* female_parens = strstr(female_base, "&nbsp;(");
			char* close_parens = strstr(&female_parens[7], ")");
			char female_val[8] = {'\0'};
			strncpy(female_val, &female_parens[7], strlen(&female_parens[7]) -  strlen(close_parens));
			
			char female_fraction[8] = {'\0'};
			percentToFraction(female_fraction, female_val);

			strcpy(record->femalePercent, female_fraction);
			printf("female percent = %s\n", record->malePercent);
		}

		if (avg_household_base != NULL) {
			char* avg_household_p = strstr(avg_household_base, "</p>");
			char* avg_household_peeps = strstr(&avg_household_p[4], " people");
			char avg_household_size[8] = {'\0'};
			strncpy(avg_household_size, &avg_household_p[4],
				strlen(&avg_household_p[4]) - strlen(avg_household_peeps));
			strcpy(record->averageHouseholdSize, avg_household_size);
			printf("avg household size = %s\n", record->averageHouseholdSize);
		}

		token = strtok(NULL, "\n");
		if (!token) {
			break;
		}
	}
}

int main(void) {
	CURL *curl = initCurl();
	FILE* fp = openFile();
	sqlite3* db = NULL;

	openDb(&db);
	initDb(&db);

	ZipCode *list_head = loadLinkedList(fp);

 	closeFile(fp);

 	int32_t zip_code_count = 0;
	for (ZipCode *prev = list_head; prev->next != NULL; prev = prev->next ) {
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

	FILE* outputFile = fopen(OUTPUT_FILE_NAME, "w");
	fputs("\"Zip Code\",\"Population 2016\",\"Population 2010\",\"Population 2000\",\"Land Area\","
		"\"Foreign Born Population\",\"Median Household Income\",\"Median Home Price\","
		"\"Median Resident Age\",\"White Population\",\"Hispanic/Latino Population\","
		"\"Black Population\",\"Asian Population\",\"American Indian Population\","
		"\"High School Diploma\",\"Bachelor's Degree\",\"Graduate Degree\",\"Male Percent\","
		"\"Female Percent\",\"Average Household Size\"\n", 
		outputFile);

	beginTransaction(db);
	char insert_format[] = "INSERT INTO zip_codes VALUES ("
		" %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s );";
	char insert_stmt[256] = {'\0'};

	for (recordIndex = 0; recordIndex < zip_code_count; ++recordIndex) {
		fprintf(outputFile,
			"\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\""
			",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
			zipCodeRecords[recordIndex].code,
			zipCodeRecords[recordIndex].population,
			zipCodeRecords[recordIndex].population2010,
			zipCodeRecords[recordIndex].population2000,
			zipCodeRecords[recordIndex].landArea,
			zipCodeRecords[recordIndex].foreignBornPopulation,
			zipCodeRecords[recordIndex].medianHouseholdIncome,
			zipCodeRecords[recordIndex].medianHomePrice,
			zipCodeRecords[recordIndex].medianResidentAge,
			zipCodeRecords[recordIndex].whitePopulation,
			zipCodeRecords[recordIndex].hispanicLatinoPopulation,
			zipCodeRecords[recordIndex].blackPopulation,
			zipCodeRecords[recordIndex].asianPopulation,
			zipCodeRecords[recordIndex].americanIndianPopulation,
			zipCodeRecords[recordIndex].highSchool,
			zipCodeRecords[recordIndex].bachelorsDegree,
			zipCodeRecords[recordIndex].graduateDegree,
			zipCodeRecords[recordIndex].malePercent,
			zipCodeRecords[recordIndex].femalePercent,
			zipCodeRecords[recordIndex].averageHouseholdSize);

		sprintf(insert_stmt, insert_format,
			zipCodeRecords[recordIndex].code,
			zipCodeRecords[recordIndex].population,
			zipCodeRecords[recordIndex].population2010,
			zipCodeRecords[recordIndex].population2000,
			zipCodeRecords[recordIndex].landArea,
			zipCodeRecords[recordIndex].foreignBornPopulation,
			zipCodeRecords[recordIndex].medianHouseholdIncome,
			zipCodeRecords[recordIndex].medianHomePrice,
			zipCodeRecords[recordIndex].medianResidentAge,
			zipCodeRecords[recordIndex].whitePopulation,
			zipCodeRecords[recordIndex].hispanicLatinoPopulation,
			zipCodeRecords[recordIndex].blackPopulation,
			zipCodeRecords[recordIndex].asianPopulation,
			zipCodeRecords[recordIndex].americanIndianPopulation,
			zipCodeRecords[recordIndex].highSchool,
			zipCodeRecords[recordIndex].bachelorsDegree,
			zipCodeRecords[recordIndex].graduateDegree,
			zipCodeRecords[recordIndex].malePercent,
			zipCodeRecords[recordIndex].femalePercent,
			zipCodeRecords[recordIndex].averageHouseholdSize);

		doInsert(db, insert_stmt);
	}

	commitTransaction(db);
	sqlite3_close(db);

	fclose(outputFile);

	for (recordIndex = 0; recordIndex < zip_code_count; ++recordIndex) {
		free(zipCodeRecords[recordIndex].code);
		free(zipCodeRecords[recordIndex].population);
		free(zipCodeRecords[recordIndex].population2010);
		free(zipCodeRecords[recordIndex].population2000);
		free(zipCodeRecords[recordIndex].landArea);
		free(zipCodeRecords[recordIndex].foreignBornPopulation);
		free(zipCodeRecords[recordIndex].medianHouseholdIncome);
		free(zipCodeRecords[recordIndex].medianHomePrice);
		free(zipCodeRecords[recordIndex].medianResidentAge);
		free(zipCodeRecords[recordIndex].whitePopulation);
		free(zipCodeRecords[recordIndex].hispanicLatinoPopulation);
		free(zipCodeRecords[recordIndex].blackPopulation);
		free(zipCodeRecords[recordIndex].asianPopulation);
		free(zipCodeRecords[recordIndex].americanIndianPopulation);
		free(zipCodeRecords[recordIndex].highSchool);
		free(zipCodeRecords[recordIndex].bachelorsDegree);
		free(zipCodeRecords[recordIndex].graduateDegree);
		free(zipCodeRecords[recordIndex].malePercent);
		free(zipCodeRecords[recordIndex].femalePercent);
		free(zipCodeRecords[recordIndex].averageHouseholdSize);
	}

	freeLinkedList(list_head);

	curl_easy_cleanup(curl);

	return EXIT_SUCCESS;
}
