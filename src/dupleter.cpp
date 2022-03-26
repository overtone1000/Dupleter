#include <iostream>
#include <map>
#include <filesystem>
#include <fstream> // for reading file
#include <boost/program_options.hpp>
#include <openssl/sha.h>

namespace po = boost::program_options;

void console(std::string message)
{
    std::cout<<message<<std::endl;
}

typedef std::filesystem::path path;
typedef std::map<std::string,std::vector<path>*> stringmap;
typedef std::map<long,std::vector<path>*> sizemap;

std::string get_file_hash(path path)
{
	std::fstream fp;
    unsigned char digest[SHA256_DIGEST_LENGTH];

	fp.open(path,std::ios::in);
	if(!(fp.is_open())){
		fprintf(stderr,"Unable to open the file\n");
		return "";
	}
	else {
        //Genrating hash of the file
        SHA256_CTX ctx;
        //Initializing
        SHA256_Init(&ctx);

		std::string line;
		while(fp >> line){
			SHA256_Update(&ctx,line.c_str(),line.size());
		}
        SHA256_Final(digest,&ctx);
	}
	fp.close();

    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }

    return ss.str();
}

void scan_directory_size(const path current_directory, sizemap* map_by_size, bool verbose)
{
    for (const std::filesystem::directory_entry child : std::filesystem::directory_iterator(current_directory)) {
        const std::string filenameStr = child.path().filename().string();
        if (child.is_directory()) {
            if(verbose){console("Moving into subdirectory " + child.path().string());}
            scan_directory_size(child.path(),map_by_size, verbose);
        }
        else if (child.is_regular_file()) {
            long size=std::filesystem::file_size(child.path());

            std::vector<path>* path_vec;
            if(map_by_size->count(size)<=0)
            {
                path_vec = new std::vector<path>();
                map_by_size->insert_or_assign(size,path_vec);
            }
            else
            {
                path_vec = map_by_size->at(size);
            }

            path_vec->push_back(child.path());
        }
        else {
            console("Unhandled directory member " + child.path().string());
        }
    }
}

std::string decopify_filename(std::filesystem::path path, bool verbose)
{
    std::string name = path.filename();

    if(verbose){console("Decopifying " + name);}

    if(path.filename().has_extension())
    {
        name=name.substr(0,name.length()-path.filename().extension().string().length());
        if(verbose){console("Removing extension. Now: " + name);}
    }

    const std::string copy=" copy";

    if(name.length()>copy.length())
    {
        for(int n=1;n<name.length()-copy.length()+1;n++)
        {
            const std::string substring = name.substr(n,copy.length());
            if(substring==copy)
            {
                name=name.substr(0,n);
                break;
            }
        }
    }

    while(true)
    {
        int last_open_paren = name.length()-1;
        for(int n=name.length()-1;n>=0;n--)
        {
            if(name.at(n)=='(')
            {
                last_open_paren=n;
                break;
            }
        }
        if(last_open_paren<name.length()-1)
        {
            const std::string substr = name.substr(last_open_paren+1,name.length()-last_open_paren-2);
            for (char const &c : substr) {
                if (std::isdigit(c) != 0){
                    break;
                }
            }
            name=name.substr(0,last_open_paren-1);
        }
        else
        {
            break;
        }
    }

    return name+path.filename().extension().string();
}

void scan_directory_name(const path current_directory, long fuzzy_size, bool del, bool verbose, ulong* deletion_tally, ulong* file_tally)
{
    std::map<std::string,std::filesystem::path> files;
    for (const std::filesystem::directory_entry child : std::filesystem::directory_iterator(current_directory)) {
        std::filesystem::path current_path = child.path();
        const std::string filenameStr = current_path.filename().string();
        if (child.is_directory()) {
            if(verbose){console("Moving into subdirectory " + current_path.string());}
            scan_directory_name(current_path, fuzzy_size, del, verbose, deletion_tally, file_tally);
        }
        else if (child.is_regular_file()) {
            long size=std::filesystem::file_size(current_path);
            const std::string decopified_filename=decopify_filename(current_path.filename().string(),verbose);
            if(verbose){console("Decopified filename:"+decopified_filename);}
            (*file_tally)++;
            if(files.count(decopified_filename))
            {
                std::filesystem::path previous_path = files.at(decopified_filename);
                if(verbose){
                    console("Copy found by name:");
                    console("   "+previous_path.filename().string());
                    console("   "+current_path.filename().string());
                }
                
                long size_difference = std::filesystem::file_size(current_path)- std::filesystem::file_size(previous_path);
                size_difference = std::abs(size_difference);
                if(size_difference<=fuzzy_size)
                {
                    std::filesystem::path to_del;
                    //delete the one with the longer path
                    if(current_path.filename().string().length()>previous_path.filename().string().length())
                    {
                        to_del=current_path;
                    }
                    else
                    {
                        to_del=previous_path;
                        files.insert_or_assign(decopified_filename,current_path);
                    }

                    if(del)
                    {
                        console("Deleting " + to_del.filename().string());
                        std::filesystem::remove(to_del);
                    }
                    else
                    {
                        console("Dry run. Would have deleted " + to_del.filename().string());
                    }

                    (*deletion_tally)++;
                }
                else
                {
                    if(verbose){console("Skipping. File size difference is above threshold at " + std::to_string(size_difference));}
                }
            }
            else
            {

                files.insert_or_assign(decopified_filename,current_path);
            }
        }
        else {
            console("Unhandled directory member " + current_path.string());
        }
    }
}

