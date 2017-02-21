#!/usr/bin/env python
#
# create Eclipse projects corresponding to make targets
#
# note that before you execute this script you need to run:
#
# %CI_ENV_ROOT%\scripts\ci\ci-init-env.bat
# $CI_ENV_ROOT/scripts/ci/ci-init-env.sh
#
# to configure the CI environment and define the following roots:
# DIST_ROOT_DEPS1
# DIST_ROOT_DEPS2
# DIST_ROOT_DEPS3
#
# this script can be used in two different ways:
#
#   1) if you simply run it, the configurations relevant to the OS where you
#   run it from will be output to "bld/makeclipse". On Linux, you will get
#   configurations for the gcc and clang toolchains. On Windows, you will get
#   configurations for vc, including 32-bit and 64-bit builds. This option
#   should be used when the git tree is in the default location
#
#   2) if you specify the -o option (and a directory), the OS-specific
#   configurations will be output in that directory instead. This is useful for
#   pre-populating your Eclipse workspace. For instance,
#   "generate-eclipse-project-config.py -o /home/user/workspace" will
#   pre-populate the workspace located at "/home/user/workspace" with all
#   projects. This option should be used, for example, when the git tree is NOT
#   in the default location and you want your own private version of the
#   projects with references to your own git repository.
#

import argparse;
import posixpath;
import re;
import string;
import sys;

from sys import argv, exit, platform, stderr
from re import search
from os import getenv, makedirs, name, listdir
from os.path import exists, basename, dirname, join, abspath, realpath

def convertToPosixPath( path ):
    if sys.platform.startswith( 'linux' ) or sys.platform.startswith( 'darwin' ):
        return path
    else:
        return path.replace( "\\", "/" ).replace( "c:", "C:" )

distRootDeps1 = getenv('DIST_ROOT_DEPS1')
distRootDeps2 = getenv('DIST_ROOT_DEPS2')
distRootDeps3 = getenv('DIST_ROOT_DEPS3')

if not distRootDeps1 or not distRootDeps2 or not distRootDeps3:
    exit( "ERROR: please first run %CI_ENV_ROOT%\scripts\ci\ci-init-env.bat or $CI_ENV_ROOT/scripts/ci/ci-init-env.sh" )

# the script is expected to be in scripts folder in the source root
srcRoot = convertToPosixPath( dirname( dirname( abspath( argv[ 0 ] ) ) ) )

distRootDeps1 = convertToPosixPath( distRootDeps1 )
distRootDeps2 = convertToPosixPath( distRootDeps2 )
distRootDeps3 = convertToPosixPath( distRootDeps3 )

projectRootDir = "baselib"

#### configurations

linuxBuildConfigurations = {
    'gcc492'   : 'TOOLCHAIN=gcc492',
    'clang35'  : 'TOOLCHAIN=clang35',
    'gcc630'   : 'TOOLCHAIN=gcc630',
    'clang391' : 'TOOLCHAIN=clang391',
    'clang730' : 'TOOLCHAIN=clang730',
}

windowsBuildConfigurations = {
    'vc12'     : 'ARCH=x64 TOOLCHAIN=vc12',
    'vc1232'   : 'ARCH=x86 TOOLCHAIN=vc12',
}

# external dependencies: TODO: this is incomplete, but more importantly it can
# be spit out by the make file process, I just did not have time to write it

