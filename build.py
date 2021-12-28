#!/usr/bin/env python3
#
# Author: Andreas Pohl

import os, argparse, sys, shutil, glob, subprocess, zipfile

def execute(cmd):
    print('>>> ' + cmd)
    if os.system(cmd) != 0:
        sys.exit(1)

def executeReadOutput(cmd):
    print('>>> ' + cmd)
    return subprocess.run(cmd.split(' '), stdout=subprocess.PIPE).stdout.decode('utf8').rstrip()

def getPlatform(args):
    if args is not None and args.platform is not None:
        return args.platform
    if sys.platform == 'darwin':
        return 'macos'
    elif sys.platform in ('linux', 'linux2'):
        return 'linux'
    elif sys.platform == 'win32':
        return 'windows'

def getArch(args):
    if args is not None and args.arch is not None:
        return args.arch
    platform = getPlatform(args)
    if platform in ('macos', 'linux'):
        return executeReadOutput('uname -m')
    else:
        return 'x86_64'

def getPlatformArch(args):
    platform = getPlatform(args)
    s = platform
    if platform == 'macos':
        s += '-' + args.macostarget
    s += '-' + getArch(args)
    return s

def getBuildDir(args):
    if args.builddir is not None:
        return args.builddir
    else:
        return 'build-' + getPlatformArch(args)

def getDebugSymDir(args):
    return 'debug-sym-' + getPlatformArch(args)

def getDepsDir(args):
    depsDir = args.depsroot
    if depsDir is None:
        depsDir = os.environ.get('AG_DEPS_ROOT')
    if not os.path.isdir(depsDir):
        print('error: ' + depsDir + ' does not exist')
        exit(1)
    return os.path.abspath(depsDir).replace('\\', '/') + '/' + getPlatformArch(args)

def getSDKsDir(args):
    sdksDir = args.sdksroot
    if sdksDir is None:
        depsDir = os.environ.get('AG_SDKS_ROOT')
    if not os.path.isdir(sdksDir):
        return None
    return os.path.abspath(sdksDir).replace('\\', '/')

def getVersion():
    if not os.path.isfile('package/VERSION'):
        if getPlatform(None) == 'macos':
            execute('package/setversion.sh')
        else:
            print('can only set version from macos')
            return 'dev-build'
    with open('package/VERSION') as f:
        return f.readline().rstrip()
    return ''


def getMacToolchain(args):
    toolchain = ''
    if args.macostarget in ('10.7', '10.8'):
        toolchain = '/Library/Developer/10/CommandLineTools'
    elif args.macostarget == '11.1':
        toolchain = '/Library/Developer/CommandLineTools'
    return toolchain

def setMacToolchain(args, toolchain=None):
    newToolchain = getMacToolchain(args)
    if toolchain is not None:
        newToolchain = toolchain
    lastToolchain = executeReadOutput('xcode-select -p')
    if newToolchain != lastToolchain and os.path.isdir(newToolchain):
        print('required toolchain not selected, trying xcode-select')
        execute('sudo xcode-select -s ' + newToolchain)
    else:
        newToolchain = lastToolchain
    print('Using toolchain: ' + newToolchain)
    return (newToolchain, lastToolchain, newToolchain + '/SDKs/MacOS.sdk')

def conf(args):
    cmake_params = []

    platform = getPlatform(args)
    buildDir = getBuildDir(args)
    depsDir = getDepsDir(args)
    sdksDir = getSDKsDir(args)

    if os.path.isdir(buildDir):
        shutil.rmtree(buildDir, ignore_errors=True)

    os.makedirs(buildDir + '/bin', exist_ok=True)

    cmake_params.append('-B ' + buildDir)
    cmake_params.append('-DAG_DEPS_ROOT=' + depsDir)

    if sdksDir is not None:
        cmake_params.append('-DAG_SDKS_ROOT=' + sdksDir)
        cmake_params.append('-DAG_VST2_PLUGIN_ENABLED=ON')
        if platform in ('macos', 'windows'):
            cmake_params.append('-DAG_AAX_PLUGIN_ENABLED=ON')

    if platform in ('macos', 'linux'):
        cmake_params.append('-DCMAKE_BUILD_TYPE=' + args.buildtype)
        shutil.copy(depsDir + '/bin/crashpad_handler', buildDir + '/bin')

    if platform == 'macos':
        cmake_params.append('-DCMAKE_OSX_ARCHITECTURES=' + getArch(args))
        cmake_params.append('-DAG_MACOS_TARGET=' + args.macostarget)
        if not args.nosigning:
            execute('codesign --force --sign AudioGridder --timestamp=none ' + buildDir + '/bin/crashpad_handler')

    if platform == 'windows':
        cmake_params.append('-A x64')
        shutil.copy(depsDir + '/bin/crashpad_handler.exe', buildDir + '/bin')

    if args.sentrydsn is not None:
        cmake_params.append('-DAG_ENABLE_SENTRY=ON')
        cmake_params.append('-DAG_SENTRY_DSN=' + args.sentrydsn)
    if args.nosigning:
        cmake_params.append('-DAG_ENABLE_CODE_SIGNING=OFF')
    if args.disablecopystep:
        cmake_params.append('-DAG_ENABLE_DEBUG_COPY_STEP=OFF')

    cmake_command = 'cmake ' + ' '.join(cmake_params)
    execute(cmake_command)

