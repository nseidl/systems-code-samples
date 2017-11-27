// nush.c
//   - Basic parent and child forking based off of Nat Tuck's lecture notes
//   - http://www.ccs.neu.edu/home/ntuck/courses/2017/09/cs3650/notes/09-fork-exec/sort-pipe/sort-pipe.c

// strarrlist.c
//   - code is from Nat Tuck's lecture notes 07-io-and-shell
//   - http://www.ccs.neu.edu/home/ntuck/courses/2017/09/cs3650/notes/07-io-and-shell/calc/
//   - strarrlist_contains and strarrlist_delete were written by myself (Nick Seidl)

// strarrlist.h
//   - This code is from Nat Tuck's lecture notes 07-io-and-shell
//   - http://www.ccs.neu.edu/home/ntuck/courses/2017/09/cs3650/notes/07-io-and-shell/calc/
//   - strarrlist_contains and strarrlist_delete were written by myself (Nick Seidl)

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

// string arraylist utility
#include "strarrlist.h"

// for parsing input line into tokens and executing the tokens
int is_letter(char c);
int is_operator(char c);
int is_space(char c);
int is_end(char c);
int is_not_operator(char* t);
strarrlist* parse_line(char *line);
int handle_command(strarrlist* sal);
int process_line(char* line);

// for executing a specific type of command
void execute_normal(char* cmd, char* args[]);
char* execute_pipe(char* left_args[], char* right_args[]);
char* execute_in(char* left_args[], char* right_args[]);
int execute_and(char* left_args[], char* right_args[]);
void execute_or(char* left_args[], char* right_args[]);
void execute_out(char* left_args[], char* right_args[]);


int
main(int argc, char* argv[])
{
  // set output to unbuffered mode
  // necessary to get sleep test working
  setvbuf(stdout, NULL, _IONBF, 0);

  size_t size;
  char* line = NULL;

  if (argc == 1) { // if nush is in live user interactive mode
    printf("nush$ ");
    while (1)
    {
      while (getline(&line, &size, stdin) != -1)
      {
        if (process_line(line) == 0)
        {
          free(line);
          return 0;
        }

        printf("nush$ ");
      }

      free(line);
      printf("\n");
      return 0;
    }
  }
  else // if nush is being run by a script / has input passed in
  {
    char* file_to_read_from = argv[1];
    FILE* input = fopen(file_to_read_from, "r");
    
    while (getline(&line, &size, input) != -1)
    {
      if (process_line(line) == 0)
      {
        free(line);
        return 0;
      }
    }
  }

  free(line);
  return 0;
}

