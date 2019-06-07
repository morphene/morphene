Exchange Quickstart
-------------------

System Requirements: A dedicated server or virtual machine with a minimum of 4GB of RAM, and at least 10GB of SSD storage.

With the right equipment and technical configuration a reindex should take **no longer than 24 hours**.  If recommendations are not followed precisely, the reindex can drag on for days or even weeks with significant slowdowns towards the end.

Morphene replays and resyncs currently are very fast and should not required much time at all to perform. Overtime, as the network usage increases, replay times will also increase.

You can save a lot of time by replaying from a `block_log`.

We recommend using docker to both build and run Morphene for exchanges. Docker is the world's leading containerization platform and using it guarantees that your build and run environment is identical to what our developers use. You can still build from source and you can keep both blockchain data and wallet data outside of the docker container. The instructions below will show you how to do this in just a few easy steps.

### Install docker and git (if not already installed)

On Ubuntu 16.04+:
```
apt-get update && apt-get install git docker.io -y
```

On other distributions you can install docker with the native package manager or with the script from get.docker.com:
```
curl -fsSL get.docker.com -o get-docker.sh
sh get-docker.sh
```

### Clone the morphene repo

Pull in the morphene repo from the official source on github and then change into the directory that's created for it.
```
git clone https://github.com/morphene/morphene
cd morphene
```

### Build the image from source with docker

Docker isn't just for downloading already built images, it can be used to build from source the same way you would otherwise build. By doing this you ensure that your build environment is identical to what we use to develop the software. Use the below command to start the build:

```
docker build -t=morphene/morphene .
```

Don't forget the `.` at the end of the line which indicates the build target is in the current directory.

This will build everything available in the source directory. It will take anywhere from ten minutes to an hour depending on how fast your equipment is.

When the build completes you will see a message indicating that it is 'successfully built'.

### Using our official Docker images without building from source

If you'd like to use our already pre-built official binary images, it's as simple as downloading it from the Dockerhub registry with only one command:

```
docker pull morphene/morphene
```

### Running a binary build without a Docker container

If you build with Docker but do not want to run steemd from within a docker container, you can stop here with this step and instead extract the binary from the container with the commands below. If you are going to run steemd with docker (recommended method), skip this step altogether. We're simply providing an option for everyone's use-case. Our binaries are built mostly static, only dynamically linking to linux kernel libraries. We have tested and confirmed binaries built in Docker work on Ubuntu and Fedora and will likely work on many other Linux distrubutions. Building the image yourself or pulling one of our pre-built images both work.

To extract the binary you need to start a container and then copy the file from it.

```
docker run -d --name morphened-exchange morphene/morphene
docker cp morphened-exchange:/usr/local/morphened-default/bin/morphened /local/path/to/morphened
docker cp morphened-exchange:/usr/local/morphened-default/bin/cli_wallet /local/path/to/cli_wallet
docker stop morphened-exchange
```

For your convenience, we have provided a provided an [example\_config](example\_config.ini) that we expect should be sufficient to run your exchange node. Be sure to rename it to simply `config.ini`.

### Create directories to store blockchain and wallet data outside of Docker

For re-usability, you can create directories to store blockchain and wallet data and easily link them inside your docker container.

```
mkdir blockchain
mkdir morphenewallet
```

### Run the container

The below command will start a daemonized instance opening ports for p2p and RPC  while linking the directories we created for blockchain and wallet data inside the container. Fill in `TRACK_ACCOUNT` with the name of your exchange account that you want to follow. The `-v` flags are how you map directories outside of the container to the inside, you list the path to the directories you created earlier before the `:` for each `-v` flag. The restart policy ensures that the container will automatically restart even if your system is restarted.

```
docker run -d --name morphened-exchange -p 2001:2001 -p 8090:8090 -v /path/to/morphenewallet:/var/morphenewallet -v /path/to/blockchain:/var/lib/morphened/blockchain --restart always morphene/morphene
```

You can see that the container is running with the `docker ps` command.

To follow along with the logs, use `docker logs -f`.

Initial syncing will take between 1 and 60 minutes depending on your equipment, faster storage devices will take less time and be more efficient. Subsequent restarts will not take as long.

### Running the cli_wallet

The command below will run the cli_wallet from inside the running container while mapping the `wallet.json` to the directory you created for it on the host.

```
docker exec -it morphened-exchange /usr/local/morphened-default/bin/cli_wallet -w /var/morphenewallet/wallet.json
```