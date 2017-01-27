#!/usr/bin/env python
#
# run msvs compiler with /showincludes and generate make dependencies
#

from optparse import OptionParser, BadOptionError
from os.path import basename, splitext
from subprocess import Popen, PIPE, STDOUT
from sys import argv

# an options parser that will pass-through unrecognized options
class PassThroughOptionParser(OptionParser):
  def _process_long_opt(self, rargs, values):
    arg = rargs[0]
    try:
      OptionParser._process_long_opt(self, rargs, values)
    except BadOptionError, err:
      self.largs.append(arg)

  def _process_short_opts(self, rargs, values):
    arg = rargs[0]
    try:
      OptionParser._process_short_opts(self, rargs, values)
    except BadOptionError, err:
      self.largs.append(arg)

# callback for dependency option, adds -showIncludes to the command
def handle_dependency_option(option, opt, value, parser):
  setattr(parser.values, option.dest, True)
  parser.largs.append('-showIncludes')

# callback for extracting the target from the output option (-Fo)
def handle_output_options(option, opt, value, parser):
  # option parse watches for -F
  # everything else is the option value
  # we only need to take special action on -Fo
  if value[0]=='o':
    setattr(parser.values, option.dest, value[1:])
    setattr(parser.values, 'depfile', '%s.d' % (splitext(value[1:])[0],))
  parser.largs.append(opt + value)

parser = PassThroughOptionParser()
parser.add_option('-M',
          action='callback',
          callback=handle_dependency_option,
          dest='dependencies',
          help='write dependency file')
parser.add_option('-F',
          action='callback',
          callback=handle_output_options,
          type='string', dest='target',
          help='standard MSVC cl.exe output options')

# parse options
(options, args) = parser.parse_args()

# if not output option is specified, attempt to infer
# the target from the first non-option argument
if options.dependencies and not options.target:
  firstNonOpt = filter(lambda a: a[0] not in ('-', '/'), args)[0]
  options.target = '%s.obj' % (splitext(firstNonOpt)[0],)
  
# insert the compiler command (basename of this script)
args.insert(0, splitext(basename(argv[0]))[0])

# run the command tracking dependencies
deps = []
p = Popen(args, stdout=PIPE, stderr=STDOUT)
for line in p.stdout:
  line = line.rstrip()
  if line.startswith('Note: including file:'):
    dep = line.split()[-1]
    if dep not in deps:
      deps.append(dep)
  else:
    print line
p.wait()

# if successful, write the dependency file
if p.returncode == 0 and options.dependencies:
  f = open('%s.d' % splitext(options.target)[0], 'wt')
  f.writelines([options.target, ': \\\n' ] +
               [' %s \\\n' % (dep,) for dep in deps])
  f.writelines(['\n\n' ] +
               ['%s:\n' % (dep,) for dep in deps])
  f.close()

exit(p.returncode)
