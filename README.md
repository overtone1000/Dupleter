# Dupleter
Command line application to delete duplicate files in a file system

# Configuring the Development Environment
* Compiled with `g++`, C++ version 17. `g++` must be installed and available on the PATH.
* Boost: 
    1. Uses the program_options library, which requires a build.
    2. Download boost and extract into repository directory `src/deps`. This directory is ignored from the git repository.
    3. Open the boost directory in a terminal.
    ```
    ./bootstrap.sh --prefix=./build --with-libraries=program_options
    ./b2
    ./b2 headers
    ```
    4. `tasks.json` contains the compilation command. The includes and linking arguments may need to be updated to the correct Boost version number.
* OpenSSL: Pre-installed on most Linux OSs. This is simply linked with the `-lcrypto` argument in the compile task.