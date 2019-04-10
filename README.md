# Introducing Morphene (beta)

Morphene is a Delegated Proof of Stake blockchain that uses a "Proof of Brain" social consensus algorithm for token allocation.

  - Currency symbol MORPH.
  - 49.5% APR inflation narrowing to 0.95% APR over 10 years.
  - 100% of inflation to block producers.

# No Support & No Warranty

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

# Quickstart

Just want to get up and running quickly? We have pre-built docker images for your convenience. More details are in our [quickstart guide](https://github.com/morphene/morphene/blob/master/doc/exchangequickstart.md).

# Building

We strongly recommend using one of our pre-built Docker images or using Docker to build Morphene. Both of these processes are described in the [quickstart guide](https://github.com/morphene/morphene/blob/master/doc/exchangequickstart.md).

If you would still like to build from source, we do have [build instructions](https://github.com/morphene/morphene/blob/master/doc/building.md) for Linux (Ubuntu LTS) and macOS.

## Dockerized p2p Node

To run a p2p node (ca. 2GB of memory is required at the moment):

    docker run \
        -d -p 2001:2001 -p 8090:8090 --name morphened-default \
        morphene/morphene

    docker logs -f morphened-default  # follow along

## Dockerized Full Node

To run a node with *all* the data (e.g. for supporting a content website)
that uses ca. 14GB of memory and growing:

    docker run \
        --env USE_WAY_TOO_MUCH_RAM=1 --env USE_FULL_WEB_NODE=1 \
        -d -p 2001:2001 -p 8090:8090 --name morphened-full \
        morphene/morphene

    docker logs -f morphened-full

# Environment variables

There are quite a few environment variables that can be set to run morphened in different ways:

* `USE_WAY_TOO_MUCH_RAM` - if set to true, morphened starts a 'full node'
* `USE_FULL_WEB_NODE` - if set to true, a default config file will be used that enables a full set of API's and associated plugins.
* `USE_NGINX_FRONTEND` - if set to true, this will enable an NGINX reverse proxy in front of morphened that proxies websocket requests to morphened. This will also enable a custom healthcheck at the path '/health' that lists how many seconds away from current blockchain time your node is. It will return a '200' if it's less than 60 seconds away from synced.
* `USE_MULTICORE_READONLY` - if set to true, this will enable morphened in multiple reader mode to take advantage of multiple cores (if available). Read requests are handled by the read-only nodes, and write requests are forwarded back to the single 'writer' node automatically. NGINX load balances all requests to the reader nodes, 4 per available core. This setting is still considered experimental and may have trouble with some API calls until further development is completed.
* `HOME` - set this to the path where you want morphened to store it's data files (block log, shared memory, config file, etc). By default `/var/lib/morphened` is used and exists inside the docker container. If you want to use a different mountpoint (like a ramdisk, or a different drive) then you may want to set this variable to map the volume to your docker container.

# PaaS mode

Morphened now supports a PaaS mode (platform as a service) that currently works with Amazon's Elastic Beanstalk service. It can be launched using the following environment variables:

* `USE_PAAS` - if set to true, morphened will launch in a format that works with AWS EB. Containers will exit upon failure so that they can be relaunched automatically by ECS. This mode assumes `USE_WAY_TOO_MUCH_RAM` and `USE_FULL_WEB_NODE`, they do not need to be also set.
* `S3_BUCKET` - set this to the name of the S3 bucket where you will store shared memory files for morphened in Amazon S3. They will be stored compressed in bz2 format with the file name `blockchain-$VERSION-latest.tar.bz2`, where $VERSION is the release number followed by the git short commit hash stored in each docker image at `/etc/morphenedversion`.
* `SYNC_TO_S3` - if set to true, the node will function to only generate shared memory files and upload them to the specified S3 bucket. This makes fast deployments and autoscaling for morphened possible.

# Config File

Run `morphened` once to generate a data directory and config file. The default location is `$HOME/.morphened`. Kill `morphened`. It won't do anything without seed nodes. If you want to modify the config to your liking, we have two example configs used in the docker images. ( [consensus node](contrib/config-for-docker.ini), [full node](contrib/fullnode.config.ini) ) All options will be present in the default config file and there may be more options needing to be changed from the docker configs (some of the options actually used in images are configured via command line).

# Seed Nodes

A list of some seed nodes to get you started can be found in
[doc/seednodes.txt](doc/seednodes.txt).

This same file is baked into the docker images and can be overridden by
setting `MORPHENED_SEED_NODES` in the container environment at `docker run`
time to a whitespace delimited list of seed nodes (with port).

# CLI Wallet

We provide a basic cli wallet for interfacing with `morphened`. The wallet is self documented via command line help. The node you connect to via the cli wallet needs to be running the `account_by_key_api`, `account_history_api`, and `network_broadcast_api` and needs to be configured to accept websocket connections via `webserver-ws-endpoint`.

# Building

See [doc/building.md](doc/building.md) for detailed build instructions, including
compile-time options, and specific commands for Linux (Ubuntu LTS) or macOS.
