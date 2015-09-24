/**************************************************************/
/**************************************************************/
  Sep. 25, 2015.

This is the README file for "myshell_141013_kim_bonggeun_final.c"(hereinafter, the source file).

Current version of the source file is ver.1.0.0.

The source file has been made as a term project in Operating System coursework in the 2nd Semester, 2015 in NEXT Institute.

The source file has been made by Bong-Geun Kim(the author of this README text) under the guidance of Prof. Bae Young-Hyun.

I really appreciate my Professor's kind as well as detail lectures and guides.

The source file is about simplified "shell" program coded in C programming language.

Since the "shell" program is quite a simple version of bash, some functions need to be described.

The following is a brief description of some features about "myshell" program.

/**************************************************************/
/*************************************************************/

"myshell" implements some basic commands of bash in the source code.

They are "ls", "ll", "cp", "rm", "mv", "cd", "pwd", "mkdir", "rmdir", and "dcp."

Also, "myshell" implements a single directional "redirection(>)" and a pipe(|) function.

Although "myshell" does not implement various important commands other than the above-listed commands,

	"myshell" also provides those various commands based on fork() and exec() system calls from bash program.

Hereinafter, simple usages and functions of the basic commands are described.

The brackets([,]) represent that some arguments may be added in the position therebetween.

//=========================

	ls

Usage: ls [directory]

Function: list file names in the directory. 

		Files in the present directory are listed if the directory is not specified.

		(Options of "ls" may be provided as an externally implemented bash program.)

//=========================

	ll

Usage: ll [directory]

Function: list file names and their attributes in the directory.

		The attributes are: directory or not, modes, file size(in bytes), user Id, group Id, number of link, recently modified time.

		Like "ls", the present directory is specified as default.

//========================

	cp

Usage: cp <original file> <target file>

Function: copy the original file to the target file.

		"cp" does not provide directory copy function.

		In order to copy a directory, use "dcp" commands below.

//=======================

	rm

Usage: rm <file>

Function: remove file or directory.

		Although "rm" may remove a directory as well as file, "rmdir" for removing a directory is also implemented in "myshell."

//======================

	mv

Usage: mv <origianl file> <target file>

Function: rename original file into target file.

		"mv" may also move a file from one directory to another directory.

//======================

	cd

Usage: cd <directory>

Function: change directory into <directory>.

//======================

	pwd

Usage: pwd

Function: show present working directory.

//=====================

	dcp

Usage: dcp <source directory> <target directory>

Function: copy the source directory into target directory.

		Only files in the source directory may be copied into the target directory.

		"dcp" does not provides recursively copying sub-directories and files in them.

//====================

	mkdir

Usage: mkdir <directory>

Function: make new directory.

//====================

	rmdir

Usage: rmdir <directory>

Function: remove directory.

//====================



Ver.1.0.0 README
