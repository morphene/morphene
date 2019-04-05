Quickstart
----------

### Get current morphened
Use docker:
```
docker run \
    -d -p 2001:2001 -p 8090:8090 --name morphened-default \
    --restart unless-stopped morphene/morphene
```
#### Low memory node?
Above runs low memory node, which is suitable for:
- seed nodes
- witness nodes
- exchanges, etc.
For full api node use:

```
docker run \
    --env USE_WAY_TOO_MUCH_RAM=1 --env USE_FULL_WEB_NODE=1 \
    -d -p 2001:2001 -p 8090:8090 --name morphened-full \
    --restart unless-stopped \
    morphene/morphene
```
### Configure for your use case
#### Full API node
You need to use `USE_WAY_TOO_MUCH_RAM=1` and `USE_FULL_WEB_NODE=1` as stated above.
You can Use `contrib/fullnode.config.ini` as a base for your `config.ini` file.

#### Exchanges
Use low memory node.

Also make sure that your `config.ini` contains:
```
enable-plugin = account_history
public-api = database_api login_api
track-account-range = ["yourexchangeid", "yourexchangeid"]
```
Do not add other APIs or plugins unless you know what you are doing.

This configuration exists in Docker with the following command

```
docker run -d --env TRACK_ACCOUNT="yourexchangeid" \
    --name morphened \
    --restart unless-stopped \
    morphene/morphene
```

#### Other use cases
Shared memory file size varies, depends on your specific configuration but it is expected to be somewhere between 1GB and 64GB.
