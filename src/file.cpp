#include <cstdint>
#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <mutex>
#include "../include/filesystem/file.h"
#include "disk.h"


string File::get_data(){
    return this->data;
}

void File::set_data(string data){
    this->data = data;
}

File::File(){
    this->name = "";
    this->id = -1;
    this->file_lock = new mutex();
}
File::File(string name,int id){
    this->name = name;
    this->id = id;
    this->file_lock = new mutex();

}

void File::update_file(multimap<int,pair<string,vector<int>>>& metadata){
    this->file_lock->lock();
    auto file_data = metadata.find(this->id);
    this->file_lock->unlock();
    vector<int> file_segments;
    if(file_data != metadata.end()){
        file_segments = file_data->second.second;
    }

    fstream fin("../file_system.txt",ios::in);

    string contents = "";

    for (auto i = file_segments.begin(); i != file_segments.end(); ++i){
        fin.seekg(2002 + ((*i) * 101));
        string buffer = "";
        getline(fin,buffer,'\n');
        contents += buffer;
    }

    fin.close();
    
    this->set_data(contents);

}

int File::write_to_file(Disk& disk,string text,int mode){
    if(mode == 1){
        write_to_file(disk,this->data.length() - 1,text);
        return 0;
    }
    vector<int> free_segments = disk.free_segments;
    multimap<int,pair<string,vector<int>>> metadata = disk.get_file_metadata();
    vector<string> splits;
    auto file_data = metadata.find(this->id);
    vector<int> file_segments;
    
    if(file_data != metadata.end()){

        if(!file_data->second.second.empty()){
            free_segments.insert( free_segments.end(), file_data->second.second.begin(), file_data->second.second.end() );
        }
        size_t file_length = text.length();
        if(free_segments.size() * 100 <  file_length){
            return -2;
        }
        if(file_length > 100){
            for (unsigned i = 0; i < file_length; i += 100) {
                if(file_length % 100 != 0 && i > (file_length - 100)){
                    splits.push_back(text.substr(i,file_length % 100));
                } else {
                    splits.push_back(text.substr(i,100));
                }
                file_segments.push_back(free_segments[0]);
                free_segments.erase(free_segments.begin());

            }
        } else {
            splits.push_back(text);
            file_segments.push_back(free_segments[0]);
            free_segments.erase(free_segments.begin());
        }   

        disk.free_segments = free_segments;
        file_data->second.second = file_segments;

        string new_metadata = disk.set_file_metadata(metadata);

        fstream fout;
        fout.open("../file_system.txt");

        if(new_metadata != "-1"){
            fout << new_metadata;
        } else {
            return -1;
        }



        for(auto i = file_segments.begin(); i != file_segments.end(); ++i){
            fout.seekg(2002 + ((*i) * 101));
            string curr_string  = splits[0];
            curr_string.push_back('\n');
            curr_string.resize(101);
            fout << curr_string;
            splits.erase(splits.begin());
        }
        fout.close();
        this->data = text;
    }
    return 0;
}

int File::write_to_file(Disk& disk,int write_at,string text){
    multimap<int,pair<string,vector<int>>> metadata = disk.get_file_metadata();
    auto file_data = metadata.find(this->id);
    vector<int> file_segments;
    if(file_data != metadata.end()){
        file_segments = file_data->second.second;
    }

    vector<int> free_segments = disk.free_segments;
    vector<string> splits;
    string curr_text = this->get_data();
    string new_text = this->get_data().insert(write_at,text);
    new_text.resize(write_at+ text.length());

    int segment_number = write_at/100;


    for(auto i = (segment_number == 0 ? file_segments.begin() :  file_segments.begin() + segment_number); i != file_segments.end();++i){
        free_segments.push_back(*i);
    } 
    file_segments.erase(file_segments.begin() + segment_number,file_segments.end());
    sort(free_segments.begin(),free_segments.end());

    if(free_segments.size() * 100 <  new_text.length()){
        return -2;
    } else {
        this->set_data(new_text);
    }

    size_t file_length = new_text.length();
    if(file_length > 100){
        for (unsigned i = (segment_number * 100); i < file_length; i += 100) {
            if(file_length % 100 != 0 && i > (file_length - 100)){
                splits.push_back(new_text.substr(i,file_length % 100));
            } else {
                splits.push_back(new_text.substr(i,100));
            }
            file_segments.push_back(free_segments[0]);
            free_segments.erase(free_segments.begin());
        };
    } else {
        splits.push_back(new_text);
        file_segments.push_back(free_segments[0]);
        free_segments.erase(free_segments.begin());
    }

    disk.free_segments = free_segments;
    file_data->second.second = file_segments;

    string new_metadata = disk.set_file_metadata(metadata);

    fstream fout;
    fout.open("../file_system.txt");

    if(new_metadata != "-1"){
        fout << new_metadata;
    } else {
        return -1;
    }

    for (auto i = file_segments.begin(); i != file_segments.end(); ++i){
        fout.seekg(2002 + ((*i) * 101));
        string curr_string  = splits[0];
        curr_string.push_back('\n');
        curr_string.resize(101);
        fout << curr_string;
        splits.erase(splits.begin());
    }
    fout.close();
    return 0;
}

string File::read_from_file(){
    return this->data;
    
}

string File::read_from_file(int start,int size){
    return this->data.substr(start,size);
}

int File::truncate_file(Disk& disk,int max_size){
    multimap<int,pair<string,vector<int>>> metadata = disk.get_file_metadata();
    auto file_data = metadata.find(this->id);
    vector<int> file_segments;
    if(file_data != metadata.end()){
        file_segments = file_data->second.second;
    }
    vector<int> free_segments = disk.free_segments;
    vector<string> splits;



    this->set_data(this->get_data().erase(max_size));
    string new_text = this->get_data();

    int segmetns_to_remove = (max_size/100) + 1;
    int segment_number = max_size/100;

    for(auto i = (file_segments.begin() + segmetns_to_remove); i != file_segments.end();++i){
        free_segments.push_back(*i);
        file_segments.erase(i);
        if(file_segments.size() == 0){
            break;
        }
    }    
    
    sort(free_segments.begin(),free_segments.end());

    size_t file_length = new_text.length();
    disk.free_segments = free_segments;
    file_data->second.second = file_segments;

    string new_metadata = disk.set_file_metadata(metadata);

    fstream fout;
    fout.open("../file_system.txt");

    if(new_metadata != "-1"){
        fout << new_metadata;
    } else {
        return -1;
    }

    string last_segment_data = new_text.substr(segment_number * 100,max_size % 100);

    fout.seekg(2002 + ((segment_number) * 101));
    last_segment_data.push_back('\n');
    last_segment_data.resize(101);
    fout << last_segment_data;
    this->set_data(new_text);
    fout.close();
    return 0;
}

int File::move_within_file(Disk& disk,int start,int size,int target){
    string contents = this->data;
    string src = contents.substr(start,size);
    string dest = contents.substr(target,size);
    contents.replace(target,size,src);
    contents.replace(start,size,dest);

    int result = this->write_to_file(disk,contents);
    return result;
}



