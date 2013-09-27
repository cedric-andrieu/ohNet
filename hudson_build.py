#!/usr/bin/env python
#
import os, subprocess
from optparse import OptionParser
from Helpers.valgrind_parser import *
from Helpers.remote import *
import sys
from os import path

class PostActions():
    def valgrind_parse(self):
        val = valgrindParser()
        val.get_files('vgout')
        val.parse_file()

    def valgrind_stub(self):
        val = valgrindParser()
        val.put_dummy('vgout')

    def do_release(self,platform):
        rem = remote()
        release_targets = []
        release_targets.append('release')
        release_targets.append('debug')

    def gen_docs(self):
        rem = remote()
        ret = subprocess.check_call('make docs', shell=True)
        if ret != 0:
            print ret
            sys.exit(10)
        ret = rem.rsync_gc('hudson-rsync','openhome.org','Build/Docs/','~/build/nightly/docs')
        if ret != 0:
            print ret
            sys.exit(10)

    def arm_tests(self,type,release):
        # type will be either 'nightly' or 'commit'
        # release will be either '0' or '1'
        rem = remote()
        ret = rem.rsync('root','sheeva010.linn.co.uk','Build','~/', excludes=['*.o', '*.a', 'Bundles'])
        if ret != 0:
            print ret
            sys.exit(10)
        ret = rem.rsync('root','sheeva010.linn.co.uk','AllTests.py','~/')
        if ret != 0:
            print ret
            sys.exit(10)

        alltests_cmd = 'python AllTests.py -t' # Setup AllTests cmd line to run tests only.

        if type == 'nightly':
            alltests_cmd += ' -f'        # Add 'full test' flag if in nightly-mode
        if release == '0':
            alltests_cmd += ' --debug'   # Run binaries residing in Build/Debug instead of Build/Release

        ret = rem.rssh('root','sheeva010.linn.co.uk', alltests_cmd)
        if ret != 0:
            print ret
            sys.exit(10)
    
