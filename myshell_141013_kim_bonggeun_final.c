// 헤더 파일
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

// 상수
#define MAXLINE 1024
#define MAXARGS 128
#define MAXPATH 1024
#define MAXTHREAD 10

#define DEFAULT_FILE_MODE	0664
#define DEFAULT_DIR_MODE	0775

// 전역 변수
char prompt[] = "myshell> ";
const char delim[] = " \t\n";

int saved_stdout = 1; 	// 표준 출력 파일 디스크립터
pid_t pid = 0; 			// 자식 프로세스 관련 pid

int REDIRECT_RUN = 0; 	// 리다이렉션(>) 관련 flag 변수
int redirectPos = 0; 		// 리다이렉션(>) 위치 in argv

int BG_RUN = 0; 		// 백그라운드(&) 실행 관련 flag 변수

int PIPE_RUN = 0; 		// 파이프(|) 실행 관련 flag 변수
int pipePos = 0;		// 파이프(|) 위치
char *out_cmd;				// 파이프(|)에 출력할 명령어 변수
char *out_cmd_v[MAXARGS];	// 파이프(|)에 출력할 명령 인자 벡터

char *argv1;	// copy_directory에서, 스레드 함수와 공유할 argv[1]
char *argv2;	// copy_directory에서, 스레드 함수와 공유할 argv[2]

typedef struct { 		// 스레드 함수에 인자로 전달할 구조체
	int thread_num;			// 스레드 번호(카운트하는 번호)
	char *d_name;			// 파일 이름
} thread_arg_t;


// 함수 선언
void process_command(char *commandline);
int parse_line(char *commandline, char **argv);
int builtin_cmd(int argc, char **argv);

// 내장 명령어 처리 함수
int list_files(int argc, char **argv);
int copy_file(int argc, char **argv);
int remove_file(int argc, char **argv);
int move_file(int argc, char **argv);
int change_directory(int argc, char **argv);
int print_working_directory(void);
int make_directory(int argc, char **argv);
int remove_directory(int argc, char **argv);
int copy_directory(int argc, char **argv);

// 스레드 함수
void * thread_fn(void * thread_arg);



/** myShell main 시작 **/

int main(){

	char commandline[MAXLINE];

	while(1){

		// 프롬프트 출력
		printf("%s", prompt);
		fflush(stdout);

		// 명령어 입력 받기
		if (fgets(commandline, MAXLINE, stdin) == NULL){
			return 1;
		}
		
		// 명령어 처리
		process_command(commandline);


		// 백그라운드에서 자식 프로세스가 종료된 경우 화면에 종료 표시.
		if ((pid > 0) && (waitpid(pid, NULL, WNOHANG) > 0)){
			printf("PID %u is terminated.\n", pid);
			pid = 0;
			BG_RUN = 0;
		}

		fflush(stdout);
	}

	return 0;
}


