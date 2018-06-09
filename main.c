#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

//!Used for example at is regular file function!
#include<sys/types.h>
#include<sys/stat.h>
#include<dirent.h> //! Used in remove_folder_tree for DIR* etc

#include <readline/readline.h>
#include <readline/history.h>

void init_shell();
void execute_command(char* buffer);
int _execvp(char* argv[], int size);
int _execvp_pipe_redirect(char* buffer, int nr_of_pipes);

int _cd(char* argv[], int size);

int _rm(char* argv[], int size);
int is_directory(const char* path);
int is_file(const char* path);
int remove_file(char* path_to_delete, char* cmd,char* argv);
int remove_folder_tree(const char* path_to_delete);
char* str_replace(char* string, const char* substr, const char* replacement);

int _mkdir(char* argv[],int argc);

int _basename(char* argv[],int argc);

//!Globals

char* pipes_redirect[20];
int nr_of_pipes_redirect_found = 0;

int main()
{
    init_shell();

    return 0;
}

void init_shell(){

    char* buffer;

    while(1){

        char current_path[1024];
        getcwd(current_path,sizeof(current_path));
        strcat(current_path,"<_ ");
        //printf("%s",current_path);

        //!Read line
        while((buffer = readline(current_path))!=NULL){

            if(strcmp(buffer,"exit")==0){
                return;
            }

            if(buffer[0] != 0){

                if(buffer[strlen(buffer)-1]=='\n'){
                    buffer[strlen(buffer)-1] ='\0';
                }

                add_history(buffer);
            }

            //!Execute function
            execute_command(buffer);

            free(buffer);

            break;
        }

    }

}

void execute_command(char* buffer){

    nr_of_pipes_redirect_found = 0;

    //!parse buffer with a pointer and delete space after '\'
    char* pointer = buffer;
    int limit = strlen(buffer);
    for(int i =0;i<limit;i++){
        if(*pointer=='\\'){
            //*pointer = '/';
            pointer++;
            i++;
            if(*pointer==' '){
                *pointer='/';
                pointer--;
                *pointer='/';
                pointer++;
            }
        }
        pointer++;
    }

    char* buffercpy = strdup(buffer);
    int word_counter = 0;
    char* p = buffercpy;

    //!Count '|' and '<' if any!
    p = buffercpy;

    while(*p!='\0'){
        if(*p == '|' || *p == '>'){
            char str[2] = {*p,'\0'};
            pipes_redirect[nr_of_pipes_redirect_found] = strdup(str);
            nr_of_pipes_redirect_found++;
        }
        p++;
    }

    p = strtok(buffercpy," ");

    while(p!=NULL){
        word_counter++;
        p=strtok(NULL," ");
    }

    if(word_counter!=0){

        if(nr_of_pipes_redirect_found){

            _execvp_pipe_redirect(buffer,nr_of_pipes_redirect_found);

        }
        else{

            char* argv[word_counter+1];
            int i = 0;

            p = strtok(buffer," ");

            while(p!=NULL){

                argv[i] = p;
                p = strtok(NULL," ");

                i++;
            }

            free(p);

            argv[word_counter] = NULL;

            //!Replace the "//" substrings with " " for all argv strings!
            for(int l = 0; l<word_counter; l++){

                argv[l] = str_replace(argv[l],"//"," ");

            }

            _execvp(argv,word_counter);

        }

    }

    free(buffercpy);

    return;

}

char* str_replace(char* string, const char* substr, const char* replacement) {

	char* tok = NULL;
	char* newstr = NULL;
	char* oldstr = NULL;
	int   oldstr_len = 0;
	int   substr_len = 0;
	int   replacement_len = 0;

	newstr = strdup(string);
	substr_len = strlen(substr);
	replacement_len = strlen(replacement);

	if (substr == NULL || replacement == NULL) {
		return newstr;
	}

	while ((tok = strstr(newstr, substr))) {
		oldstr = newstr;
		oldstr_len = strlen(oldstr);
		newstr = (char*)malloc(sizeof(char) * (oldstr_len - substr_len + replacement_len + 1));

		if (newstr == NULL) {
			return NULL;
		}

		memcpy(newstr, oldstr, tok - oldstr);
		memcpy(newstr + (tok - oldstr), replacement, replacement_len);
		memcpy(newstr + (tok - oldstr) + replacement_len, tok + substr_len, oldstr_len - substr_len - (tok - oldstr));
		memset(newstr + oldstr_len - substr_len + replacement_len, 0, 1);

	}


	return newstr;

}


