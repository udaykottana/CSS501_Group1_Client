#include <iostream>
#include <unordered_map>
#include <string>
#include <fstream>
#include <msgpack.hpp>
#include <iomanip>
#include <sys/wait.h>
#include <cstdlib>
#include <dirent.h>
#include "rpc/client.h"

namespace FSS_Client {
    class File
    {
    public:
        // data members of a File type, adjust according to your needs
        std::string name, file_id, author, location_on_disc, last_update_time, access_to;
        size_t size;
        unsigned int num_downloads;

        // constructor to the File Class
        File()
        {
            this->access_to = "*";
            this->author = "none";
            this->file_id = "0";
            this->last_update_time = "0";
            this->location_on_disc = "null";
            this->name = "no_file";
            this->num_downloads = 0;
            this->size = 0;
        }
        File(const std::string name, const std::string file_id, const std::string author, const std::string location_on_disc, const std::string last_update_time, const size_t size, const unsigned int num_downloads, std::string access_to)
        {
            this->access_to = access_to;
            this->author = author;
            this->file_id = file_id;
            this->last_update_time = last_update_time;
            this->location_on_disc = location_on_disc;
            this->name = name;
            this->num_downloads = num_downloads;
            this->size = size;
        }
        MSGPACK_DEFINE(name, file_id, author, location_on_disc, last_update_time, size, num_downloads, access_to);
    };

    std::vector<std::string> split(std::string s, std::string delimiter)
    {
        size_t pos_start = 0, pos_end, delim_len = delimiter.length();
        std::string token;
        std::vector<std::string> res;

        while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
        {
            token = s.substr(pos_start, pos_end - pos_start);
            pos_start = pos_end + delim_len;
            res.push_back(token);
        }

        res.push_back(s.substr(pos_start));
        return res;
    }

    class Client
    {
    private:
        // data members
        std::string user_id;
        bool is_signedin = false;
        // vector<string> server_ips; // list of server ips
        std::unordered_map<std::string, double> download_status; // download_status of files; file_id -> int percentage

        rpc::client *client;
        std::unordered_map<std::string, File> download_list;
        std::unordered_set<std::string> downloaded_files;

        // private helper functions
        void __draw_init_pattern();
        
        // contributed by @ajay
        std::string __getFileContent(const std::string &filepath, std::size_t &fileSize);
        
        // contributed by @ajay
        void __upload_file(std::string permissions, std::string path);
        
        // contributed by @uday
        void __download_file(std::string file_id);

        // function to handle resumption of uploads
        void __resumeUpload(std::string file_id, std::string file_name, std::string permissions, std::pair<bool, std::vector<std::string>>& uploaded_chunks);

        // function to show files to the user with the list of files on the server
        // contributed by @rudra
        void __view_files();
    public:
        Client(rpc::client &client);
        
        ~Client();
        
        // contributed by @ajay
        void upload();
        
        // contributed by @uday
        void download();

        // login function
        void login();
        
        // signup function
        void signup();

        // function to show up the program menu
        void init();
    };
}

void FSS_Client::Client::__draw_init_pattern()
{
    std::cout << "_____  _  _         ____   _                   _                 ____               _                   " << std::endl
            << "|  ___|(_)| |  ___  / ___| | |__    __ _  _ __ (_) _ __    __ _  / ___|  _   _  ___ | |_  ___  _ __ ___  " << std::endl
            << "| |_   | || | / _ \\ \\___ \\ | '_ \\  / _` || '__|| || '_ \\  / _` | \\___ \\ | | | |/ __|| __|/ _ \\| '_ ` _ \\ " << std::endl
            << "|  _|  | || ||  __/  ___) || | | || (_| || |   | || | | || (_| |  ___) || |_| |\\__ \\| |_|  __/| | | | | |" << std::endl
            << "|_|    |_||_| \\___| |____/ |_| |_| \\__,_||_|   |_||_| |_| \\__, | |____/  \\__, ||___/ \\__|\\___||_| |_| |_|" << std::endl
            << "                                                          |___/          |___/                           " << std::endl;
}

std::string FSS_Client::Client::__getFileContent(const std::string &filepath, std::size_t &fileSize)
{
    std::ifstream file(filepath, std::ios::binary);
    std::stringstream content;

    if (file.is_open())
    {
        file.seekg(0, std::ios::end);                      // Move to the end of the file
        fileSize = static_cast<std::size_t>(file.tellg()); // Get the file size
        file.seekg(0, std::ios::beg);                      // Move back to the beginning

        content << file.rdbuf();
        file.close();
    }
    else
    {
        throw std::runtime_error("Unable to open file: " + filepath);
    }

    return content.str();
}

void FSS_Client::Client::__upload_file(std::string file_id, std::string path)
{
    // upload a single file to a server
    size_t size_of_file;
    // supply name, author, size, content and permissions to the API Call
    std::string content = __getFileContent(path, size_of_file);
    
    // get the filename from the chunk
    auto splitted_path = split(path, "/");

    // make an RPC Call to upload the chunk
    client->call("upload", file_id, splitted_path.back(), content);
}

