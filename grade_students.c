
#include <stdio.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <wait.h>

#define ERROR "Error in system call\n"
#define DELIMITERS "\n"

#define NO_C_FILE "NO_C_FILE\n"
#define COMPILATION_ERROR "COMPILATION_ERROR\n"
#define TIMEOUT "TIMEOUT\n"
#define BAD_OUTPUT "BAD_OUTPUT\n"
#define SIMILAR_OUTPUT "SIMILAR_OUTPUT\n"
#define GREAT_JOB "GREAT_JOB\n"

#define MAX_CONF_SIZE 482
#define MAX_LINE 160
#define OUTPUT_FILE_PATH "output.txt"
#define COMP_EXE "./comp.out"
#define STUD_EXE_NAME "stud_exe.out"
#define STUD_EXE "./stud_exe.out"

/// get files content
/// \param file_path file to read
/// \param buf buffer to put file content in
void getFileContent(char *file_path, char *buf);

/// recursively search directories in given directory for c file
/// \param dir_path directory to search in
/// \param c_path the c file path
void dirSearch(char *dir_path, char *c_path);

/// scan given directory file and directories
/// \param dir_path directory path
/// \param input_path input for c programs
/// \param correct_output_path correct output to compare
/// \param result_fd file descriptor to write results to
void scanDirectory(char *dir_path, char *input_path, char *correct_output_path,
                  int result_fd);

/// handle given c file
/// \param c_path path to c file
/// \param dir_name parent directory of c file
/// \param input_path input for c program
/// \param correct_output_path correct output to compare
/// \param result_fd file descriptor to write results to
void handleFile(char *c_path, char *dir_name, char *input_path,
                char *correct_output_path, int result_fd);

/// compile c file
/// \param c_path path to file
/// \return status
int compile(char *c_path);

/// run executable file created from compile
/// \param input_path input to executable
/// \return status
int run(char *input_path);

/// compare c program output to correct output
/// \param correct_output_path correct output
/// \return result of comparison
int compare(char *correct_output_path);

/// return files type
/// \param parent path of parent directory
/// \param name name of entry in parent
/// \return type
int isDirectory(char *parent, char *name);

/// check if file is a c file
/// \param file_name file to check
/// \return 0 if is c file, 1 o.w
int isCextension(const char *file_name);

/// main
/// \param argc count of argv
/// \param argv args to program
/// \return status
int main(int argc, char *argv[]) {

  int result_fd = open("results.csv", O_RDWR | O_TRUNC | O_CREAT, S_IRUSR |
      S_IWUSR);
  if (result_fd < 0) {
    write(2, ERROR, strlen(ERROR));
    exit(-1);
  }

  char conf_file_buffer[MAX_CONF_SIZE];

  // get content of config file
  getFileContent(argv[1], conf_file_buffer);

  // get students file path
  char students_file[MAX_LINE];
  strcpy(students_file, strtok(conf_file_buffer, DELIMITERS));

  // get input file path
  char input_file[MAX_LINE];
  strcpy(input_file, strtok(NULL, DELIMITERS));

  // get output file path
  char correct_output_file[MAX_LINE];
  strcpy(correct_output_file, strtok(NULL, DELIMITERS));

  // scan students directory
  scanDirectory(students_file, input_file, correct_output_file, result_fd);

  // close result file
  close(result_fd);
}

int compare(char *correct_output_path) {

  // fork -- run
  int child_ret_val = 1;
  int child_pid = fork();
  if (child_pid != 0) {
    // parent
    // wait for child
    waitpid(child_pid, &child_ret_val, 0);

  } else {
    // child

    char *args[] = {COMP_EXE, OUTPUT_FILE_PATH, correct_output_path, NULL};
    execvp(args[0], args);
  }

  unlink(OUTPUT_FILE_PATH);

  return WEXITSTATUS(child_ret_val);
}