int _execvp(char* argv[], int size){

    char* cmd = argv[0];

    if(strcmp(cmd,"cd")==0){

        int err = _cd(argv,size);

        if(err < 0){
            printf("Failed to execute command : %s",cmd);
            return err;
        }

        return err;
    }

    pid_t pid;
    int status;

    if((pid=fork())<0){
        printf("Cannot create child!\n");
        return 7;
    }
    else if(pid==0){

        if(strcmp(cmd,"rm")==0){

            int err = _rm(argv,size);

            if(err < 0){
                printf("Failed to execute command : %s\n",cmd);
                exit(err);
            }

            exit(err);

        }

        if(strcmp(cmd,"mkdir")==0){

            int err = _mkdir(argv,size);

            if(err<0){
                printf("Failed to execute command : %s\n",cmd);
                exit(err);
            }

            exit(0);

        }

        else if(strcmp(cmd,"basename")==0){

            int err = _basename(argv,size);

            if(err < 0){
                printf("Failed to execute command : %s\n",cmd);
                exit(err);
            }

            exit(err);

        }
        else{

            int err = execvp(cmd,argv);
            if(err<0){
                printf("%s: command not found\n",cmd);
                exit(errno);
            }

            exit(err);
        }

        //return 0;

    }

    wait(&status);
    //printf("Child finished with status %d!",status);

    return 0;

}

//!<----- EXECUTE COMMANDS WITH PIPES AND REDIRECT! ----->

int _execvp_pipe_redirect(char* buffer, int nr_of_pipes_redirect_found){

    char* command_table[nr_of_pipes_redirect_found+1];
    char** table[nr_of_pipes_redirect_found+1];
    char strtok_delimiters[] = "|,>";

    /*for(int i =0; i<nr_of_pipes_redirect_found;i++){
        printf("%s\n", pipes_redirect[i]);
    }*/

    char* token = strtok(buffer,strtok_delimiters);
    int position = 0;

    while(token!=NULL){

        command_table[position] = token;
        position++;
        token = strtok(NULL,strtok_delimiters);

    }

    if(position != nr_of_pipes_redirect_found+1){
        printf("Command for pipe or path for redirect missing!\n");
        return -1;
    }

    for(int i=0;i<nr_of_pipes_redirect_found+1;i++){

        char* command = strdup(command_table[i]);
        char* command_copy = strdup(command_table[i]);

        char* token_2 = strtok(command_copy," ");
        int count_command_arguments = 0;

        while(token_2 != NULL){
            count_command_arguments++;
            token_2 = strtok(NULL," ");
        }

        char** command_argv = malloc((count_command_arguments+1)*sizeof(char*));

        int command_arg_position = 0;

        token_2 = strtok(command," ");

        while(token_2 != NULL){
            command_argv[command_arg_position] = strdup(token_2);
            command_arg_position++;
            token_2 = strtok(NULL," ");
        }

        command_argv[count_command_arguments] = NULL;

        for(int l = 0; l<count_command_arguments; l++){

            command_argv[l] = str_replace(command_argv[l],"//"," ");

        }

        table[i] = command_argv;

    }

    int i = 0;
    pid_t pid;
    int status;

    //!Check if first argument is cd
    if(strcmp(table[0][0],"cd")==0){

        int argc = 0;
        while(table[0][argc]!= NULL){
            argc++;
        }

        _cd(table[0],argc);

    }

    if((pid=fork())==0){

        int current_pipe_or_redirect = 0;
        int in,fd[2];

        in = 0;

        for(i = 0;i<nr_of_pipes_redirect_found;i++){

            if(pipe(fd)<0){
                printf("Cannot create pipe!\n");
                exit(8);
            }

            //printf("read : %d, write %d \n",fd[0],fd[1]);

            if(strcmp(pipes_redirect[current_pipe_or_redirect],"|")==0){
                //printf("here \n");
                spawn_process(in, fd[1],table[i]);
            }
            else{

                //!Check for the case of consecutive '>' if tere are more than 1 pipings/redirects
                //!If there are consecutive '>', only the first will redirect, the next ones won't do anything
                //!(Real terminal behaviour)
                if(current_pipe_or_redirect >= 1){
                    if(strcmp(pipes_redirect[current_pipe_or_redirect-1],">")==0){
                        i++;//! Do nothing
                    }
                    else{
                        spawn_process_wredirect(in, table[i+1],table[i]);
                        i++;
                    }
                }
                else{
                    spawn_process_wredirect(in, table[i+1],table[i]);
                    i++;
                }
                current_pipe_or_redirect++;//! Jump one pipe/redirect if there was a redirect (redirect has not output, nothing for a pipe)
            }

            close(fd[1]);

            in = fd[0];

            current_pipe_or_redirect++;

            wait();

        }

        if(in !=0){
            //printf("in\n");
            dup2(in,0);
        }

        //!Last table argument is executed only if current command is not '>' (that would be a path, not a command!)
        if(strcmp(pipes_redirect[nr_of_pipes_redirect_found-1],">")!=0){

            int err = execvp(table[i][0],table[i]);
            if(err < 0){

                printf("%s: command not found\n",table[i][0]);
                exit(errno);

            }

       }

        //printf("%d\n", current_pipe_or_redirect);

        //!To Check
        close(fd[0]);
        exit(0);

    }
    else if(pid < 0){

        printf("Cannot create child !");
        exit(9);//!KERROR

    }

    wait(&status);

    //!Clear pipes_redirect
    for(int i=0;i<20;i++){
        pipes_redirect[i] = NULL;
    }

    return 0;

}

