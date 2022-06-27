#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fs.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define test_fs_error(fmt, ...) \
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)

#define die(...)				\
do {							\
	test_fs_error(__VA_ARGS__);	\
	exit(1);					\
} while (0)

#define die_perror(msg)			\
do {							\
	perror(msg);				\
	exit(1);					\
} while (0)


struct thread_arg {
	int argc;
	char **argv;
};

void thread_fs_script(void *arg)
{
	struct thread_arg *t_arg = arg;
	struct stat st;
	char *diskname, *script;
	FILE *fd_script;
	char *command, *data_source, *data_description, *data, *fs_filename;
	const int total_command_parts = 4;
	char *command_args[total_command_parts];
	int offset;
	char mounted = 0;

	char line_buffer[1024];
	int command_index = 1;

	if (t_arg->argc < 2)
		die("Usage: <diskname> <script filename>");

	diskname = t_arg->argv[0];
	script = t_arg->argv[1];

	/* Open script on host computer */
	fd_script = fopen(script, "r");
	if (!fd_script)
		die_perror("fopen");

	int fs_fd = -1;

	/* Loop through the script and execute the specified commands */
	while (fgets(line_buffer, 1024, fd_script) != NULL) {
		/* Remove trailing newline from command line */
		char *nl = strchr(line_buffer, '\n');
		if (nl)
			*nl = '\0';

		/* Tokenize line */
		command_args[0] = strtok(line_buffer, "\t");
		command_index = 1;
		do {
			command_args[command_index] = strtok(NULL, "\t");
		} while (command_index < total_command_parts && command_args[command_index++] != NULL);
		command = command_args[0];

		int data_fd;
		int count, data_size;

		char *read_buf;

		/* End when no command present */
		if (!command)
			break;

		if (strcmp(command, "MOUNT") == 0) {
			if (fs_mount(diskname))
				die("Cannot mount disk");
			else {
				printf("MOUNT successful.\n");
				mounted = 1;
			}

		} else if (strcmp(command, "UMOUNT") == 0) {
			if (mounted && fs_umount())
				die("Cannot unmount");
			else {
				printf("UMOUNT successful.\n");
				mounted = 0;
			}

		} else if (strcmp(command, "CREATE") == 0) {
			fs_filename = command_args[1];

			if(fs_create(fs_filename)) {
				fs_umount();
				die("Cannot create file");
			}

			printf("CREATE successful.\n");

		} else if (strcmp(command, "DELETE") == 0) {
			fs_filename = command_args[1];

			if(fs_delete(fs_filename)) {
				fs_umount();
				die("Cannot delete file");
			}

			printf("DELETE successful.\n");

		} else if (strcmp(command, "OPEN") == 0) {
			fs_filename = command_args[1];

			fs_fd = fs_open(fs_filename);

			if (fs_fd < 0) {
				fs_umount();
				die("Cannot open file");
			}

			printf("OPEN successful.\n");

		} else if (strcmp(command, "CLOSE") == 0) {
			if (fs_close(fs_fd)) {
				fs_umount();
				die("Cannot close file");
			}

			printf("CLOSE successful.\n");

		} else if (strcmp(command, "SEEK") == 0) {
			offset = atoi(command_args[1]);

			if (fs_lseek(fs_fd, offset)) {
				fs_umount();
				die("Cannot seek to position");
			} else {
				printf("SEEK successful.\n");
			}

		} else if (strcmp(command, "WRITE") == 0) {
			data_source = command_args[1];
			data_description = command_args[2];

			if (strcmp(data_source, "DATA") == 0) {
				data = data_description;
				data_size = strlen(data);
			} else if (strcmp(data_source, "FILE") == 0) {
				data_fd = open(data_description, O_RDONLY);
				if (data_fd < 0) {
					fs_umount();
					die_perror("open");
				}
				if (fstat(data_fd, &st)) {
					fs_umount();
					die_perror("fstat");
				}
				if (!S_ISREG(st.st_mode)) {
					fs_umount();
					die("Not a regular file: %s\n", data_description);
				}
				data_size = st.st_size;
				data = mmap(NULL, data_size, PROT_READ, MAP_PRIVATE, data_fd, 0);
			} else {
				data = NULL;
				data_size = 0;
			}

			if (!data) {
				fs_umount();
				die_perror("Could not find data to write");
			}

			count = fs_write(fs_fd, data, data_size);
			if (count < 0) {
				fs_umount();
				die("write error");
			}
			printf("Wrote %d bytes to file.\n", count);

		} else if (strcmp(command, "READ") == 0) {
			int read_req_length = atoi(command_args[1]);
			data_source = command_args[2];
			data_description = command_args[3];

			char file_loaded = 0;

			if (strcmp(data_source, "DATA") == 0) {
				data = data_description;
				data_size = strlen(data);
			} else if (strcmp(data_source, "FILE") == 0) {
				data_fd = open(data_description, O_RDONLY);
				if (data_fd < 0) {
					fs_umount();
					die_perror("open");
				}
				if (fstat(data_fd, &st)) {
					fs_umount();
					die_perror("fstat");
				}
				if (!S_ISREG(st.st_mode)) {
					fs_umount();
					die("Not a regular file: %s\n", data_description);
				}

				FILE *data_file = fopen(data_description, "r");
				data_size = st.st_size;
				data = calloc(data_size+1, sizeof(char));
				size_t n = fread (data, sizeof(char), data_size, data_file);
				assert(n == sizeof(char) * data_size);
				fclose(data_file);
				file_loaded = 1;
			} else {
				fs_umount();
				die("Invalid data description");
			}

			if (!data) {
				fs_umount();
				die_perror("Could not find data to write");
			}

			if (read_req_length < 0) {
				fs_umount();
				die("invalid data read length");
			}

			read_buf = calloc(read_req_length+1, sizeof(char));
			count = fs_read(fs_fd, read_buf, read_req_length);

			if (count < 0) {
				fs_umount();
				die("read error");
			}

			// both data and read_buf were allocated with an extra zero byte
			// +1 here to check for the canaries
			if (memcmp(data, read_buf, data_size+1) == 0)
				printf("Read %d bytes from file. Compared %d correct.\n", count, data_size);
			else
				printf("Read unexpected data! %s read vs given %s\n", read_buf, data);

			free(read_buf);
			if(file_loaded){
				free(data);
			}
		}
	}

	/* unmount at the end just to be safe in case there is
	   no UMOUNT command in script */
	if (mounted && fs_umount())
		die("Cannot unmount diskname");

	fclose(fd_script);
}