/** 명령어 처리 함수 **/
void process_command(char *commandline)
{
	int argc, fd;
	char *argv[MAXARGS];
	int pipefd[2];

	// 명령 라인 파싱
	argc = parse_line(commandline, argv);

	if (argc == 0) return;

	// 리다이렉션(>) 실행인 경우 표준 출력 변경
	if (REDIRECT_RUN == 1) {
		fd = creat(argv[redirectPos+1], DEFAULT_FILE_MODE);
		if (fd < 0) return;

		saved_stdout = dup(1);
		dup2(fd, 1);
		close(fd);
	}

	// 파이프(|) 실행인 경우 pipe 생성
	if (PIPE_RUN == 1) {
		if (pipe(pipefd) == -1) {
			fprintf(stderr, "Pipe failed.\n");
			return;
		}
	}


	// 내장 명령어 실행
	if (builtin_cmd(argc, argv) == 0) {

    	// 리다이렉션(>)한 경우 표준 출력 복구
		if (saved_stdout != 1){
			dup2(saved_stdout, 1);
			REDIRECT_RUN = 0;
		}

		return;
	}


	// 내장 명령어 아닌 경우 자식 프로세스 생성
	pid = fork();

	if (pid < 0) {
		fprintf(stderr, "Fork Failed.\n");
		return;
	}
	else if (pid == 0) {  // 자식 프로세스 처리

		if (PIPE_RUN == 1) {  // 파이프 실행인 경우 파이프(|) 앞 명령어 실행
			
			close(pipefd[0]);
			dup2(pipefd[1], 1);
			if (execvp(out_cmd, out_cmd_v) == -1) {
				printf("%s : Command not found.\n", out_cmd);
				exit(0);
			}
		}

		if (REDIRECT_RUN) 
			argv[redirectPos] = (char *) NULL;

		argv[argc] = (char *) NULL;
		if (execvp(argv[0], argv) == -1){
			printf("%s : Command not found.\n", argv[0]);
			exit(0);
		}
	}
	

	if (PIPE_RUN == 1) {
		
		// 파이프(|) 실행인 경우 자식 프로세스 추가 생성, 파이프(|) 뒤 명령어 실행
		if (fork() == 0) {

			close(pipefd[1]);
			dup2(pipefd[0], 0);
			
			argv[argc] = (char *) NULL;
			if (execvp(argv[0], argv) == -1) {
				printf("%s : Command not found.\n", argv[0]);
				exit(0);
			}
		}

		// 부모 프로세스의 파이프는 모두 close
		close(pipefd[0]);
		close(pipefd[1]);
	}


	// 부모 프로세스 처리

	if (BG_RUN == 1){ // 백그라운드 실행(&)인 경우 nohang 처리
		printf("[bg] %u : %s\n", pid, argv[0]);
		
		if (waitpid(pid, NULL, WNOHANG) > 0) {
			printf("PID %u is terminated.\n", pid);
			pid = 0;
			BG_RUN = 0;
		}
	}
	else {  // 포어 그라운드 실행인 경우
		while(wait(NULL) >= 0);
	}
	

    // 리다이렉션(>)한 경우 표준 출력 복구
	if (saved_stdout != 1){
		dup2(saved_stdout, 1);
		REDIRECT_RUN = 0;
	}

	// 파이프(|) 실행인 경우 flag 복구
	if (PIPE_RUN)
		PIPE_RUN = 0;


	return;
}



/** 명령 라인 입력, 리다이렉션, 백그라운드, 파이프 실행 해석 함수 **/

int parse_line(char *commandline, char **argv)
{

	int i, count = 0;
	char *tok = strtok(commandline, delim);
	char *temp[MAXARGS];

	// delim 기준으로 명령 라인 입력을 분절하면서 개수 카운트
	while (tok != NULL) {
		argv[count] = tok;
		tok = strtok(NULL, delim);
		count++;
	}

	// redirect(>), pipe(|) 위치 찾기
	for (i = 0; i < count; i++) {
		if (strchr(argv[i], '>') != NULL) 
			redirectPos = i;

		if (strchr(argv[i], '|') != NULL) 
			pipePos = i;
	}

	// 리다이렉션(>) 해석
	if ((count > 2) && (!strcmp(argv[redirectPos], ">"))){
		REDIRECT_RUN = 1;
	}

	// 백그라운드 실행(&)인 경우
	if ((count > 0) && (!strcmp(argv[count-1], "&"))) {
		BG_RUN = 1;
	}

	// 파이프 실행인 경우 파이프(|) 앞뒤로 구분해서 parsing
	if ((count > 2) && (!strcmp(argv[pipePos], "|"))) {
		PIPE_RUN = 1;

		// pipe "앞"의 명령어 파싱
		out_cmd = argv[0];
		for (i = 0; i < pipePos; i++) {
			out_cmd_v[i] = argv[i];
		}
		out_cmd_v[pipePos] = (char *) NULL;

		// pipe "뒤"의 명령어 파싱
		count -= (pipePos+1);
		for (i = 0; i < count; i++) {
			temp[i] = argv[i+pipePos+1];
		}
		for (i = 0; i < count; i++) {
			argv[i] = temp[i];
		}
	}

	return count;
}



/**  내장 명령어 호출 함수 **/

