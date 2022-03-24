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

std::map<std::string,std::filesystem::path>* file_hash_map;

std::string* get_file_hash(std::filesystem::path path)
{
	std::fstream fp;
    unsigned char digest[SHA256_DIGEST_LENGTH];

	fp.open(path,std::ios::in);
	if(!(fp.is_open())){
		fprintf(stderr,"Unable to open the file\n");
		return NULL;
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

    std::string* return_value=new std::string((char*)&digest);
    return return_value;
}

void scan_directory(std::filesystem::path path)
{
    for (const std::filesystem::directory_entry child : std::filesystem::directory_iterator(path)) {
        const std::string filenameStr = child.path().filename().string();
        if (child.is_directory()) {
            console("Moving into subdirectory " + child.path().string());
            scan_directory(child.path());
        }
        else if (child.is_regular_file()) {
            std::string* hash=get_file_hash(child.path());
            if(hash!=NULL)
            {
                //console("Hash is " + *hash);
                file_hash_map->insert_or_assign(*hash,child.path());
            }
            else
            {
                console("Got a null hash. Skipping.");
            }
        }
        else {
            console("Unhandled directory member " + child.path().string());
        }
    }
}

void process_map()
{
    console("");
    console("Iterating through hash map.");
    for (auto it = file_hash_map->begin(); it != file_hash_map->end(); ++it) {
        std::cout << it->first << "," << it->second << std::endl;
    }
}

int main( int argc,      // Number of strings in array argv
          char *argv[],   // Array of command-line argument strings
          char *envp[] )  // Array of environment variable strings
{
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "Display this help message")
        ("dir", po::value<std::string>(), "Specify the directory to search")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    if (vm.count("dir")) {
        file_hash_map = new std::map<std::string,std::filesystem::path>();
        const std::filesystem::path main_directory = vm["dir"].as<std::string>();
        console("Searching for duplicates in " + main_directory.string());
        scan_directory(main_directory);
        process_map();
    }
    else
    {
        console("Please specify the output directory with --dir");
        return 0;
    }

    return 0;
}