def build(args):
    cmake_params = []

    platform = getPlatform(args)
    buildDir = getBuildDir(args)

    cmake_params.append('--build ' + buildDir)

    if platform == 'windows':
        cmake_params.append('--config ' + args.buildtype)

    if args.parallel:
        cmake_params.append('--parallel')
    elif args.jobs > 0:
        cmake_params.append('-j ' + str(args.jobs))

    if not args.target is None:
        cmake_params.append('--target ' + args.target)

    if args.clean:
        cmake_params.append('--clean-first')

    if args.verbose:
        cmake_params.append('--verbose')

    cmake_command = 'cmake ' + ' '.join(cmake_params)
    execute(cmake_command)

    if args.debugsymbols:
        debSymDir = getDebugSymDir(args)

        print('Creating debug symbols in ' + debSymDir + ' ...')

        if os.path.isdir(debSymDir):
            shutil.rmtree((debSymDir))

        os.mkdir(debSymDir)

        if platform == 'macos':
            shutil.copytree(buildDir + '/lib', debSymDir + '/lib')
            shutil.copytree(buildDir + '/bin', debSymDir + '/bin')
            if not os.path.isdir(debSymDir + '/AU'):
                os.mkdir(debSymDir + '/AU')
            if not os.path.isdir(debSymDir + '/VST'):
                os.mkdir(debSymDir + '/VST')
            if not os.path.isdir(debSymDir + '/VST3'):
                os.mkdir(debSymDir + '/VST3')
            if not os.path.isdir(debSymDir + '/AAX'):
                os.mkdir(debSymDir + '/AAX')
            execute('dsymutil -o ' + debSymDir + '/AudioGridderServer.dSYM '     + buildDir + '/bin/AudioGridderServer.app/Contents/MacOS/AudioGridderServer')
            execute('dsymutil -o ' + debSymDir + '/AudioGridderPluginTray.dSYM ' + buildDir + '/bin/AudioGridderPluginTray.app/Contents/MacOS/AudioGridderPluginTray')
            execute('dsymutil -o ' + debSymDir + '/AU/AudioGridder.dSYM '        + buildDir + '/lib/AudioGridder.component/Contents/MacOS/AudioGridder')
            execute('dsymutil -o ' + debSymDir + '/AU/AudioGridderInst.dSYM '    + buildDir + '/lib/AudioGridderInst.component/Contents/MacOS/AudioGridderInst')
            execute('dsymutil -o ' + debSymDir + '/AU/AudioGridderMidi.dSYM '    + buildDir + '/lib/AudioGridderMidi.component/Contents/MacOS/AudioGridderMidi')
            execute('dsymutil -o ' + debSymDir + '/VST/AudioGridder.dSYM '       + buildDir + '/lib/AudioGridder.vst/Contents/MacOS/AudioGridder')
            execute('dsymutil -o ' + debSymDir + '/VST/AudioGridderInst.dSYM '   + buildDir + '/lib/AudioGridderInst.vst/Contents/MacOS/AudioGridderInst')
            execute('dsymutil -o ' + debSymDir + '/VST/AudioGridderMidi.dSYM '   + buildDir + '/lib/AudioGridderMidi.vst/Contents/MacOS/AudioGridderMidi')
            execute('dsymutil -o ' + debSymDir + '/VST3/AudioGridder.dSYM '      + buildDir + '/lib/AudioGridder.vst3/Contents/MacOS/AudioGridder')
            execute('dsymutil -o ' + debSymDir + '/VST3/AudioGridderInst.dSYM '  + buildDir + '/lib/AudioGridderInst.vst3/Contents/MacOS/AudioGridderInst')
            execute('dsymutil -o ' + debSymDir + '/VST3/AudioGridderMidi.dSYM '  + buildDir + '/lib/AudioGridderMidi.vst3/Contents/MacOS/AudioGridderMidi')
            execute('dsymutil -o ' + debSymDir + '/AAX/AudioGridder.dSYM '       + buildDir + '/lib/AudioGridder.aaxplugin/Contents/MacOS/AudioGridder')
            execute('dsymutil -o ' + debSymDir + '/AAX/AudioGridderInst.dSYM '   + buildDir + '/lib/AudioGridderInst.aaxplugin/Contents/MacOS/AudioGridderInst')
            execute('dsymutil -o ' + debSymDir + '/AAX/AudioGridderMidi.dSYM '   + buildDir + '/lib/AudioGridderMidi.aaxplugin/Contents/MacOS/AudioGridderMidi')

        elif platform == 'linux':
            shutil.copy(buildDir + '/bin/AudioGridderPluginTray', debSymDir)
            for f in glob.glob(buildDir + '/lib/*.so'):
                shutil.copy(f, debSymDir)

        elif platform == "windows":
            shutil.copy(buildDir + '/bin/AudioGridderServer.exe', debSymDir)
            shutil.copy(buildDir + '/bin/AudioGridderServer.pdb', debSymDir)
            shutil.copy(buildDir + '/bin/AudioGridderPluginTray.exe', debSymDir)
            shutil.copy(buildDir + '/bin/AudioGridderPluginTray.pdb', debSymDir)
            if not os.path.isdir(debSymDir + '/VST'):
                os.mkdir(debSymDir + '/VST')
            if not os.path.isdir(debSymDir + '/VST3'):
                os.mkdir(debSymDir + '/VST3')
            if not os.path.isdir(debSymDir + '/AAX'):
                os.mkdir(debSymDir + '/AAX')
            for f in glob.glob(buildDir + '/lib/VST/*.pdb') + glob.glob(buildDir + '/lib/VST/*.dll'):
                shutil.copy(f, debSymDir + '/VST')
            for f in glob.glob(buildDir + '/lib/VST3/*.pdb') + glob.glob(buildDir + '/lib/VST3/*.vst3'):
                shutil.copy(f, debSymDir + '/VST3')
            for f in glob.glob(buildDir + '/lib/AAX/*.pdb') + glob.glob(buildDir + '/lib/AAX/*.aaxplugin'):
                shutil.copy(f, debSymDir + '/AAX')

