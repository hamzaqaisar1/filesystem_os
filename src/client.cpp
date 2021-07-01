#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <numeric>
#include <fstream>
#include <thread>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <sys/types.h>
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
#include "../include/filesystem/file.h"

#define PORT "90"  

#define MAXDATASIZE 1000


using namespace std;
string sentinel = "0";
int sockfd, numbytes;  
string path = "root/";
int curr_dir = 0;
int is_file_open = 0;
string server_ip;


char change_case (char c) {
    if (std::isupper(c)) 
        return std::tolower(c); 
    else
        return std::toupper(c); 
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

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
string create_request(vector<string> command){
    string parsed_command = to_string(curr_dir) + "|" + to_string(is_file_open) + "|";
    for(int i = 0; i < command.size();i++){
        parsed_command += command[i] + "|";
    }
    cout << parsed_command << endl;
    return parsed_command;
}

string send_request(string s_command){
    int numbytes;
    char *message =  &s_command[0];
    

    char buffer[MAXDATASIZE] = {};
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    vector<string> arguments;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    

    if ((rv = getaddrinfo( server_ip.c_str(),PORT, &hints, &servinfo)) != 0) {
        printf("getaddrinfo: %s\n",gai_strerror(rv));
        return "";
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        return "client: failed to connect\n";
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);

    freeaddrinfo(servinfo); // all done with this structure


    if (send(sockfd, message, strlen(message), 0) == -1){
        cout << "\nError: Command not sent\n";
        return "-1";
    }

    if(recv(sockfd, buffer, MAXDATASIZE-1, 0) == -1)
    {
       printf("Receive failed\n");
    }
    close(sockfd);
    return buffer;

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
// File parse_file_info(string raw_data){
//     stringstream buffer(raw_data);
//     string id, name,data;
//     getline(buffer,id, '|');
//     getline(buffer, name, '|');
//     getline(buffer, data, '|');
//     if(!stoi(id)||name[0] == string::npos||data[0] == string::npos){
//         cout << "Info sent is not correct" << endl;
//         File not_found;
//         return not_found;
//     }
//     File curr_file = File(name,stoi(id));
//     curr_file.set_data(data);

//     return curr_file;
    


// }

string call_file_functions(vector<string> arguments,int mode = 0){
    if(arguments[0] == "1"){
        //Write to file
        int file_write_mode;
        string text,buffer;
        if(mode == 1){
            cout << "Enter the text to write into the file: ";
            cin.clear();
            fflush(stdin);
            getline(cin,text,'\n');
            cin.clear();
            arguments.push_back(text);
            cout << "Do you want to append to the file? (Y/N): ";
            cin.clear();
            fflush(stdin);
            getline(cin,buffer,'\n');
            cin.clear();
            arguments.push_back(buffer);
            if(buffer == "Y"){
                file_write_mode = 1;
            }
        } else {
            if(arguments[2].empty()){
                return "\nError: No text found\n";
            } else {
                text = arguments[2];
            }
            if(arguments.size() > 3){
                file_write_mode = stoi(arguments[3]);
            }

        }
        string req = create_request(arguments);
        cout << req << endl;
        string result = send_request(req);

        cout << result << endl;
    } else if(arguments[0] == "2"){
        //Write to file at a position
        string text,buffer;
        int pos;

        if(mode == 1){
            cout << "Enter the position to write the text into the file: ";
            cin.clear();
            fflush(stdin);
            getline(cin,buffer,'\n');
            cin.clear();
            pos = stoi(buffer);
            arguments.push_back(buffer);
            cout << "Enter the text to write into the file: ";
            cin.clear();
            fflush(stdin);
            getline(cin,text,'\n');
            cin.clear();
            arguments.push_back(text);
        } else {
            if(arguments[2].empty() || arguments[3].empty()){
                return "\nError: Data not found\n";
            } else {
                pos = stoi(arguments[2]);
                text = arguments[3];
            }
        }
        
        string req = create_request(arguments);
        string result = send_request(req);

        cout << result << endl;
    } else if(arguments[0] == "3"){
        string req = create_request(arguments);
        string result = send_request(req);
        
        if(result.empty()){
            cout << "Error: No data found"<<endl;
        } else {
            if(mode == 1){
                cout << "\n" + result;
            } else {
                return "\n" + result;
            }
        }
    } else if(arguments[0] == "4"){
        int start,size;
        string buffer;
        if(mode == 1){
            cout << "Enter the position to start reading data from the file: ";
            cin.clear();
            fflush(stdin);
            getline(cin,buffer,'\n');
            start = stoi(buffer);
            cin.clear();
            arguments.push_back(buffer);
            cout << "Enter the size of data  to be read from the file: ";
            cin.clear();
            fflush(stdin);
            getline(cin,buffer,'\n');
            size = stoi(buffer);
            cin.clear();
            arguments.push_back(buffer);
        } else {
            if(arguments[2].empty() || arguments[3].empty()){
                return "\nError: Data not found\n";
            } else {
                start = stoi(arguments[2]);
                size = stoi(arguments[3]);
            }
        }
        
        string req = create_request(arguments);
        string result = send_request(req);
        if(result.empty()){
            cout << "Error: No data found"<<endl;
        } else {
            cout << "\n" + result;
        }

    } else if(arguments[0] == "5"){
        string buffer;
        int max_size;
        if(mode == 1){
            cout << "Enter the max size of the file: ";
            cin.clear();
            fflush(stdin);
            getline(cin,buffer,'\n');
            max_size = stoi(buffer);
            arguments.push_back(buffer);
            cin.clear();
        } else {
            if(arguments[2].empty()){
                return "\nError: Data not found\n";
            } else {
                max_size = stoi(arguments[2]);
            }
        }
        string req = create_request(arguments);
        string result = send_request(req);
        cout << result << endl;
    } else if(arguments[0] == "6"){
        // Move within file
        int start,size,target;
        string buffer;
        if(mode == 1){
            cout << "Enter the position to start reading data from the file: ";
            cin.clear();
            fflush(stdin);
            getline(cin,buffer,'\n');
            start = stoi(buffer);
            arguments.push_back(buffer);
            cin.clear();
            cout << "Enter the size of data  to be read from the file: ";
            cin.clear();
            fflush(stdin);
            getline(cin,buffer,'\n');
            size = stoi(buffer);
            arguments.push_back(buffer);
            cin.clear();
            cout << "Enter the position to put the data into within the file: ";
            cin.clear();
            fflush(stdin);
            getline(cin,buffer,'\n');
            target = stoi(buffer);
            arguments.push_back(buffer);
            cin.clear();
        } else {
            if(arguments[2].empty() || arguments[3].empty() || arguments[4].empty()){
                return "\nError: Data not found\n";
            } else {
                start = stoi(arguments[2]);
                size = stoi(arguments[3]);
                target = stoi(arguments[4]);
            }
        }

        string req = create_request(arguments);
        string result = send_request(req);
        cout << result << endl;
    } else if(arguments[0] == "-1"){
        string req = create_request(arguments);
        string result = send_request(req);
        cout << result << endl;
        is_file_open = 0;
        return "";
    }
    return "";
}


string call_disk_functions(vector<string> arguments,int mode = 0){
    string fname;
    if(arguments[0] == "1"){
        //Create File
        if(mode == 1){
            cout << "Enter file name (Invalid Characters : |,& or a comma): ";
            cin.clear();
            fflush(stdin);
            getline(cin,fname,'\n');
            arguments.push_back(fname);
        } else {
            if(arguments[1].empty()){
                return "\nError: Invalid File Name\n";
            } else {
                fname = arguments[1];
            }
        }
        
        if(!validate_file_name(fname)){
            if(mode == 1){
                cout << "\nError: Invalid File Name"<<endl;
                return "";
            } else {
                return "\nError: Invalid File Name\n";

            }
        }
        string req = create_request(arguments);
        string result = send_request(req);
        cout << "\n" << result << endl;
    } else if (arguments[0] == "2"){
        // Delete File
        if(mode == 1){
            cout << "Enter file name (Invalid Characters : |,& or a comma): ";
            cin.clear();
            fflush(stdin);
            getline(cin,fname,'\n');
            arguments.push_back(fname);
        } else {
            if(arguments[1].empty()){
                return "\nError: Invalid File Name\n";
            } else {
                fname = arguments[1];
            }
        }
        
        if(!validate_file_name(fname)){
            if(mode == 1){
                cout << "\nError: Invalid File Name"<<endl;
                return "";
            } else {
                return "\nError: Invalid File Name\n";
            }
        }
        
        string req = create_request(arguments);
        string result = send_request(req);
        cout << "\n" << result << endl;
    } else if (arguments[0] == "3"){
        //Open File
        if(mode == 1){
            cout << "Enter file name (Invalid Characters : |,& or a comma): ";
            cin.clear();
            fflush(stdin);
            getline(cin,fname,'\n');
            arguments.push_back(fname);
        } else {
            if(arguments[1].empty()){
                return "\nError: Invalid File Name\n";
            } else {
                fname = arguments[1];
            }
        }
        if(!validate_file_name(fname)){
            if(mode == 1){
                cout << "\nError: Invalid File Name"<<endl;
                return "";
            } else {
                return "\nError: Invalid File Name\n";
            }
        }
        
        
        if(mode == 1){
            string req = create_request(arguments);
            string result = send_request(req);
            if(result.find("Error") != string::npos){
                cout << "\n" << result << endl;
                return "";
            } else  {
                is_file_open = stoi(result.substr(0,result.find("|")));
            }
            string nest_sentinel;
            while(nest_sentinel != "-1"){
                if(!nest_sentinel.empty() && mode == 1){
                    string yn;
                    cout << "Do you want to continue? (Y/N): "<<endl;
                    cin.clear();
                    fflush(stdin);
                    getline(cin,yn,'\n');
                    cin.clear();
                    if(yn[0] == 'N'){
                        cout << "\n\nClosing File..."<<endl;
                        cout << "\n\n";
                        is_file_open = 0;
                        break;
                    }
                }
                cout << "\nEnter the number written corresponding to the action to move further"<<endl;
                cout << "1 -> Write To File"<<endl;
                cout << "2 -> Write To File at a specific position"<<endl;
                cout << "3 -> Read File"<<endl;
                cout << "4 -> Read File from a specific position"<<endl;
                cout << "5 -> Truncate File"<<endl;
                cout << "6 -> Move Within File"<<endl;
                cout << "-1 -> Quit"<<endl;
                cin.clear();
                cout << "Enter an action: ";
                fflush(stdin);
                getline(cin,nest_sentinel,'\n');
                cin.clear();

                vector<string> arguments;

                if(nest_sentinel != "-1"){
                    arguments.clear();
                    arguments.push_back(nest_sentinel);
                    call_file_functions(arguments,1);
                } else {
                    arguments.clear();
                    arguments.push_back(nest_sentinel);
                    string req = create_request(arguments);
                    string result = send_request(req);
                    cout << result << endl;
                    is_file_open = 0;
                    cout << "\n\nClosing File..."<<endl;
                    cout << "\n\n";
                    break;
                }
                
            }

        } 
        /*
        else if(file_entry != filesystem.metadata.end() && mode != 1) {
            file = filesystem.open(fname,file_entry->first);
            is_file_open = true;
            return "";
        }*/
    } else if (arguments[0] == "4"){
        //Memory Map
        string buffer;
        if(mode == 1){
            cout << "Do you want to only see current directory(Y/N) : ";
            cin.clear();
            fflush(stdin);
            getline(cin,buffer,'\n');
            transform(buffer.begin(),buffer.end(),buffer.begin(),change_case);
            if(buffer == "Y"){
                arguments.push_back("Y");
            } else {
                arguments.push_back("Y");
            }
            string req = create_request(arguments);
            string result = send_request(req);
            cout << result;
        } else {
            cout << "\n";
        }
    } else if(arguments[0] == "5"){
        //Change Directory
        if(mode == 1){
            cout << "Enter the path to the folder (Invalid Characters : |,& or a comma): ";
            cin.clear();
            fflush(stdin);
            getline(cin,fname,'\n');
            arguments.push_back(fname);
            
        } else {
            if(arguments[1].empty()){
                return "\nError: Invalid File Name\n";
            } else {
                fname = arguments[1];
            }
        }
        
        if(!validate_file_name(fname)){
            if(mode == 1){
                cout << "\nError: Invalid File Name"<<endl;
                return "";
            } else {
                return "\nError: Invalid File Name\n";
            };
        }
        int new_dir;
        string req = create_request(arguments);
        string result = send_request(req);
        if(result.find("Error") != string::npos){
            cout << "\n" << result << endl;
            return "";
        } else  {
            string delimiter = "|";
            string token = result.substr(0, result.find(delimiter));
            new_dir = stoi(result);

            result.erase(0,result.find(delimiter) + 1);
            path = result;
           
        }
        if(new_dir == -1){
            if(mode == 1){
                cout << "\nError: Invalid Path\n"<<endl;
                return "";
            } else {
                return "\nError: Invalid Path\n\n";
            }
        } else {
            curr_dir = new_dir;
        }
    } else if(arguments[0] == "6"){
        //Make New Directory
        if(mode == 1){
            cout << "Enter the name of the folder (Invalid Characters : |,& or a comma): ";
            cin.clear();
            fflush(stdin);
            getline(cin,fname,'\n');
            arguments.push_back(fname);
        } else {
            if(arguments[1].empty()){
                return "\nError: Invalid File Name\n";
            } else {
                fname = arguments[1];
            }
        }
        
        if(!validate_file_name(fname)){
            if(mode == 1){
                cout << "\nError: Invalid File Name"<<endl;
                return "";
            } else {
                return "\nError: Invalid File Name\n";
            }
        }
        
        
        string req = create_request(arguments);
        string result = send_request(req);
        cout << result << endl;
    } else if(arguments[0] == "7"){
        string fname,dir_path;
        if(mode == 1){
            cout << "Enter the name of the file you want to move (Invalid Characters : |,& or a comma): ";
            cin.clear();
            fflush(stdin);
            getline(cin,fname,'\n');
            arguments.push_back(fname);
            cout << "Enter the absolute path of the target folder (Invalid Characters : |,& or a comma): ";
            cin.clear();
            fflush(stdin);
            getline(cin,dir_path,'\n');
            arguments.push_back(dir_path);
        } else {
            if(arguments[1].empty() || arguments[2].empty()){
                return "\nError: Invalid File Name\n";
            } else {
                fname = arguments[1];
                dir_path = arguments[2];
            }
        }
        if(!validate_file_name(fname) || !validate_file_name(dir_path)){
            if(mode == 1){
                cout << "\nError: Invalid File Name"<<endl;
                return "";
            } else {
                return "\nError: Invalid File Name\n";
            }
        }
        string req = create_request(arguments);
        string result = send_request(req);
        cout << result << endl;
    } else if(arguments[0] == "-1"){
        cout << "\nExiting...\n";
        return "";
    }
    return "";
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
}

*/





int main(int argc,char * argv[]){
    if(argc > 1){
        //Create threads if arguments are passed
        // filesystem.path = "root/";
        // int thread_count = argc-1;
        // thread threads[thread_count];

        // for(int i = 0;i < thread_count;i++){
        //     string path = argv[i + 1];
        //     cout << path;
        //     threads[i] = thread(process_script,path);
        // }

        // for(int i = 0;i < thread_count;i++){
        //     threads[i].join();
        // }
    } else {
        

        
        vector<string> arguments;
       
        cout << "Enter the server IP: ";
        getline(cin,server_ip,'\n');
        while(sentinel != "-1"){
            
            cout << "\nEnter the number written corresponding to the action to move further"<<endl;
            cout << "Current Path: " << path << endl;
            cout << "1\t-> Create File"<<endl;
            cout << "2\t-> Delete File"<<endl;
            cout << "3\t-> Open File"<<endl;
            cout << "4\t-> View Map"<<endl;
            cout << "5\t-> Change Directory"<< " Current: "<< path << endl;
            cout << "6\t-> Make Directory"<<endl;
            cout << "7\t-> Change File Location"<<endl;
            cout << "-1\t-> Quit"<<endl;
            cout << "Enter an action: ";
            cin.clear();
            getline(cin,sentinel,'\n');
            cin.clear();



            if(sentinel != "-1"){
                
                arguments.clear();
                arguments.push_back(sentinel);
                call_disk_functions(arguments,1);
            } else{
                cout << "\nExiting...\n";
            }

            
        }
    }
    return 0;
}