void thread_fs_stat(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname, *filename;
	int fs_fd;
	int stat;

	if (t_arg->argc < 2)
		die("need <diskname> <filename>");

	diskname = t_arg->argv[0];
	filename = t_arg->argv[1];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	fs_fd = fs_open(filename);
	if (fs_fd < 0) {
		fs_umount();
		die("Cannot open file");
	}

	stat = fs_stat(fs_fd);
	if (stat < 0) {
		fs_close(fs_fd);
		fs_umount();
		die("Cannot stat file");
	}
	if (!stat) {
		fs_close(fs_fd);
		fs_umount();
		/* Nothing to read, file is empty */
		printf("Empty file\n");
		return;
	}

	if (fs_close(fs_fd)) {
		fs_umount();
		die("Cannot close file");
	}

	if (fs_umount())
		die("cannot unmount diskname");

	printf("Size of file '%s' is %d bytes\n", filename, stat);
}

void thread_fs_cat(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname, *filename, *buf;
	int fs_fd;
	int stat, read;

	if (t_arg->argc < 2)
		die("need <diskname> <filename>");

	diskname = t_arg->argv[0];
	filename = t_arg->argv[1];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	fs_fd = fs_open(filename);
	if (fs_fd < 0) {
		fs_umount();
		die("Cannot open file");
	}

	stat = fs_stat(fs_fd);
	if (stat < 0) {
		fs_umount();
		die("Cannot stat file");
	}
	if (!stat) {
		/* Nothing to read, file is empty */
		printf("Empty file\n");
		return;
	}
	buf = malloc(stat);
	if (!buf) {
		perror("malloc");
		fs_umount();
		die("Cannot malloc");
	}

	read = fs_read(fs_fd, buf, stat);

	if (fs_close(fs_fd)) {
		fs_umount();
		die("Cannot close file");
	}

	if (fs_umount())
		die("cannot unmount diskname");

	printf("Read file '%s' (%d/%d bytes)\n", filename, read, stat);
	printf("Content of the file:\n");
	fwrite(buf, 1, stat, stdout);
	fflush(stdout);

	free(buf);
}