int builtin_cmd(int argc, char **argv)
{

	if ((!strcmp(argv[0], "quit")) || (!strcmp(argv[0], "exit"))) {
		exit(0);
	}

	if ((!strcmp(argv[0], "ls")) || (!strcmp(argv[0], "ll"))) {

		// ls 옵션(-)이나 파이프(|)는 외장 명령어로 처리
		if (PIPE_RUN == 1){
			if (!strncmp(out_cmd, "ll", 2))
				printf("Usage: ls -l | <piped_command>\n");
			return 1;
		}
		if ((argc > 1) && (strchr(argv[1], '-') != NULL))
			return 1;

		// 내장 ls, ll 처리
		if (!list_files(argc, argv))
			return 0;
	}

	if (!strcmp(argv[0], "cp")){   // 파일 copy
		if (!copy_file(argc, argv))
			return 0;
	}

	if (!strcmp(argv[0], "rm")){
		if (!remove_file(argc, argv))
			return 0;
	}

	if (!strcmp(argv[0], "mv")){
		if (!move_file(argc, argv))
			return 0;
	}

	if (!strcmp(argv[0], "cd")){
		if (!change_directory(argc, argv))
			return 0;
	}

	if (!strcmp(argv[0], "pwd")){
		if (!print_working_directory())
			return 0;
	}

	if (!strcmp(argv[0], "mkdir")){
		if (!make_directory(argc, argv))
			return 0;
	}

	if (!strcmp(argv[0], "rmdir")){
		if (!remove_directory(argc, argv))
			return 0;
	}

	if (!strcmp(argv[0], "dcp")){  // 디렉토리 copy
		if (!copy_directory(argc, argv))
			return 0;
	}

	return 1;
}


/******************************
  내장 명령어 처리 함수
*****************************/

// ls, ll 처리 함수
int list_files(int argc, char **argv)
{
	DIR *dp;
	struct dirent *d_entry;
	struct stat st = {0};
	struct tm *tm;
	time_t t;
	char buffer[128] = {};
	int i;

	// dir 오픈
	if ((argc == 1) || ((argc > 1) && (redirectPos > 0))) {
		dp = opendir(".");
	} else{
		dp = opendir(argv[1]);
	}

	if (dp == NULL){
		fprintf(stderr, "directory open error.\n");
		return 1;
	}

	d_entry = readdir(dp);
	
	// ls 수행
	if (!strcmp(argv[0], "ls")){
		
		while (d_entry != NULL){
			printf("%s\n", d_entry->d_name);
			d_entry = readdir(dp);
		}
	}

	// ll 수행
	if (!strcmp(argv[0], "ll")){

		while (d_entry != NULL){

			if (stat(d_entry->d_name, &st)){
				fprintf(stderr, "stat read error.\n");
				printf("Default usage: ls <directory_name>.\n");
				return 1;
			}

			// stat 모드 초기화 및 쓰기
			for (i = 0; i < 10; i++){
				buffer[i] = '-';
			}
		
			if (S_ISDIR(st.st_mode)) buffer[0] = 'd';

			if (st.st_mode & S_IRUSR) buffer[1] = 'r';
			if (st.st_mode & S_IWUSR) buffer[2] = 'w';
			if (st.st_mode & S_IXUSR) buffer[3] = 'x';

			if (st.st_mode & S_IRGRP) buffer[4] = 'r';
			if (st.st_mode & S_IWGRP) buffer[5] = 'w';
			if (st.st_mode & S_IXGRP) buffer[6] = 'x';

			if (st.st_mode & S_IROTH) buffer[7] = 'r';
			if (st.st_mode & S_IWOTH) buffer[8] = 'w';
			if (st.st_mode & S_IXOTH) buffer[9] = 'x';

			for (i = 0; i < 10; i++){
				printf("%c", buffer[i]);
			}

			printf(" %u %u %u %ld\t", st.st_nlink, st.st_uid, st.st_gid, st.st_size); 

			t = st.st_mtime;
			tm = localtime(&t);
			strftime(buffer, sizeof(buffer), "%m월 %d일 %Y %H:%M:%S", tm);
			printf("%s ", buffer);

			printf("%s\n", d_entry->d_name);

			d_entry = readdir(dp);
		}
	}

	if(closedir(dp)) return 1;

	return 0;
}