stringmap* process_size_map(sizemap* map, bool verbose)
{
    console("");

    if(verbose){console("Size map details:");}

    stringmap* map_by_sizehash=new stringmap();

    int size_match_count=0;
    int size_match_file_count=0;
    for (auto it = map->begin(); it != map->end(); ++it) {
        std::vector<path>* path_vec = it->second;
        if(path_vec->size()>1)
        {
            size_match_count++;
            size_match_file_count+=path_vec->size();
            if(verbose)
            {
                std::cout << "Match (" << path_vec->size() << "): "<<std::endl;
                for(int n=0;n<path_vec->size();n++)
                {
                    std::cout << "   " << path_vec->at(n).string() << std::endl;
                }
            }

            for(int n=0;n<path_vec->size();n++)
            {
                std::string hash=get_file_hash(path_vec->at(n));
                if(hash!="")
                {
                    std::string key = std::to_string(it->first) + "_" + hash;

                    std::vector<path>* sub_path_vec;
                    if(map_by_sizehash->count(key)<=0)
                    {
                        sub_path_vec = new std::vector<path>();
                        map_by_sizehash->insert_or_assign(key,sub_path_vec);
                    }
                    else
                    {
                        sub_path_vec = map_by_sizehash->at(key);
                    }

                    sub_path_vec->push_back(path_vec->at(n));
                }
                else
                {
                    console("Got a null hash. Skipping.");
                }
            }
        }
    }
    std::cout << size_match_count << " matches by size involving " << size_match_file_count << " files." << std::endl;
    return map_by_sizehash;
}

void process_sizehash_map(stringmap* map, bool verbose, bool del)
{
    console("");

    if(verbose){console("Size-hash map details:");}

    int sizehash_match_count=0;
    int sizehash_match_file_count=0;
    for (auto it = map->begin(); it != map->end(); ++it) {
        std::vector<path>* path_vec = it->second;
        if(path_vec->size()>1)
        {
            sizehash_match_count++;
            sizehash_match_file_count+=path_vec->size();

            if(verbose)
            {
                std::cout << "Match (" << path_vec->size() << "): "<<std::endl;
                for(int n=0;n<path_vec->size();n++)
                {
                    std::cout << "   " << path_vec->at(n).string() << std::endl;
                }
            }

            if(del)
            {
                for(int n=path_vec->size()-1;n>0;n--)
                {
                    if(verbose){std::cout << "Deleting" << path_vec->at(n).string() << std::endl;}
                    std::filesystem::remove(path_vec->at(n));
                }
            }
        }
    }
    
    std::cout << sizehash_match_count << " matches by size-hash involving " << sizehash_match_file_count << " files." << std::endl;
}

int main( int argc,      // Number of strings in array argv
          char *argv[],   // Array of command-line argument strings
          char *envp[] )  // Array of environment variable strings
{
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "Display this help message.")
        ("dir", po::value<std::string>(), "Specify the directory to search. Default is the working directory.")
        ("mode",po::value<std::string>(), "Specify the duplicate checking mode. Options are \"size-hash\" and \"fuzzy-size-name\"")
        ("fuzzy-size",po::value<long>(), "For fuzzy-size-name mode, specifies file size difference in bytes allowed for matches. Defaults to 0 (i.e. non-fuzzy).")
        ("delete", "Delete duplicates arbitrarily, leaving only one copy.")
        ("verbose", "See verbose output.")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    bool verbose = vm.count("verbose");
    bool del = vm.count("delete");

    if(del)
    {
        console("Duplicates will be deleted.");
    }
    else
    {
        console("Dry run only. No files will be deleted.");
    }

    std::string mode = vm["mode"].as<std::string>();
    if(mode!="size-hash" && mode!="fuzzy-size-name")
    {
        console("Please specify mode. See help for options.");
        return 1;
    }

    path main_directory = "./";
    if (vm.count("dir")) {
        main_directory = vm["dir"].as<std::string>();
    }

    console("Searching for duplicates in " + main_directory.string());

    if(mode=="size-hash")
    {
        sizemap* file_size_map = new sizemap();
        scan_directory_size(main_directory, file_size_map, verbose);
        console("Processing size map");
        stringmap* sizestringmap = process_size_map(file_size_map, verbose);
        console("Processing size-hash map");
        process_sizehash_map(sizestringmap,verbose,del);
    }
    else if(mode=="fuzzy-size-name")
    {
        long fuzzy_size=0;
        if(vm.count("fuzzy-size"))
        {
            fuzzy_size=vm["fuzzy-size"].as<long>();
        }
        ulong* deletion_tally = new ulong();
        ulong* file_tally = new ulong();
        scan_directory_name(main_directory,fuzzy_size,del,verbose,deletion_tally,file_tally);
        std::string action="would have been deleted";
        if(del){action="deleted";}
        std::cout<< *file_tally << " files processed. " << *deletion_tally << " files " << action << "." << std::endl;
    }

    return 0;
}