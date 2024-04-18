#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int prepare(void){
    /* prepare program before executing proccess_arglist */

    // overwrite default behaviour of ctrl+c
    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {   // if condition is met error occured with SIGNAL handling
        perror("SIGINT handler");
        return -1;// -1 indicates failure
    }
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) { // dealing with zombies
        perror("Error - failed to change signal SIGCHLD handling");
        return -1;
    }
    return 0; // completed successfuly
}

int finalize(void){ //signals are handeled inside funcitons by instructions
    return 0; // completed successfuly
}

int find_pipe_location(int count, char **arglist){
    /*
    find the pipe operator "|" in arglist
    $ret --> index: if exists the index of | else -1
    */
   for(int i=0; i < count; i++){
    if(*arglist[i] == '|'){
        return i;
    }
   }
   return -1; // if pipe | isn't found
}

int executing_commands(char **arglist) {
    /*
    execution of general commands that are not one of the special ones: | < > &
    parent proccess must WAIT for child to complete
    */
    pid_t pid = fork();
    if (pid == -1) { // fork failed
        perror("fork");
        return 0; // parent process failed
    } else if (pid == 0) { // Child process

        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            // // restore to default sigint handling 
            perror("SIGINT handling");
            exit(1);
        }
        if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
             // restore to default sigchild handling 
            perror("SIGCHLD handling");
            exit(1);
        }

        if (execvp(arglist[0], arglist) == -1) { // executing command failed
            perror("execvp - execution failed");
            exit(EXIT_FAILURE); //child process failed
        }
    }

    if (waitpid(pid, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {
        // ECHILD and EINTR in the parent shell after waitpid are not considered as errors
        perror("waitpid");
        return 0; // parent process failed
    }
    return 1;  //parent process completed succesfully
}

int background_command(int count,char** arglist)
{   /* run background commands (ends with &) with a child process */
    pid_t pid = fork();
   
   if(pid == -1){
    /*handle fork failure*/
    perror("fork");
    return 0; //parent proccess error
   }

   else if(pid == 0){
    //child proccess
        arglist[count - 1] = NULL; // don't pass "&"

        if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
             // restore to default sigchild handling 
            perror("SIGCHLD handling");
            exit(1);
        }

        if (execvp(arglist[0], arglist) == -1) { // executing command failed
            perror("execvp - execution failed");
            exit(EXIT_FAILURE); //child process failed
        }
    }
    return 1;  //parent process completed succesfully
}