// cp 파일 처리 함수
int copy_file(int argc, char **argv)
{
	int in_fd, out_fd, rd_count, wr_count;
	char buffer[4096];
	struct stat st = {0};

	// 잘못된 사용법인 경우
	if (argc != 3) {
		fprintf(stderr, "Usage: cp <original_file_name> <new_file_name>\n");
		return 1;
	}

	// 파일 이름이 없는 경우
	if (stat(argv[1], &st)) {
		perror("cp error.\n");
		return 1;
	}

	// cp 수행
	if ((in_fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "file open error.\n");
	}
	if ((out_fd = creat(argv[2], DEFAULT_FILE_MODE)) < 0){
		fprintf(stderr, "file create error.\n");
	}

	while (1) {
		if ((rd_count = read(in_fd, buffer, 4096)) <= 0)
			break;
		if ((wr_count = write(out_fd, buffer, rd_count)) <= 0)
			return 1;
	}

	if (rd_count < 0){
		fprintf(stderr, "file read error.\n");
		return 1;
	}
	if (wr_count < 0){
		fprintf(stderr, "file write error.\n");
		return 1;
	}

	if (close(in_fd)) return 1;
	if (close(out_fd)) return 1;

	return 0;
}

// rm 파일 처리 함수
int remove_file(int argc, char **argv)
{
	struct stat st = {0};

	// 잘못된 사용법인 경우
	if (argc != 2){
		fprintf(stderr, "Usage: rm <file_name>\n");
		return 1;
	}

	// 파일 이름이 없는 경우
	if (stat(argv[1], &st)) {
		perror("rm error.\n");
		return 1;
	}

	// rm 수행
	if (remove(argv[1])){
		fprintf(stderr, "rm error.\n");
		return 1;
	}

	return 0;
}


// mv 파일 처리 함수
int move_file(int argc, char **argv)
{
	struct stat st = {0};

	// 잘못된 사용법인 경우
	if (argc != 3){
		fprintf(stderr, "Usage: mv <old_name> <new_name>\n");
		return 1;
	}

	// 파일 이름이 없는 경우
	if (stat(argv[1], &st)) {
		perror("mv error.\n");
		return 1;
	}

	// mv 수행
	if (rename(argv[1], argv[2])){
		fprintf(stderr, "move error.\n");
		return 1;
	}

	return 0;
}


// cd 처리 함수
int change_directory(int argc, char **argv)
{
	struct stat st = {0};

	// 잘못된 사용법인 경우
	if (argc != 2){
		fprintf(stderr, "Usage: cd <directory_name>\n");
		return 1;
	}

	// 디렉토리 이름이 없는 경우
	if (stat(argv[1], &st)) {
		perror("cd error.\n");
		return 1;
	}

	// cd 수행
	if (chdir(argv[1])){
		fprintf(stderr, "cd error.\n");
		return 1;
	}

	return 0;
}


// pwd 처리 함수
int print_working_directory(void)
{
	char dir[MAXPATH];

	if (getcwd(dir, sizeof(dir)) != NULL)
		printf("%s\n", dir);
	else
		perror("getcwd() error");

	return 0;
}


// mkdir 처리 함수
int make_directory(int argc, char **argv)
{
	struct stat st = {0};

	// 잘못된 사용법인 경우
	if (argc != 2){
		fprintf(stderr, "Usage: mkdir <directory_name>\n");
		return 1;
	}

	// 디렉토리 이름 중복된 경우
	if (!stat(argv[1], &st)) {
		fprintf(stderr, "There exists the same directory name. mkdir failed.\n");
		return 1;
	}

	// mkdir 수행
	if (mkdir(argv[1], DEFAULT_DIR_MODE)){
		fprintf(stderr, "mkdir error.\n");
		return 1;
	}

	return 0;
}


// rmdir 처리 함수
int remove_directory(int argc, char **argv)
{
	struct stat st = {0};

	// 잘못된 사용법인 경우
	if (argc != 2){
		fprintf(stderr, "Usage: rmdir <directory_name>\n");
		return 1;
	}

	// 디렉토리 이름 없는 경우
	if (stat(argv[1], &st)) {
		perror("rmdir error\n");
		return 1;
	}

	// rmdir 수행
	if (rmdir(argv[1])) {
		fprintf(stderr, "rmdir error.\n");
		return 1;
	}

	return 0;
}