//! Get and send through pipe (in and out) input and output of the command when there is a pipeing ('|')
int spawn_process(int in, int out, char* argv[]){

    pid_t pid;
    int status;
    int saved_stdout = dup(1);//!Save the stdout to restore it to the terminal if there is an error executing a command!

    if((pid=fork())==0){

        if(in != 0){
            //printf("%d old in\n", in);
            dup2(in,0);
            close(in);
        }

        if(out != 1){
            //printf("%d old out\n", out);
            dup2(out, 1);
            close(out);
        }

         if(strcmp(argv[0],"rm")==0){

            dup2(saved_stdout,1);//! does not write in folder location, but else it wouldn't print the -i case ....??

            int argc = 0;
            while(argv[argc]!=NULL){
                argc++;
            }

            int err = _rm(argv,argc);
            if(err<0){
                dup2(saved_stdout,1);
                exit(err);
            }

            exit(0);

        }
        else if (strcmp(argv[0],"mkdir")==0){

            int argc = 0;
            while(argv[argc]!=NULL){
                argc++;
            }

            int err = _mkdir(argv,argc);
            if(err < 0){
                dup2(saved_stdout,1);
                exit(err);
            }

            exit(0);

        }
        else if(strcmp(argv[0],"basename")==0){

            int argc = 0;
            while(argv[argc]!=NULL){//! in the table, every 'argv* []' has last position == NULL!!!!
                argc++;
            }

            int err = _basename(argv,argc);
            if(err < 0){
                dup2(saved_stdout,1);
                exit(err);
            }

            exit(0);

        }
        else if(strcmp(argv[0],"cd")==0){

            //!leave pipe empty, real terminal behaviour

        }
        else{

            int err = execvp(argv[0],argv);
            if(err < 0){
                dup2(saved_stdout,1);//!Restore the stdout using the saved_stdout.
                printf("%s: command not found\n",argv[0]);
                exit(errno);
            }

            exit(0);
        }
    }
    else if(pid < 0){

        printf("Cannot create child !");
        exit(10);//!KERROR

    }

    wait(&status);

    return 0;

}

//! Get the output from the previous command through the pipe (in) and redirect it to a file (char* arg[])!
int spawn_process_wredirect(int in, char* arg[], char* argv[]){

    pid_t pid;
    int status;
    int saved_stdout = dup(1);
    //int saved_stdin = dup(0);

    if((pid=fork())==0){

        if(in != 0){
            //printf("%d old in\n", in);
            dup2(in,0);
            close(in);
        }
        //printf("%s\n", arg[0]);
        FILE* fp = fopen(arg[0],"w");
        if(fp == NULL){
            printf("bash: %s: No such file or directory\n",arg[0]);
            exit(12);//!KERROR
        }
        else{

            FILE* file = freopen(arg[0],"w",stdout);
            if(file == NULL){
                dup2(saved_stdout,1);
                printf("Failed to open file!\n");
                exit(13);//!KERROR
            }

            if(strcmp(argv[0],"rm")==0){

                //dup2(saved_stdout,1);//! does not write in folder location, but else it wouldn't print the -i case ....??

                int argc = 0;
                while(argv[argc]!=NULL){
                    argc++;
                }

                int err = _rm(argv,argc);
                if(err<0){
                    dup2(saved_stdout,1);
                    exit(err);
                }

                exit(0);

            }
            else if (strcmp(argv[0],"mkdir")==0){

                int argc = 0;
                while(argv[argc]!=NULL){
                    argc++;
                }

                int err = _mkdir(argv,argc);
                if(err < 0){
                    dup2(saved_stdout,1);
                    exit(err);
                }

                exit(0);

            }
            else if(strcmp(argv[0],"basename")==0){

                int argc = 0;
                while(argv[argc]!=NULL){//! in the table, every 'argv* []' has last position == NULL!!!!
                    argc++;
                }

                int err = _basename(argv,argc);
                if(err < 0){
                    dup2(saved_stdout,1);
                    exit(err);
                }

                exit(0);

            }
            else if(strcmp(argv[0],"cd")==0){

                //!leave pipe empty, real terminal behaviour

            }
            else{

                int err = execvp(argv[0],argv);
                if(err < 0){
                    dup2(saved_stdout,1);
                    printf("%s: command not found\n",argv[0]);
                    close(file);
                    exit(errno);
                }

                close(file);
                exit(0);

            }

        }
    }
    else if(pid < 0){

        printf("Cannot create child !");
        exit(11);

    }

    wait(&status);

    return 0;

}