class JenkinsBuild():
    def get_options(self):
        env_platform = os.environ.get('PLATFORM')
        env_nightly = os.environ.get('NIGHTLY')
        env_release = os.environ.get('PUBLISH')
        env_version = os.environ.get('PUBLISH_VERSION')

        parser = OptionParser()
        parser.add_option("-p", "--platform", dest="platform",
            help="Linux-x86, Linux-x64, Windows-x86, Windows-x64, Linux-ARM, Linux-ppc32, Mac-x64, Core-ppc32, Core-armv5, Core-armv6, iOs-armv7, iOs-x86")
        parser.add_option("-n", "--nightly",
                  action="store_true", dest="nightly", default=False,
                  help="Perform a nightly build")
        parser.add_option("-r", "--publish",
          action="store_true", dest="release", default=False,
          help="publish release")
        parser.add_option("-v", "--version", dest="version",
            help="version number for release")
        parser.add_option("-j", "--parallel",
            action="store_true", dest="parallel", default=False,
            help="Tell AllTests.py to parallelise the build.")
        (self.options, self.args) = parser.parse_args()

        # check if env variables are set
        # if they are, ignore what is on the command line.

        if env_platform != None:
            self.options.platform = env_platform
        if env_version != None:
            self.options.version = env_version
        if env_nightly == 'true' or self.options.nightly == True:
             self.options.nightly = '1'
        else:
            self.options.nightly = '0'
        if env_release == 'true' or self.options.release == True:
            self.options.release = '1'
        else:
            self.options.release = '0'

        print "options for build are nightly:%s and release:%s on platform %s" %(self.options.nightly,self.options.release,self.options.platform)

    def get_platform(self):
        platforms = { 
                'Linux-x86': { 'os':'linux', 'arch':'x86', 'publish':True, 'system':'Linux'},
                'Linux-x64': { 'os':'linux', 'arch':'x64', 'publish':True, 'system':'Linux'},
                'Linux-ppc32': { 'os':'linux', 'arch':'ppc32', 'publish':True, 'system':'Linux'},
                'Windows-x86': { 'os': 'windows', 'arch':'x86', 'publish':True, 'system':'Windows'},
                'Windows-x64': { 'os': 'windows', 'arch':'x64', 'publish':True, 'system':'Windows'},
                'Macos-x64': { 'os': 'macos', 'arch':'x86', 'publish':False, 'system':'Mac'}, # Old Jenkins label
                'Mac-x64': { 'os': 'macos', 'arch':'x64', 'publish':True, 'system':'Mac'}, # New Jenkins label, matches downstream builds
                'Mac-x86': { 'os': 'macos', 'arch':'x86', 'publish':True, 'system':'Mac'}, # New Jenkins label, matches downstream builds
                'Linux-ARM': { 'os': 'linux', 'arch': 'armel', 'publish':True, 'system':'Linux'},
                'iOs-ARM': { 'os': 'iOs', 'arch':'armv7', 'publish':True, 'system':'iOs'}, # Old Jenkins label
                'iOs-x86': { 'os': 'iOs', 'arch':'x86', 'publish':True, 'system':'iOs'},
                'iOs-armv7': { 'os': 'iOs', 'arch':'armv7', 'publish':True, 'system':'iOs'},
                'Core-ppc32': { 'os': 'Core', 'arch':'ppc32', 'publish':True, 'system':'Core'},
                'Core-armv5': { 'os': 'Core', 'arch':'armv5', 'publish':True, 'system':'Core'},
                'Core-armv6': { 'os': 'Core', 'arch':'armv6', 'publish':True, 'system':'Core'},
                'Android-anycpu': { 'os': 'Android', 'arch':'anycpu', 'publish':True, 'system':'Android'},
        }
        current_platform = self.options.platform
        self.platform = platforms[current_platform]

    def set_platform_args(self):
        os_platform = self.platform['os']
        arch = self.platform['arch']
        args=[]

        if os_platform == 'windows' and arch == 'x86':
            args.append('vcvarsall.bat')
        if os_platform == 'windows' and arch == 'x64':
            args.append('vcvarsall.bat')
            args.append('amd64')
            os.environ['CS_PLATFORM'] = 'x64'
        if os_platform == 'linux' and arch == 'armel':
            os.environ['CROSS_COMPILE'] = '/usr/local/arm-2011.09/bin/arm-none-linux-gnueabi-'
        if os_platform == 'Core' and arch == 'ppc32':
            os.environ['CROSS_COMPILE'] = '/opt/rtems-4.11/bin/powerpc-rtems4.11-'
        if os_platform == 'Core' and (arch == 'armv5' or arch == 'armv6'):
            os.environ['CROSS_COMPILE'] = '/opt/rtems-4.11/bin/arm-rtemseabi4.11-'

        self.platform_args = args

    def get_build_args(self):
        nightly = self.options.nightly
        arch = self.platform['arch']
        os_platform = self.platform['os']
        args=[]
        args.append('python')
        args.append('AllTests.py')
        args.append('--silent')

        self.platform_make_args = []

        if (arch in ['armel', 'armhf', 'armv7', 'armv6']) or (arch == 'ppc32' and os_platform == 'Core') or (os_platform == 'Android'):
            args.append('--buildonly')
        elif arch == 'x64':
            args.append('--native')
        if os_platform == 'windows' and arch == 'x86':
            args.append('--js')
            args.append('--java')
        if os_platform == 'linux' and arch == 'x86':
            args.append('--java')
        if os_platform == 'macos':
            if arch == 'x64':
                args.append('--mac-64')
                self.platform_make_args.append('mac-64=1')
        if os_platform == 'iOs':
            if arch == 'x86':
                args.append('--iOs-x86')
                self.platform_make_args.append('iOs-x86=1')
            elif arch == 'armv7':
                args.append('--iOs-armv7')
                self.platform_make_args.append('iOs-armv7=1')
            # 32 and 64-bit builds run in parallel on the same slave.
            # Overlapping test instances interfere with each other so only run tests for the (assumed more useful) 32-bit build.
            # Temporarily disable all tests on mac as publish jobs hang otherwise
            args.append('--buildonly')
        if os_platform == 'Android':
            args.append('--Android-anycpu')
            self.platform_make_args.append('Android-anycpu=1')
        if os_platform == 'Core':
            args.append('--core')
        if nightly == '1':
            args.append('--full')
            if os_platform == 'linux' and arch == 'x86':
                args.append('--valgrind')    
        if self.options.parallel:
            args.append('--parallel')
        self.build_args = args

    def do_build(self):
        nightly = self.options.nightly
        release = self.options.release
        os_platform = self.platform['os']
        build_args = self.build_args
        platform_args = self.platform_args
        common_args = []
            
        if platform_args == []:
            common_args.extend(build_args)
        else:
            common_args.extend(platform_args)
            common_args.append('&&')
            common_args.extend(build_args)
            
        build_targets = []
        build_targets.append('debug')

        if release == '1':
            build_targets.append('release')

        for build_t in build_targets:
            build = list(common_args)
            if build_t == 'debug':
                build.append('--debug')
            if build_t == 'release':
                build.append('--incremental')

            print 'running build with %s' %(build,)

            ret = subprocess.check_call(build)            
            if ret != 0:
                print ret
                sys.exit(10)

    def do_release(self):
        rem = remote()

        nightly = self.options.nightly
        release = self.options.release
        platform_args = self.platform_args
        platform = self.options.platform
        version = self.options.version
        openhome_system = self.platform['system']
        openhome_architecture = self.platform['arch']
        
        release_targets = []
        release_targets.append('release')
        release_targets.append('debug')
        
        for release in release_targets:
            openhome_configuration = release.title()
            build = []
            if platform_args != []:
                build.extend(platform_args)
                build.append('&&')
            build.append('make')
            build.append('tt')
            build.append('uset4=yes')
            ret = subprocess.check_call(build)
            if ret != 0:
                print ret
                sys.exit(10)

            build = []
            if platform_args != []:
                build.extend(platform_args)
                build.append('&&')

            build.append('make')
            build.append('bundle')
            #build.append('targetplatform=%s' %(platform,))
            #build.append('releasetype=%s' %(release,))
            build.append('uset4=yes')
            build.append('openhome_system=' + openhome_system)
            build.append('openhome_architecture=' + openhome_architecture)
            build.append('openhome_configuration=' + openhome_configuration)
            build.extend(self.platform_make_args)

            print "doing release with bundle %s" %(build,)

            ret = subprocess.check_call(build)
            if ret != 0:
                print ret
                sys.exit(10)

            native_bundle_name = os.path.join('Build/Bundles',"ohNet-%s-%s-%s.tar.gz" %(openhome_system, openhome_architecture, openhome_configuration))
            native_dest = os.path.join('Build/Bundles',"ohNet-%s-%s-%s-%s.tar.gz" %(version, openhome_system, openhome_architecture, openhome_configuration))
            if os.path.exists(native_dest):
                os.remove(native_dest)
            os.rename(native_bundle_name, native_dest)
            rem.check_rsync('releases','www.openhome.org','Build/Bundles/','~/www/artifacts/ohNet/')
                        
    
    def do_postAction(self):
        nightly = self.options.nightly
        release = self.options.release
        os_platform = self.platform['os']
        arch = self.platform['arch']
        postAction = PostActions()

        # generate dummy XML even on on-commit tests
        postAction.valgrind_stub()

        if nightly == '1':
            if os_platform == 'linux' and arch == 'x86':
                postAction.valgrind_parse()
                postAction.gen_docs()

            if os_platform == 'linux' and arch in ['armel', 'armhf']:
                postAction.arm_tests('nightly', release)
        else:
            if os_platform == 'linux' and arch in ['armel', 'armhf']:
                postAction.arm_tests('commit', release)
        if self.platform['publish'] and release == '1':
            self.do_release()

def switch_to_script_directory():
    ohnet_dir = path.split(path.realpath(__file__))[0]
    os.chdir(ohnet_dir)

def main():
    switch_to_script_directory()
    Build = JenkinsBuild()
    Build.get_options()
    Build.get_platform()
    Build.set_platform_args()
    Build.get_build_args()
    Build.do_build()
    Build.do_postAction()

if __name__ == "__main__":
    main()

