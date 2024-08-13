// SPDX-License-Identifier: GPL-2.0-or-later

#define MAX_LINE_LENGTH 2048

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* DON'T FORGET TO FREE THE MEMORY FOR GET CHAR */
/* Improved version that gets the value irrespective of number of spaces */
// Function to trim spaces and other whitespace characters from the start and end of a string
char *trim(char *str) {
	char *start = str;
	char *end = str + strlen(str) - 1;
	// Trim leading whitespace characters
	while (isspace((unsigned char)*start)) {
		start++;
	}
	// Trim trailing whitespace characters
	while (end > start && isspace((unsigned char)*end)) {
		*end-- = '\0';
	}
	return strdup(start);
}

int get_int_value_from_conf(char *fullPathToConf, char *keyToGetValueFrom) {
	setlocale(LC_NUMERIC, "C");

	char buffer[MAX_LINE_LENGTH];
	char value[MAX_LINE_LENGTH];
	int found = 0;

	FILE *pathToConfig = fopen(fullPathToConf, "r");

	if (pathToConfig == NULL) {
		perror("Error opening file");
		return 1;
	}

	while (fgets(buffer, sizeof(buffer), pathToConfig) != NULL) {
		// Check if the line contains the key and is not a comment
		if (strstr(buffer, keyToGetValueFrom) != NULL && strchr(buffer, '#') == NULL) {
			char *pos = strchr(buffer, '=');
			if (pos != NULL) {
				*pos = '\0'; // Split the string into key and value
				char *key = trim(buffer);
				if (strcmp(key, keyToGetValueFrom) == 0) {
					strcpy(value, pos + 1);
					strcpy(value, trim(value)); // Trim the value
					value[strcspn(value, "\r\n")] = '\0'; // Remove trailing newline characters
					found = 1;
					free(key);
					break;
				}
			}
		}
	}

	fclose(pathToConfig);

	if (!found) {
		return 1; // Return 1 if the key was not found
	}
	int valueToNumber = atoi(value);
	return valueToNumber;
}

double get_double_value_from_conf(char *fullPathToConf, char *keyToGetValueFrom) {
    setlocale(LC_NUMERIC, "C");

    char buffer[MAX_LINE_LENGTH];
    char value[MAX_LINE_LENGTH];
    int found = 0;

    FILE *pathToConfig = fopen(fullPathToConf, "r");

    if (pathToConfig == NULL) {
        perror("Error opening file");
        return 1;
    }

    while (fgets(buffer, sizeof(buffer), pathToConfig) != NULL) {
        // Print out each line for debugging
        ///printf("Read line: %s", buffer);
        // Check if the line contains the key and is not a comment
        if (strstr(buffer, keyToGetValueFrom) != NULL && strchr(buffer, '#') == NULL) {
            char *pos = strchr(buffer, '=');
            if (pos != NULL) {
                *pos = '\0'; // Split the string into key and value
                char *key = trim(buffer);
                if (strcmp(key, keyToGetValueFrom) == 0) {
                    strcpy(value, pos + 1);
                    value[strcspn(value, "\r\n")] = '\0'; // Remove trailing newline characters
                    found = 1;
					free(key);
                    break;
                }
            }
        }
    }

    fclose(pathToConfig);

    if (!found) {
        return 1; // Return 1 if the key was not found
    }

    // Manually extract the numerical part of the value string
    char *num_part = trim(value);
    ///printf("Numerical part: '%s'\n", num_part);

    // Convert the numerical part directly to a double
    double valueToNumber = strtod(num_part, NULL);
    free(num_part);
    num_part = NULL;

    return valueToNumber;
}

// Function to get the value associated with a key from a config file
char *get_char_value_from_conf(const char *fullPathToConf, const char *keyToGetValueFrom) {
	setlocale(LC_NUMERIC, "C");
	char buffer[MAX_LINE_LENGTH];
	char value[MAX_LINE_LENGTH];
	int found = 0;
	FILE *pathToConfig = fopen(fullPathToConf, "r");
	if (pathToConfig == NULL) {
		return NULL; // Return NULL if the file cannot be opened
	}

	while (fgets(buffer, sizeof(buffer), pathToConfig) != NULL) {
		// Check if the line contains the key and is not a comment
		if (strstr(buffer, keyToGetValueFrom) != NULL && strchr(buffer, '#') == NULL) {
			char *pos = strchr(buffer, '=');
			if (pos != NULL) {
				*pos = '\0'; // Split the string into key and value
				char *key = trim(buffer);
				if (strcmp(key, keyToGetValueFrom) == 0) {
					strcpy(value, pos + 1);
					const char *tmp_value = trim(value);
					strcpy(value, tmp_value); // Trim the value
					value[strcspn(value, "\r\n")] = '\0'; // Remove trailing newline characters
					found = 1;
					free((void *)tmp_value);
					free(key);
					break;
				}
			}
		}
	}

	fclose(pathToConfig);
	if (!found) {
		return NULL; // Return NULL if the key was not found
	}
	return strdup(value);
}