//!<----- CD ----->

int _cd(char* argv[], int size){

    if(size >= 2){

        char current_directory[1024] = {0};
        char target[1024] = {0};

        getcwd(current_directory, sizeof(current_directory));

        //!Create target from argv[]
        strcat(target,argv[1]);
        strcat(target,"/");

        if(chdir(target)==0){
            getcwd(current_directory, sizeof(current_directory));
            //printf("Now in %s",current_directory);
            return 0;
        }
        else{
            char check_path[1024] = {0};
            strcat(check_path,current_directory);
            strcat(check_path,"/");
            strcat(check_path,target);
            //!Remove '/' from check_path and target
            check_path[strlen(check_path)-1] = '\0';
            target[strlen(target)-1] = '\0';

            int is_f = is_file(check_path);//! We already know it is not a directory, check only if file!

            if(!is_f){
                printf("bash: %s: %s: No such file or directory\n",argv[0],target);
                return 1;
            }
            else{
                printf("bash: %s: %s: Not a directory\n",argv[0],target);
                return 2;
            }
        }

    }
    else{

        char current_directory[1024] = {0};
        char* home = getenv("HOME");

        if(chdir(home)==0){
            getcwd(current_directory, sizeof(current_directory));
            //printf("Now in %s",current_directory);
            return 0;
        }
        else{
            printf("Error accessing home directory");
            return 3;
        }

    }

    return -3;
}


//!<<---- READLINE FUNCTION TO MODIFY out descriptor to stderr! ---->(used in rm function where -i is applied)
void line_handler(char* line){
    fprintf(stderr,"%s",line);
    return;
}

//!<----- REMOVE FILE OR DIRECTORY ----->