void thread_fs_rm(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname, *filename;

	if (t_arg->argc < 2)
		die("need <diskname> <filename>");

	diskname = t_arg->argv[0];
	filename = t_arg->argv[1];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	if (fs_delete(filename)) {
		fs_umount();
		die("Cannot delete file");
	}

	if (fs_umount())
		die("Cannot unmount diskname");

	printf("Removed file '%s'\n", filename);
}

void thread_fs_add(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname, *filename, *buf;
	int fd, fs_fd;
	struct stat st;
	int written;

	if (t_arg->argc < 2)
		die("Usage: <diskname> <host filename>");

	diskname = t_arg->argv[0];
	filename = t_arg->argv[1];

	/* Open file on host computer */
	fd = open(filename, O_RDONLY);
	if (fd < 0)
		die_perror("open");
	if (fstat(fd, &st))
		die_perror("fstat");
	if (!S_ISREG(st.st_mode))
		die("Not a regular file: %s\n", filename);

	/* Map file into buffer */
	buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!buf)
		die_perror("mmap");

	/* Now, deal with our filesystem:
	 * - mount, create a new file, copy content of host file into this new
	 *   file, close the new file, and umount
	 */
	if (fs_mount(diskname))
		die("Cannot mount diskname");

	if (fs_create(filename)) {
		fs_umount();
		die("Cannot create file");
	}

	fs_fd = fs_open(filename);
	if (fs_fd < 0) {
		fs_umount();
		die("Cannot open file");
	}

	written = fs_write(fs_fd, buf, st.st_size);

	if (fs_close(fs_fd)) {
		fs_umount();
		die("Cannot close file");
	}

	if (fs_umount())
		die("Cannot unmount diskname");

	printf("Wrote file '%s' (%d/%zu bytes)\n", filename, written,
		   st.st_size);

	munmap(buf, st.st_size);
	close(fd);
}

void thread_fs_ls(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname;

	if (t_arg->argc < 1)
		die("Usage: <diskname>");

	diskname = t_arg->argv[0];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	fs_ls();

	if (fs_umount())
		die("Cannot unmount diskname");
}

void thread_fs_info(void *arg)
{
	struct thread_arg *t_arg = arg;
	char *diskname;

	if (t_arg->argc < 1)
		die("Usage: <diskname>");

	diskname = t_arg->argv[0];

	if (fs_mount(diskname))
		die("Cannot mount diskname");

	fs_info();

	if (fs_umount())
		die("Cannot unmount diskname");
}

size_t get_argv(char *argv)
{
	long int ret = strtol(argv, NULL, 0);
	if (ret == LONG_MIN || ret == LONG_MAX)
		die_perror("strtol");
	return (size_t)ret;
}

static struct {
	const char *name;
	void(*func)(void *);
} commands[] = {
	{ "info",	thread_fs_info },
	{ "ls",		thread_fs_ls },
	{ "add",	thread_fs_add },
	{ "rm",		thread_fs_rm },
	{ "cat",	thread_fs_cat },
	{ "stat",	thread_fs_stat },
	{ "script",	thread_fs_script }
};

void usage(char *program)
{
	size_t i;
	fprintf(stderr, "Usage: %s <command> [<arg>]\n", program);
	fprintf(stderr, "Possible commands are:\n");
	for (i = 0; i < ARRAY_SIZE(commands); i++)
		fprintf(stderr, "\t%s\n", commands[i].name);
	exit(1);
}

int main(int argc, char **argv)
{
	size_t i;
	char *program;
	char *cmd;
	struct thread_arg arg;

	program = argv[0];

	if (argc == 1)
		usage(program);

	/* Skip argv[0] */
	argc--;
	argv++;

	cmd = argv[0];
	arg.argc = --argc;
	arg.argv = &argv[1];

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (!strcmp(cmd, commands[i].name)) {
			commands[i].func(&arg);
			break;
		}
	}
	if (i == ARRAY_SIZE(commands)) {
		test_fs_error("invalid command '%s'", cmd);
		usage(program);
	}

	return 0;
}