int redirect(char *filename, char **arglist, char direction)
{
    /*
    input: filename , arglist, direction = < or >
    notation: cmd < input.txt
    create child proccess
    redirect child's stdin/stdout to be input/output.txt using dup2()
    execute the cmd using execvp
    */
    int fd=-1;
    pid_t pid = fork();

    if (pid==-1){ //failed to create child process
        perror("fork");
        return 0; // error in parent process
    }
    if (pid==0){ //child process

        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
                // // restore to default sigint handling 
                perror("SIGINT handling");
                exit(1);
        }
        if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
            // restore to default sigchild handling 
            perror("SIGCHLD handling");
            exit(1);
        }

        if (direction == '<') { // Input redirection

            fd = open(filename, O_RDONLY); // Open file for reading
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            // Redirect standard input (file descriptor 0) to fd
            if (dup2(fd, STDIN_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
        }

        else if (direction == '>') { // Output redirection
            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777); // Open file for writing, create if not exists, truncate
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            // Redirect standard output (file descriptor 1) to fd
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
        }
        close(fd); // Close the original file descriptor
        if (execvp(arglist[0], arglist) == -1) { // executing command failed
            perror("Execvp - failed execution");
            exit(1);
        }
    }
    
    // Parent process
    if (waitpid(pid, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {
        // ECHILD and EINTR in the parent shell after waitpid are not considered as errors
        perror("waitpid");
        return 0; // error in parent process
    }
    return 1; // parent process succesfuly completed

}

int pipe_command(int pipe_index, char** arglist)
{
    /*  
    input arglist, pipe_index = index of | operator
    steps:
        create child1 - writing child to stdout.
        redirect stdout to the write side of the pipe
        execute left/first command
        close pipe
        create child2 - reading child from stdin.
        redirect stdin to the read side of the pipe
        execute right/second command
        close pipes and wait for children to return
     */
    int pfds[2];
    arglist[pipe_index] = NULL; // diving the command to 2 executables
    if(pipe(pfds)==-1){//failed to start a pipe
        perror("Pipe");
        return 0; //error in parent process
    }

    pid_t pid1 = fork();
   
   if(pid1==-1){
    /*handle fork failure*/
    perror("fork");
    return 0; //error in parent process
   }

    //child 1 --> process writing child
    else if(pid1==0){

        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
                // // restore to default sigint handling 
                perror("SIGINT handling");
                exit(1);
        }
        if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
            // restore to default sigchild handling 
            perror("SIGCHLD handling");
            exit(1);
        }

        close(pfds[0]); //close reading side of pipe

        if (dup2(pfds[1], STDOUT_FILENO) == -1) { // Redirect stdout to the write side of the pipe
            perror("dup2 - redirecting failed");
            exit(EXIT_FAILURE);
        }

        close(pfds[1]); //close writing side of pipe
        // Execute the First command
        if(execvp(arglist[0], arglist) == -1){
            // If execvp returns, an error occurred
            perror("execvp - execution failed");
            return 0; // parent process failure
        };
    }

    pid_t pid2 = fork();
   
   if(pid2==-1){
    /*handle fork failure*/
    perror("fork");
    return 0; //error in parent process
   }

    //child 2 --> process reading child
    else if(pid2==0){

        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
                // // restore to default sigint handling 
                perror("SIGINT handling");
                exit(1);
        }
        if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
            // restore to default sigchild handling 
            perror("SIGCHLD handling");
            exit(1);
        }

        close(pfds[1]); //close writing side of pipe

        if (dup2(pfds[0], STDIN_FILENO) == -1) { // Redirect stdout to the read side of the pipe
            perror("dup2 - redirecting failed");
            exit(EXIT_FAILURE);
        }

        close(pfds[0]); //close reading side of pipe
        // Execute the second command
        if(execvp(arglist[pipe_index + 1], arglist + pipe_index + 1) == -1){
            // If execvp returns, an error occurred
            perror("execvp - execution failed");
            return 0; // parent process failure
        };
    }

    // Close both ends of the pipe in the parent process
    close(pfds[0]);
    close(pfds[1]);

    // Wait for both child processes to complete
    // waiting for the first child
    if (waitpid(pid1, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {
        // ECHILD and EINTR in the parent shell after waitpid are not considered as errors
        perror("waitpid");
        return 0; //error in parent process
    }
    // waiting for the first child
    if (waitpid(pid2, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {
        // ECHILD and EINTR in the parent shell after waitpid are not considered as errors
        perror("waitpid");
        return 0; //error in parent process
    }

    return 1; //parent process completed succesfully
}

int process_arglist(int count,char** arglist)
    {
    /*
    1. split arglist into different commands - cmd
    2. execute cmd from a child proccess using fork() and 
       execvp(filename of program to execute,
              array[strings] = args list for program)
       note: shell symbols: < | > & etc' cant be sent as-is
    3. choose the corresponding function to call based on the shell symbol
    4. execution of shell symbols:
        a. & = background commands: parent does not wait for child
        b. | = piping commands: 
        c. < or > = input/output redirection: 

    $ret = 1 success or 0 error
    */ 
   int pipe_index = find_pipe_location(count, arglist);

   if (arglist==NULL || arglist[0]==NULL){// problem with arglist input
    perror("BAD COMMAND");
    return 0;
   }

   else if (count == 1){ //single word command special case
    return executing_commands(arglist);
   }

   else if (pipe_index != -1){// single pipe command
   return pipe_command(pipe_index, arglist);}

   else if (*arglist[count-1]=='&'){// backgroud command
   return background_command(count, arglist);}

   else if (*arglist[count-2]=='<'){// intput redirection
   return redirect(arglist[count - 1], arglist, '<');}
   
   else if (*arglist[count-2]=='>'){// output redirection
   return redirect(arglist[count - 1], arglist, '>');}

   else{// a shell command that isn't one of the special cases
   return executing_commands(arglist);}

   return 0;
}