def packs(args):
    version = getVersion()
    macPackages = []
    macBuildDirs = []
    for p in args.platforms.split(','):
        platform = None
        arch = None
        macosTarget = None
        buildDir = None
        parts = p.split('-')
        platform = parts[0]
        if len(parts) < 3:
            arch = parts[1]
            buildDir = 'build-' + platform + '-' + arch
        else:
            macosTarget = parts[1]
            arch = parts[2]
            buildDir = 'build-' + platform + '-' + macosTarget + '-' + arch
        packReal(platform, arch, version, buildDir, macosTarget)

        if args.macosarchives:
            serverName = 'package/build/AudioGridderServer_' + version + '_macOS'
            pluginName = 'package/build/AudioGridderPlugin_' + version + '_macOS'
            if macosTarget == '10.7':
                serverName += '-10.7'
                pluginName += '-10.7'
            serverName += '-' + arch + '.pkg'
            pluginName += '-' + arch + '.pkg'
            macPackages.append(serverName)
            macPackages.append(pluginName)
            macBuildDirs.append(buildDir)

    if args.macosarchives:
        execute('zip -j -9 package/build/AudioGridder_' + version + '-MacOS-Installers.zip ' + ' '.join(macPackages))
        execute('zip -r -9 package/build/AudioGridder_' + version + '-MacOS.zip ' + ' '.join(macBuildDirs) + \
                ' -i "*/bin/*" "*/lib/*" -x "*.a"')

def pack(args):
    packReal(getPlatform(args), getArch(args), getVersion(), getBuildDir(args), args.macostarget)