externalIncludes = {

    'gcc492'   :
        [
            "&quot;" + distRootDeps3 + "/toolchain-gcc/4.9.2/ub14-x64-gcc492-release/lib/gcc/x86_64-unknown-linux-gnu/4.9.2/include&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/4.9.2/ub14-x64-gcc492-release/lib/gcc/x86_64-unknown-linux-gnu/4.9.2/include-fixed&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/4.9.2/ub14-x64-gcc492-release/include/c++/4.9.2&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/4.9.2/ub14-x64-gcc492-release/include/c++/4.9.2/x86_64-unknown-linux-gnu&quot;",
            "&quot;" + srcRoot + "/src/versioning&quot;",
            "&quot;" + srcRoot + "/src/include&quot;",
            "&quot;" + srcRoot + "/src/local&quot;",
            "&quot;" + srcRoot + "/src/utests/include&quot;",
            "&quot;" + distRootDeps3 + "/boost/1.58.0-devenv2/ub14-x64-gcc492/include&quot;",
            "&quot;" + distRootDeps3 + "/openssl/1.0.2d-devenv2/ub14-x64-gcc492-debug/include&quot;",
            "&quot;" + distRootDeps2 + "/json-spirit/4.08/source&quot;",
        ],

    'clang35' :
        [
            "&quot;" + distRootDeps3 + "/toolchain-clang/3.5/ub14-x64-clang35-release/lib/clang/3.5.0/include&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/4.9.2/ub14-x64-gcc492-release/lib/gcc/x86_64-unknown-linux-gnu/4.9.2/include&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/4.9.2/ub14-x64-gcc492-release/lib/gcc/x86_64-unknown-linux-gnu/4.9.2/include-fixed&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/4.9.2/ub14-x64-gcc492-release/include/c++/4.9.2&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/4.9.2/ub14-x64-gcc492-release/include/c++/4.9.2/x86_64-unknown-linux-gnu&quot;",
            "&quot;" + srcRoot + "/src/versioning&quot;",
            "&quot;" + srcRoot + "/src/include&quot;",
            "&quot;" + srcRoot + "/src/local&quot;",
            "&quot;" + srcRoot + "/src/utests/include&quot;",
            "&quot;" + distRootDeps3 + "/boost/1.58.0-devenv2/ub14-x64-gcc492/include&quot;",
            "&quot;" + distRootDeps3 + "/openssl/1.0.2d-devenv2/ub14-x64-gcc492-debug/include&quot;",
            "&quot;" + distRootDeps2 + "/json-spirit/4.08/source&quot;",
        ],

    'gcc630'   :
        [
            "&quot;" + distRootDeps3 + "/toolchain-gcc/6.3.0/ub16-x64-gcc630-release/lib/gcc/x86_64-pc-linux-gnu/6.3.0/include&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/6.3.0/ub16-x64-gcc630-release/lib/gcc/x86_64-pc-linux-gnu/6.3.0/include-fixed&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/6.3.0/ub16-x64-gcc630-release/include/c++/6.3.0&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/6.3.0/ub16-x64-gcc630-release/include/c++/6.3.0/x86_64-pc-linux-gnu&quot;",
            "&quot;" + srcRoot + "/src/versioning&quot;",
            "&quot;" + srcRoot + "/src/include&quot;",
            "&quot;" + srcRoot + "/src/local&quot;",
            "&quot;" + srcRoot + "/src/utests/include&quot;",
            "&quot;" + distRootDeps3 + "/boost/1.63.0/ub16-x64-gcc630/include&quot;",
			"&quot;" + distRootDeps3 + "/openssl/1.1.0d/source&quot;",
            "&quot;" + distRootDeps3 + "/openssl/1.1.0d/ub16-x64-gcc630-debug/include&quot;",
            "&quot;" + distRootDeps3 + "/json-spirit/4.08/source&quot;",
        ],

    'clang391' :
        [
            "&quot;" + distRootDeps3 + "/toolchain-clang/3.9.1/ub16-x64-clang391-release/lib/clang/3.9.1/include&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/6.3.0/ub16-x64-gcc630-release/lib/gcc/x86_64-pc-linux-gnu/6.3.0/include&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/6.3.0/ub16-x64-gcc630-release/lib/gcc/x86_64-pc-linux-gnu/6.3.0/include-fixed&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/6.3.0/ub16-x64-gcc630-release/include/c++/6.3.0&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-gcc/6.3.0/ub16-x64-gcc630-release/include/c++/6.3.0/x86_64-pc-linux-gnu&quot;",
            "&quot;" + srcRoot + "/src/versioning&quot;",
            "&quot;" + srcRoot + "/src/include&quot;",
            "&quot;" + srcRoot + "/src/local&quot;",
            "&quot;" + srcRoot + "/src/utests/include&quot;",
            "&quot;" + distRootDeps3 + "/boost/1.63.0/ub16-x64-gcc630/include&quot;",
			"&quot;" + distRootDeps3 + "/openssl/1.1.0d/source&quot;",
            "&quot;" + distRootDeps3 + "/openssl/1.1.0d/ub16-x64-gcc630-debug/include&quot;",
            "&quot;" + distRootDeps3 + "/json-spirit/4.08/source&quot;",
        ],

    'clang730' :
        [
            "&quot;" + srcRoot + "/src/versioning&quot;",
            "&quot;" + srcRoot + "/src/include&quot;",
            "&quot;" + srcRoot + "/src/local&quot;",
            "&quot;" + srcRoot + "/src/utests/include&quot;",
            "&quot;" + distRootDeps3 + "/boost/1.63.0/d156-x64-clang730/include&quot;",
			"&quot;" + distRootDeps3 + "/openssl/1.1.0d/source&quot;",
            "&quot;" + distRootDeps3 + "/openssl/1.1.0d/d156-x64-clang730-debug/include&quot;",
            "&quot;" + distRootDeps3 + "/json-spirit/4.08/source&quot;",
        ],

    'vc12'  :
        [
            "&quot;" + srcRoot + "/src/versioning&quot;",
            "&quot;" + srcRoot + "/src/include&quot;",
            "&quot;" + srcRoot + "/src/local&quot;",
            "&quot;" + srcRoot + "/src/utests/include&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-msvc/vc12-update4/default/VC/include&quot;",
            "&quot;" + distRootDeps3 + "/winsdk/8.1/default/Include&quot;",
            "&quot;" + distRootDeps3 + "/openssl/1.0.2d-devenv2/win7-x64-vc12-debug/include&quot;",
            "&quot;" + distRootDeps3 + "/boost/1.58.0-devenv2/win7-x64-vc12/include&quot;",
            "&quot;" + distRootDeps2 + "/json-spirit/4.08/source&quot;",
        ],

    'vc1232':
        [
            "&quot;" + srcRoot + "/src/versioning&quot;",
            "&quot;" + srcRoot + "/src/include&quot;",
            "&quot;" + srcRoot + "/src/local&quot;",
            "&quot;" + srcRoot + "/src/utests/include&quot;",
            "&quot;" + distRootDeps3 + "/toolchain-msvc/vc12-update4/default/VC/include&quot;",
            "&quot;" + distRootDeps3 + "/winsdk/8.1/default/Include&quot;",
            "&quot;" + distRootDeps3 + "/openssl/1.0.2d-devenv2/win7-x86-vc12-debug/include&quot;",
            "&quot;" + distRootDeps3 + "/boost/1.58.0-devenv2/win7-x86-vc12/include&quot;",
            "&quot;" + distRootDeps2 + "/json-spirit/4.08/source&quot;",
        ]
}

