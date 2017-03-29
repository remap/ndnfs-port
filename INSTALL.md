# NDNFS: NDN-friendly file system (based on FUSE)

### Prerequisites

###### For Mac OS X (>=10.8.4, Using MacPort)

* OSXFuse (tested with 2.5.6)
Download and install [OSXFUSE](http://osxfuse.github.io/2013/05/01/OSXFUSE-2.5.6.html)
* pkgconfig
<pre>
    $ sudo port install pkgconfig
</pre>
* Sqlite3
<pre>
    $ sudo port install sqlite3
</pre>
* protobuf-cpp
<pre>
    $ sudo port install protobuf-cpp
</pre>
* boost library (NDNFS-port uses boost async I/O from NDN-CPP, please compile NDN-CPP with boost if possible.)
<pre>
    $ sudo port install boost
</pre>
* [NDN-CPP library](github.com/named-data/ndn-cpp)

###### For Ubuntu (>=12.04)

* Fuse (tested with 2.5.6)
<pre>
    $ sudo apt-get install fuse libfuse-dev
</pre>
* pkgconfig
<pre>
    $ sudo apt-get install pkg-config
</pre>
* Sqlite3
<pre>
    $ sudo apt-get install sqlite3
</pre>
* protobuf-cpp
<pre>
    $ sudo apt-get install libprotobuf-dev protobuf-compiler
</pre>
* boost library (NDNFS-port uses boost async I/O from NDN-CPP, please compile NDN-CPP with boost if possible.)
<pre>
    $ sudo apt-get install libboost1.54-all-dev
</pre>
* [NDN-CPP library](github.com/named-data/ndn-cpp)

### Build

Please make sure that PKG\_CONFIG\_PATH includes the folder containing protobuf.pc and osxfuse.pc/fuse.pc;
(And for some systems, default LD\_LIBRARY\_PATH may not include the default library installation path of protobuf).
For example, do:
<pre>
    $ export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig 
    $ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
</pre>
And then, in NDNFS folder do:
<pre>
    $ ./waf configure
    $ ./waf
</pre>

Optionally, instead of "./waf configure" you can enter:
<pre>
    $ ./waf configure --debug
</pre>

### Note

On Ubuntu 14.04, if boost is installed in "/usr/lib/x86_64-linux-gnu/" and waf configure cannot figure out boost lib path, can do
<pre>
    $ ./waf configure --boost-lib=/usr/lib/x86_64-linux-gnu/
</pre>