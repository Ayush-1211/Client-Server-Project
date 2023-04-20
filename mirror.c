#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>

//mirror server will be deployed on port 9000
#define PORT 9000
#define MAX_CLIENTS 4
#define BUFFER_SIZE 1024
//temporary tar.gz file created by the mirror server to be sent to the client
#define TAR_FILE_NAME "mirror_server_temp.tar.gz"
#define MAX_FILES 6
#define MAX_FILENAME_LEN 50

//commands used by client to send request to server
#define FIND_FILE "findfile"
#define S_GET_FILES "sgetfiles"
#define D_GET_FILES "dgetfiles"
#define GET_FILES "getfiles"
#define GET_TAR_GZ "gettargz"
#define QUIT "quit"

//to Find the file in the server from /home/ayush-1211
char* findfile(char* filename) {
    char str_appended[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -name %s -printf '%%f|%%s|%%T@\\n' | head -n 1", filename);
    FILE* fp = popen(command, "r");
    char path[BUFFER_SIZE];
    if (fgets(path, BUFFER_SIZE, fp) != NULL) {
        printf("Requested File Found.\n");
        path[strcspn(path, "\n")] = 0; //remove trailing newline
        //extract the filename, size, and date from the path string
        char* filename_ptr = strtok(path, "|");
        char* size_ptr = strtok(NULL, "|");
        char* date_ptr = strtok(NULL, "|");
        //convert the size and date strings to integers
        int size = atoi(size_ptr);
        time_t date = atoi(date_ptr);
        //creating Filename string to be sent to client for displaying data
        char print_filename[BUFFER_SIZE];
        strcpy(print_filename, "File Name: ");
        strcat(print_filename, filename_ptr);
        strcat(print_filename, "\n");
        //creating File Size string to be sent to client for displaying data
        char print_size[BUFFER_SIZE];
        strcpy(print_size, "File Size: ");
        strcat(print_size, size_ptr);
        strcat(print_size, "\n");
        //creating File Created At string to be sent to client for displaying data
        char print_created[BUFFER_SIZE];
        strcpy(print_created, "File Created At: ");
        strcat(print_created, ctime(&date));
        strcat(print_created, "\n");
        //appending all necessary data of the file to be sent to client as a single string
        strcpy(str_appended, print_filename);
        strcat(str_appended, print_size);
        strcat(str_appended, print_created);
    } else {
        printf("File not Found.\n");
        strcpy(str_appended, "File not Found.\n");
    }
    pclose(fp);
    char *ptr_client_str = str_appended;
    return ptr_client_str;
}

//to handle sending stream of file
void send_tar_file(int socket_fd) {
    int break_flag = 0;
    FILE* fp = fopen(TAR_FILE_NAME, "rb");
    if (!fp) {
        perror("Error opening tar file");
        return;
    }

    //send the tar file size to the client
    fseek(fp, 0, SEEK_END);
    long tar_file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char size_buffer[BUFFER_SIZE];
    sprintf(size_buffer, "%ld", tar_file_size);
    if (send(socket_fd, size_buffer, strlen(size_buffer), 0) != strlen(size_buffer)) {
        perror("Error sending tar file size to client");
        fclose(fp);
        return;
    }

    //receive acknowledgement from the client
    if (recv(socket_fd, size_buffer, BUFFER_SIZE, 0) == -1) {
        perror("Error sending tar file size to client");
        fclose(fp);
        return;
    }
    printf("Client Acknowledged.\n");
    printf("File of size %ld being sent.\n", tar_file_size);

    //sending the tar file contents to the client
    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
        if (send(socket_fd, buffer, n, 0) != n) {
            perror("Error sending tar file contents to client");
            break_flag = 1;
            break;
        }
    }
    if (break_flag) {
        printf("Unable to send tar.gz file to client\n");
    } else {
        printf("File sent successfully\n");
    }
    fclose(fp);
}

//to get the files in a range of size
void sgetfiles(int socket_fd, int size1, int size2) {
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -type f -size +%d -size -%d -print0 | tar -czvf %s --null -T -", size1, size2, TAR_FILE_NAME);
    FILE* fp = popen(command, "r");
    send_tar_file(socket_fd);
}

//to get the files in a range of dates
void dgetfiles(int socket_fd, char* date1, char* date2) {
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -type f -newermt \"%s\" ! -newermt \"%s\" -print0 | tar -czvf %s --null -T -", date1, date2, TAR_FILE_NAME);
    FILE* fp = popen(command, "r");
    send_tar_file(socket_fd);
}

