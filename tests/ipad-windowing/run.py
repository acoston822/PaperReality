#!/usr/bin/env python3
"""Run an isolated SDL iPad windowing diagnostic, without game code or assets."""
import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native-ipad-only', action='store_true',
                    help='Verify native iPad mode and SDL sizing without rerunning the compatibility comparisons.')
args = parser.parse_args()

root = Path(__file__).resolve().parents[2]
out = root / 'build-ipad-windowing'
out.mkdir(exist_ok=True)


def run(*args, **kwargs):
    kwargs.setdefault('timeout', 90)
    return subprocess.check_output(args, text=True, **kwargs).strip()


def logged(name, args, timeout=600, env=None):
    # Keep progress in the log. Actions caps annotations per step; reserve
    # notices for the runtime limitation and the three actual measurements.
    print('Starting ' + name, flush=True)
    path = out / (name + '.log')
    with path.open('w') as log:
        result = subprocess.run(args, stdout=log, stderr=subprocess.STDOUT,
                                timeout=timeout, env=env)
    text = path.read_text(errors='replace')
    for line in text.splitlines():
        if 'IPAD_WINDOWING_OBSERVATION' in line:
            print(line, flush=True)
    if result.returncode:
        print(text[-14000:], flush=True)
        raise RuntimeError(f'{name} failed: exit {result.returncode}')
    return text


print(run('xcodebuild', '-version'), flush=True)
runtimes = json.loads(run('xcrun', 'simctl', 'list', 'runtimes', '-j'))['runtimes']
available = [runtime for runtime in runtimes if runtime.get('isAvailable')
             and runtime['identifier'].startswith('com.apple.CoreSimulator.SimRuntime.iOS-')]
print('Available iPadOS runtimes:', [(r['name'], r.get('version')) for r in available], flush=True)
runtime = next((r for r in available if r.get('version') == '26.2'), None)
if runtime is None:
    raise RuntimeError('The requested comparison runtime 26.2 is unavailable; no reproduction ran.')
print('::notice::Testing iPadOS 26.2 only; this cannot verify the reported iPadOS 27.2 behavior.', flush=True)
types = json.loads(run('xcrun', 'simctl', 'list', 'devicetypes', '-j'))['devicetypes']
device_type = next(d for d in types if d['name'] == 'iPad Pro 13-inch (M4)')
device = run('xcrun', 'simctl', 'create', 'PaperReality isolated iPad window probe',
             device_type['identifier'], runtime['identifier'])
