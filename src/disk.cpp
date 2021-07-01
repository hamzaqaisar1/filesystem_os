
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <sched.h>
#include <string>
#include <sstream>
#include <iterator>
#include <map>
#include <fstream> 
#include <sstream>
#include <mutex>
#include <vector>
#include "disk.h"
#include "../include/filesystem/file.h"

using namespace std;


/*
File Delimeters:

Metadata Entry: \n 
File Info Element: &
Segment Separator: ,
FileSystem Starts After: |
FilesSeparator: \n*/
mutex data_lock;

multimap<int,tuple<string,vector<int>,vector<int>>> Disk::get_dir_metadata(){
	return this->dir_metadata;
}   

void Disk::print_dir_metadata(){
    multimap<int,tuple<string,vector<int>,vector<int>>>dir_metadata = this->dir_metadata;
    for(auto i = dir_metadata.begin();i != dir_metadata.end();i++){
        cout << i->first << " : " << get<0>(i->second) << "&";
        for(int j = 0;j < get<1>(i->second).size();j++){
            cout<<get<1>(i->second)[j] << ",";
        }
        cout << "&";
        for(int j = 0;j < get<2>(i->second).size();j++){
            cout<<get<2>(i->second)[j] << ",";
        }
        cout << "&";
    }
}

void Disk::print_metadata(){
    multimap<int,pair<string,vector<int>>>metadata = this->metadata;
    for(auto i = metadata.begin();i != metadata.end();i++){
        cout << i->first << " : " << i->second.first << endl;
    }
}


string Disk::set_dir_metadata(multimap<int,tuple<string,vector<int>,vector<int>>> data){
	string new_dir_metadata = "";
	multimap<int,tuple<string,vector<int>,vector<int>>>::iterator itr; 
	for (itr = data.begin(); itr != data.end(); ++itr) { 
		string sfile_segments;
		ostringstream out;
		vector<int> dirs = get<1>(itr->second);
		vector<int> files = get<2>(itr->second);
		

		if (!dirs.empty()){
			copy(dirs.begin(), dirs.end() - 1,ostream_iterator<int>(out, ","));
			out << dirs.back();
		}

		out << ",&";

		if(!files.empty()){
			copy(files.begin(), files.end() - 1,ostream_iterator<int>(out, ","));
			out << files.back();
		}


		new_dir_metadata += (to_string(itr->first) + "&" + get<0>(itr->second) +"&" +out.str()+",&\n");
	}
	if(new_dir_metadata.length() > this->meta_data_limit){
		return "-1";
	}
	this->dir_metadata = data;
	new_dir_metadata.push_back('|');

	new_dir_metadata.resize(1001);

	return new_dir_metadata;
}

//Finding the specific directory entry from the absolute path
int Disk::find_dir_id(int dir_id,vector<string> path){
	auto curr_dir= this->dir_metadata.find(dir_id);
	vector<string> curr_path = Disk::parse_path(this->path);
	bool found = false;
	int i;
	if(path[0] == ".."){
		i  = 0;
	} else {
		i = 1;
	}
	for(;i < path.size();i++){
		if(path[i] == ".." && curr_path.size() > 1){
			path[i] = curr_path[curr_path.size() - 2];
			for(auto j = dir_metadata.begin();j != dir_metadata.end();j++){
				if(get<0>(j->second) ==  curr_path[curr_path.size() - 2] && 
				find(get<1>(j->second).begin(),get<1>(j->second).end(),dir_id) != get<1>(j->second).end()){
					curr_dir = j;
					found = true;
					break;
				} else {
					found = false;
				}
			}
			curr_path.erase(curr_path.end() - 1);
		} else if(path[i] != "..") {
			cout << path[i] << endl << curr_dir->first << endl;
			vector<int> dir_list = get<1>(curr_dir->second);
			cout << dir_list.size() << endl;
			for(int j = 0;j < dir_list.size();j++){
				auto temp = this->dir_metadata.find(dir_list[j]);
				cout << dir_list[j] << endl;
				if(get<0>(temp->second) == path[i]){
					curr_dir = temp;
					found = true;
					break;
				} else {
					found = false;
				}
			}
		}
		if(found){
			continue;
		} else {
			break;
		}
		
	}
	if(found){
		return curr_dir->first;
	} else {
		return -1;
	}
}

