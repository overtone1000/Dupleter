# Dupleter
Command line application to delete duplicate files in a file system

# Configuring the Development Environment
Dependencies are in `./src/deps` but are excluded from this repository in `.gitignore`. These must be added to the devleopment environment. These are included in `tasks.json`. Version numbers in tasks.json may need to be updated.
* Boost: Uses program_options, which requires a build. Run `bootstrap.sh` and then `./b2`.
* OpenSSL: For now just using pre-installed openssl on ubuntu.

#Pushing to a Docker Container
`sudo docker cp ./build/dupleter $CONTAINER_NAME:$DIRECTORY`