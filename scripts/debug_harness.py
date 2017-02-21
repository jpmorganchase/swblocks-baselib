#!/usr/bin/env python
#
# run programs and capture diagnostics on failure
#
# on linux, core dump must be enabled
# see /etc/security/limits.conf and maybe /etc/profile
#
# on windows, crash dumps must be enabled per
# http://msdn.microsoft.com/en-us/library/windows/desktop/bb787181%28v=vs.85%29.aspx
# also, see settings/windows/enable-user-mode-dumps.reg
#

from sys import argv, exit, platform, stderr
from re import search
from subprocess import Popen, call, PIPE, STDOUT
from os import rename, getenv
from os.path import exists, basename, dirname, join

# per-os configuration
cfg = {
  'linux2': {
    'debugger': ['gdb', '--batch', '-ex', 'thread apply all bt',
                 '-ex', 'quit', '%(testfile)s', '%(dumpfile)s'],
    'dumpfile': 'core.%(pid)d'
  },
  'darwin': {
    'debugger': ['gdb', '--batch', '-ex', 'thread apply all bt',
                 '-ex', 'quit', '%(testfile)s', '%(dumpfile)s'],
    'dumpfile': 'core.%(pid)d'
  },
  'win32': {
    'debugger': ['cdb', '-c', '!analyze -v; q',
                 '-z', '%(dumpfile)s'],
    'dumpfile': join(getenv('LOCALAPPDATA', ''),
                     'CrashDumps', '%(testname)s.%(pid)d.dmp')
  }
}

def handle_failure(proc):
  # setup parameters
  testdir = dirname(argv[1])
  params = { 'testdir': testdir, 'testname': basename(argv[1]),
             'testfile': argv[1], 'pid': proc.pid }
  dumpfile = cfg[platform]['dumpfile'] % params
  params['dumpfile'] = dumpfile

  # locate dumpfile
  if exists(dumpfile):
    print '!' * 5, 'crash dump file', dumpfile, 'found'

    # replace formatting strings in debugger command line
    debugger = [a % params for a in cfg[platform]['debugger']]

    # run debugger to dump stack trace to stdout
    call(debugger)

    newfile = join(testdir, basename(dumpfile))
    print '!' * 5, 'crash dump file upload path is', newfile

    # move dump file to build directory for upload
    rename(dumpfile, newfile)

def process_output(proc):
  for line in proc.stdout:
    line = line.rstrip()
    if search(': (fatal|error|warn)', line) and not line.startswith('DEBUG:'):
      print >>stderr, str('\n######### Failure in %s #########\n %s \n' %(basename(argv[1]), line))
      print >>stderr, 'Please see full error details in the log file (search for the test name)\n'
    print line
  proc.wait()

# run command and deal with failures
p = Popen(argv[1:], stdout=PIPE, stderr=STDOUT)
process_output(p)
if p.returncode:
  handle_failure(p)
exit(p.returncode)
