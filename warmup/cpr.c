#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

/* make sure to use syserror() when a system call fails. see common.h */

void usage() {
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

void recursiveCopy(char* src, char* dst); // recursively copy a directory from src to dst
void copyFile(char* src, char* dst); // copy a file from src to dst

int main(int argc, char *argv[]) {
	if (argc != 3) {
		usage();
	}

	struct stat sourceDirInfo;
	if (stat(argv[1], &sourceDirInfo) < 0) syserror(stat, argv[1]); // get stats for source dir, and call syserror if it fails (ie. returns a value lower than 0)
	if (mkdir(argv[2], sourceDirInfo.st_mode) < 0) syserror(mkdir, argv[2]); // make destination dir with same permissions as source dir, and call syserror if it fails
	recursiveCopy(argv[1], argv[2]);

	return 0;
}

void recursiveCopy(char* src, char* dst) {
	DIR* srcDir = opendir(src); // open the source directory
	if (srcDir == NULL) syserror(opendir, src); // error handling

	errno = 0;
	struct dirent* next_entry = readdir(srcDir); // getting the next file/dir in the current source dir
	if (errno != 0) syserror(readdir, src); // error handling

	while (next_entry != NULL) {
		fprintf(stdout, "Next Entry: %s\n", next_entry -> d_name);
		// move onto the next entry if the current entry is either the current or previous directory
		if (strcmp(next_entry -> d_name, ".") == 0 || strcmp(next_entry -> d_name, "..") == 0) {
			errno = 0;
			next_entry = readdir(srcDir);
			if (errno != 0) syserror(readdir, src); // error handling
			continue;
		}

		char curr_src[1000];
		char curr_dst[1000];
		
		// char* curr_src = (char*) malloc(sizeof(char) * (strlen(next_entry -> d_name) + strlen(src) + 1));
		strcpy(curr_src, src); strcat(curr_src, "/"); strcat(curr_src, next_entry -> d_name);
		// char* curr_dst = (char*) malloc(sizeof(char) * (strlen(next_entry -> d_name) + strlen(dst) + 1));
		strcpy(curr_dst, dst); strcat(curr_dst, "/"); strcat(curr_dst, next_entry -> d_name);

		fprintf(stdout, "Curr Src: %s\n", curr_src);
		fprintf(stdout, "Curr Dst: %s\n", curr_dst);

		struct stat sourceInfo;

		if (stat(curr_src, &sourceInfo) < 0) syserror(stat, curr_src); // error handling

		if (S_ISREG(sourceInfo.st_mode)) {
			// if it is a file, then copy it
			copyFile(curr_src, curr_dst);
			// chmod at the end so that it matches the src file
			if (chmod(curr_dst, sourceInfo.st_mode) < 0) syserror(chmod, curr_dst);
		} else if (S_ISDIR(sourceInfo.st_mode)) {
			// if it is a directory, make a shallow copy of the directory
			// then recurse to copy over all contents
			if (mkdir(curr_dst, 0777) < 0) syserror(mkdir, curr_dst);
			recursiveCopy(curr_src, curr_dst);
			// chmod at the end so that it matches the src dir
			if (chmod(curr_dst, sourceInfo.st_mode) < 0) syserror(chmod, curr_dst);
		}

		// traverse to next file/subdirectory
		errno = 0;
		next_entry = readdir(srcDir);
		if (errno != 0) syserror(readdir, src); // error handling
	}
}

void copyFile(char* src, char* dst) {
	int fd_dst = open(dst, O_WRONLY|O_CREAT, 0777); // create a new file at dst if it doesn't exist already with all permissions temporarily enabled
	int fd_src = open(src, O_RDONLY); // open the src file as read only

	// error handling
	if (fd_dst < 0) syserror(open, dst);
	if (fd_src < 0) syserror(open, src);

	// copy over characters from the src file to the dst file by using a buffer
	char buf[4096];

	int numRead = read(fd_src, buf, 4096);

	while (numRead > 0) {
		write(fd_dst, buf, numRead);
		numRead = read(fd_src, buf, 4096);
	}

	// close files
	close(fd_dst);
	close(fd_src);
}