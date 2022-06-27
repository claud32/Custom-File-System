# Tester scripting

The `test_fs.c` program accepts a `script` command which reads a sequence of
commands from a specified file, and performs them on a filesystem.

This scripting ability is particularly useful to test your reading and writing
functions, and check that your implementation properly handles the
special cases (e.g., R/W operations from a non-null offset, small R/W operations
within a block, large R/W operations spanning multiple blocks, etc.).

## Usage

The first argument to the script command is the name of the virtual block device
file and the second is the name of the script file.

```
$ ./test_fs.x script <disk.fs> <script_file>
```

The script file contains a sequence of commands to be performed on the given
filesystem. Each command must be on its own line. If a command has arguments,
arguments are delimited by a tab character. The list of possible commands is:

`MOUNT`
: Mounts the file system given on the test script command line.

`UMOUNT`
: Unmounts currently mounted file system if mounted.

`CREATE	<filename>`
: Create empty file named `<filename>` on filesystem.

`DELETE	<filename>`
: Delete file named `<filename>` from filesystem.

`OPEN	<filename>`
: Open file named `<filename>` on filesystem.

`CLOSE`
: Close currently opened file.

`SEEK	<offset>`
: Seeks to the given offset.

`WRITE	DATA	<data>`
: Writes `<data>` at the current offset given in the script file.

`WRITE	FILE	<filename>`
: Writes data read from file located on host computer with name `<filename>`.

`READ	<len>	DATA	<data>`
: Reads `<len>` bytes from the current offset, and compares it to `<data>`.

`READ	<len>	FILE	<filename>`
: Reads `<len>` bytes from the current offset, and compares it to the file
located on host computer with name `<filename>`.

## Example

An example script is provided in `script.example`, and shows how to use most of
the available commands as described above.

To try it out, type:

```console
$ cd apps/
$ dd if=/dev/random of=test_file bs=4096 count=1
$ ./fs_make.x test.fs 100
$ ./test_fs.x script test.fs scripts/script.example
...
```

It is strongly suggested to write longer scripts, testing writing and reading
back data both within blocks and across block boundaries, to ensure your
implementation is robust.