void FSS_Client::Client::__download_file(std::string file_id)
{
    // just download the file and return the data

    // check permissions if given or not
    // make an api call
    std::string file_content = client->call("download", file_id).as<std::string>();
    // write to disc in same folder /downloads/file_id
    std::cout << "Download in progress..." << std::endl;
    std::ofstream new_file("downloads/" + file_id + "-" + download_list[file_id].name);
    new_file << file_content;
    new_file.close();

    std::cout << "Download complete" << std::endl;
    downloaded_files.insert(file_id);
}

void FSS_Client::Client::__view_files()
{
    std::cout << ">>> Showing all files. " << std::endl;
    download_list = client->call("get_files_list").as<std::unordered_map<std::string, File>>();
    std::cout << "+---------------------+--------------+-----------+-----------------------+--------+----+------------+" << std::endl
            << "| " << std::left << std::setw(20) << "file_id" << std::setw(1) << "| "
            << std::setw(13) << "name" << std::setw(1) << "|"
            << std::setw(10) << "author" << std::setw(1) << "|"
            << std::setw(24) << "last_update_time" << std::setw(1) << "|"
            << std::setw(8) << "access?" << std::setw(1) << "|"
            << std::setw(4) << "size" << std::setw(1) << "|"
            << std::setw(12) << "downloaded?" << std::setw(1) << "|" << std::endl;

    for (auto &it : download_list)
    {
        std::string file_id = it.first;
        File val = it.second;
        std::vector<std::string> accesses = split(val.access_to, " ");
        bool access = (val.access_to == "*" or find(accesses.begin(), accesses.end(), user_id) != accesses.end());
        bool downloaded = downloaded_files.find(file_id) != downloaded_files.end();

        std::cout << "+---------------------+--------------+-----------+-----------------------+--------+----+------------+" << std::endl
                << "| " << std::left << std::setw(20) << file_id << std::setw(1) << "| "
                << std::setw(10) << val.name << std::setw(1) << "|"
                << std::setw(10) << val.author << std::setw(1) << "|"
                << std::setw(24) << val.last_update_time.substr(0, 24) << std::setw(1) << "|"
                << std::setw(8) << access << std::setw(1) << "|"
                << std::setw(4) << val.size << std::setw(1) << "|"
                << std::setw(12) << downloaded << std::setw(1) << "|" << std::endl
                << "+---------------------+--------------+-----------+-----------------------+--------+----+------------+" << std::endl;
    }
}

FSS_Client::Client::Client(rpc::client &client)
{
    this->client = &client;
    this->client->call("ping").as<bool>();
    // timeout is set to 60 sec
    this->client->set_timeout(60*1000);
}

FSS_Client::Client::~Client()
{
    this->user_id = "";
    this->is_signedin = false;
}

void FSS_Client::Client::__resumeUpload(std::string file_id, std::string file_name, std::string permissions, std::pair<bool, std::vector<std::string>>& uploaded_chunks) {
    // resume the upload from the point it was paused
    std::string new_path = "pending_uploads/"+file_name;
    std::cout << ">> Retrying Uploads..." << std::endl;

    DIR *d;
    struct dirent *dir;
    d = opendir(new_path.c_str());

    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0] == '.') continue;
            // do not upload if the chunk is already uploaded
            if(std::find(uploaded_chunks.second.begin(), uploaded_chunks.second.end(), dir->d_name) != uploaded_chunks.second.end()) continue;

            std::cout << ">> Uploading chunk: " << dir->d_name;
            __upload_file(file_id, new_path+"/"+dir->d_name);
            std::cout << "\t Uploaded " << std::endl;
        }
        closedir(d);
    }

    // when the upload is complete, remove all the chunks and also the directory
    auto isUploadComplete = this->client->call("finish-upload", file_id, file_name, user_id, permissions).as<bool>();
    if(isUploadComplete) {
        system(("rm -rf "+new_path).c_str());
    }

    std::cout << "\t  Uploaded Complete! " << std::endl;
}

