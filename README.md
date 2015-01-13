# NDNFS

Zhehao's port of NDN file system based on [work](https://github.com/wentaoshang/NDNFS) by Wentao and Qiuhan.
See the file [INSTALL.md](https://github.com/zhehaowang/ndnfs-port/blob/master/INSTALL.md) for build and install instructions.

### Usage

NDNFS is an NDN-friendly file system. When mounted, data in the file system can be fetched through NDN through NDNFS-server.

To run on Mac:
<pre>
    $ mkdir /tmp/ndnfs
    $ ./build/ndnfs -s /tmp/ndnfs
</pre>
Use '-d' flag to see all debug output of ndnfs:
<pre>
    $ ./build/ndnfs -d /tmp/ndnfs
</pre>
Use '-f' flag to run in foreground and see debug info (if you compiled with --debug option):
<pre>
    $ ./build/ndnfs -s -f /tmp/ndnfs
</pre>
If '-f' is used, NDNFS is unmounted automatically when you kill 'ndnfs' process.

This will mount the file system to local folder '/tmp/ndnfs/'. To unmount NDNFS, type:
<pre>
    $ umount /tmp/ndnfs
</pre>
If the resource is busy, use
<pre>
    $ umount -f /tmp/ndnfs
</pre>
instead.

To specify a global prefix for all the files stored in NDNFS:
<pre>
    $ ./build/ndnfs -s -f /tmp/ndnfs -o prefix=/ndn/broadcast/ndnfs
</pre>
In this case, the NDN Content Object name is the global prefix + absolute file path.

### NDNFS-server

NDNFS-server supports read access of NDNFS by remote through NDN.

To run on Mac:

Install and configure [NFD](https://github.com/named-data/NFD) before running the FS server.

Run:
<pre>
    $ ./build/ndnfs-server
</pre>
Use '-p' flag to configure prefix, '-d' flag to select db file, and '-f' flag to identify file system root
(All these should be the same with NDNFS configuration)

### NDNFS-client

To access NDNFS data remotely, please use [NDN-JS Firefox plugin](https://github.com/named-data/ndn-js), or a built-in client in tests (documentation coming soon)

Please make sure the nfd that you are connected to has a route to NDNFS-server. An easy setup using the Firefox plugin would be
* Run nfd locally with websocket enabled;
* Set the plugin's hub to localhost;
* nfdc to the host of ndnfs-server (can be localhost as well, in which case nfdc won't be needed);
* Type in a "\<root\>/\<file or folder\>" path as URI and fetch.

Right now, the published mime_type is not utilized by the Firefox plugin, and all data will be displayed as plain text on the webpage. (which may take a long time for a large file)

### New features
* Instead of the network-ready data packets, store only the signature in sqlite3 database, and assemble NDN data packets when requested;
* Publish mime_type in a new meta-info branch;
* Updated to work with NDNJS Firefox plugin as-is, and latest version of NDN-CPP;
* Sign asynchronously (experimental, and still in local branch).