try:
    sdl = out / 'SDL'
    logged('fetch-sdl', ['git', 'clone', '--depth', '1', '--branch', 'release-2.32.10',
                         'https://github.com/libsdl-org/SDL.git', str(sdl)])
    # Compiler probes are much faster with Ninja than repeated xcodebuild
    # projects. Finish the dependency build before booting the simulator VM.
    logged('configure-sdl', ['cmake', '-S', str(sdl), '-B', str(out / 'sdl-build'), '-G', 'Ninja',
                            '-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_SYSROOT=iphonesimulator',
                            '-DCMAKE_BUILD_TYPE=Debug',
                            '-DCMAKE_OSX_ARCHITECTURES=' + platform.machine(),
                            '-DCMAKE_OSX_DEPLOYMENT_TARGET=16.3',
                            '-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO',
                            '-DSDL_CMAKE_DEBUG_POSTFIX=',
                            '-DSDL_SHARED=OFF', '-DSDL_STATIC=ON', '-DSDL_TEST=OFF',
                            '-DSDL_TESTS=OFF'])
    logged('build-sdl', ['cmake', '--build', str(out / 'sdl-build'), '--config', 'Debug',
                        '--target', 'SDL2-static', 'SDL2main', '--parallel', '3'])
    for name in ['libSDL2.a', 'libSDL2main.a']:
        library = next((out / 'sdl-build').rglob(name))
        shutil.copyfile(library, out / name)
    # Keep only the production platform configuration, not branding or resources.
    template = (root / 'ios/plist.in').read_text().replace('@PROJECT_VERSION@', '1.0')
    plist = plistlib.loads(template.encode())
    for key in ['CFBundleIconFile', 'UILaunchStoryboardName', 'CFBundleURLTypes']:
        plist.pop(key, None)
    plist.update(CFBundleIdentifier='org.paperreality.window-probe',
                 CFBundleName='WindowProbe', CFBundleDisplayName='WindowProbe',
                 CFBundleVersion='1', CFBundleShortVersionString='1.0',
                 UILaunchScreen={}, UIDeviceFamily=[1, 2])
    (out / 'Info.plist').write_bytes(plistlib.dumps(plist))
    frameworks = ['UIKit', 'Foundation', 'CoreGraphics', 'QuartzCore', 'Metal', 'CoreVideo',
                  'CoreMotion', 'GameController', 'AudioToolbox', 'AVFoundation',
                  'CoreAudio', 'OpenGLES', 'CoreHaptics', 'CoreBluetooth']
    project = {
        'name': 'WindowProbe',
        'settings': {'base': {'CODE_SIGNING_ALLOWED': 'NO', 'TARGETED_DEVICE_FAMILY': '1,2',
                              'CLANG_ENABLE_OBJC_ARC': 'YES', 'IPHONEOS_DEPLOYMENT_TARGET': '16.3'}},
        'targets': {
            'WindowProbe': {
                'type': 'application', 'platform': 'iOS',
                'sources': [str(root / 'tests/ipad-windowing/App.mm')],
                'settings': {'base': {
                    'INFOPLIST_FILE': str(out / 'Info.plist'),
                    'PRODUCT_BUNDLE_IDENTIFIER': 'org.paperreality.window-probe',
                    'CLANG_CXX_LANGUAGE_STANDARD': 'c++20',
                    'HEADER_SEARCH_PATHS': [str(sdl / 'include'), str(root / 'build-uikit-backend/include')],
                    'LIBRARY_SEARCH_PATHS': [str(out)],
                    'OTHER_LDFLAGS': ['-lSDL2main', '-lSDL2', '-liconv', '-ObjC']
                }},
                'dependencies': [{'sdk': name + '.framework'} for name in frameworks]
            },
            'WindowTests': {
                'type': 'bundle.ui-testing', 'platform': 'iOS',
                'sources': [str(root / 'tests/ipad-windowing/WindowTests.m')],
                'dependencies': [{'target': 'WindowProbe'}],
                'settings': {'base': {'GENERATE_INFOPLIST_FILE': 'YES',
                                      'PRODUCT_BUNDLE_IDENTIFIER': 'org.paperreality.window-probe-tests',
                                      'TEST_TARGET_NAME': 'WindowProbe'}}
            }
        },
        'schemes': {'WindowProbe': {'build': {'targets': {'WindowProbe': 'all', 'WindowTests': ['test']}},
                                   'test': {'targets': ['WindowTests'], 'gatherCoverageData': False}}}
    }
    (out / 'project.json').write_text(json.dumps(project, indent=2))
    subprocess.run(['xcodegen', 'generate', '--spec', str(out / 'project.json'),
                    '--project', str(out)], check=True)
    print('Starting simulator boot', flush=True)
    run('xcrun', 'simctl', 'boot', device)
    subprocess.run(['xcrun', 'simctl', 'bootstatus', device, '-b'], check=True, timeout=600)
    print('Simulator boot finished', flush=True)
    destination = 'platform=iOS Simulator,id=' + device
    project_path = str(out / 'WindowProbe.xcodeproj')
    derived = out / 'DerivedData'
    build_args = ['xcodebuild', 'build-for-testing', '-project', project_path,
                  '-scheme', 'WindowProbe', '-configuration', 'Debug',
                  '-destination', destination, '-derivedDataPath', str(derived),
                  'CODE_SIGNING_ALLOWED=NO']
    variants = ['universal-ipad'] if args.native_ipad_only else [
        'v1-window-placement', 'current-iphone-only', 'universal-ipad']
    for variant in variants:
        current = dict(plist)
        (out / 'Info.plist').write_bytes(plistlib.dumps(current))
        logged('build-' + variant, build_args)
        # Both published IPAs have UIDeviceFamily=[1]. Change only the installed
        # metadata for the universal comparison; keep the test runner iPad-capable.
        built_plist = derived / 'Build/Products/Debug-iphonesimulator/WindowProbe.app/Info.plist'
        built_info = plistlib.loads(built_plist.read_bytes())
        built_info['UIDeviceFamily'] = [1, 2] if variant == 'universal-ipad' else [1]
        built_plist.write_bytes(plistlib.dumps(built_info))
        for bundle in (derived / 'Build/Products/Debug-iphonesimulator').glob('*.app'):
            subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(bundle)], check=True)
        print('Installed device families:', built_info['UIDeviceFamily'], flush=True)
        xctestrun = next((derived / 'Build/Products').glob('*.xctestrun'))
        test_configuration = plistlib.loads(xctestrun.read_bytes())
        # Xcodegen writes the UI runner environment in each test target.
        def set_environment(node):
            if isinstance(node, dict):
                if 'TestBundlePath' in node:
                    environment = node.setdefault('EnvironmentVariables', {})
                    if variant == 'v1-window-placement':
                        environment['PROBE_LEGACY'] = '1'
                    else:
                        environment.pop('PROBE_LEGACY', None)
                for value in node.values():
                    set_environment(value)
            elif isinstance(node, list):
                for value in node:
                    set_environment(value)
        set_environment(test_configuration)
        xctestrun.write_bytes(plistlib.dumps(test_configuration))
        print('Configuration:', variant, flush=True)
        text = logged('test-' + variant, ['xcodebuild', 'test-without-building', '-xctestrun', str(xctestrun),
                        '-destination', destination, '-parallel-testing-enabled', 'NO',
                        '-resultBundlePath', str(out / (variant + '.xcresult'))], timeout=420)
        # XCTest may keep NSLog output inside xcresult instead of xcodebuild's
        # stdout. Export through the UI runner's sandbox, not log scraping.
        runner = next((derived / 'Build/Products/Debug-iphonesimulator').glob('*-Runner.app'))
        runner_id = plistlib.loads((runner / 'Info.plist').read_bytes())['CFBundleIdentifier']
        container = Path(run('xcrun', 'simctl', 'get_app_container', device, runner_id, 'data'))
        observation = json.loads((container / 'Documents/WindowObservation.json').read_text())
        # Geometry is encoded as JSON by the app's accessible measurement label.
        for key in ['before', 'after']:
            observation[key] = json.loads(observation[key])
        initial = observation['before']
        observation['fillsReportedScreen'] = all(
            abs(initial['window' + axis] - initial['screen' + axis]) <= 1 for axis in ['Width', 'Height'])
        if args.native_ipad_only:
            for key in ['before', 'after']:
                geometry = observation[key]
                assert geometry['idiom'] == 1, 'App did not enter native iPad mode'
                assert geometry['windowWidth'] > 0 and geometry['windowHeight'] > 0, 'Empty iPad window'
                for axis in ['Width', 'Height']:
                    assert abs(geometry['window' + axis] - geometry['sdl' + axis]) <= 1, \
                        'SDL content does not match the native iPad window: ' + axis
            observation['nativeIpadVerified'] = True
        observation['configuration'] = variant
        (out / (variant + '-observation.json')).write_text(json.dumps(observation, indent=2))
        summary = json.dumps(observation, sort_keys=True)
        print('::notice::IPAD_WINDOWING_RESULT ' + summary.replace('%', '%25'), flush=True)
finally:
    for command in ['shutdown', 'delete']:
        try:
            subprocess.run(['xcrun', 'simctl', command, device], check=False, timeout=30)
        except subprocess.TimeoutExpired:
            print('Simulator cleanup timed out:', command, flush=True)
