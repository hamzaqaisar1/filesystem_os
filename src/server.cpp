#include <cctype>
#include <semaphore.h> 
#include <unistd.h> 
#include <chrono>
#include <cstdio>
#include <map>
#include <string_view>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <thread>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mutex>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <tuple>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include "disk.h"
#include "../include/filesystem/file.h"

#define PORT "90"  

#define MAXDATASIZE 1000

#define BACKLOG 50

using namespace std;

Disk filesystem = Disk(1000);
string sentinel = "0";

map<int,tuple<sem_t,mutex *,int>> open_file_ids;


int validate_file_name(string& fname){
    if(fname.find('|') != string::npos || 
       fname.find('&') != string::npos || 
       fname.find(',') != string::npos ||
       fname.find('\n') != string::npos || 
       fname.find('\t') != string::npos ||
       fname.empty()){
        return 0;
    } else {
        return 1;
    }
}

void sigchld_handler(int s){
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

vector<vector<string>> read_script(string path){
    fstream fin(path,ios::in);
    string entry,temp;
    vector<vector<string>> commands;

    while (!fin.eof()) { 
        getline(fin, entry);
        stringstream buffer(entry);
        string word;
        vector<string> command;
        command.clear();
        while(getline(buffer,word,'|')){
            command.push_back(word);
        }
        commands.push_back(command);
    }   
    fin.close(); 
    return commands;
}

vector<string> parse_command(vector<string> command,int mode = 0){
    vector<string> arguments;
    if(mode == 0){
        if(command[0] == "create"){
            arguments.push_back("1"); 
            arguments.push_back(command[1]);
        } else if(command[0] == "delete"){
            arguments.push_back("2"); 
            arguments.push_back(command[1]);
        } else if(command[0] == "open"){
            arguments.push_back("3");
            arguments.push_back(command[1]);
        } else if(command[0] == "show_map"){
            arguments.push_back("4");
        } else if(command[0] == "chdir"){
            arguments.push_back("5");
            arguments.push_back(command[1]);
        } else if(command[0] == "mkdir"){
            arguments.push_back("6");
            arguments.push_back(command[1]);
        } else if(command[0] == "move"){
            arguments.push_back("7");
            arguments.push_back(command[1]);
            arguments.push_back(command[2]);
        }
    } else if (mode == 1){
        if(command[0] == "write"){
            arguments.push_back("1"); 
            arguments.push_back(command[1]);
            arguments.push_back(command[2]);
            if(command.size() > 3){
                arguments.push_back(command[3]);
            }
        } else if(command[0] == "write_at"){
            arguments.push_back("2"); 
            arguments.push_back(command[1]);
            arguments.push_back(command[2]);
            arguments.push_back(command[3]);
        } else if(command[0] == "read"){
            arguments.push_back("3");
            arguments.push_back(command[1]);
        } else if(command[0] == "read_at"){
            arguments.push_back("4");
            arguments.push_back(command[1]);
            arguments.push_back(command[2]);
            arguments.push_back(command[3]);
        } else if(command[0] == "truncate"){
            arguments.push_back("5");
            arguments.push_back(command[1]);
            arguments.push_back(command[2]);
        } else if(command[0] == "move"){
            arguments.push_back("6");
            arguments.push_back(command[1]);
            arguments.push_back(command[2]);
            arguments.push_back(command[3]);
            arguments.push_back(command[4]);
        } else if(command[0] == "close"){
            arguments.push_back("-1");
            arguments.push_back(command[1]);

        }
    }
    return arguments;
}

string call_file_functions(vector<string> arguments,File& file,int& is_file_open,int& curr_dir){
    string return_string;
    filesystem.update_metadata();
    
    curr_dir = filesystem.dir_metadata.find(curr_dir)->first;
    file.update_file(filesystem.metadata);
    auto file_pos_list = open_file_ids.find(is_file_open);
    if(arguments[0] == "1"){
        //Write to file
          
        int file_write_mode;
        string text,buffer;
        if(arguments[1].empty()){
            return "\nError: No text found\n";
        } else {
            text = arguments[1];
        }
        if(arguments.size() > 2){
            if(arguments[2] == "Y"){
                file_write_mode = 1;
            } else {
                file_write_mode = 0;
            }
        }
        sem_wait(&get<0>(file_pos_list->second));
        int result = file.write_to_file(filesystem,text,file_write_mode);
        sem_post(&get<0>(file_pos_list->second));

        if(result != 0){
            return "\nError: Hard Disk is Full\n";
        } else {
            return_string ="\nSuccess: File written to successfully\n";
        }
    } else if(arguments[0] == "2"){
        //Write to file at a position
        string text,buffer;
        int pos;
        if(arguments[1].empty() || arguments[2].empty()){
            return "\nError: Data not found\n";
        } else {
            pos = stoi(arguments[1]);
            text = arguments[2];
        }
        
        
        if(pos > file.get_data().length()){
            return "\nError: Invalid Value\n";
        }
        sem_wait(&get<0>(file_pos_list->second));
        int result = file.write_to_file(filesystem,pos,text);
        sem_post(&get<0>(file_pos_list->second));
        

        if(result != 0){
            return  "\nError: Hard Disk is Full\n";
        }else {
            return_string ="\nSuccess: File written to successfully\n";
        }
    } else if(arguments[0] == "3"){
        get<1>(file_pos_list->second)->lock();
        get<2>(file_pos_list->second)++;
        if(get<2>(file_pos_list->second) == 1){
            sem_wait(&get<0>(file_pos_list->second));
        }
        get<1>(file_pos_list->second)->unlock();

        string content = file.read_from_file();

        get<1>(file_pos_list->second)->lock();
        get<2>(file_pos_list->second)--;
        if(get<2>(file_pos_list->second) == 0) {
            sem_post(&get<0>(file_pos_list->second));
        }
        get<1>(file_pos_list->second)->unlock();

        if(content.empty()){
            return "Error: No data found\n";
        } else {
            return "\n\n" + file.name + " : " + content + "\n\n\n";
        }
    } else if(arguments[0] == "4"){
        int start,size;
        string buffer;
        string contents = file.get_data();
        if(arguments[1].empty() || arguments[2].empty()){
            return "\nError: Data not found\n";
        } else {
            start = stoi(arguments[1]);
            size = stoi(arguments[2]);
        }
        
        if(start > contents.length() || size + start > contents.length()){
            return "\nError: Invalid value\n";
        }
        
        get<1>(file_pos_list->second)->lock();
        get<2>(file_pos_list->second)++;
        if(get<2>(file_pos_list->second) == 1){
            sem_wait(&get<0>(file_pos_list->second));
        }
        get<1>(file_pos_list->second)->unlock();

        string content = file.read_from_file(start,size);

        get<1>(file_pos_list->second)->lock();
        get<2>(file_pos_list->second)--;
        if(get<2>(file_pos_list->second) == 0) {
            sem_post(&get<0>(file_pos_list->second));
        }
        get<1>(file_pos_list->second)->unlock();
        
        if(content.empty()){
            return "\nError: No data found\n";
        } else {
            return "\n\n" + file.name + " : " + content + "\n\n\n";
        }

    } else if(arguments[0] == "5"){
        string buffer;
        int max_size;
        if(arguments[1].empty()){
            return "\nError: Data not found\n";
        } else {
            max_size = stoi(arguments[1]);
        }
        
        if((max_size > file.get_data().length() || max_size == 0)){
            return "\nError: Invalid value\n";
        }
        sem_wait(&get<0>(file_pos_list->second));
        int result = file.truncate_file(filesystem,max_size);
        sem_post(&get<0>(file_pos_list->second));

        if(result != 0){
            return "\nError: Hard Disk is Full\n";
        } else {
            return_string ="\nSuccess: File modified successfully\n";
        }

    } else if(arguments[0] == "6"){
        // Move within file
        int start,size,target;
        string buffer;
        string contents = file.get_data();
        if(arguments[1].empty() || arguments[2].empty() || arguments[3].empty()){
            return "\nError: Data not found\n";
        } else {
            start = stoi(arguments[1]);
            size = stoi(arguments[2]);
            target = stoi(arguments[3]);
        }
        
        if(start > contents.length() || size + start > contents.length() || target + size > contents.length()){
            return "\nError: Invalid value\n";
        }
        sem_wait(&get<0>(file_pos_list->second));
        int result = file.move_within_file(filesystem,start,size,target);
        sem_post(&get<0>(file_pos_list->second));
        if(result == -1 || result == -2){
            return "\nError: Hard Disk Full\n";
        }   else {
            return_string ="\nSuccess: File modified successfully\n";
        }

    } else if(arguments[0] == "-1"){
        auto file_pos_list = open_file_ids.find(is_file_open);
        if(file_pos_list != open_file_ids.end()){
            sem_destroy(&get<0>(file_pos_list->second));
            open_file_ids.erase(file_pos_list);
        } 
        return_string = "File closed!!";
    }
    return return_string;
}

string call_disk_functions(vector<string> arguments,int& is_file_open,int& curr_dir){
    string fname,return_string;
    

    if(arguments[0] == "1"){
        //Create File
        if(arguments[1].empty()){
            return "\nError: Invalid File Name\n";
        } else {
            fname = arguments[1];
        }

        vector<int> curr_dir_files = get<2>(filesystem.dir_metadata.find(curr_dir)->second);
        multimap<int,pair<string,vector<int>>>::iterator file_ptr;
        for(auto i = curr_dir_files.begin();i != curr_dir_files.end();i++){
            file_ptr = filesystem.metadata.find(*i);
            if(file_ptr->second.first == fname){
                return "\nError: File name already exists\n";
            } 
        }
        int result = filesystem.create(fname,curr_dir);
        if(result == -1){
            return "\nError: Hard Disk is full\n";
        } else {
            return_string = "\nSuccess: File created sucessfully\n";
        }
    } else if (arguments[0] == "2"){
        // Delete File
        if(arguments[1].empty()){
            return "\nError: Invalid File Name\n";
        } else {
            fname = arguments[1];
        }
        
        vector<int> curr_dir_files = get<2>(filesystem.dir_metadata.find(curr_dir)->second);
        multimap<int,pair<string,vector<int>>>::iterator file_ptr;
        for(auto i = curr_dir_files.begin();i != curr_dir_files.end();i++){
            file_ptr = filesystem.metadata.find(*i);
            if(file_ptr->second.first == fname){
                break;
            }
        }
        if(file_ptr->second.first != fname){
            return "\nError: File does not exist\n\n";
        }
        int result = filesystem.del(fname,file_ptr->first,curr_dir);
        if(result == -1){
            return "\nError: Hard Disk is full\n";
        } else {
            return_string = "\nSuccess: File deleted sucessfully\n";
        }
    } else if (arguments[0] == "3"){
        //Open File
        if(arguments[1].empty()){
            return "\nError: Invalid File Name\n";
        } else {
            fname = arguments[1];
        }

        vector<int> curr_dir_files = get<2>(filesystem.dir_metadata.find(curr_dir)->second);
        multimap<int,pair<string,vector<int>>>::iterator file_entry;
        for(auto i = curr_dir_files.begin();i != curr_dir_files.end();i++){
            file_entry = filesystem.metadata.find(*i);
            if(file_entry->second.first == fname){
                break;
            }
        }
        if(file_entry->second.first != fname){
            return "\nError: File does not exist\n\n";
        }
        if(file_entry != filesystem.metadata.end()) {
            File file = filesystem.open(fname,file_entry->first);
            is_file_open = file.id;
            return_string =  to_string(file.id) + "|";
        } else if(file_entry == filesystem.metadata.end()){
            return "\nError: File name not found\n\n";
        }
    } else if (arguments[0] == "4"){
        //Memory Map
        string buffer = "";
        if(arguments[1] == "Y"){
            buffer = filesystem.memory_map(curr_dir,1);
        } else {
            buffer = filesystem.memory_map(0,1);
        }
        return_string = buffer;
    } else if(arguments[0] == "5"){
        //Change Directory
        if(arguments[1].empty()){
            return "\nError: Invalid File Name\n";
        } else {
            fname = arguments[1];
        }
        
        
        int new_dir = filesystem.chdir(fname,curr_dir);
        if(new_dir == -1){
            return "\nError: Invalid Path\n\n";
        } else {
            curr_dir = new_dir;
            return_string =  to_string(curr_dir) + "|" + filesystem.path;
        }
    } else if(arguments[0] == "6"){
        //Make New Directory
        if(arguments[1].empty()){
            return "\nError: Invalid File Name\n";
        } else {
            fname = arguments[1];
        }
    
        int result = filesystem.mkdir(fname,curr_dir);
        if(result == -1){
            return "\nError: Folder name already exists\n\n";
        } else {
            return_string = "\nSucess: Folder created successfully\n";
        }
    } else if(arguments[0] == "7"){
        string fname,dir_path;
        if(arguments[1].empty() || arguments[2].empty()){
            return "\nError: Invalid File Name\n";
        } else {
            fname = arguments[1];
            dir_path = arguments[2];
        }
        vector<int> curr_dir_files = get<2>(filesystem.dir_metadata.find(curr_dir)->second);
        multimap<int,pair<string,vector<int>>>::iterator file_entry;
        for(auto i = curr_dir_files.begin();i != curr_dir_files.end();i++){
            file_entry = filesystem.metadata.find(*i);
            if(file_entry->second.first == fname){
                break;
            }
        }
        if(file_entry->second.first != fname){
            return "\nError: File does not exist\n\n";
        }
        int result = filesystem.move(file_entry->first,dir_path,curr_dir);
        if(result == -1){
            return "\nError: File not found\n\n";
        } else {
            return_string ="\nSuccess: File moved successfully\n";
        }
    }
    return return_string;
}

File open_file_server(int id,int& curr_dir){
    //Open File
    File placeholder;
    vector<int> curr_dir_files = get<2>(filesystem.dir_metadata.find(curr_dir)->second);
    multimap<int,pair<string,vector<int>>>::iterator file_entry;
    for(auto i = curr_dir_files.begin();i != curr_dir_files.end();i++){
        file_entry = filesystem.metadata.find(*i);
        if(file_entry->first == id){
            break;
        }
    }

    if(file_entry->first != id){
        placeholder.id = -1;
    }
    if(file_entry != filesystem.metadata.end()) {
        placeholder = filesystem.open(file_entry->second.first,file_entry->first);
    } else if(file_entry == filesystem.metadata.end()){
        placeholder.id = -1;
    }
    return placeholder;
}

/*
void process_script(string path){
    vector<vector<string>> commands = read_script(path);
    
    File file;
    bool is_file_open = false;

    int curr_dir = 0;
    for(int i = 0;i < commands.size();i++){
        if(!is_file_open){
            //Execute Disk Functions
            vector<string> arguments = parse_command(commands[i]);
            string result = call_disk_functions(arguments,file,is_file_open,curr_dir);
            if(result != "" && result.find("Error") != string::npos){
                cout << result << endl;
                break;
            } 
        } else {
            //Execute File Functions
            vector<string> arguments = parse_command(commands[i],1);
            string result = call_file_functions(arguments,file,is_file_open,curr_dir);
            if(result != "" && result.find("Error") != string::npos){
                cout << result << endl;
                break;
            } else if(result != "" && result.find("Error") == string::npos){
                cout << result <<endl;
                continue;
            }
        }
    }
    curr_dir = filesystem.chdir("root",0);
    filesystem.memory_map(curr_dir);
}*/

vector<string> parse_request(string command){
    string word;
    stringstream buffer(command);
    vector<string> parsed_command;


    while (getline(buffer, word,'|')) {     
        parsed_command.push_back(word);
    }
    return parsed_command;
}

bool is_number(const std::string& s)
{
    return !s.empty() && find_if(s.begin(), 
        s.end(), [](unsigned char c) { return !isdigit(c); }) == s.end();
}

void processSocket(int clientSocket){
    char message[MAXDATASIZE] = {};
    char buffer[MAXDATASIZE] = {};
    int newSocket = clientSocket;
    int curr_dir,is_file_open;
    recv(newSocket,buffer,MAXDATASIZE - 1,0);
    // Process the command sent in by client
    cout << buffer << endl;
    vector<string> commands = parse_request(string(buffer));
    if(is_number(commands[0]) && is_number(commands[1])){
        curr_dir = stoi(commands[0]);
        is_file_open = stoi(commands[1]);
        cout << curr_dir << is_file_open << endl;
        auto start = commands.begin() + 2; 
        auto end = commands.end(); 
        vector<string> result((commands.size()) - 2); 
        copy(start, end, result.begin());
        filesystem.update_metadata();
        if(is_file_open){
            // Open File
            auto file_pos_list = open_file_ids.find(is_file_open);
            if(file_pos_list == open_file_ids.end()){
                sem_t write_file;
                sem_init(&write_file, 1,1);
                mutex * mt;
                int readcount = 0;
                tuple<sem_t,mutex *,int>  temp;
                temp = make_tuple(write_file,mt,readcount);
                open_file_ids.insert(pair<int,tuple<sem_t,mutex *,int>> (is_file_open,temp));
            }
            cout << result[0] <<endl;
            File curr_file = open_file_server(is_file_open, curr_dir);
            if(curr_file.id != -1){
                string temp = call_file_functions(result,curr_file,is_file_open,curr_dir) + '\0';
                strcpy(message,temp.c_str());
            }
        } else {
            
            string temp = call_disk_functions(result, is_file_open,curr_dir) + '\0';
            strcpy(message,temp.c_str());
        }
    } else {
        strcpy(buffer,"\nError: Invalid Input\n");
    }
    //Send data
    send(newSocket,message,strlen(message),0);
    
    //Close Socket
    close(newSocket);
}

int Disk::total_files = 0;
int Disk::total_folders = 0;

int main(int argc,char * argv[]){
    //Setup  a socket
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    struct sigaction sa;
    socklen_t sin_size;
    int yes=1;
    pid_t pid[50];
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");
    pthread_t tid[BACKLOG];

    int i = 0;
    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        int pid_c = 0;
        if ((pid_c = fork()) == 0) { // this is the child process
            processSocket(new_fd);
        } else {
            pid[i++] = pid_c;
            if(i > BACKLOG){
                i = 0;
                while(i < BACKLOG){
                    waitpid(pid[i++],NULL,0);
                }
            }
        }
    }
    
       

         
    /*
    {
        filesystem.path = "root/";
        int thread_count = argc-1;
        thread threads[thread_count];      
        for(int i = 0;i < thread_count;i++){
            string path = argv[i + 1];
            cout << path;
            threads[i] = thread(process_script,path);
        }

        for(int i = 0;i < thread_count;i++){
            threads[i].join();
        }
        } else {
            vector<string> arguments;
            File file;
            bool is_file_open;
            int curr_dir = 0;
            filesystem.memory_map(0,1);
            while(sentinel != "-1"){

                cout << "\nEnter the number written corresponding to the action to move further"<<endl;
                cout << "Current Path: " << filesystem.path << endl;
                cout << "1\t-> Create File"<<endl;
                cout << "2\t-> Delete File"<<endl;
                cout << "3\t-> Open File"<<endl;
                cout << "4\t-> View Map"<<endl;
                cout << "5\t-> Change Directory"<< " Current: "<< filesystem.path << endl;
                cout << "6\t-> Make Directory"<<endl;
                cout << "7\t-> Change File Location"<<endl;
                cout << "-1\t-> Quit"<<endl;
                cout << "Enter an action: ";
                cin.clear();
                getline(cin,sentinel,'\n');
                cin.clear();

                filesystem.update_metadata();


                if(sentinel != "-1"){
                    
                    arguments.clear();
                    arguments.push_back(sentinel);
                    call_disk_functions(arguments,file,is_file_open,curr_dir,1);
                } else{
                    cout << "\nExiting...\n";
                }

                
            }
            
        }
        }*/

    
    return 0;
}