def packReal(platform, arch, version, buildDir, macosTarget):
    if platform == 'macos':
        if arch == 'universal':
            execute('package/createUniversalBinaries.sh')

        serverProject = 'package/AudioGridderServer-'
        serverTarget = 'package/build/AudioGridderServer_' + version + '_macOS'
        pluginProject = 'package/AudioGridderPlugin-'
        pluginTarget = 'package/build/AudioGridderPlugin_' + version + '_macOS'
        if macosTarget == '10.7':
            serverProject += '10.7-'
            serverTarget += '-10.7'
            pluginProject += '10.7-'
            pluginTarget += '-10.7'
        serverProject += arch + '.pkgproj'
        serverTarget += '-' + arch + '.pkg'
        pluginProject += arch + '.pkgproj'
        pluginTarget += '-' + arch + '.pkg'

        execute('packagesbuild --package-version "' + version + '" ' + serverProject)
        execute('packagesbuild --package-version "' + version + '" ' + pluginProject)

        os.rename('package/build/AudioGridderServer.pkg', serverTarget)
        os.rename('package/build/AudioGridderPlugin.pkg', pluginTarget)

    elif platform == 'windows':
        os.chdir('package')
        execute('"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Obuild AudioGridderServer.iss')
        execute('"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Obuild AudioGridderPlugin.iss')

        print('creating installers archive...')
        os.chdir('build')
        with zipfile.ZipFile('AudioGridder_' + version + '-Windows-Installers.zip', 'w',
                             compression=zipfile.ZIP_DEFLATED) as zf:
            zf.write('AudioGridderServer_' + version + '.exe')
            zf.write('AudioGridderPlugin_' + version + '.exe')
            zf.close()

        print('creating no-installers archive...')
        os.chdir('../../' + buildDir)
        with zipfile.ZipFile('../package/build/AudioGridder_' + version + '-Windows.zip', 'w',
                             compression=zipfile.ZIP_DEFLATED) as zf:
            for f in glob.glob('lib/*/*.dll') + \
                glob.glob('lib/*/*.vst3') + \
                glob.glob('lib/*/*.aaxplugin') + \
                glob.glob('bin/*.exe'):
                zf.write(f)
            zf.close()

    elif platform == 'linux':
        os.makedirs('package/build/linux/vst', exist_ok=True)
        os.makedirs('package/build/linux/vst3/AudioGridder.vst3/Contents/x86_64-linux', exist_ok=True)
        os.makedirs('package/build/linux/vst3/AudioGridderInst.vst3/Contents/x86_64-linux', exist_ok=True)
        os.makedirs('package/build/linux/vst3/AudioGridderMidi.vst3/Contents/x86_64-linux', exist_ok=True)
        os.makedirs('package/build/linux/bin', exist_ok=True)

        print('copying vst plugins...')
        for src in glob.glob(buildDir + '/lib/lib*.so'):
            dst = src.replace(buildDir + '/lib/lib', 'package/build/linux/vst/')
            shutil.copyfile(src, dst)
        print('copying vst3 plugins...')
        for src in glob.glob(buildDir + '/lib/AudioGridder*.so'):
            name = src.replace(buildDir + '/lib/', '').replace('.so', '')
            dst = 'package/build/linux/vst3/' + name + '.vst3/Contents/x86_64-linux/' + name + '.so'
            shutil.copyfile(src, dst)
        print('copying binaries...')
        shutil.copy(buildDir + '/bin/AudioGridderPluginTray', 'package/build/linux/bin')
        shutil.copy(buildDir + '/bin/crashpad_handler', 'package/build/linux/bin')

        os.chdir('package/build/linux')
        execute('zip -r ../AudioGridder_' + version + '-Linux.zip vst vst3 bin')
        execute('zip -j ../AudioGridder_' + version + '-Linux.zip ../../install-trayapp-linux.sh')

        os.chdir('../../..')
        shutil.rmtree('package/build/linux', ignore_errors=True)

def upload(args):
    sentry_params = []
    sentry_params.append('--auth-token ' + args.sentryauth)
    sentry_params.append('upload-dif -o e47 -p audiogridder')
    sentry_params.append(getDebugSymDir(args))

    sentry_command = 'sentry-cli ' + ' '.join(sentry_params)
    execute(sentry_command)