// tokenizes line via parsing; then executes
// return 1 - continue
// return 0 - quit
int
process_line(char* line)
{
  line[strlen(line)-1] = '\0';
  strarrlist* tokens = parse_line(line);
  int rv = handle_command(tokens);
  free_strarrlist(tokens);
  if (rv == 0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

// executes a normal command (one without an operator)
// exit and cd are implemented as native so are excluded from this
void
execute_normal(char* cmd, char* args[])
{
  // immediately return if either is invalid
  if (cmd == NULL || args == NULL)
  {
    return;
  }

  // necessary for dealing with children and piping
  int cpid;
  int p_io[2];
  pipe(p_io);
  int p_read = p_io[0];
  int p_write = p_io[1];

  if ((cpid = fork())) { // in parent, print the output of the child
    int status;
    waitpid(cpid, &status, 0);
    close(p_write);

    // print whatever is in our pipe
    char *output = malloc(1);
    int i = 0;
    while(read(p_read, output+i, 1))
    {
      i++;
      output = realloc(output, i+1);
    }
    output[i] = '\0';
    printf("%s", output);

    free(output);
    return;
  }
  else { // in child, save output to pipe, and execute command
    close(p_read);
    dup2(p_write, 1); // change stdout of child to go to p_write

    execvp(cmd, args);
  }

  return;
}

// executes left_args and then pipes that output to right_args
char*
execute_pipe(char* left_args[], char* right_args[])
{
  if (left_args == NULL || right_args == NULL 
    || left_args[0] == NULL || right_args[0] == NULL)
  {
    return "Error.";
  }

  int cpid;
  int p_io[2];
  pipe(p_io);
  int p_read = p_io[0];
  int p_write = p_io[1];

  int new_cpid;
  int p_result[2];
  pipe(p_result);
  int p_result_read = p_result[0];
  int p_result_write = p_result[1];

  // run left args; continue with program in parent
  if ((cpid = fork())) {

  }
  else // in child, run the left side
  {
    close(p_read);
    dup2(p_write, 1); // change stdout of the child to go to p_write

    execvp(left_args[0], left_args);
  }

  // print output of right command in parent
  if ((new_cpid = fork()))
  {
    // close pipe stuff we don't need
    close(p_write);
    close(p_result_write);
    close(p_read);

    // return output of right command
    char *output = malloc(1);
    int i = 0;
    while(read(p_result_read, output+i, 1))
    {
      i++;
      output = realloc(output, strlen(output)+1);
    }
    output[i] = '\0';

    return output;
  }
  else // run right command and save output to pipe
  {
    close(p_write);
    dup2(p_read, 0); // change stdin of the child to come from p_read

    close(1);
    dup2(p_result_write, 1); // change stdout of the child to go to p_result_write

    execvp(right_args[0], right_args);
  }

  return "Error.";
}

// left_args gets right_arg's output as input
char* 
execute_in(char* left_args[], char* right_args[])
{
  if (left_args == NULL || right_args == NULL || left_args[0] == NULL || right_args[0] == NULL)
  {
    return "Error.";
  }

  int cpid;

  int p_io[2];
  pipe(p_io);
  int p_read = p_io[0];
  int p_write = p_io[1];

  // execute the left hand side and pipe output back
  if ((cpid = fork())) 
  {
    int status;
    waitpid(cpid, &status, 0);
    close(p_write);

    char *output = malloc(1);
    
    int i = 0;
    while(read(p_read, output+i, 1))
    {
      i++;
      output = realloc(output, strlen(output)+1);
    }

    output[i] = '\0';

    close(p_read);

    return output;
  }
  else 
  {
    dup2(fileno(fopen(right_args[0], "r")), STDIN_FILENO); // change stdin of child to get from file
    dup2(p_write, 1); // change stdout of child to go to p_write
    execvp(left_args[0], left_args);
  }

  return "Error.";
}

// right_args gets its input as left_arg's output
void
execute_out(char* left_args[], char* right_args[])
{
  if (left_args == NULL || right_args == NULL || left_args[0] == NULL || right_args[0] == NULL)
  {
    return;
  }

  int cpid;

  // execute the left hand side and pipe output back
  if ((cpid = fork())) 
  {
    int status;
    waitpid(cpid, &status, 0);
  }
  else 
  {
    dup2(fileno(fopen(right_args[0], "w")), 1); // change stdout of child to go to p_write
    execvp(left_args[0], left_args);
  }
}

// if left_args is false; return immediately
// else, execute the right, and either print both or return
int
execute_and(char* left_args[], char* right_args[])
{
  if (left_args == NULL || right_args == NULL || left_args[0] == NULL || right_args[0] == NULL)
  {
    return 0;
  }

  int cpid;
  int p_io[2];
  pipe(p_io);
  int p_read = p_io[0];
  int p_write = p_io[1];
  if ((cpid = fork())) // in parent, check if left failed; if it didn't, continue to right
  {
    int status;
    waitpid(cpid, &status, 0);
    close(p_write);
    if (status != 0) // immediately return if left side of and is false
    {
      close(p_read);
      return 0;
    }
  }
  else 
  {
    dup2(p_write, 1); // save output of left in our pipe
    execvp(left_args[0], left_args);
  }

  // if the right is to exit, exit
  if (strncmp("exit", right_args[0], 4) == 0)
  {
    close(p_read);
    return 1;
  }

  int new_cpid;
  int p_io_res[2];
  pipe(p_io_res);
  int p_read_res = p_io_res[0];
  int p_write_res = p_io_res[1];
  if ((new_cpid = fork())) // in parent, print left out then right out if right succeeded
  {
    int status;
    waitpid(new_cpid, &status, 0);

    close(p_write_res);

    if (status == 0) // if right was successful, print both
    {
      char *right_output = malloc(1);
      char *left_output = malloc(1);

      int i = 0;
      while(read(p_read_res, right_output+i, 1))
      {
        i++;
        right_output = realloc(right_output, strlen(right_output)+1);
      }

      right_output[i] = '\0';

      i = 0;
      while(read(p_read, left_output+i, 1))
      {
        i++;
        left_output = realloc(left_output, strlen(left_output)+1);
      }

      left_output[i] = '\0';

      close(p_read);
      close(p_read_res);

      printf("%s", left_output);
      printf("%s", right_output);

      free(right_output);
      free(left_output);

      return 0;
    }
    else // if right wasn't successful, and was a failure
    {
      close(p_read_res);
      close(p_read);
      return 1;
    }
  }
  else
  {
    dup2(p_write_res, 1); // save output of right args into p_write_res
    execvp(right_args[0], right_args);
  }

  return 1;
}

// if left is successful, return
// else, print output of right
void
execute_or(char* left_args[], char* right_args[])
{
  if (left_args == NULL || right_args == NULL || left_args[0] == NULL || right_args[0] == NULL)
  {
    return;
  }

  int cpid;

  if ((cpid = fork())) // in parent, return immediately if the left was successful
  {
    int status;
    waitpid(cpid, &status, 0);
    if (status == 0) // immediately return if left is successful
    {
      return;
    }
  }
  else
  {
    execvp(left_args[0], left_args);
  }

  int new_cpid;

  int p_io[2];
  pipe(p_io);
  int p_read = p_io[0];
  int p_write = p_io[1];

  if ((new_cpid = fork())) // in parent, print output of right
  {
    int status;
    waitpid(cpid, &status, 0);

    close(p_write);

    char *output = malloc(1);
    
    int i = 0;
    while(read(p_read, output+i, 1))
    {
      i++;
      output = realloc(output, strlen(output)+1);
    }

    output[i] = '\0';

    close(p_read);

    printf("%s", output);
  }
  else // in child, change command's output to our pipe
  {
    dup2(p_write, 1);
    execvp(right_args[0], right_args);
  }
}

// takes tokenized user's input and executes it
// currently can only handle one operand per command (besides semicolons)
int
handle_command(strarrlist* tokens)
{
  while(tokens->size > 0) // only do something if their tokenized input is non-empty
  {
    char** args = (char**)malloc((tokens->size + 1) * sizeof(char*));

    int len;
    for (len = 0; len < tokens->size + 1; len++)
    {
      args[len] = NULL;
    }

    // delete first token if it's an operator
    if (is_not_operator(strarrlist_get(tokens, 0)) == 0)
    {
      strarrlist_delete_first(tokens);
    }
    
    // exit program if input is exit
    if (strncmp(tokens->data[0], "exit", 4) == 0)
    {
      int fr;
      for (fr = 0; fr < len - 1; fr++)
      {
        if (args[fr] != NULL)
        {
          free(args[fr]);
        }
      }
      free(args);
      return 0;
    }
    else if (strncmp(tokens->data[0], "cd", 2) == 0) // cd if input is to cd
    {
      chdir(tokens->data[1]);
      strarrlist_delete_first(tokens); // delete cd
      strarrlist_delete_first(tokens); // delete directory
    }
    else
    {
      // put all the stuff to the left of the first semicolon into the current thing to be executed
      int i = 0;
      while (i < tokens->size && strncmp(strarrlist_get(tokens, i), ";", 1) != 0)
      {
        args[i] = strdup(strarrlist_get(tokens, i));
        //printf("<%s>\n", args[i]);
        i++;
      }
      args[i] = 0;

      // indices of operators
      int pipe = strarrlist_contains(tokens, "|");
      int bg = strarrlist_contains(tokens, "&");
      int in = strarrlist_contains(tokens, "<");
      int out = strarrlist_contains(tokens, ">");
      int and = strarrlist_contains(tokens, "&&");
      int or = strarrlist_contains(tokens, "||");

      if (pipe >= 0) // if a pipe was present
      {
        int left_ind = 0;
        int right_ind = 0;
        char *left_args[tokens->size];
        char *right_args[tokens->size];

        // add arguments left of the pipe to the left args
        // add arguments right of the pipe to the right args
        int a;
        for (a = 0; a < len; a++)
        {
          if (a < pipe)
          {
            left_args[left_ind] = args[a];
            //printf("arg %i LEFT of |: %s\n", left_ind, left_args[left_ind]);
            left_ind++;
          }
          else if (a > pipe)
          {
            right_args[right_ind] = args[a];
            //printf("arg %i RIGHT of |: %s\n", right_ind, right_args[right_ind]);
            right_ind++;
          }
        }

        left_args[left_ind] = 0;
        right_args[right_ind] = 0;

        char* result = execute_pipe(left_args, right_args);
        printf("%s", result);
      }
      else if (and >= 0)
      {
        int left_ind = 0;
        int right_ind = 0;
        char *left_args[tokens->size];
        char *right_args[tokens->size];

        int a;
        for (a = 0; a < len; a++)
        {
          if (a < and)
          {
            left_args[left_ind] = args[a];
            //printf("arg %i LEFT of &&: %s\n", left_ind, left_args[left_ind]);
            left_ind++;
          }
          else if (a > and)
          {
            right_args[right_ind] = args[a];
            //printf("arg %i RIGHT of &&: %s\n", right_ind, right_args[right_ind]);
            right_ind++;
          }
        }

        left_args[left_ind] = 0;
        right_args[right_ind] = 0;

        // if exit was result of the and, free args and return
        int ex = execute_and(left_args, right_args);
        if (ex == 1)
        {
          int fr;
          for (fr = 0; fr < len - 1; fr++)
          {
            if (args[fr] != NULL)
            {
              free(args[fr]);
            }
          }
          free(args);
          return 0;
        }
      }
      else if (or >= 0)
      {
        int left_ind = 0;
        int right_ind = 0;
        char *left_args[tokens->size];
        char *right_args[tokens->size];

        int a;
        for (a = 0; a < len; a++)
        {
          if (a < or)
          {
            left_args[left_ind] = args[a];
            //printf("arg %i LEFT of &&: %s\n", left_ind, left_args[left_ind]);
            left_ind++;
          }
          else if (a > or)
          {
            right_args[right_ind] = args[a];
            //printf("arg %i RIGHT of &&: %s\n", right_ind, right_args[right_ind]);
            right_ind++;
          }
        }

        left_args[left_ind] = 0;
        right_args[right_ind] = 0;

        execute_or(left_args, right_args);
      }
      else if (bg >= 0)
      {
        int left_ind = 0;
        char *left_args[tokens->size];

        int a;
        for (a = 0; a < len; a++)
        {
          if (a < bg)
          {
            left_args[left_ind] = args[a];
            left_ind++;
          }
        }

        left_args[left_ind] = 0;

        // no need for a helper method, just immediately kick off the child 
        // and parent continues program
        if (left_args[0] != NULL)
        {
          int cpid;

          if ((cpid = fork()))
          {

          }
          else
          {
            execvp(left_args[0], left_args);
          } 
        }
      }
      else if (in >= 0)
      {
        int left_ind = 0;
        int right_ind = 0;
        char *left_args[tokens->size];
        char *right_args[tokens->size];

        int a;
        for (a = 0; a < len; a++)
        {
          if (a < in)
          {
            left_args[left_ind] = args[a];
            // printf("arg %i LEFT of <: %s\n", left_ind, left_args[left_ind]);
            left_ind++;
          }
          else if (a > in)
          {
            right_args[right_ind] = args[a];
            // printf("arg %i RIGHT of <: %s\n", right_ind, right_args[right_ind]);
            right_ind++;
          }
        }

        left_args[left_ind] = 0;
        right_args[right_ind] = 0;

        char* result = execute_in(left_args, right_args);
        printf("%s", result);
      }
      else if (out >= 0)
      {
        int left_ind = 0;
        int right_ind = 0;
        char *left_args[tokens->size];
        char *right_args[tokens->size];

        int a;
        for (a = 0; a < len; a++)
        {
          if (a < out)
          {
            left_args[left_ind] = args[a];
            // printf("arg %i LEFT of >: %s\n", left_ind, left_args[left_ind]);
            left_ind++;
          }
          else if (a > out)
          {
            right_args[right_ind] = args[a];
            // printf("arg %i RIGHT of >: %s\n", right_ind, right_args[right_ind]);
            right_ind++;
          }
        }

        left_args[left_ind] = 0;
        right_args[right_ind] = 0;

        execute_out(left_args, right_args); 
      }
      else // if there were no special operators, just execute the command normally
      {
        execute_normal(args[0], args);
      }

      // delete the tokens we just executed
      int q;
      for (q = 0; q <= i; q++)
      {
        if (tokens->size > 0)
        {
          strarrlist_delete_first(tokens);
        }
      }
    }

    // free args
    int fr;
    for (fr = 0; fr < len - 1; fr++)
    {
      if (args[fr] != NULL)
      {
        free(args[fr]);
      }
    }
    free(args);
  }

  return 1;
}

/* Separates the user input into tokens
*/
strarrlist*
parse_line(char *line)
{
  strarrlist* tokens_of_line = make_strarrlist();

  int i = 0;
  char *token = malloc(1);
  int token_ind = 0;

  // iterate through the string
  while (i < strlen(line))
  { 
    char c = line[i];
    char next = line[i+1];
    if (i == strlen(line) - 1)
    {
      next = '\0';
    }

    if (is_letter(c))
    {
      if (is_operator(next) || is_space(next) || is_end(next))
      {
        // add the final character to our current token
        // terminate our token string with null terminator
        // add the token to tokens
        // reset token to empty
        token[token_ind] = c;
        token_ind++;
        token = realloc(token, token_ind+1);
        token[token_ind] = '\0';
        strarrlist_add(tokens_of_line, token);
        free(token);
        //printf("let<%s>\n", token);
        token = malloc(1);
        token_ind = 0;
      }
      else
      {
        token[token_ind] = c;
        token_ind++;
        token = realloc(token, token_ind+1);
      }
    }
    else if (is_operator(c))
    {
      if (is_letter(next) || is_space(next) || is_end(next))
      {
        token[token_ind] = c;
        token_ind++;
        token = realloc(token, token_ind+1);
        token[token_ind] = '\0';
        strarrlist_add(tokens_of_line, token);
        free(token);
        //printf("op<%s>\n", token);
        token = malloc(1);
        token_ind = 0;
      }
      else
      {
        token[token_ind] = c;
        token_ind++;
        token = realloc(token, token_ind+1);
      }
    }

    //move to the next char
    i++;
  }

  free(token);
  return tokens_of_line;
}

/* Determines if the given char is a letter (within the scope of assignment 2)
Returns:
1 - is a letter
0 - is not a letter
*/
int
is_letter(char c)
{
  return c == '/' || (!is_operator(c) && !is_space(c) && !is_end(c));
}

/* Determines if the given char is an operator (within the scope of assignment 2)
Returns:
1 - is an operator
0 - is not an operator
*/
int
is_operator(char c)
{
  return c == '<' || c == '>' || c == '|' || c == '&' || c == ';';
}

/* Determines if the given char is a space (within the scope of assignment 2)
Returns:
1 - is an space
0 - is not a space
*/
int
is_space(char c)
{
  return c == ' ' || c == '\t' || c == '\v';
}

/* Determines if the given char is a string terminator (within the scope of assignment 2)
Returns:
1 - string temrinator
0 - not string terminator
*/
int 
is_end(char c)
{
  return c == '\0';
}

// resturns 1 if the char is not an operator (a letter, /, or .)
int
is_not_operator(char* t)
{
  return strncmp(t, "|", 1) != 0
  && strncmp(t, "||", 2) != 0
  && strncmp(t, "&", 1) != 0
  && strncmp(t, "&&", 2) != 0
  && strncmp(t, "<", 1) != 0
  && strncmp(t, ">", 1) != 0
  && strncmp(t, ";", 1) != 0;
}
