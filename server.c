#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//define server port number 
#define PORT 8000
//define mirror port number
#define MIRROR_PORT 9000
//ip address of mirror server that can handle access clients that main server cannot
#define MIRROR_SERVER_IP_ADDR "127.0.0.1"
#define IP_LENGTH 16
#define PORT_LENGTH 6
//send message when connection is successful 
#define CONN_SUCCESS "success"
//server handle max client
#define MAX_CLIENTS 4
#define BUFFER_SIZE 1024
#define TAR_FILE_NAME "server_temp.tar.gz"
//MAX_FILES no that the server can handle
#define MAX_FILES 6
//MAX_FILENAME_LEN that the server can handle executing the file 
#define MAX_FILENAME_LEN 50
//list of client commands to request information from the server
#define FIND_FILE "findfile"
#define S_GET_FILES "sgetfiles"
#define D_GET_FILES "dgetfiles"
#define GET_FILES "getfiles"
#define GET_TAR_GZ "gettargz"
#define QUIT "quit"

//to find the file in the server from /home/arjun93
char* findfile(char* filename) {
    char str_appended[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -name %s -printf '%%f|%%s|%%T@\\n' | head -n 1", filename);
    FILE* fp = popen(command, "r");
    char path[BUFFER_SIZE];
    if (fgets(path, BUFFER_SIZE, fp) != NULL) {
        printf("Requested File Found.\n");
        path[strcspn(path, "\n")] = 0;
        //extract the filename, size, date from the path string
        char* filename_ptr = strtok(path, "|");
        char* size_ptr = strtok(NULL, "|");
        char* date_ptr = strtok(NULL, "|");
        //convert size and date string to integer
        int size = atoi(size_ptr);
        time_t date = atoi(date_ptr);
        //generate filename string for displaying client data
        char print_filename[BUFFER_SIZE];
        strcpy(print_filename, "File Name: ");
        strcat(print_filename, filename_ptr);
        strcat(print_filename, "\n");
        //create file size str to send and display data to client
        char print_size[BUFFER_SIZE];
        strcpy(print_size, "File Size: ");
        strcat(print_size, size_ptr);
        strcat(print_size, "\n");
        //generate str representing file creation date to send to client for data display
        char print_created[BUFFER_SIZE];
        strcpy(print_created, "File Created At: ");
        strcat(print_created, ctime(&date));
        strcat(print_created, "\n");
        //to append data of the file that sent to the client
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
    //to open tar_file_name in binary read mode
    FILE* fp = fopen(TAR_FILE_NAME, "rb");
    if (!fp) {
        perror("Error opening tar file");
        return;
    }
    //to send tar_file_size to the client
    fseek(fp, 0, SEEK_END);
    long tar_file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char size_buffer[BUFFER_SIZE];
    sprintf(size_buffer, "%ld", tar_file_size);
    if (send(socket_fd, size_buffer, strlen(size_buffer), 0) != strlen(size_buffer)) {
        perror("An error sending tar file size to the client");
        fclose(fp);
        return;
    }
    //to receive ack from the client for tar_file_size share
    if (recv(socket_fd, size_buffer, BUFFER_SIZE, 0) == -1) {
        perror("An error sending tar file size to client");
        fclose(fp);
        return;
    }
    printf("Client Acknowledged.\n");
    printf("File of size %ld is being sent.\n", tar_file_size);
    //to send contents of tar file to client
    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
        if (send(socket_fd, buffer, n, 0) != n) {
            perror("An error sending tar file contents to client");
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
//to retrive files within a specified size range
void sgetfiles(int socket_fd, int size1, int size2) {
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -type f -size +%d -size -%d -print0 | tar -czvf %s --null -T -", size1, size2, TAR_FILE_NAME);
    FILE* fp = popen(command, "r");
    send_tar_file(socket_fd);
}
//to get files within a specified date range
void dgetfiles(int socket_fd, char* date1, char* date2) {
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -type f -newermt \"%s\" ! -newermt \"%s\" -print0 | tar -czvf %s --null -T -", date1, date2, TAR_FILE_NAME);
    FILE* fp = popen(command, "r");
    send_tar_file(socket_fd);
}
//to find all the files present in dir rooted at dir_name passed as argument
int find_files(const char *dir_name, const char *filename, char *tar_file) {
    int found = 0;
    DIR *dir;
    struct dirent *entry;
    struct stat file_info;
    char path[PATH_MAX];
    //open the dir for traversal
    if ((dir = opendir(dir_name)) == NULL) {
        perror("opendir");
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        //creating path for the entry being checked for
        snprintf(path, PATH_MAX, "%s/%s", dir_name, entry->d_name);
        if (lstat(path, &file_info) < 0) {
            perror("lstat");
            continue;
        }
        //recursively search for filename within dir tree using find_files 
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
    //to get current dir path
    char *dir_path = getenv("HOME");
    if (dir_path == NULL) {
        fprintf(stderr, "An error getting HOME directory path\n");
        return NULL;
    }
    //create tar_cmd
    char tar_cmd[BUFFER_SIZE] = "tar -czvf ";
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
        //run the tar_cmd
        system(tar_cmd);
        //send tar_file to the client
        FILE *tar_file = fopen(TAR_FILE_NAME, "r");
        if (tar_file == NULL) {
            fprintf(stderr, "Error opening tar file\n");
            return NULL;
        }
        fclose(tar_file);
        //create socket and send_tar_file to client
        send_tar_file(socket_fd);
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
//recursive function to create list of matching files with extensions
void find_gettargz_files(const char *dir_path, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions, FILE *temp_list) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        printf("Error: could not open directory %s\n", dir_path);
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            //this is a regular file
            char *name = entry->d_name;
            for (int i = 0; i < num_extensions; i++) {
                char *extension = extensions[i];
                int len_ext = strlen(extension);
                int len_name = strlen(name);
                //add file to list if it has matching extension
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
//to find and send the tar.gz file of files with maching extensions
char* gettargz(int socket_fd, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions) {
    int file_found = 0;
    //create a temporary file to store the list of matching files
    FILE *temp_list = tmpfile();
    if (!temp_list) {
        printf("Error: could not create temporary file\n");
        return NULL;
    }
    //to search recursively for files with matching extensions in the dir tree starting from the user dir /home/arjun93
    find_gettargz_files(getenv("HOME"), extensions, num_extensions, temp_list);
    rewind(temp_list);
    char filename[BUFFER_SIZE];

    while (fgets(filename, sizeof(filename), temp_list) != NULL) {
        //remove newline char at end of filename
        filename[strcspn(filename, "\n")] = 0;
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
            //add filename to tar command
            strcat(command, " ");
            strcat(command, filename);
        }
        int result = system(command);
        //create socket and send_tar_file to client
        send_tar_file(socket_fd);
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
//to read cmd arguments and determine desired files/extensions
void read_filenames(char* buffer, char filenames[MAX_FILES][MAX_FILENAME_LEN], int* num_files) {
    char* token;
    char delim[] = " ";
    int i = 0;
    //get the first token
    token = strtok(buffer, delim);
    token = strtok(NULL, delim);
    //read the filenames
    while (token != NULL && i < MAX_FILES) {
        if (strcmp(token, "-u") == 0) {
            //if "-u" is present ignore it in the server
        }
        //else store the filename in the array 
        else {
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
        printf("-----------------------------------------------------\n");
        printf("Processing the command...\n");

        if (strncmp(buffer, FIND_FILE, strlen(FIND_FILE)) == 0) {
            //client request for finding a file details
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
            //retun command back to client if no handler is available
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
int main(int argc, char const *argv[]) {
    //counter to track no of clients
    int num_of_clients = 0, num_of_server_clients = 0;
    //int to store the socket connection file descriptors
    int server_fd, client_fd;
    //address will store the details regarding the socket connections being handled like port to be connected to
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int optval = 1;
    pid_t childpid;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("TCP Server-Socket Error");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("TCP Server-setsockopt Error");
        exit(EXIT_FAILURE);
    }
    memset(&address, '0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    //binding the address structure to the socket opened
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("TCP Server-Bind Error");
        exit(EXIT_FAILURE);
    }
    //connect to the network and await until client connects
    if (listen(server_fd, MAX_CLIENTS - 1) < 0) {
        perror("TCP Server-Listen Error");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", PORT);
    while (1) {
        //assigning the connecting client info to fd
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if ((num_of_clients >= MAX_CLIENTS && num_of_clients < 2*MAX_CLIENTS) || (num_of_clients >= 2*MAX_CLIENTS && num_of_clients % 2 != 0)) {
            //redirection to the mirror handled here
            char mirror_port[PORT_LENGTH];
            sprintf(mirror_port, "%d", MIRROR_PORT);
            char mirror_address[IP_LENGTH + PORT_LENGTH + 1] = MIRROR_SERVER_IP_ADDR;
            strcat(mirror_address, ":");
            strcat(mirror_address, mirror_port);
            printf("Redirecting client to mirror server\n");
            //create new socket connection with the mirror server
            if (send(client_fd, mirror_address, strlen(mirror_address), 0) < 0) {
                perror("TCP Server-Mirror Address Send failed");
                exit(EXIT_FAILURE);
            }
            //increment no of clients to track clients and to make server usage
            num_of_clients++;   
        } else {
            //server handles the client connection
            if (client_fd < 0) {
                perror("TCP Server-Accept Error");
                continue;
            }
            //client socket connection successful acknowledgment
            if (send(client_fd, CONN_SUCCESS, strlen(CONN_SUCCESS), 0) < 0) {
                perror("TCP Server-Connection Acknowledgement Send failed");
                exit(EXIT_FAILURE);
            }
            printf("Client connected.------------\n");
            // to handle single client connection individually
            childpid = fork();
            if (childpid < 0) {
                perror("TCP Server-Fork Error");
                exit(EXIT_FAILURE);
            }
            if (childpid == 0) {
                //child process handling client connection requests
                close(server_fd);
                //handling client requests
                int exit_status = processClient(client_fd);
                if (exit_status == 0) {
                    printf("Client Disconnected with Success Code-------------\n");
                    exit(EXIT_SUCCESS);
                } else {
                    printf("Client Disconnected with Error Code---------------\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                //parent process
                num_of_clients++;
                num_of_server_clients++;
                printf("The no of clients currently connected with server and mirror is: %d\n", num_of_clients);
                printf("The no of clients connected to the server is : %d\n", num_of_server_clients);
                close(client_fd);
                while (waitpid(-1, NULL, WNOHANG) > 0);
            }
        }
    }
    close(server_fd);
    return 0;
}