int run(char *input_path) {

  // fork -- run
  int child_ret_val = 1;
  int child_pid = fork();
  if (child_pid != 0) {
    // parent
    // wait for child
    sleep(5);

    int is_timeout = waitpid(child_pid, &child_ret_val, WNOHANG);

    if(is_timeout == 0) {
      return 1;
    }
    unlink(STUD_EXE);

    return 0;

  } else {
    // child
    int output_fd =
        open(OUTPUT_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    int input_fd = open(input_path, O_RDONLY);

    dup2(output_fd, 1);
    dup2(input_fd, 0);

    close(output_fd);
    close(input_fd);

    char *args[] = {STUD_EXE, NULL};
    execvp(args[0], args);
  }
}

int compile(char *c_path) {
  // fork -- compile
  int child_ret_val = 1;
  int child_pid = fork();
  if (child_pid != 0) {
    // parent
    // wait for child
    waitpid(child_pid, &child_ret_val, 0);

  } else {
    // child
    char *args[] = {"gcc", "-o", STUD_EXE_NAME, c_path, NULL};
    execvp(args[0], args);
  }

  return WEXITSTATUS(child_ret_val);
}

void handleFile(char *c_path, char *dir_name, char *input_path,
                char *correct_output_path, int result_fd) {

  if (strcmp(c_path, "") == 0) {
    char result[MAX_LINE];
    sprintf(result, "%s,%d,%s\0", dir_name, 0, NO_C_FILE);
    write(result_fd, result, strlen(result));
    return;

  } else if (compile(c_path) != 0) {
    char result[MAX_LINE];
    sprintf(result, "%s,%d,%s\0", dir_name, 0, COMPILATION_ERROR);
    write(result_fd, result, strlen(result));
    return;
  }

  if (run(input_path)) {
    char result[MAX_LINE];
    sprintf(result, "%s,%d,%s\0", dir_name, 0, TIMEOUT);
    write(result_fd, result, strlen(result));
    return;
  }

  int comp_status = compare(correct_output_path);

  if (comp_status == 1) {
    char result[MAX_LINE];
    sprintf(result, "%s,%d,%s\0", dir_name, 60, BAD_OUTPUT);
    write(result_fd, result, strlen(result));
    return;
  } else if (comp_status == 2) {
    char result[MAX_LINE];
    sprintf(result, "%s,%d,%s\0", dir_name, 80, SIMILAR_OUTPUT);
    write(result_fd, result, strlen(result));
    return;
  } else if (comp_status == 3) {
    char result[MAX_LINE];
    sprintf(result, "%s,%d,%s\0", dir_name, 100, GREAT_JOB);
    write(result_fd, result, strlen(result));
    return;
  } else {
    printf("handle file bad comp status: %d\n", comp_status);
    write(2, ERROR, strlen(ERROR));
    exit(-1);
  }
}

void scanDirectory(char *dir_path, char *input_path, char *correct_output_path,
                  int result_fd) {
  DIR *cur_dir;
  struct dirent *dir;

  char c_path[PATH_MAX] = "";

  cur_dir = opendir(dir_path);
  if (cur_dir) {
    while ((dir = readdir(cur_dir)) != NULL) {
      strcpy(c_path, "");
      if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {

        char *temp_dir = malloc(strlen(dir_path) + strlen(dir->d_name) + 2);
        sprintf(temp_dir, "%s/%s", dir_path, dir->d_name);
        dirSearch(temp_dir, c_path);
        free(temp_dir);

        handleFile(c_path, dir->d_name, input_path, correct_output_path,
                   result_fd);
      }

    }

    closedir(cur_dir);
  }
}

int isDirectory(char *parent, char *name) {
  struct stat st_buf;
  if (!strcmp(".", name) || !strcmp("..", name)) {
    return -1;
  }
  char *path = alloca(strlen(name) + strlen(parent) + 2);
  sprintf(path, "%s/%s", parent, name);
  stat(path, &st_buf);

  return S_ISDIR(st_buf.st_mode);
}

int isCextension(const char *file_name) {
  int i = 0;
  while (file_name[i] != '\0') {
    if (file_name[i] == '.' && file_name[i + 1] == 'c' && file_name[i + 2] == '\0') {
      return 1;
    }
    i++;
  }
  return 0;
}

void dirSearch(char *dir_path, char *c_path) {

  DIR *dir = opendir(dir_path);
  struct dirent *ent;

  while ((ent = readdir(dir))) {
    char *entry_name = ent->d_name;

    int type = isDirectory(dir_path, entry_name);

    if (type != -1 && type != 0) {
      // type of entry is a directory without '.' and '..'

      char *next = malloc(strlen(dir_path) + strlen(entry_name) + 2);
      sprintf(next, "%s/%s", dir_path, entry_name);

      // continue searching
      dirSearch(next, c_path);

      free(next);
    } else if (type == 0) {
      // type of entry is a file

      if (isCextension(entry_name) != NULL) {
        // found the .c file
        sprintf(c_path, "%s/%s", dir_path, entry_name);
        break;
      }
    }
  }
  closedir(dir);
}

void getFileContent(char *file_path, char *buf) {

  int file = open(file_path, O_RDONLY);
  char c;

  if (file > 0) {
    while ((read(file, &c, 1)) > 0) {
      *(buf++) = c;
    }
    *(buf) = '\0';

    close(file);
  }
}