// copy_directory 처리 함수 
int copy_directory(int argc, char **argv)
{
	int i, thread_count = 0;
	DIR *dp_source, *dp_target;
	struct dirent *d_entry;
	struct stat st = {0};
	pthread_t tid[MAXTHREAD];
	thread_arg_t thread_arg[MAXTHREAD]; // thread 함수에 전달할 인자들(배열에서 하나씩 인자로 전달)
	

	// 잘못된 사용법인 경우
	if (argc != 3) {
		fprintf(stderr, "Usage: dcp <source_directory_name> <target_directory_name>\n");
		return 1;
	}

	// source directory 오픈
	dp_source = opendir(argv[1]);

	if (dp_source == NULL){
		perror("source directory open error.\n");
		return 1;
	}

	// target directory 오픈
	dp_target = opendir(argv[2]);

	if (dp_target == NULL){ // target directory 없는 경우, 새로 생성.
		if (mkdir(argv[2], DEFAULT_DIR_MODE)){
			fprintf(stderr, "target directory makedir error.\n");
			return 1;
		}
	}

	// thread 함수와 공유할 전역 변수의 값을 설정
	//   (스레드 함수 내에서 읽기 전용이므로 별도의 동기화 처리 하지 않음)
	argv1 = argv[1];
	argv2 = argv[2];


	// dir 항목 읽기
	d_entry = readdir(dp_source);
	
	while (d_entry != NULL){

		if (stat(d_entry->d_name, &st)){
			fprintf(stderr, "dir_entry stat read error.\n");
			return 1;
		}

		if (S_ISREG(st.st_mode)) { // file만 복사(하위 디렉토리는 제외)

			// "스레드 개수 < MAX 스레드 개수" 인 경우
			if (thread_count < MAXTHREAD) {

				// 스레드 생성해서 파일 복사
				thread_arg[thread_count].thread_num = thread_count;
				thread_arg[thread_count].d_name = d_entry->d_name;

				pthread_create(&tid[thread_count], NULL, thread_fn, (void *) &thread_arg[thread_count]);
				thread_count++;

			} else { // "스레드 개수 >= MAX 개수" 인 경우
				
				// 모든 스레드의 join을 기다림
				for (i = 0; i < MAXTHREAD; i++) {
					pthread_join(tid[i], NULL);
				}
				
				// 스레드 개수 다시 초기화 후 스레드 새로 생성
				thread_count = 0;

				thread_arg[thread_count].thread_num = thread_count;
				thread_arg[thread_count].d_name = d_entry->d_name;

				pthread_create(&tid[thread_count], NULL, thread_fn, (void *) &thread_arg[thread_count]);
				thread_count++;

			}
		}
		
		d_entry = readdir(dp_source);
	}

	return 0;
}



/**** 스레드 함수 구현 *****/

void * thread_fn(void * thread_arg)
{
	int in_fd, out_fd, rd_count, wr_count;
	char buffer[4096];
	char target_file[MAXLINE];
	thread_arg_t *arg = (thread_arg_t *) thread_arg;


	// thread 함수 인자로 전달받은 filename을 open
	if ((in_fd = open(arg->d_name, O_RDONLY)) < 0) {
		fprintf(stderr, "source file %s open error.\n", arg->d_name);
		return NULL;
	}

	// 생성될 file의 pathname/filename의 문자열(target_file)을 만들고, 해당 파일을 creat
	memset(target_file, '\0', strlen(target_file));
	strcpy(target_file, argv2);
	strcat(target_file, "/");
	strcat(target_file, arg->d_name);

	if ((out_fd = creat(target_file, DEFAULT_FILE_MODE)) < 0){
		fprintf(stderr, "target file %s create error.\n", target_file);
		return NULL;
	}

	// 스레드 내용을 화면에 출력
	printf("Thread[%d]: copy '%s/%s' into '%s'\n", arg->thread_num, argv1, arg->d_name, target_file);

	// copy file 시작
	while (1) {
		if ((rd_count = read(in_fd, buffer, 4096)) <= 0)
			break;
		if ((wr_count = write(out_fd, buffer, rd_count)) <= 0)
			return NULL;
	}

	if (rd_count < 0){
		fprintf(stderr, "file read error.\n");
		return NULL;
	}
	if (wr_count < 0){
		fprintf(stderr, "file write error.\n");
		return NULL;
	}

	if (close(in_fd)) return NULL;
	if (close(out_fd)) return NULL;

	pthread_exit(NULL);
}