multimap<int,pair<string,vector<int>>> Disk::get_file_metadata(){
	return this->metadata;
}

string Disk::set_file_metadata(multimap<int,pair<string,vector<int>>> data){
	string new_metadata = "";
	multimap<int,pair<string,vector<int>>>::iterator itr; 
	for (itr = data.begin(); itr != data.end(); ++itr) { 
		string sfile_segments;
		ostringstream out;

		if (!itr->second.second.empty())
		{
			copy(itr->second.second.begin(), itr->second.second.end() - 1,ostream_iterator<int>(out, ","));
			out << itr->second.second.back();
		}


		new_metadata += (to_string(itr->first) +"&"  + itr->second.first + "&" +out.str()+",&\n");
	}
	if(new_metadata.length() > this->meta_data_limit){
		return "-1";
	}
	this->metadata = data;
	new_metadata.push_back('|');

	new_metadata.resize(1001);

	return new_metadata;
}

vector<string> Disk::parse_path(string path){
	size_t pos = 0;
	vector<string> dir_list;
	string token;
	string delimiter = "/";
	while ((pos = path.find(delimiter)) != string::npos) {
		token = path.substr(0, pos);
		if(token.empty()){
			break;
		}
		dir_list.push_back(token);
		path.erase(0,pos + delimiter.size());
	}

	return dir_list;
}

void Disk::update_metadata(){
	//Attempting to read the file metadata
	multimap<int,pair<string,vector<int>>> data;
	multimap<int,tuple<string,vector<int>,vector<int>>> dir_metadata;
	vector<int> occupied_segments,free_segments,total_segments;
	fstream fin("../file_system.txt",ios::in);
	string metadata,field,s_dir_metadata;

	for(int i = 0;i < 144;i++){
		total_segments.push_back(i);
	}

	getline(fin,metadata,'|');

	fin.seekg(1001);

	getline(fin,s_dir_metadata,'|');


	if(metadata.length() != 0){

		stringstream buffer(metadata);

		while(getline(buffer, field, '\n')){
			vector<int> segment_array = {};

			string id ="",name="",segments="";


			stringstream field_buffer(field);


			getline(field_buffer,id,'&');

			getline(field_buffer,name,'&');

			getline(field_buffer,segments,'&');


			size_t pos = 0;
			string token;
			string delimiter = ",";
			while ((pos = segments.find(delimiter)) != string::npos) {
				token = segments.substr(0, pos);
				if(token.empty()){
					break;
				}
				segment_array.push_back(stoi(token));
				occupied_segments.push_back(stoi(token));
				segments.erase(0, pos + delimiter.length());
			}
			
			
			
			data.insert(pair<int,pair<string,vector<int>>>(stoi(id),pair<string,vector<int>>(name,segment_array)));
			this->disk_lock->lock();
			if(this->metadata.find(stoi(id)) == this->metadata.end()){
				++Disk::total_files;
			}
			this->disk_lock->unlock();
		}
	}
   




	if(s_dir_metadata.length() != 0){
		stringstream dir_buffer(s_dir_metadata);
			while(getline(dir_buffer, field, '\n') && s_dir_metadata.length() > 0){
			vector<int> file_array, dir_array;

			string id,name ="",dirs="",files="";


			stringstream field_buffer(field);



			getline(field_buffer,id,'&');



			getline(field_buffer,name,'&');

			getline(field_buffer,dirs,'&');

			getline(field_buffer,files,'&');


			size_t pos = 0;
			string token;
			string delimiter = ",";
			while ((pos = dirs.find(delimiter)) != string::npos) {
				token = dirs.substr(0, pos);
				if(token.empty()){
					break;
				}
				dir_array.push_back(stoi(token));
				dirs.erase(0, pos + delimiter.length());
			}
			pos = 0;
			token="";
			while ((pos = files.find(delimiter)) != string::npos) {
				token = files.substr(0, pos);
				if(token.empty()){
					break;
				}
				file_array.push_back(stoi(token));
				files.erase(0, pos + delimiter.length());
			}
			
			dir_metadata.insert(pair<int,tuple<string,vector<int>,vector<int>>>(stoi(id),tuple<string,vector<int>,vector<int>>(name,dir_array,file_array)));
			this->disk_lock->lock();
			if(this->dir_metadata.find(stoi(id)) == this->dir_metadata.end()){
				++Disk::total_folders;
			}
			this->disk_lock->unlock();
		}
	}

	//Tinkering out the free segments from the occupied one
	sort(total_segments.begin(), total_segments.end());
	sort(occupied_segments.begin(), occupied_segments.end());

	set_symmetric_difference(total_segments.begin(), total_segments.end(), 
							occupied_segments.begin(), occupied_segments.end(), 
								back_inserter(free_segments));
	sort(free_segments.begin(), free_segments.end()); 

	//Setting values
	this->disk_lock->lock();
	this->free_segments = free_segments;
	this->metadata = data;
	this->dir_metadata = dir_metadata;
	this->disk_lock->unlock();
	fin.close();
}


