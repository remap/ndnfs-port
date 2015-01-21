# NDNFS: NDN-friendly file system (based on FUSE)

### Prerequisites

* OSXFUSE 2.5.6
* Sqlite
* [NDN-CPP library](github.com/named-data/ndn-cpp)
* boost library (optional)
* protobuf-cpp
* pkgconfig

Following are the detailed steps for each platform to install the prerequisites.

* Mac OS X 10.8.4, Mac OS X 10.9.5
Install Xcode.
In Xcode Preferences > Downloads, install "Command Line Tools".

From the following web site, download and install [OSXFUSE](http://osxfuse.github.io/2013/05/01/OSXFUSE-2.5.6.html)

Install MacPorts from http://www.macports.org/install.php, and install prerequisite with MacPorts
<pre>
    $ sudo port install boost
    $ sudo port install protobuf-cpp
    $ sudo port install pkgconfig
</pre>
Install NDN-CPP as follows (boost is optional for ndn-cpp, but right now we only tested with ndn-cpp compiled with boost shared-ptrs and functions):
<pre>
    $ git clone https://github.com/named-data/ndn-cpp.git
    $ cd ndn-cpp
    $ ./configure
    $ make
    $ sudo make install
</pre>

### Build

Please make sure that PKG\_CONFIG\_PATH includes the folder containing libndn-cxx.pc, protobuf.pc and osxfuse.pc/fuse.pc;
(And for some systems, default LD\_LIBRARY\_PATH may not include the default library installation path of protobuf (should be fixed in future wscripts)).
For example, do:
<pre>
    $ export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig 
    $ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
</pre>
And then, do:
<pre>
    $ ./waf configure
    $ ./waf
</pre>

Optionally, instead of "./waf configure" you can enter:
<pre>
    $ ./waf configure --debug
</pre>