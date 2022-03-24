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
            console(line);
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

void scan_directory(const path current_directory, sizemap* map_by_size, bool verbose)
{
    for (const std::filesystem::directory_entry child : std::filesystem::directory_iterator(current_directory)) {
        const std::string filenameStr = child.path().filename().string();
        if (child.is_directory()) {
            if(verbose){console("Moving into subdirectory " + child.path().string());}
            scan_directory(child.path(),map_by_size, verbose);
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

                    std::cout << "   Hash key:" << key << " for " << path_vec->at(n) << std::endl;
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
                for(int n=1;n<path_vec->size();n++)
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

    path main_directory = "./";
    if (vm.count("dir")) {
        main_directory = vm["dir"].as<std::string>();
    }
   
    console("Searching for duplicates in " + main_directory.string());
    sizemap* file_size_map = new sizemap();
    scan_directory(main_directory, file_size_map, verbose);
    console("Processing size map");
    stringmap* sizestringmap = process_size_map(file_size_map, verbose);
    console("Processing size-hash map");
    process_sizehash_map(sizestringmap,verbose,del);

    return 0;
}