monitoredSourceCodeDirectories = [ 'apps', 'plugins', 'utests' ]

# format: project qualified name, desired source code references
projects = {
    'apps/bl-messaging-broker' :  [ "src/include", "src/versioning", "src/local", "notes" ],
    'apps/bl-tool' :  [ "src/include", "src/versioning", "src/local", "notes" ],

    'utests/include' :  [], # an empty set for the source code references means don't generate a configuration
    'utests/utf_baselib' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_async' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_basictask' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_blobtransfer' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_cmdline' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_data' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_http' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_io' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_loader' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_messaging' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_parsing' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_plugin' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_security' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_setprio' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
    'utests/utf_baselib_tasks' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
	'utests/utf_baselib_utils' : [ "src/include", "src/versioning", "src/local", "src/utests/include", "notes" ],
}

#### .project template

dotProjectFileTemplate = """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<projectDescription>
    <name>@projectName@</name>
    <comment></comment>
    <projects>
    </projects>
    <buildSpec>
        <buildCommand>
            <name>org.eclipse.cdt.managedbuilder.core.genmakebuilder</name>
            <triggers>full,incremental,</triggers>
            <arguments>
            </arguments>
        </buildCommand>
        <buildCommand>
            <name>org.eclipse.cdt.managedbuilder.core.ScannerConfigBuilder</name>
            <triggers>full,incremental,</triggers>
            <arguments>
            </arguments>
        </buildCommand>
    </buildSpec>
    <natures>
        <nature>org.eclipse.cdt.core.cnature</nature>
        <nature>org.eclipse.cdt.core.ccnature</nature>
        <nature>org.eclipse.cdt.managedbuilder.core.managedBuildNature</nature>
        <nature>org.eclipse.cdt.managedbuilder.core.ScannerConfigNature</nature>
    </natures>
    <linkedResources>
        @insertLinkedResources@
    </linkedResources>
</projectDescription>""";