//to find all the files present in directory
int find_files(const char *dir_name, const char *filename, char *tar_file) {
    int found = 0;
    DIR *dir;
    struct dirent *entry;
    struct stat file_info;
    char path[PATH_MAX];

    if ((dir = opendir(dir_name)) == NULL) {
        perror("opendir");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, PATH_MAX, "%s/%s", dir_name, entry->d_name);	//creating path for the entry being checked for

        //fetching the stats
        if (lstat(path, &file_info) < 0) {
            perror("lstat");
            continue;
        }

        if (S_ISDIR(file_info.st_mode)) {
            find_files(path, filename, tar_file);
        } else if (S_ISREG(file_info.st_mode)) {
            if (strcmp(entry->d_name, filename) == 0) {
                strncat(tar_file, " ", BUFFER_SIZE - strlen(tar_file) - 1);
                strncat(tar_file, path, BUFFER_SIZE - strlen(tar_file) - 1);
                printf("File found at: %s\n", path);
                found = 1;
            }
        }
    }

    closedir(dir);
    return found;
}

//to get the files
char* getfiles(int socket_fd, char files[MAX_FILES][MAX_FILENAME_LEN], int num_files) {
    char *dir_path = getenv("HOME");	//get the current directory path
    if (dir_path == NULL) {
        fprintf(stderr, "Error getting HOME directory path\n");
        return NULL;
    }

    char tar_cmd[BUFFER_SIZE] = "tar -czvf ";	//create the tar command
    strcat(tar_cmd, TAR_FILE_NAME);

    int file_found = 0;
    for (int i = 0; i < num_files; i++) {
        printf("Finding Filename: %s\n", files[i]);
        char file_path[BUFFER_SIZE];
        snprintf(file_path, BUFFER_SIZE, "%s/%s", dir_path, files[i]);

        const char *homedir = getenv("HOME");
        if (homedir == NULL) {
            printf("Could not get HOME directory\n");
            return 0;
        }

        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", homedir, files[i]);
        file_found += find_files(homedir, files[i], tar_cmd);
    }

    if (file_found) {
        printf("File(s) found\n");
        system(tar_cmd);
        FILE *tar_file = fopen(TAR_FILE_NAME, "r");		//send the tar file to the client
        if (tar_file == NULL) {
            fprintf(stderr, "Error opening tar file\n");
            return NULL;
        }
        fclose(tar_file);
        send_tar_file(socket_fd);	//create socket and send the tar file to client
    } else {
        printf("No file found.\n");
        if (send(socket_fd, "0", strlen("0"), 0) != strlen("0")) {
            perror("Error sending tar file size to client");
            return NULL;
        }
        return "No file found.";
    }

    return NULL;
}

//to create a list of matching files with extensions
void find_gettargz_files(const char *dir_path, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions, FILE *temp_list) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        printf("Error: could not open directory %s\n", dir_path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char *name = entry->d_name;
            for (int i = 0; i < num_extensions; i++) {
                char *extension = extensions[i];
                int len_ext = strlen(extension);
                int len_name = strlen(name);
                if (len_name >= len_ext && strcmp(name + len_name - len_ext, extension) == 0) {
                    fprintf(temp_list, "%s/%s\n", dir_path, name);
                    break;
                }
            }
        } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char subdir_path[BUFFER_SIZE];
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", dir_path, entry->d_name);
            find_gettargz_files(subdir_path, extensions, num_extensions, temp_list);
        }
    }

    closedir(dir);
}

//to find and send the tar.gz file with the files with extensions from extensions list
char* gettargz(int socket_fd, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions) {
    int file_found = 0;
    FILE *temp_list = tmpfile();	//create a temporary file to store the list of matching files
    if (!temp_list) {
        printf("Error: could not create temporary file\n");
        return NULL;
    }

    find_gettargz_files(getenv("HOME"), extensions, num_extensions, temp_list);
    rewind(temp_list);
    char filename[BUFFER_SIZE];

    while (fgets(filename, sizeof(filename), temp_list) != NULL) {
        filename[strcspn(filename, "\n")] = 0;	//to remove the newline character at the end of the filename
        file_found++;
    }

    if (file_found) {
        printf("Atleast 1 file found\n");
        //create a tar file containing the matching files
        rewind(temp_list);
        char command[BUFFER_SIZE] = "tar -czvf ";
        strcat(command, TAR_FILE_NAME);
        char filename[BUFFER_SIZE];
        while (fgets(filename, sizeof(filename), temp_list) != NULL) {
            filename[strcspn(filename, "\n")] = 0;
            strcat(command, " ");
            strcat(command, filename);
        }
        int result = system(command);
        send_tar_file(socket_fd);	//create socket and send the tar file to client
        fclose(temp_list);
    } else {
        printf("No file found.\n");
        if (send(socket_fd, "0", strlen("0"), 0) != strlen("0")) {
            perror("Error sending tar file size to client");
            return NULL;
        }
        fclose(temp_list);
        return "No file found.";
    }
    return NULL;
}

//to read the arguments sent in the command to determine the files
void read_filenames(char* buffer, char filenames[MAX_FILES][MAX_FILENAME_LEN], int* num_files) {
    char* token;
    char delim[] = " ";
    int i = 0;
    token = strtok(buffer, delim);
    token = strtok(NULL, delim);

    //read the filenames
    while (token != NULL && i < MAX_FILES) {
        if (strcmp(token, "-u") == 0) {
            //ignore it in server if "-u" is present
        } else {
            //store the filename in the array
            strncpy(filenames[i], token, MAX_FILENAME_LEN);
            i++;
        }
        token = strtok(NULL, delim);
    }

    *num_files = i;
}