Disk::Disk(int meta_data_limit){
	this->disk_lock = new mutex();
	this->meta_data_limit = meta_data_limit;
	multimap<int,pair<string,vector<int>>> data;
	multimap<int,tuple<string,vector<int>,vector<int>>> dir_metadata;
	vector<int> occupied_segments,free_segments,total_segments;
	fstream fin("../file_system.txt",ios::in);
	ofstream fout;
	string metadata,field,s_dir_metadata;
	int file_count = 0;
	
	for(int i = 0;i < 144;i++){
		total_segments.push_back(i);
	}


	if (fin.fail()) {

		vector<int> root_files;
		vector<int> root_dirs;
		fout.open("../file_system.txt",ios::app); 
		string s_file_metadata =this->set_file_metadata(data);
		fout << s_file_metadata;
		dir_metadata.insert(pair<int,tuple<string,vector<int>,vector<int>>>(0,tuple<string,vector<int>,vector<int>>("root",root_dirs,root_files)));

		string s_dir_metadata = this->set_dir_metadata(dir_metadata);
		fout.seekp(1001);
		fout << s_dir_metadata;
		this->free_segments = total_segments;
		this->path = "root/";
		fin.close();
		fout.close();
	} else  {

		//Attempting to read the file metadata
		getline(fin,metadata,'|');

		fin.seekg(1001);
		getline(fin,s_dir_metadata,'|');

		fin.close();

		if(metadata.length() != 0){

			stringstream buffer(metadata);

			while(getline(buffer, field, '\n')){
				vector<int> segment_array = {};

				string id ="",name="",segments="";

				file_count += 1;

				stringstream field_buffer(field);


				getline(field_buffer,id,'&');

				getline(field_buffer,name,'&');

				getline(field_buffer,segments,'&');


				size_t pos = 0;
				string token;
				string delimiter = ",";
				while ((pos = segments.find(delimiter)) != string::npos) {
					token = segments.substr(0, pos);
					if(token.empty()){
						break;
					}
					segment_array.push_back(stoi(token));
					occupied_segments.push_back(stoi(token));
					segments.erase(0, pos + delimiter.length());

				}
				
				data.insert(pair<int,pair<string,vector<int>>>(stoi(id),pair<string,vector<int>>(name,segment_array)));
				++Disk::total_files;
			}
		}
		//Attempting to read the folder metadata 
		


		if(s_dir_metadata.length() != 0){
			stringstream dir_buffer(s_dir_metadata);
			 while(getline(dir_buffer, field, '\n') && s_dir_metadata.length() > 0){
				vector<int> file_array, dir_array;

				string id,name ="",dirs="",files="";


				stringstream field_buffer(field);



				getline(field_buffer,id,'&');



				getline(field_buffer,name,'&');

				getline(field_buffer,dirs,'&');

				getline(field_buffer,files,'&');


				size_t pos = 0;
				string token;
				string delimiter = ",";
				while ((pos = dirs.find(delimiter)) != string::npos) {
					token = dirs.substr(0, pos);
					if(token.empty()){
						break;
					}
					dir_array.push_back(stoi(token));
					dirs.erase(0, pos + delimiter.length());
				}
				pos = 0;
				token="";
				while ((pos = files.find(delimiter)) != string::npos) {
					token = files.substr(0, pos);
					if(token.empty()){
						break;
					}
					file_array.push_back(stoi(token));
					files.erase(0, pos + delimiter.length());
				}
				
				dir_metadata.insert(pair<int,tuple<string,vector<int>,vector<int>>>(stoi(id),tuple<string,vector<int>,vector<int>>(name,dir_array,file_array)));
				++Disk::total_folders;
			}

		}

		//Tinkering out the free segments from the occupied one
		sort(total_segments.begin(), total_segments.end());
		sort(occupied_segments.begin(), occupied_segments.end());

		set_symmetric_difference(total_segments.begin(), total_segments.end(), 
								occupied_segments.begin(), occupied_segments.end(), 
									back_inserter(free_segments));
		sort(free_segments.begin(), free_segments.end()); 

		//Setting values
		this->free_segments = free_segments;
		this->metadata = data;
		this->dir_metadata = dir_metadata;
		this->path = "root/";

		fout.close();


		// Commented out writing back to the file
		// string new_metadata = this->set_file_metadata(data);
		// if(new_metadata != "-1" && metadata.empty() != 0){
		//     fstream fout;
		//     fout.open("../file_system.txt");

		//     fout << new_metadata;
		// }
	}
}

