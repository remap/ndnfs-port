# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
VERSION='0.2'
APPNAME='NDNFS'

from waflib import Build, Logs, Utils, Task, TaskGen, Configure

def options(opt):
    opt.add_option('--debug', action='store_true', default=True, dest='debug', help='''debugging mode''')
    
    opt.add_option('--ndn-cpp-dir', type='string', default='/usr/local/lib', dest='_ndn_cpp_dir', help='''Path to libndn-cpps''')
    opt.add_option('--ndn-cpp-inc', type='string', default='/usr/local/include', dest='_ndn_cpp_inc', help='''Path to ndn-cpp includes''')

    # if Utils.unversioned_sys_platform () == "darwin":
    #     pass

    opt.load('compiler_c compiler_cxx')
    opt.load('boost protoc', tooldir=['waf-tools'])

def configure(conf):
    conf.load("compiler_c compiler_cxx")

    if conf.options.debug:
        conf.define ('NDNFS_DEBUG', 1)
        conf.add_supported_cxxflags (cxxflags = ['-O0',
                                                 '-std=c++11',
                                                 '-std=c++0x',
                                                 '-Wall',
                                                 '-Wno-unused-variable',
                                                 '-g3',
                                                 '-Wno-unused-private-field', # only clang supports
                                                 '-fcolor-diagnostics',       # only clang supports
                                                 '-Qunused-arguments'         # only clang supports
                                                 ])
    else:
        conf.add_supported_cxxflags (cxxflags = ['-O3', 
                                                 '-std=c++11',
                                                 '-std=c++0x',
                                                 '-g'])

    conf.define ("FUSE_NDNFS_VERSION", VERSION)

    try:
        conf.check_cfg(package='osxfuse', args=['--cflags', '--libs'], uselib_store='FUSE', mandatory=True)
        conf.define("NDNFS_OSXFUSE", 1)
    except:
        try:
            conf.check_cfg(package='fuse', args=['--cflags', '--libs'], uselib_store='FUSE', mandatory=True)
            conf.define("NDNFS_FUSE", 1)
        except:
            conf.fatal ("Cannot find FUSE libraries")

    conf.check_cfg(package='sqlite3', args=['--cflags', '--libs'], uselib_store='SQLITE3', mandatory=True)

    # if Utils.unversioned_sys_platform () == "darwin":
    #     pass

    conf.write_config_header('config.h')
    
    # TODO: dir and inc does not work as intended for now.
    conf.env.LIBPATH_NDNCPP = [conf.options._ndn_cpp_dir]
    conf.env.INCLUDES_NDNCPP = [conf.options._ndn_cpp_inc]
    
    conf.check(
      features='cxx cxxprogram',
      lib=['ndn-cpp'],
      libpath=[conf.options._ndn_cpp_dir], 
      cflags=['-Wall'],
      uselib_store='NDNCPP',
      mandatory=True)
    
    conf.load('boost')
    conf.check_boost(lib='system test iostreams thread chrono')
        
    conf.load('protoc')

def build (bld):
    bld (
        target = "ndnfs",
        features = ["cxx", "cxxprogram"],
        source = bld.path.ant_glob(['fs/*.cc']),
        use = ['FUSE', 'NDNCPP', 'SQLITE3'],
        includes = ['.']
        )
    bld (
        target = "ndnfs-server",
        features = ["cxx", "cxxprogram"],
        source = bld.path.ant_glob(['server/*.cc', 'server/*.proto']),
        use = ['NDNCPP', 'SQLITE3', 'PROTOBUF'],
        includes = ['fs', 'server']
        )
    bld (
        target = "test-client",
        features = ["cxx", "cxxprogram"],
        source = bld.path.ant_glob(['test/client.cc', 'test/handler.cc', 'server/*.proto', 'server/namespace.cc']),
        use = ['NDNCPP', 'PROTOBUF'],
        includes = ['server']
        )
    bld (
        target = "cat-file",
        features = ["cxx", "cxxprogram"],
        source = bld.path.ant_glob(['test/cat_file.cc', 'server/*.proto']),
        use = ['BOOST', 'NDNCPP', 'PROTOBUF'],
        includes = ['server']
        )
    bld (
        target = "cat-file-pipe",
        features = ["cxx", "cxxprogram"],
        source = bld.path.ant_glob(['test/cat_file_pipe.cc', 'server/*.proto']),
        use = ['BOOST', 'NDNCPP', 'PROTOBUF'],
        includes = ['server']
        )

@Configure.conf
def add_supported_cxxflags(self, cxxflags):
    """
    Check which cxxflags are supported by compiler and add them to env.CXXFLAGS variable
    """
    self.start_msg('Checking allowed flags for c++ compiler')

    supportedFlags = []
    for flag in cxxflags:
        if self.check_cxx (cxxflags=[flag], mandatory=False):
            supportedFlags += [flag]

    self.end_msg (' '.join (supportedFlags))
    self.env.CXXFLAGS += supportedFlags