//!<-- ATTENTION! BIG RM FUNCTION WITH MANY CASES! -->
int _rm(char* argv[], int size){

    //! Save current path
    char current_path[1024];

    //!If current path is !=NULL continure
    if(getcwd(current_path, sizeof(current_path))!=NULL){

    //!If only 1 argument which is rm
    if(size == 1){
        fprintf(stderr, "%s: missing operand\n",argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n",argv[0]);

        return 4;
    }

    if(size == 2){
        if(strcmp(argv[1],"-i")==0 || strcmp(argv[1],"-r")==0 || strcmp(argv[1],"-R")==0 || strcmp(argv[1],"-v")==0){
            fprintf(stderr, "%s: missing operand\n",argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n",argv[0]);

            return 5;
        }
        else{
            //!Concatenate current path with the user input
            strcat(current_path,"/");
            strcat(current_path,argv[1]);

            //!Check if file is folder/register
            int is_f = is_file(current_path);
            int is_dir = is_directory(current_path);

            int check = remove_file(current_path,argv[0],argv[1]);

            return check;
        }
    }

    if(size > 2){

        int rOrRFound = 0;
        int iFound = 0;
        int vFound = 0;
        int options_found = 0;

        //!Find if there are any options!
        for(int i = 1; i < size; i++){

            if(strcmp(argv[i],"-r")==0 || strcmp(argv[i],"-R")==0){
                rOrRFound = 1;
            }
            else if(strcmp(argv[i],"-i")==0){
                iFound = 1;
            }
            else if(strcmp(argv[i],"-v")==0){
                vFound = 1;
            }
        }

        //!Check if there are only options with no arguments
        for(int i = 0; i < size; i++){

            if(strcmp(argv[i],"-i") == 0 || strcmp(argv[i],"-r") == 0 || strcmp(argv[i],"-v") == 0){
                options_found++;
            }

        }

        //printf("%d %d\n", argc, options_found);

        if(options_found == size-1){

            fprintf(stderr, "%s: missing operand\n",argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n",argv[0]);

            return 17;
        }

        //printf("%d, %d, %d", rOrRFound, iFound, vFound);

        //!If size is >2 and there are no options, proceed as normal rm with all inputs (ex" rm i.txt test.txt ...)
        //!remove file or folder by case

        //!Case 1
        if(rOrRFound == 0 && iFound == 0 && vFound == 0){

            int check;

            for(int i = 1; i<size ; i++){

                    //!Concatenate current path with the user input
                    getcwd(current_path, sizeof(current_path));
                    strcat(current_path,"/");
                    strcat(current_path,argv[i]);

                    check = remove_file(current_path,argv[0],argv[i]);

                    if(check < 0){
                        return check;
                    }

            }

            return 0;

        }
        //!Case 2
        if(rOrRFound == 0 && iFound == 0 && vFound == 1){

            int check;

            for(int i = 1; i<size ; i++){

                //!Don't take into account options
                if(strcmp(argv[i],"-v")!=0){

                    //!Concatenate current path with the user input
                    getcwd(current_path, sizeof(current_path));
                    strcat(current_path,"/");
                    strcat(current_path,argv[i]);

                    int check = remove_file(current_path,argv[0],argv[i]);
                    if(check){
                        printf("removed '%s'\n",argv[i]);
                    }
                    else if(check < 0){
                        return check;
                    }


                }

            }

            return 0;

        }
        //!Case 3
        if(rOrRFound == 0 && iFound == 1 && vFound == 0){

            int check;

            for(int i = 1; i<size ; i++){

                //!Don't take into account options
                if(strcmp(argv[i],"-i")!=0){

                    //!Concatenate current path with the user input
                    getcwd(current_path, sizeof(current_path));
                    strcat(current_path,"/");
                    strcat(current_path,argv[i]);

                    //!CHECK IF FILE IS FOLDER OR IF FILES EXISTS BEFORE!!!!
                    int is_f = is_file(current_path);
                    int is_dir = is_directory(current_path);
                    if(!is_f && !is_dir){
                        fprintf(stderr, "%s: cannot remove %s: No such file or directory \n", argv[0], argv[i]);
                    }
                    else if(is_dir){
                        fprintf(stderr, "%s: cannot remove %s: Is a directory \n", argv[0], argv[i]);
                    }
                    else{
                        fprintf(stderr,"%s: remove file '%s'?",argv[0],argv[i]);
                        int temp = rl_outstream;//!Save initial readline outstream
                        rl_outstream = stderr;//!Set it to stderr//
                        char* line = readline(" ");
                        rl_outstream = temp;
                        if(strcmp(line,"y")==0 || strcmp(line,"Y")==0){
                            check = remove_file(current_path,argv[0],argv[i]);
                            if(check < 0){
                                return check;
                            }
                        }
                    }

                }

            }

            return 0;

        }
        //!Case 4
        if(rOrRFound == 0 && iFound == 1 && vFound == 1){

            int check;

            for(int i = 1; i<size ; i++){

                //!Don't take into account options
                if(strcmp(argv[i],"-i")!=0 && strcmp(argv[i],"-v")!=0){

                    //!Concatenate current path with the user input
                    getcwd(current_path, sizeof(current_path));
                    strcat(current_path,"/");
                    strcat(current_path,argv[i]);

                    //!CHECK IF FILE IS FOLDER OR IF FILES EXISTS BEFORE!!!!
                    int is_f = is_file(current_path);
                    int is_dir = is_directory(current_path);
                    if(!is_f && !is_dir){
                        fprintf(stderr, "%s: cannot remove %s: No such file or directory \n", argv[0], argv[i]);
                    }
                    else if(is_dir){
                        fprintf(stderr, "%s: cannot remove %s: Is a directory \n", argv[0], argv[i]);
                    }
                    else{
                        fprintf(stderr,"%s: remove file '%s'?",argv[0],argv[i]);//!Use s print f
                        int temp = rl_outstream;//!Save initial readline outstream
                        rl_outstream = stderr;//!Set it to stderr//
                        char* line = readline(" ");
                        rl_outstream = temp;
                        if(strcmp(line,"y")==0 || strcmp(line,"Y")==0){
                            check = remove_file(current_path,argv[0],argv[i]);
                            if(check < 0){
                                return check;
                            }
                            printf("removed '%s'\n",argv[i]);
                        }
                    }

                }

            }

            return 0;

        }
        //!Case 5
        if(rOrRFound == 1 && iFound == 0 && vFound == 0){

            int check;

            for(int i = 1; i<size ; i++){

                //!Don't take into account options
                if(strcmp(argv[i],"-r")!=0 && strcmp(argv[i],"-R")!=0){

                    //!Concatenate current path with the user input
                    getcwd(current_path, sizeof(current_path));
                    strcat(current_path,"/");
                    strcat(current_path,argv[i]);

                    //!CHECK IF FILE IS FOLDER OR IF FILES EXISTS BEFORE!!!!
                    int is_f = is_file(current_path);
                    int is_dir = is_directory(current_path);
                    if(!is_f && !is_dir){
                        fprintf(stderr, "%s: cannot remove %s: No such file or directory \n", argv[0], argv[i]);
                    }
                    else if(is_f){
                        check = remove_file(current_path,argv[0],argv[i]);//!remove file
                        if(check < 0){
                            return check;
                        }
                    }
                    else{
                        check = remove_folder_tree(current_path);
                        if(check < 0){
                            return check;
                        }
                    }

                }

            }

            return 0;

        }
        //!Case 6
        if(rOrRFound == 1 && iFound == 1 && vFound == 0){

            int check;

            for(int i = 1; i<size ; i++){

                //!Don't take into account options
                if(strcmp(argv[i],"-r")!=0 && strcmp(argv[i],"-R")!=0 && strcmp(argv[i],"-i")!=0){

                    //!Concatenate current path with the user input
                    getcwd(current_path, sizeof(current_path));
                    strcat(current_path,"/");
                    strcat(current_path,argv[i]);

                    //!CHECK IF FILE IS FOLDER OR IF FILES EXISTS BEFORE!!!!
                    int is_f = is_file(current_path);
                    int is_dir = is_directory(current_path);
                    if(!is_f && !is_dir){
                        fprintf(stderr, "%s: cannot remove %s: No such file or directory \n", argv[0], argv[i]);
                    }
                    else if(is_f){
                        fprintf(stderr,"%s: remove file '%s'?",argv[0],argv[i]);
                        int temp = rl_outstream;//!Save initial readline outstream
                        rl_outstream = stderr;//!Set it to stderr//
                        char* line = readline(" ");
                        rl_outstream = temp;
                        if(strcmp(line,"y")==0 || strcmp(line,"Y")==0){
                            check = remove_file(current_path,argv[0],argv[i]);//!remove file
                            if(check < 0){
                                return check;
                            }
                        }
                    }
                    else{
                        fprintf(stderr,"%s: remove folder '%s'?",argv[0],argv[i]);
                        char* line = readline(" ");
                        if(strcmp(line,"y")==0 || strcmp(line,"Y")==0){
                            check = remove_folder_tree(current_path);
                            if(check < 0){
                                return check;
                            }
                        }
                    }

                }

            }

            return 0;

        }
        //!Case 7
        if(rOrRFound == 1 && iFound == 0 && vFound == 1){

            int check;

            for(int i = 1; i<size ; i++){

                //!Don't take into account options
                if(strcmp(argv[i],"-r")!=0 && strcmp(argv[i],"-R")!=0 && strcmp(argv[i],"-v")!=0){

                    //!Concatenate current path with the user input
                    getcwd(current_path, sizeof(current_path));
                    strcat(current_path,"/");
                    strcat(current_path,argv[i]);

                    //!CHECK IF FILE IS FOLDER OR IF FILES EXISTS BEFORE!!!!
                    int is_f = is_file(current_path);
                    int is_dir = is_directory(current_path);
                    if(!is_f && !is_dir){
                        fprintf(stderr, "%s: cannot remove %s: No such file or directory \n", argv[0], argv[i]);
                    }
                    else if(is_f){
                        check = remove_file(current_path,argv[0],argv[i]);//!remove file
                        if(check < 0){
                            return check;
                        }
                        printf("removed '%s'\n",argv[i]);
                    }
                    else{
                        check = remove_folder_tree(current_path);
                        if(check < 0){
                            return check;
                        }
                        printf("removed directory '%s'\n",argv[i]);
                    }

                }

            }

            return 0;

        }
        //!Case 8
        if(rOrRFound == 1 && iFound == 1 && vFound == 1){

            int check;

            for(int i = 1; i<size ; i++){

                //!Don't take into account options
                if(strcmp(argv[i],"-r")!=0 && strcmp(argv[i],"-R")!=0 && strcmp(argv[i],"-i")!=0 && strcmp(argv[i],"-v")!=0){

                    //!Concatenate current path with the user input
                    getcwd(current_path, sizeof(current_path));
                    strcat(current_path,"/");
                    strcat(current_path,argv[i]);

                    //!CHECK IF FILE IS FOLDER OR IF FILES EXISTS BEFORE!!!!
                    int is_f = is_file(current_path);
                    int is_dir = is_directory(current_path);
                    if(!is_f && !is_dir){
                        fprintf(stderr, "%s: cannot remove %s: No such file or directory \n", argv[0], argv[i]);
                    }
                    else if(is_f){
                        fprintf(stderr,"%s: remove file '%s'?",argv[0],argv[i]);
                        int temp = rl_outstream;//!Save initial readline outstream
                        rl_outstream = stderr;//!Set it to stderr//
                        char* line = readline(" ");
                        rl_outstream = temp;
                        if(strcmp(line,"y")==0 || strcmp(line,"Y")==0){
                            check = remove_file(current_path,argv[0],argv[i]);//!remove file
                            if(check < 0){
                                return check;
                            }
                            printf("removed '%s'\n",argv[i]);
                        }
                    }
                    else{
                        fprintf(stderr,"%s: remove folder '%s'?",argv[0],argv[i]);
                        int temp = rl_outstream;//!Save initial readline outstream
                        rl_outstream = stderr;//!Set it to stderr//
                        char* line = readline(" ");
                        rl_outstream = temp;
                        if(strcmp(line,"y")==0 || strcmp(line,"Y")==0){
                            check = remove_folder_tree(current_path);
                            if(check < 0){
                                return check;
                            }
                            printf("removed directory '%s'\n",argv[i]);
                        }
                    }

                }

            }

            return 0;

        }


    }

    }

    return -4;

}

//!Check if path is directory
int is_directory(const char* path){
    struct stat path_stat;
    if(stat(path, &path_stat)){
        return 0;
    }
    return S_ISDIR(path_stat.st_mode);
}

//!Check if path is file
int is_file(const char* path){
    struct stat path_stat;
    if(stat(path, &path_stat)){
        return 0;
    }
    return S_ISREG(path_stat.st_mode);
}

//!Used in _rm, Return 1 if file is deleted, else 0 !
int remove_file(char* path_to_delete, char* cmd,char* argv){

    char current_path[1024];
    getcwd(current_path,sizeof(current_path));

    //!Open current path
    DIR* dp = opendir(current_path);

    //!Check if file is folder/register
    //printf("%s", path_to_delete);

    int is_f = 0;
    is_f = is_file(path_to_delete);
    int is_dir = 0;
    is_dir = is_directory(path_to_delete);

    if(!is_f && !is_dir){
        fprintf(stderr, "%s: cannot remove %s: No such file or directory \n", cmd, argv);
        close(dp);

        return 0;
    }
    else if(is_dir){
        fprintf(stderr, "%s: cannot remove %s: Is a directory \n", cmd, argv);
        close(dp);

        return 0;
    }
    else{
        //!Delete file!
        unlink(argv);
        close(dp);

        return 1;
    }

    return -2;

}

int remove_folder_tree(const char* path_to_delete){

    DIR* dp;
    struct dirent* ep;
    char p_buf[1024] = {0};

    dp = opendir(path_to_delete);

    if(dp == NULL){
        return -3;
    }

    while((ep = readdir(dp))!= NULL){

        if(strcmp(ep->d_name,".")==0 || strcmp(ep->d_name,"..")==0){
                continue;
        }

        if(strcmp(ep->d_name,".")!=0 || strcmp(ep->d_name,"..")!=0)
        {
            sprintf(p_buf, "%s/%s", path_to_delete, ep->d_name);
            if(is_directory(p_buf)){
                remove_folder_tree(p_buf);
            }
            else{
                unlink(p_buf);
            }
        }

    }

    closedir(dp);
    rmdir(path_to_delete);

    return 0;

}

//!<----- MKDIR ----->

int _mkdir(char* argv[], int argc){

    int mFound = 0;
    char mode[1024];
    int pFound = 0;
    int vFound = 0;
    int options_found = 0;//!If the no. of options found is eqaul to argc-1 (excludeing cmd mkdir ) then there are no arguments!
    mode_t umask_code = 000;
    umask(umask_code);

    mode_t standard_mode = 0775;
    mode_t custom_mode = 0000;

    for(int i = 0; i < argc; i++){
        if(strcmp(argv[i],"-m") == 0){
            mFound = 1;
            if(argc >= i+2)
            strcpy(mode,argv[i+1]);
        }
        else if(strcmp(argv[i],"-p") == 0){
            pFound = 1;
        }
        else if(strcmp(argv[i],"-v") == 0){
            vFound = 1;
        }
    }

    if(mFound == 1 && strlen(mode)!=0){

        //! Verify the string for a valid mode!
        if(strlen(mode)!=4){
            fprintf(stderr, "%s: invalid mode\n",argv[0]);
            return 18;
        }

        if(mode[0] != '0' && mode[0] != '1'){
            fprintf(stderr, "%s: invalid mode\n",argv[0]);
            return 19;
        }

        for(int i=1;i<4;i++){
            if(mode[i] != '0' && mode[i] != '1' && mode[i] != '2' && mode[i] != '3' && mode[i] != '4' && mode[i] != '5' && mode[i] != '6' && mode[i] != '7'){
                fprintf(stderr, "%s: invalid mode\n",argv[0]);
                return 20;
            }
        }

        char** end_pointer;

        mode_t new_mode = strtol(mode,&end_pointer,8);
        standard_mode = new_mode;

    }

    if(argc == 1){

        fprintf(stderr, "%s: missing operand\n",argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n",argv[0]);

        return 14;

    }
    else if(argc == 2){

        if(strcmp(argv[1],"-m") == 0 || strcmp(argv[1],"-p") == 0 || strcmp(argv[1],"-v") == 0){

            fprintf(stderr, "%s: missing operand\n",argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n",argv[0]);

            return 15;

        }
        else{

            if(mkdir(argv[1], standard_mode)!=0){
                fprintf(stderr, "%s: cannot create directory '%s': %s\n",argv[0], argv[1], strerror(errno));

                return errno;
            }

            return 0;

        }

    }
    else if(argc > 2){

        for(int i = 0; i < argc; i++){

            if(strcmp(argv[i],"-m") == 0 || strcmp(argv[i],"-p") == 0 || strcmp(argv[i],"-v") == 0 || strcmp(argv[i],mode)==0 ){
                options_found++;
            }

        }

        //printf("%d %d\n", argc, options_found);

        if(options_found == argc-1){

            fprintf(stderr, "%s: missing operand\n",argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n",argv[0]);

            return 16;
        }

        if(pFound == 0){

            for(int i = 1; i < argc; i++){

                if(strcmp(argv[i],"-m") != 0 && strcmp(argv[i],"-p") != 0 && strcmp(argv[i],"-v") != 0 && strcmp(argv[i],mode) != 0){

                    if(mkdir(argv[i], standard_mode)==0){
                        if(chmod(argv[i],standard_mode)!=0){
                            fprintf(stderr,"%s",strerror(errno));
                        }
                        if(vFound == 1){
                            printf("%s: created directory '%s'\n",argv[0],argv[i]);
                        }
                    }
                    else{
                        fprintf(stderr, "%s: cannot create directory '%s': %s\n",argv[0], argv[i], strerror(errno));
                    }

                }

            }
        }
        else if(pFound == 1){

            _mkdir_tree(argv,argc,standard_mode,vFound,mode);

        }

        return 0;

    }

    return -6;

}

//!Recursive directory creation when using the option -p!
int _mkdir_tree(char* argv[], int argc, int standard_mode, int vFound, char* mode){

    mode_t parent_mode = 0775;//! All parent directories in a tree have as default the mode '0775' - octal

    for(int i = 1; i < argc ; i++){

        if(strcmp(argv[i],"-m") != 0 && strcmp(argv[i],"-p") != 0 && strcmp(argv[i],"-v") != 0 && strcmp(argv[i],mode) != 0){

            char temp_path[1024];
            char* p = NULL;
            int len;

            strcpy(temp_path,argv[i]);

            len = strlen(temp_path);
            if(temp_path[len-1] == '/'){
                temp_path[len - 1] = 0;
            }
            for(p = temp_path + 1; *p ; p++){
                if(*p == '/'){
                    *p = 0;
                    mkdir(temp_path,parent_mode);
                    if(vFound == 1){
                        printf("%s: created directory '%s'\n",argv[0],temp_path);
                    }
                    *p = '/';
                }
            }

            mkdir(temp_path, standard_mode);
            if(vFound == 1){
                printf("%s: created directory '%s'\n",argv[0],temp_path);
            }

        }

    }

    return 0;

}

void removeSubstring(char *s,const char *toremove)
{
  while( s=strstr(s,toremove) )
    memmove(s,s+strlen(toremove),1+strlen(s+strlen(toremove)));
}

int str_ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t suffix_len = strlen(suffix);

    return suffix_len <= slen && !strcmp(s + slen - suffix_len, suffix);
}

//!<----- BASENAME ----->

int _basename(char* argv[],int argc){

    if(argc > 1){

        char* cmd = argv[0];

        char* to_split = strdup(argv[1]);
        char* token = to_split;

        char* suffix[1024];
        char* previous_suffix[1024];

        while((token=strsep(&to_split,"/"))!=NULL){
            strcpy(previous_suffix,suffix);
            if(token!=NULL)
            strcpy(suffix,token);
        }

        if(strcmp(suffix,"")!=0){//! Case : a/h/
            char* point = suffix;

            if(argc > 2){ //! Case we want to remove suffix of file not path!
                if((point = strrchr(suffix,'.')) != NULL ) {
                   if(strcmp(point,argv[2]) == 0) {
                        removeSubstring(suffix,argv[2]);
                   }
                }
            }

            printf("%s\n",suffix);

        }
        else{
            char* point = previous_suffix;

            if(argc > 2){ //! Case we want to remove suffix of file not path!
                if((point = strrchr(previous_suffix,'.')) != NULL ) {
                   if(strcmp(point,argv[2]) == 0) {
                        removeSubstring(previous_suffix,argv[2]);
                   }
                }
            }

            printf("%s\n",previous_suffix);//! Print this instead!
        }

        return 0;

    }
    else{

        fprintf(stderr,"%s: missing operand\n",argv[0]);
        fprintf(stderr,"Try '%s --help' for more information.\n",argv[0]);

        return 6;
    }

    return -5;

}