#### .cproject template

cDotProjectFileTemplate = """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<?fileVersion 4.0.0?>

<cproject storage_type_id="org.eclipse.cdt.core.XmlProjectDescriptionStorage">
    <storageModule moduleId="org.eclipse.cdt.core.settings">
        <cconfiguration id="cdt.managedbuild.toolchain.gnu.base.1418557620">
            <storageModule buildSystemId="org.eclipse.cdt.managedbuilder.core.configurationDataProvider" id="cdt.managedbuild.toolchain.gnu.base.1418557620" moduleId="org.eclipse.cdt.core.settings" name="Default">
                <externalSettings/>
                <extensions>
                    <extension id="org.eclipse.cdt.core.ELF" point="org.eclipse.cdt.core.BinaryParser"/>
                    <extension id="org.eclipse.cdt.core.GmakeErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.CWDLocator" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.GCCErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.GASErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.GLDErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                    <extension id="org.eclipse.cdt.core.VCErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                </extensions>
            </storageModule>
            <storageModule moduleId="cdtBuildSystem" version="4.0.0">
                <configuration buildProperties="" description="" id="cdt.managedbuild.toolchain.gnu.base.1418557620" artifactName="@projectName@" errorParsers="org.eclipse.cdt.core.GmakeErrorParser;org.eclipse.cdt.core.CWDLocator;org.eclipse.cdt.core.GCCErrorParser;org.eclipse.cdt.core.GASErrorParser;org.eclipse.cdt.core.GLDErrorParser;org.eclipse.cdt.core.VCErrorParser" name="Default" parent="org.eclipse.cdt.build.core.emptycfg">
                    <folderInfo id="cdt.managedbuild.toolchain.gnu.base.1418557620.1099587585" name="/" resourcePath="">
                        <toolChain id="cdt.managedbuild.toolchain.gnu.base.755120562" name="cdt.managedbuild.toolchain.gnu.base" superClass="cdt.managedbuild.toolchain.gnu.base">
                            <targetPlatform archList="all" binaryParser="org.eclipse.cdt.core.ELF" id="cdt.managedbuild.target.gnu.platform.base.127772109" name="Debug Platform" osList="all" superClass="cdt.managedbuild.target.gnu.platform.base"/>
                            <builder arguments="@buildSettingsAndTarget@" buildPath="@topLevelDir@" command="make" enableCleanBuild="false" id="cdt.managedbuild.target.gnu.builder.base.1047112421" incrementalBuildTarget="" keepEnvironmentInBuildfile="false" managedBuildOn="false" name="Gnu Make Builder" parallelBuildOn="true" parallelizationNumber="optimal" superClass="cdt.managedbuild.target.gnu.builder.base"/>
                            <tool id="cdt.managedbuild.tool.gnu.cpp.compiler.base.1986614653" name="GCC C++ Compiler" superClass="cdt.managedbuild.tool.gnu.cpp.compiler.base">
                                <option id="gnu.cpp.compiler.option.include.paths.710363963" superClass="gnu.cpp.compiler.option.include.paths" valueType="includePath">
                                    @insertExternalIncludes@
                                </option>
                                <inputType id="cdt.managedbuild.tool.gnu.cpp.compiler.input.1504746194" superClass="cdt.managedbuild.tool.gnu.cpp.compiler.input"/>
                            </tool>
                        </toolChain>
                    </folderInfo>
                    <sourceEntries>
                        @insertReferences@
                    </sourceEntries>
                </configuration>
            </storageModule>
            <storageModule moduleId="org.eclipse.cdt.core.externalSettings"/>
        </cconfiguration>
    </storageModule>
    <storageModule moduleId="scannerConfiguration">
        <autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>
        <scannerConfigBuildInfo instanceId="cdt.managedbuild.toolchain.gnu.base.1418557620;cdt.managedbuild.toolchain.gnu.base.1418557620.1099587585;cdt.managedbuild.tool.gnu.cpp.compiler.base.815294144;cdt.managedbuild.tool.gnu.cpp.compiler.input.1991193760">
            <autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>
        </scannerConfigBuildInfo>
        <scannerConfigBuildInfo instanceId="cdt.managedbuild.toolchain.gnu.base.1418557620;cdt.managedbuild.toolchain.gnu.base.1418557620.1099587585;cdt.managedbuild.tool.gnu.c.compiler.base.1259566155;cdt.managedbuild.tool.gnu.c.compiler.input.1501200454">
            <autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>
        </scannerConfigBuildInfo>
    </storageModule>
    <storageModule moduleId="org.eclipse.cdt.core.LanguageSettingsProviders"/>
</cproject>""";