int Disk::create(string fname,int curr_dir){
	this->disk_lock->lock();
	multimap<int,pair<string,vector<int>>> metadata = this->get_file_metadata();
	multimap<int,tuple<string,vector<int>,vector<int>>> dir_metadata =this->get_dir_metadata();
	this->disk_lock->unlock();

	fstream fout;
	fout.open("../file_system.txt");
	
	string new_metadata = "";
	vector<int> file_segments;

	this->disk_lock->lock();
	++Disk::total_files;
	this->disk_lock->unlock();




	pair<string,vector<int>> value(fname,file_segments);
	metadata.insert(pair<int,pair<string,vector<int>>>(Disk::total_files,value));

	this->disk_lock->lock();
	new_metadata = this->set_file_metadata(metadata);
	this->disk_lock->unlock();

	if(new_metadata != "-1"){
		this->disk_lock->lock();
		fout << new_metadata;
		this->disk_lock->unlock();
		map<int,tuple<string,vector<int>,vector<int>>>::iterator dir = dir_metadata.find(curr_dir);

		get<2>(dir->second).push_back(Disk::total_files);

		this->disk_lock->lock();
		string s_dir_metadata = this->set_dir_metadata(dir_metadata);

		fout.seekg(1001);
		fout << s_dir_metadata;
		this->disk_lock->unlock();
		fout.close();
		return 0;
	} else {
		this->disk_lock->lock();
		--Disk::total_files;
		this->disk_lock->unlock();

		return -1;
	}
}

