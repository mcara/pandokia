import distutils.core
import distutils.command
import os
import os.path

import platform
# print platform.python_version()

windows = platform.system() == 'Windows'

package_list = [
    'pandokia',             # core of pandokia system
    'pandokia.runners',     # "plugin-like" things that run various kinds of tests
    'pandokia.helpers',     # modules to use in writing your tests, usually with nose/unittest
    'stsci_regtest',        # legacy STScI IRAF/PyRAF test system
]

#
# These are all commands that the user can type.  We will susbstitute strings
# in them so that they will find the pandokia we are installing even if it is
# not on PYTHONPATH, _even_ if there is _another_ pandokia on pythonpath.
python_commands = [ 
    'pdk',                      # pandokia entry point
    'pdk_filecomp',             # helper file comparisons for use in shell scripts
    'pdk_python_runner',        # exec test runner from python module
    'pdk_stsci_regress_helper', # part of regtest runner
    'pdk_stsci_regress_refs',   # ?
    'pdknose',                  # run nose with pdk plugin
    'pdkpytest',                # run py.test with pdk plugin
    'pdkrun',                   # like "pdk run"
    'junittopdk',               # convert junit/xml to pandokia format
    'tbconv',                   # table conversion; not really part of testing
     ]

shell_commands = [ 
    'pdk_gen_contact',          # create contact list for pdk import_contact
    'pdk_monthly',              # cleaner tool for stsci
    'pdk_run_helper.sh',        # helper for shell scripts using "run" runner
    'pdk_shell_runner',         # run a shell script as a test, use exit code as status
    'pdk_shell_runner_helper',  # tools to use in shell_runner scripts
    'pdk_stsci_regress',        # regtest runner
    'sendto',                   # here for convenience; not really pandokia
    'shunit2_plugin_pdk',       # pandokia plugin for shunit2
    'xtname',                   # here for convenience; not really pandokia
    ]

command_list = python_commands + shell_commands


#
# These scripts should start "#!/usr/bin/env python", not with whatever
# python we happen to be using.
use_usr_bin_env = [
    'pdknose',
    'pdkpytest',
    'pdk_stsci_regress_helper',
    'pdk_python_runner',
]

f=open("pandokia/__init__.py","r")
for x in f :
    if x.startswith('__version__') :
        exec(x)
        break
f.close()


# if the stsci distutils hack is present, use it to try to capture
# subversion information.

def du_hack() :
    try :
        import stsci.tools.stsci_distutils_hack as H
    except ImportError :
        pass
    else :
        # we have to deal with two possible versions of the distutils
        # hack - the latest, and the one in the pyetc environment.  So,
        # __set_svn_version__ is duplicated and modified here.
        version_file = "pandokia/svn_version.py"
        rev = H.__get_svn_rev__('.')
        if rev is None :
            if os.path.exists(version_file) :
                    return
            revision = 'Unable to determine SVN revision'
        else:
            if ( rev == 'exported' or rev == 'unknown' ) and os.path.exists(version_file) :
                return
            revision = str(rev)
        info = H.__get_full_info__('.')
        f = open(version_file,'w')
        f.write("__svn_version__ = %s\n" % repr(revision))
        f.write("\n__full_svn_info__ = '''\n%s'''\n\n" % info)
        f.close()

# If you are not at STScI, you do not need this.  Delete this call if
# it causes you any trouble.
du_hack()


args = {
    'name' :            'pandokia',
    'version' :         __version__,
    'description' :     'Pandokia - a test management and reporting system',
    'author' :          'Mark Sienkiewicz, Vicki Laidler',
    'author_email':     'help@stsci.edu',
    'url' :             'https://svn.stsci.edu/trac/ssb/etal/wiki/Pandokia',
    'license':          'BSD',
    'platforms':        ['Posix', 'MacOS X'],
    'scripts' :         [ "commands/"+x for x in command_list ],
    'packages':         package_list,
    'package_data':     { 'pandokia' : [ '*.sql', '*.html', '*.png', '*.gif', '*.jpg', 'sql/*.sql', 'runners/maker/*'  ]  },
}

#
# Actually do the install
#
d = distutils.core.setup(
    **args
)

dir_set = "pdk_dir = r'%s' # this was set during install by setup.py\n"

#
#
def fix_script(name) :
    fname = os.path.join(script_dir,name)

    f=open(fname,"r")
    l = f.readlines()
    if name in use_usr_bin_env :
        l[0] = '#!/usr/bin/env python\n'
    for count, line in enumerate(l) :
        if line.startswith("PDK_DIR_HERE") :
            l[count] = dir_set % lib_dir.replace('\\','/')
    f.close()

    f=open(fname,"w")
    f.writelines(l)
    f.close()

    # windows versions - we hope to use these everywhere
    # to avoid writing a lot of "if windows: x=x+'.py'"
    f=open(fname+".py","w")
    f.writelines(l)
    f.close()

    if windows :
        # make .bat files too, so the commands can have normal names
        f=open(fname+".bat","w")
        f.write("@echo off\n%s.py %%*\n" % fname)
        f.close()

    os.chmod(fname + '.py', 0755)

#
# The entrypoints file
#
entrypoints = '''
[pytest11]
pandokia = pandokia.helpers.pytest_plugin

[nose.plugins.0.10]
pandokia = pandokia.helpers.nose_plugin:Pdk

'''

# py.test and nose use setuptools to find their plugins, but whenever
# I go near setuptools, it always causes problems for me.  So, the
# procedure here is simple:  Install with distutils, then convert
# the .egg-info file that is installed into a setuptools-compatible
# .egg-info directory that contains the entrypoints definition.
def dorque_egg_info( target ) :
    print "EGG-INFO", target
    pkginfo = open(target).read()
    os.unlink(target)
    os.mkdir(target)
    open(target+"/PKG-INFO","w").write(pkginfo)
    open(target+"/not-zipe-safe","w").close()
    open(target+"/entry_points.txt","w").write(entrypoints)


if 'install' in d.command_obj :
    # they did an install

    # Convert the egg-info to a dir that looks like what setuptools
    # uses.  Set the entry points for use by nose and py.test
    dorque_egg_info( d.command_obj['install_egg_info'].target )

    # find where the scripts went
    script_dir = d.command_obj['install'].install_scripts
    lib_dir    = d.command_obj['install'].install_lib
    print 'scripts went to', script_dir
    print 'python  went to', lib_dir
    # hack the scripts for PDK_DIR_HERE
    for x in python_commands :
        fix_script(x)

    print ''
    print 'Get the CGI from ', os.path.join(script_dir, 'pdk')

    # tell the user 
    print ''
    print 'set path = ( %s $path )' % script_dir
    print 'setenv PYTHONPATH  %s:$PYTHONPATH' % lib_dir
    print ''
    print 'PATH=%s:$PATH'%script_dir
    print "PYTHONPATH=%s:$PYTHONPATH"%lib_dir
    print "export PATH PYTHONPATH"
    print ''
else :
    print "no install"

