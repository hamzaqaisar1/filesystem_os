#ifndef FILE_H
#define FILE_H
#include <string>
#include <vector>
#include <map>
#include <mutex>

using namespace std;
class Disk;
class File {
    private:
        string data;
        mutex * file_lock;
    public: 
        File();
        File(string name,int id);
        string name;
        int id;
        string get_data();
        void set_data(string data);

        //Utility Functions
        void update_file(multimap<int,pair<string,vector<int>>>& metadata);

        //Project Implementation
        int write_to_file(Disk& disk,string text,int mode = 0);
        int write_to_file(Disk& disk,int write_at,string text);

        string read_from_file();
        string read_from_file(int start,int size);

        int move_within_file(Disk& disk,int start,int size,int target);

        int truncate_file(Disk& disk,int max_size);
};
#endif