int Disk::del(string fname,int id,int dir_id){
	this->disk_lock->lock();
	multimap<int,pair<string,vector<int>>> metadata = this->get_file_metadata();
	multimap<int,tuple<string,vector<int>,vector<int>>> dir_metadata =this->get_dir_metadata();
	this->disk_lock->unlock();


	vector<string> dir_list = this->parse_path(fname);
	

	vector<int> free_segments = this->free_segments;
	auto file = metadata.find(id);
	vector<int> file_segments;

	if(file != metadata.end()){
		file_segments = file->second.second;
	}

	this->disk_lock->lock();
	free_segments.insert( free_segments.end(), file_segments.begin(), file_segments.end() );
	sort(free_segments.begin(), free_segments.end()); 

	metadata.erase(file);

	
	string new_metadata = this->set_file_metadata(metadata);
	this->disk_lock->unlock();

	if(new_metadata != "-1"){
		fstream fout;
		fout.open("../file_system.txt");

		this->disk_lock->lock();
		fout << new_metadata;
		this->disk_lock->unlock();
		map<int,tuple<string,vector<int>,vector<int>>>::iterator dir = dir_metadata.find(dir_id);

		fout.seekg(1001);

		this->disk_lock->lock();
		string s_dir_metadata = this->set_dir_metadata(dir_metadata);
		--Disk::total_files;
		fout << s_dir_metadata;
		this->disk_lock->unlock();
		fout.close();

		return 0;
	} else {
		return -1;
	}
}



File Disk::open(string fname,int id){
	this->disk_lock->lock();
	auto file_data = this->metadata.find(id);
	this->disk_lock->unlock();
	vector<int> file_segments;
	if(file_data != this->metadata.end()){
		file_segments = file_data->second.second;
	}

	fstream fin("../file_system.txt",ios::in);

	string contents = "";

	for (auto i = file_segments.begin(); i != file_segments.end() && !file_segments.empty(); ++i){
		fin.seekg(2002 + ((*i) * 101));
		string buffer = "";
		getline(fin,buffer,'\n');
		contents += buffer;
	}
	fin.close();
	
	File req_file = File(fname,id);
	req_file.set_data(contents);

	return req_file;

}

void Disk::close(string fname,int id){
	cout << "Closing File: " + fname;
}



string Disk::memory_map(int id,int level){
	if(!this->dir_metadata.empty()){
		multimap<int,pair<string,vector<int>>>::iterator itr;
		this->disk_lock->lock();
		auto curr_dir = this->dir_metadata.find(id);
		this->disk_lock->unlock();
		string result = "";
		result += "|\n|-----";
		for(int i = 1;i < level;i++){
			result += "------";
		}
		result += get<0>(curr_dir->second) + "\n|";
		
		for(auto i = get<1>(curr_dir->second).begin();i != get<1>(curr_dir->second).end() && !get<1>(curr_dir->second).empty(); i++){
			result += this->memory_map(*i,level + 1);
		}
		for(auto i = get<2>(curr_dir->second).begin();i != get<2>(curr_dir->second).end() && !get<2>(curr_dir->second).empty(); i++){
			auto itr = this->metadata.find(*i);
			ostringstream out;

			if (!itr->second.second.empty()){
				copy(itr->second.second.begin(), itr->second.second.end() - 1,ostream_iterator<int>(out, ","));
				out << itr->second.second.back();
			}

			result += "|\n|-----";
			for(int i = 0;i < level;i++){
				result+= "------";
			}
			result += itr->second.first + ", Segments -> " + out.str() + "\n|";
			// cout << itr->second.first + ", Segments -> " + out.str() + "\n|";
		} 
		return result;
	} else {
		return  "No file found\n";
	}
}