def convertToResourceName( path ):
    return path.replace( "/", "-" ) + "-ref"

def insertExternalIncludes( buildConfigurationName, outputFile ):
    for externalInclude in externalIncludes[ buildConfigurationName ]:
        entry = "\t\t\t\t\t\t\t\t\t<listOptionValue builtIn=\"false\" value=\"" + \
                externalInclude + \
                "\"/>"
        entry = entry.replace( "\t", "    " );
        outputFile.write( entry + "\n" );

def insertLinkedResources( projectQualifiedName, outputFile, gitDrive, linkedResources ):
    resources = list( linkedResources )
    resources.append( convertToPosixPath( join( "src", projectQualifiedName ) ) )
    for resource in resources:
        resourceName = convertToResourceName( resource );
        resourceLocation = join( srcRoot, resource );
        linkEntry = "\t\t<link>\n\t\t\t<name>" + \
            resourceName + \
            "</name>\n\t\t\t<type>2</type>\n\t\t\t<location>" + \
            convertToPosixPath( resourceLocation ) + \
            "</location>\n\t\t</link>"
        linkEntry = linkEntry.replace( "\t", "    " );
        outputFile.write( linkEntry + "\n" );

def insertReferences( projectQualifiedName, outputFile, linkedResources ):
    resources = list( linkedResources )
    resources.append( convertToPosixPath( join( "src", projectQualifiedName ) ) )
    for resource in resources:
        resourceName = convertToResourceName( resource );
        entry = "\t\t\t\t\t\t<entry flags=\"VALUE_WORKSPACE_PATH|RESOLVED\" kind=\"sourcePath\" name=\"" + \
            resourceName + \
            "\"/>"
        entry = entry.replace( "\t", "    " );
        outputFile.write( entry + "\n" );

def emitConfigurationFile( buildConfigurationName, projectQualifiedName, projectName, buildSettingsAndTarget, inputFileTemplate, outputFileName, gitDrive, linkedResources ):
    print "INFO: generating file '" + realpath( outputFileName ) + "'..."
    configFile = open( outputFileName, "wb" );
    inputLines = inputFileTemplate.splitlines();
    for inputLine in inputLines:
        # emitting additional configuration
        if re.search( '@insertLinkedResources', inputLine ):
            insertLinkedResources( projectQualifiedName, configFile, gitDrive, linkedResources )
            continue
        if re.search( '@insertReferences', inputLine ):
            insertReferences( projectQualifiedName, configFile, linkedResources )
            continue
        if re.search( '@insertExternalIncludes', inputLine ):
            insertExternalIncludes( buildConfigurationName, configFile )
            continue
        # pure search and replace commands
        if re.search( '@projectName@', inputLine ):
            inputLine = inputLine.replace( "@projectName@", projectName );
        if re.search( '@topLevelDir@', inputLine ):
            inputLine = inputLine.replace( "@topLevelDir@", srcRoot );
        if re.search( '@buildSettingsAndTarget@', inputLine ):
            inputLine = inputLine.replace( "@buildSettingsAndTarget@", buildSettingsAndTarget );
        configFile.write( inputLine + "\n" )
    configFile.close()

