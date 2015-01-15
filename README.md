# NDNFS

Zhehao's port of NDN file system based on [work](https://github.com/wentaoshang/NDNFS) by Wentao and Qiuhan.

See the file [INSTALL.md](https://github.com/zhehaowang/ndnfs-port/blob/master/INSTALL.md) for build and install instructions.

Report bug to Zhehao. wangzhehao410305@gmail.com

### Usage

NDNFS is an NDN-friendly file system. When mounted, data in the file system can be fetched through NDN through NDNFS-server.

To run on Mac:
<pre>
    $ mkdir /tmp/ndnfs
    $ mkdir /tmp/dir
    $ ./build/ndnfs -s /tmp/dir /tmp/ndnfs
</pre>
/tmp/dir is where files are actually stored in the local file system; while /tmp/ndnfs is the mount point.

'-s' flag tells fuse to run single threaded.

Use '-d' flag to see all debug output of ndnfs:
<pre>
    $ ./build/ndnfs -d /tmp/dir /tmp/ndnfs
</pre>
Use '-f' flag to run in foreground and see debug info:
<pre>
    $ ./build/ndnfs -s -f /tmp/dir /tmp/ndnfs
</pre>
If '-f' is used, NDNFS is unmounted automatically when you kill 'ndnfs' process.

To unmount NDNFS, type:
<pre>
    $ umount /tmp/ndnfs
</pre>
To force unmount, use '-f' flag:
<pre>
    $ umount -f /tmp/ndnfs
</pre>

Coming soon: Specify ndnfs prefix and database file path;

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