void FSS_Client::Client::upload()
{
    // upload function to be called directly from frontend
    // these functions are directly called form UI so no arguments
    std::string path = "", permissions = "*";
    char access = 'y';
    // add appropriate data input like, file path and permissions
    std::cout << "**Uploading**\nUser Options::" << std::endl;
    std::cout << "\tPath to the file: \t";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, path);
    std::cout << "\tDo you want to grant access of this file to other users ([y]/n): ";

    std::cin >> access;
    if (access == 'y')
    {
        std::cout << "\t>>> Enter space seperated user names: ";
        std::getline(std::cin, permissions);
    }

    // split the file into many chunks
    // 1. locate files to a new directory of chunks
    std::cout << ">> Copying files to temporary disk..." << std::endl;

    std::string folder_path = "pending_uploads";
    
    // 2. create a new folder based on the file_name
    std::vector<std::string> splitted_path = FSS_Client::split(path, "/");
    std::string file_name = splitted_path.back();
    std::string new_path = folder_path + "/" + file_name;

    std::string file_id = this->client->call("start-upload", file_name, this->user_id).as<std::string>();
    
    system(("mkdir " + new_path).c_str());

    // 3. now copy the file
    system(("cp "+path + " " + new_path).c_str());
    
    std::cout << ">> Splitting files into chunks..." << std::endl;

    // 4. split the file into chunks of 1 MB each
    system(("cd " +new_path + " && split -b 1m --numeric-suffixes " + file_name).c_str());

    // 5. remove the parent file
    system(("rm "+new_path+"/"+file_name).c_str());

    // 6. get the list of files inside the directory
    std::cout << ">> Getting Ready for uploading all chunks..." << std::endl;

    DIR* d;
    d = opendir(new_path.c_str());
    if (d) {
        std::cout << "Found previously paused uploads. " << std::endl;
        auto uploaded_chunks = this->client->call("check-upload", file_id).as<std::pair<bool, std::vector<std::string>>>();
        if(uploaded_chunks.first) {
            __resumeUpload(file_id, file_name, permissions, uploaded_chunks);
            return;
        } 
        std::cout << "No records found on server, starting fresh uploads." << std::endl;
    }

    struct dirent *dir;
    d = opendir(new_path.c_str());
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0] == '.') continue;
            std::cout << ">> Uploading chunk: " << dir->d_name;
            __upload_file(file_id, new_path+"/"+dir->d_name);
            std::cout << "\t Uploaded " << std::endl;
        }
        closedir(d);
    }
    
    // when the upload is complete, remove all the chunks and also the directory
    auto isUploadComplete = this->client->call("finish-upload", file_id, file_name, user_id, permissions).as<bool>();
    if(isUploadComplete) {
        system(("rm -rf "+new_path).c_str());
    }

    std::cout << "\t  Uploaded Complete! " << std::endl;
}

void FSS_Client::Client::download()
{
    // directly called by the user
    // get the file_id from the user by using suitable prompts and options
    __view_files();

    std::cout << "\tInput your file_id: \t";
    std::string inputed_fileId;
    std::cin >> inputed_fileId;

    bool access = client->call("check_access", user_id, inputed_fileId).as<bool>();

    if (!access)
    {
        std::cout << "[Error]: No Access Permissions. try contacting the owner." << std::endl;
    }
    else
    {
        __download_file(inputed_fileId);
    }
}

void FSS_Client::Client::login()
{
    // login the user, and set user_id, and is_signedin=true
    std::string username, password;
    std::cout << ">> Enter your UserID: ";
    std::cin >> username;
    std::cout << ">> Enter your Password: ";
    std::cin >> password;
    bool result = client->call("signin", username, password).as<bool>();
    if (result)
        std::cout << "\n   Successful sign-in.\n"
                << std::endl
                << "Welcome, " << username << "!\n\n";
    else
        std::cout << "!  Incorrect UserID or Password.\n"
                << std::endl;

    if (result)
        user_id = username;
    is_signedin = result;
}

void FSS_Client::Client::signup()
{
    // register a new user; and set user_id and is_signedin=true
    std::string username, password, name;
    std::cout << "Enter your name: ";
    std::cin >> name;
    std::cout << "Enter your UserID: ";
    std::cin >> username;
    std::cout << "Enter your Password: ";
    std::cin >> password;

    bool result = client->call("register", name, username, password).as<bool>();
    if (result)
        std::cout << "Successful registration." << std::endl;
    else
        std::cout << "Invalid USerID/Password." << std::endl;
    if (result)
        user_id = username;
    is_signedin = result;
}

void FSS_Client::Client::init()
{
    // showup the menu, as soon as the object is constructed, this
    // function will be called and entered into a while loop until
    // the program is being run
    int choice;
    while (true)
    {
        if (!is_signedin)
        {
            __draw_init_pattern();
            std::cout << "Options:: " << std::endl
                    << std::endl
                    << "1. Login\n2. Register\n3. Exit\n\nPlease enter your choice : ";
            std::cin >> choice;
            std::cout << std::endl;
            switch (choice)
            {
            case 1:
                login();
                break;
            case 2:
                signup();
                break;
            case 3:
                std::cout << "**Thanks for using our system**" << std::endl;
                exit(0);
            default:
                std::cout << "Please enter valid choice!" << std::endl; 
                break;
            }
        }
        else
        {
            std::cout << "Options::\n1. Upload a file \n2. Download a file" << std::endl
                    << "3. View files" << std::endl
                    << "4. Logout" << std::endl
                    << std::endl;
            std::cout << "Please enter your choice : ";
            std::cin >> choice;
            std::cout << std::endl;
            switch (choice)
            {
            case 1:
                upload();
                break;
            case 2:
                download();
                break;
            case 3:
                __view_files();
                break;
            case 4:
                std::cout << "Logged Out Successfully. " << std::endl;
                is_signedin = false;
                break;
            default:
                std::cout << "Please enter valid choice!" << std::endl;
                break;
            }
        }
        std::cout << std::endl;
    }
}
