/* Creating a cache file with all the paths to svgs from the given theme directory
   Usage:
   const char *directoryPath = "/usr/share/icons/Lyra-blue-dark";
   const char *substring = ".svg";
   FILE *outputFile = fopen("/home/diogenes/icons.cache", "w");   
   create_icon_cache(outputFile, substring, directoryPath);
   fclose(outputFile);
 */

#define _GNU_SOURCE // Required for strcasestr on Linux
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

void create_icon_cache(FILE *outputFile, const char *substring, const char *path) {
	if (!outputFile || !substring || !path) return;

	DIR *dir = opendir(path);
	if (!dir) return;

	struct dirent *entry;
	struct stat statbuf;

	while ((entry = readdir(dir)) != NULL) {
		// Skip hidden navigation directories
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		char fullpath[PATH_MAX];
		int len = snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
		if (len < 0 || len >= (int)sizeof(fullpath)) continue;

		// Use stat() instead of lstat() to follow symlinks to their targets
		if (stat(fullpath, &statbuf) == -1) {
			continue;
		}

		if (S_ISDIR(statbuf.st_mode)) {
			// Recurse into subdirectories
			create_icon_cache(outputFile, substring, fullpath);
		}
		else {
			// Case-insensitive search
			// If strcasestr is unavailable, use a custom lowercase comparison
			if (strcasestr(entry->d_name, substring)) {
				fprintf(outputFile, "%s\n", fullpath);
			}
		}
	}
	closedir(dir);
}