def emitProjectConfiguration( projectQualifiedName, buildConfigurationName, buildTarget, gitDrive, linkedResources ):
    projectDirectory = join( join( abspath( outputDirectory ), projectQualifiedName ), buildConfigurationName )
    print "INFO: creating configuration files in '" + realpath( projectDirectory ) + "'..."
    if not exists( projectDirectory ): makedirs( projectDirectory )
    buildSettingsAndTarget = buildConfigurations[ buildConfigurationName ] + " " + buildTarget
    projectName = basename( projectQualifiedName )
    emitConfigurationFile( buildConfigurationName, projectQualifiedName, projectName, buildSettingsAndTarget, dotProjectFileTemplate, join( projectDirectory, ".project" ), gitDrive, linkedResources );
    emitConfigurationFile( buildConfigurationName, projectQualifiedName, projectName, buildSettingsAndTarget, cDotProjectFileTemplate, join( projectDirectory, ".cproject" ), gitDrive, linkedResources );

######################################################## MAIN

parser = argparse.ArgumentParser( description = 'Utility used to emit Eclipse configuration files for make projects' )
parser.add_argument( '-o', '--outputDirectory', help = 'directory where configurations will be output (e.g., your Eclipse workspace)', required = False )

args = vars( parser.parse_args() )

# ensuring we can access the source tree
topDirectory = convertToPosixPath( realpath( join( join( join( dirname( argv[ 0 ] ), ".." ) ) ) ) )
gitDrive = convertToPosixPath( realpath( join( topDirectory, ".." ) ) )
print "INFO: project configurations will point to your git repository at '" + realpath( gitDrive ) + "'..."
sourceCodeDirectory = convertToPosixPath( join( topDirectory, "src" ) )
if not sourceCodeDirectory:
    exit( "ERROR: unable to locate the source code tree. Please run this script from its location in the source tree" )

print "INFO: top-level directory '" + topDirectory + "'..."
print "INFO: source code directory '" + sourceCodeDirectory + "'..."

# ensuring all pre-configured processes still exist as do their code references
validConfiguration = True
for project, projectLinkedResources in projects.iteritems():
    projectSourceCodeDirectory = join( sourceCodeDirectory, project )
    if not exists( projectSourceCodeDirectory ):
        print "ERROR: project source code directory '" + projectSourceCodeDirectory + "' doesn't appear to exist - the source tree might have changed..."
        validConfiguration = False
    for projectLinkedResource in projectLinkedResources:
        projectLinkedResourceDirectory = join( topDirectory, projectLinkedResource )
        if not exists( projectLinkedResourceDirectory ):
            print "ERROR: resource '" + projectLinkedResourceDirectory + "' linked to project '" + project + "' doesn't appear to exist - the source tree might have changed..."
            validConfiguration = False

if not validConfiguration:
    exit( "ERROR: errors validating this script's configuration, cannot proceed..." )

# ensuring we have configurations for all projects in the source tree
validConfiguration = True
for monitoredSourceCodeDirectory in monitoredSourceCodeDirectories:
    monitoredSourceCodeDirectoryPath = join( sourceCodeDirectory, monitoredSourceCodeDirectory )
    if not exists( monitoredSourceCodeDirectoryPath ):
        continue

    for fileEntry in listdir( monitoredSourceCodeDirectoryPath ):
        project = monitoredSourceCodeDirectory + "/" + fileEntry
        if not project in projects:
            print "ERROR: project '" + project + "' is not known, please add it to this script (i.e., update this script's 'projects' variable)..."
            validConfiguration = False

if not validConfiguration:
    exit( "ERROR: errors validating this script's configuration, cannot proceed..." )

outputDirectory = args[ "outputDirectory" ]
if not outputDirectory:
    outputDirectory = join( join( join( dirname( argv[ 0 ] ), ".." ), "bld" ), "makeclipse" )
    if not exists(outputDirectory):
        makedirs(outputDirectory)

if not exists( outputDirectory ): makedirs( outputDirectory )

print "INFO: outputting configuration files to '" + outputDirectory + "'..."

if sys.platform.startswith( 'linux' ) or sys.platform.startswith( 'darwin' ):
    buildConfigurations = linuxBuildConfigurations
else:
    buildConfigurations = windowsBuildConfigurations

for project, projectLinkedResources in projects.iteritems():
    print "INFO: producing configurations for project '" + project + "'..."
    if not len( projectLinkedResources ):
        continue;
    for buildConfiguration in buildConfigurations:
        print "INFO: producing '" + buildConfiguration + "' configuration for '" + project + "'..."
        emitProjectConfiguration( project, buildConfiguration, basename( project ), gitDrive, projectLinkedResources )
    print ""

exit( 0 );