int processClient(int socket_fd) {
    char *result;
    char buffer[BUFFER_SIZE];
    int n, fd;
    while(1) {
        result = NULL;
        bzero(buffer, BUFFER_SIZE);
        n = read(socket_fd, buffer, BUFFER_SIZE - 1);
        if (n < 0) {
            perror("TCP Server-Read Error");
            return 1;
        }
        if (n == 0) {
            break;
        }
        
        buffer[n] = '\0';
        printf("\n\n");
        printf("Command received: %s\n", buffer);
        printf("-------------------------------------------------------------------\n");
        printf("Processing command...\n");

        if (strncmp(buffer, FIND_FILE, strlen(FIND_FILE)) == 0) {
            //client asking for finding a file details
            char filename[BUFFER_SIZE];
            sscanf(buffer, "%*s %s", filename);
            printf("Filename: %s\n", filename);
            result = findfile(filename);
        } else if (strncmp(buffer, S_GET_FILES, strlen(S_GET_FILES)) == 0) {
            int min_value, max_value;
            sscanf(buffer, "%*s %d %d", &min_value, &max_value);
            sgetfiles(socket_fd, min_value, max_value);
            result = NULL;
            continue;
        } else if (strncmp(buffer, D_GET_FILES, strlen(D_GET_FILES)) == 0) {
            char min_date[BUFFER_SIZE];
            char max_date[BUFFER_SIZE];
            sscanf(buffer, "%*s %s %s", min_date, max_date);
            dgetfiles(socket_fd, min_date, max_date);
            result = NULL;
            continue;
        } else if (strncmp(buffer, GET_FILES, strlen(GET_FILES)) == 0) {
            char filenames[MAX_FILES][MAX_FILENAME_LEN];
            int num_files;
            read_filenames(buffer, filenames, &num_files);
            result = getfiles(socket_fd, filenames, num_files);
            if (result == NULL) {
                continue;
            }
        } else if (strncmp(buffer, GET_TAR_GZ, strlen(GET_TAR_GZ)) == 0) {
            char extensions[MAX_FILES][MAX_FILENAME_LEN];
            int num_extensions;
            read_filenames(buffer, extensions, &num_extensions);
            result = gettargz(socket_fd, extensions, num_extensions);
            if (result == NULL) {
                continue;
            }
        } else if (strcmp(buffer, QUIT) == 0) {
            //client disconnecting from the server
            printf("Client is quitting.\n");	
            break;
        } else {
            result = buffer;
        }
        if (send(socket_fd, result, strlen(result), 0) != strlen(result)) {
            perror("TCP Server-Send Error");
            close(socket_fd);
            return 1;
        }
        printf("Sent response to client: %s\n", result);
    }
    close(socket_fd);
    return 0;
}

int main(int argc, char *argv[]) {
    //counter to keep track of number of client
    int num_of_clients = 0;
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int optval = 1;
    pid_t childpid;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("TCP Server Mirror-Socket Error");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval))) {
        perror("TCP Server Mirror-setsockopt Error");
        exit(EXIT_FAILURE);
    }
    memset(&address, '0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    //binding the address structure
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("TCP Server Mirror-Bind Error");
        exit(EXIT_FAILURE);
    }
    //connect to the network and await until client connects
    if (listen(server_fd, MAX_CLIENTS - 1) < 0) {
        perror("TCP Server Mirror-Listen Error");
        exit(EXIT_FAILURE);
    }
    printf("Mirror listening on port %d...\n", PORT);
    while (1) {
        //assigning the connecting client info to the file descriptor
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);	
        if (client_fd < 0) {
            perror("TCP Server Mirror - Accept Error");
            continue;
        }
        //client socket connection successful acknowledgment
        if (send(client_fd, "success", strlen("success"), 0) < 0) {
            perror("TCP Server Mirror-Connection Acknowledgement Send failed");
            exit(EXIT_FAILURE);
        }
        printf("Client connected to mirror-------\n");
        //creating separate specific process for handling single client connection individually
        childpid = fork();
        if (childpid < 0) {
            perror("TCP Server Mirror - Fork Error");
            exit(EXIT_FAILURE);
        }

        if (childpid == 0) {
            close(server_fd);
            int exit_status = processClient(client_fd);
            if (exit_status == 0) {
                printf("Client Disconnected with Success---------\n");
                exit(EXIT_SUCCESS);
            } else {
                printf("Client Disconnected with Error-----------\n");
                exit(EXIT_FAILURE);
            }
        } else {
            //parent process
            num_of_clients++;
            printf("The no of clients connected till now: %d\n", num_of_clients);
            close(client_fd);
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    close(server_fd);
    return 0;
}