def archive(args):
    platform = getPlatform(args)
    version = getVersion()

    targetDir = args.dest + '/Builds/' + version
    if not os.path.isdir(targetDir):
        os.makedirs(targetDir)

    debSymDir = getDebugSymDir(args)

    if not os.path.isdir(debSymDir):
        print('error: ' + debSymDir + ' does not exists')
        exit(1)

    print('copying ' + debSymDir + ' -> ' + targetDir + ' ...')
    if os.path.isdir(targetDir + '/' + debSymDir):
        shutil.rmtree(targetDir + '/' + debSymDir)
    shutil.copytree(debSymDir, targetDir + '/' + debSymDir)

def main():
    parser = argparse.ArgumentParser(description='AudioGridder Build Script.')
    subparsers = parser.add_subparsers(title='Commands', dest='mode',
                                       help='Available commands, use <command> -h for more info')

    defaultPlatform = getPlatform(None)
    defaultArch = getArch(None)

    parser_conf = subparsers.add_parser('conf', help='Configure the build system')
    parser_conf.add_argument('-t', '--type', dest='buildtype', metavar='TYPE', type=str, default='RelWithDebInfo',
                             help='Build type (default: %(default)s)')
    parser_conf.add_argument('-b', '--build-dir', dest='builddir', metavar='DIR', type=str, default=None,
                             help='Override the build directory (default: %(default)s)')
    parser_conf.add_argument('--platform', dest='platform', metavar='PLATFORM', type=str, default=defaultPlatform,
                             choices=['macos', 'windows', 'linux'],
                             help='OS platform (default: %(default)s)')
    parser_conf.add_argument('--arch', dest='arch', metavar='ARCH', type=str, default=defaultArch,
                             help='Processor architecture (default: %(default)s)')
    parser_conf.add_argument('--macos-target', dest='macostarget', metavar='TRGT', type=str, default='10.8',
                             help='MacOS deplyoment target (default: %(default)s)')
    parser_conf.add_argument('--macos-toolchain', dest='macostoolchain', metavar='TC', type=str, default=None,
                             help='Use specific MacOS build toolchain (default: %(default)s)')
    parser_conf.add_argument('--sentry-dsn', dest='sentrydsn', metavar='DSN', type=str, default=None,
                             help='Sentry DSN for crash reporting (default: %(default)s)')
    parser_conf.add_argument('--disable-signing', dest='nosigning', action='store_true', default=False,
                             help='Disable code signing (default: %(default)s)')
    parser_conf.add_argument('--disable-copy-step', dest='disablecopystep', action='store_true', default=False,
                             help='Disable copying plugins into plugin folders in Debug mode on MacOS (default: %(default)s)')
    parser_conf.add_argument('--deps-root', dest='depsroot', type=str, default='audiogridder-deps',
                             help='Dependencies root directory (git clone https://github.com/apohl79/audiogridder-deps.git)')
    parser_conf.add_argument('--sdks-root', dest='sdksroot', type=str, default='audiogridder-sdks',
                             help='AAX & VST2 SDK root directory (Non public, git clone https://github.com/apohl79/audiogridder-sdks.git)')

    parser_build = subparsers.add_parser('build', help='Build the binaries')
    parser_build.add_argument('-t', '--type', dest='buildtype', metavar='TYPE', type=str, default='RelWithDebInfo',
                              help='Build type for Windows builds (default: %(default)s)')
    parser_build.add_argument('-b', '--build-dir', dest='builddir', metavar='DIR', type=str, default=None,
                              help='Override the build directory (default: %(default)s)')
    parser_build.add_argument('--platform', dest='platform', metavar='PLATFORM', type=str, default=defaultPlatform,
                              choices=['macos', 'windows', 'linux'],
                              help='OS platform (default: %(default)s)')
    parser_build.add_argument('--arch', dest='arch', metavar='ARCH', type=str, default=defaultArch,
                              help='Processor architecture (default: %(default)s)')
    parser_build.add_argument('--macos-target', dest='macostarget', metavar='TRGT', type=str, default='10.8',
                              help='MacOS deplyoment target (default: %(default)s)')
    parser_build.add_argument('--create-debug-symbols', dest='debugsymbols', action='store_true', default=False,
                              help='Create debug symbols for uploading to sentry (default: %(default)s)')
    parser_build.add_argument('--target', dest='target', type=str,
                              help='Set a specific build target')
    parser_build.add_argument('-c', '--clean', dest='clean', action='store_true', default=False,
                              help='Run cleanup before building')
    parser_build.add_argument('-p', '--parallel', dest='parallel', action='store_true', default=False,
                              help='Use multiple CPU cores (default: %(default)s)')
    parser_build.add_argument('-j', '--jobs', dest='jobs', metavar='N', type=int, default=0,
                              help='Use N CPU cores')
    parser_build.add_argument('-v', '--verbose', dest='verbose', action='store_true', default=False,
                              help='Show compile/link commands (default: %(default)s)')

    parser_pack = subparsers.add_parser('pack', help='Create installer packages')
    parser_pack.add_argument('-b', '--build-dir', dest='builddir', metavar='DIR', type=str, default=None,
                             help='Override the build directory (default: %(default)s)')
    parser_pack.add_argument('--platform', dest='platform', metavar='PLATFORM', type=str, default=defaultPlatform,
                             choices=['macos', 'windows', 'linux'],
                             help='OS platform (default: %(default)s)')
    parser_pack.add_argument('--arch', dest='arch', metavar='ARCH', type=str, default=defaultArch,
                             help='Processor architecture (default: %(default)s)')
    parser_pack.add_argument('--macos-target', dest='macostarget', metavar='TRGT', type=str, default='10.8',
                             help='MacOS deplyoment target (default: %(default)s)')

    parser_packs = subparsers.add_parser('packs', help='Create installer packages for multiple platforms')
    parser_packs.add_argument('--platforms', dest='platforms', metavar='PLATFORMS', type=str, required=True,
                              help='List of OS platforms in the form <os>[-version]-<arch> separated by comma')
    parser_packs.add_argument('--create-macos-archives', dest='macosarchives', action='store_true', default=False,
                              help='Create archives for all MacOS targets')

    parser_upload = subparsers.add_parser('upload', help='Upload debug symbols to sentry')
    parser_upload.add_argument('-b', '--build-dir', dest='builddir', metavar='DIR', type=str, default=None,
                               help='Override the build directory (default: %(default)s)')
    parser_upload.add_argument('--platform', dest='platform', metavar='PLATFORM', type=str, default=defaultPlatform,
                               choices=['macos', 'windows', 'linux'],
                               help='OS platform (default: %(default)s)')
    parser_upload.add_argument('--arch', dest='arch', metavar='ARCH', type=str, default=defaultArch,
                               help='Processor architecture (default: %(default)s)')
    parser_upload.add_argument('--macos-target', dest='macostarget', metavar='TRGT', type=str, default='10.8',
                               help='MacOS deplyoment target (default: %(default)s)')
    parser_upload.add_argument('--sentry-auth-token', dest='sentryauth', metavar='AUTH', type=str, required=True,
                               help='Sentry API auth token')

    parser_archive = subparsers.add_parser('archive', help='Archive binaries and debug symbols')
    parser_archive.add_argument('-b', '--build-dir', dest='builddir', metavar='DIR', type=str, default=None,
                                help='Override the build directory (default: %(default)s)')
    parser_archive.add_argument('--platform', dest='platform', metavar='PLATFORM', type=str, default=defaultPlatform,
                                choices=['macos', 'windows', 'linux'],
                                help='OS platform (default: %(default)s)')
    parser_archive.add_argument('--arch', dest='arch', metavar='ARCH', type=str, default=defaultArch,
                                help='Processor architecture (default: %(default)s)')
    parser_archive.add_argument('--macos-target', dest='macostarget', metavar='TRGT', type=str, default='10.8',
                                help='MacOS deplyoment target (default: %(default)s)')
    parser_archive.add_argument('--dest', dest='dest', metavar='DIR', type=str, required=True,
                                help='Destination directory')

    args = parser.parse_args()

    newToolchain = ''
    lastToolchain = ''

    if getPlatform(args) == 'macos':
        if args.arch == 'arm64':
            args.macostarget = '11.1'
        if args.mode == 'conf':
            (newToolchain, lastToolchain, sysroot) = setMacToolchain(args, args.macostoolchain)

    if args.mode == 'conf':
        conf(args)
    elif args.mode == 'build':
        build(args)
    elif args.mode == 'pack':
        pack(args)
    elif args.mode == 'packs':
        packs(args)
    elif args.mode == 'upload':
        upload(args)
    elif args.mode == 'archive':
        archive(args)
    else:
        parser.print_usage()

    if getPlatform(args) == 'macos' and newToolchain != lastToolchain:
        execute('sudo xcode-select -s ' + lastToolchain)

if __name__ == "__main__":
    main()