int Disk::mkdir(string dir_name,int curr_dir){
	multimap<int,tuple<string,vector<int>,vector<int>>> dir_metadata = this->dir_metadata;
	auto dir = dir_metadata.find(curr_dir);

	//Checking for a duplicate folder name
	for(auto i = get<1>(dir->second).begin(); i != get<1>(dir->second).end(); i++){
		string name = get<0>(dir_metadata.find(*i)->second);
		if(name == dir_name){
			return -1;
		}   
	}
	vector<int> dir_list,file_list;
	++Disk::total_folders;
	get<1>(dir->second).push_back(Disk::total_folders);
	dir_metadata.insert(pair<int,tuple<string,vector<int>,vector<int>>>(Disk::total_folders,tuple<string,vector<int>,vector<int>>(dir_name,dir_list,file_list)));
	string s_dir_metadata = this->set_dir_metadata(dir_metadata);
	fstream fout;
	fout.open("../file_system.txt");
	fout.seekg(1001);
	fout << s_dir_metadata;

	fout.close();
	return 0;
}

//Opening a directory 
int Disk::chdir(string path,int curr_dir){
	//Finding the directory
	this->disk_lock->lock();
	multimap<int,tuple<string,vector<int>,vector<int>>> dir_metadata = this->dir_metadata;
	this->disk_lock->unlock();

	vector<string> curr_path = Disk::parse_path(this->path);
	int dir_id;
	string new_path;
	vector<string> parsed_path = Disk::parse_path(path + "/");
	if(path == "root" || path == "root/"){
		this->path = "root/";
		return 0;
	} else if (path.rfind("../",0)){
		dir_id = curr_dir;
		new_path = this->path;
		for(int i = 1;i < parsed_path.size();i++){
			new_path += parsed_path[i] + "/";
		}
		cout << new_path << endl;
	} else if(path.rfind(".../",0)) {
		dir_id = curr_dir;
		int i;
		for(i = 0;i < parsed_path.size();i++){
			if(parsed_path[i] == ".."){
				curr_path.erase(curr_path.end() - 1);
			} else {
				break;
			}
		}
		for(int j = 0;j < curr_path.size();j++){
			new_path += curr_path[j] + "/";
		}
		for(;i < parsed_path.size();i++){
			new_path += parsed_path[i] + "/";
		}
	} else if(parsed_path[0] == "root" && parsed_path.size() > 1){
		dir_id = 0;
	}
	int target_dir_id = Disk::find_dir_id(dir_id,parsed_path);
	cout << "Tar_Dir_ID: " + to_string(target_dir_id) << "\nNew Path:" << new_path << endl;

	if(target_dir_id == -1){
		return -1;
	} else {
		this->path = new_path;
		return target_dir_id;
	}
}




int Disk::move(int file_id,string path,int curr_dir){
	vector<string> parsed_path = Disk::parse_path(path);
	int target_dir_id = Disk::find_dir_id(0,parsed_path);
	
	this->disk_lock->lock();
	multimap<int,tuple<string,vector<int>,vector<int>>> dir_metadata = this->dir_metadata;
	this->disk_lock->unlock();
	multimap<int,tuple<string,vector<int>,vector<int>>>::iterator target_dir_entry = dir_metadata.find(target_dir_id);
	multimap<int,tuple<string,vector<int>,vector<int>>>::iterator curr_dir_entry = dir_metadata.find(curr_dir);


	vector<int> file_list = get<2>(target_dir_entry->second);
	vector<int> curr_dir_file_list =  get<2>(curr_dir_entry->second);

	if(find(file_list.begin(), file_list.end(), file_id) != file_list.end() || 
		find(curr_dir_file_list.begin(), curr_dir_file_list.end(), file_id) == curr_dir_file_list.end()){
		return -1;
	}

	this->disk_lock->lock();
	get<2>(target_dir_entry->second).push_back(file_id);


	string s_dir_metadata = this->set_dir_metadata(dir_metadata);
	this->disk_lock->unlock();
	fstream fout;
	fout.open("../file_system.txt");

	fout.seekg(1001);
	this->disk_lock->lock();
	fout << s_dir_metadata;
	this->disk_lock->unlock();
	fout.close();
	return 0;

}




