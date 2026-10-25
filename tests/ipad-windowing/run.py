#!/usr/bin/env python3
"""Run an isolated SDL iPad windowing diagnostic, without game code or assets."""
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import time

root = Path(__file__).resolve().parents[2]
out = root / 'build-ipad-windowing'
out.mkdir(exist_ok=True)


def run(*args, **kwargs):
    kwargs.setdefault('timeout', 90)
    return subprocess.check_output(args, text=True, **kwargs).strip()


def logged(name, args, timeout=600, env=None):
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
    run('xcrun', 'simctl', 'boot', device)
    subprocess.run(['xcrun', 'simctl', 'bootstatus', device, '-b'], check=True, timeout=240)
    sdl = out / 'SDL'
    logged('fetch-sdl', ['git', 'clone', '--depth', '1', '--branch', 'release-2.32.10',
                         'https://github.com/libsdl-org/SDL.git', str(sdl)])
    logged('configure-sdl', ['cmake', '-S', str(sdl), '-B', str(out / 'sdl-build'), '-G', 'Xcode',
                            '-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_SYSROOT=iphonesimulator',
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
    destination = 'platform=iOS Simulator,id=' + device
    project_path = str(out / 'WindowProbe.xcodeproj')
    derived = out / 'DerivedData'
    build_args = ['xcodebuild', 'build-for-testing', '-project', project_path,
                  '-scheme', 'WindowProbe', '-configuration', 'Debug',
                  '-destination', destination, '-derivedDataPath', str(derived),
                  'CODE_SIGNING_ALLOWED=NO']
    for variant in ['v1-window-placement', 'current-iphone-only', 'universal-ipad']:
        current = dict(plist)
        (out / 'Info.plist').write_bytes(plistlib.dumps(current))
        logged('build-' + variant, build_args)
        # Both published IPAs have UIDeviceFamily=[1]. Change only the installed
        # metadata for the universal comparison; keep the test runner iPad-capable.
        built_plist = derived / 'Build/Products/Debug-iphonesimulator/WindowProbe.app/Info.plist'
        built_info = plistlib.loads(built_plist.read_bytes())
        built_info['UIDeviceFamily'] = [1, 2] if variant == 'universal-ipad' else [1]
        built_plist.write_bytes(plistlib.dumps(built_info))
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
        observations = [line.split('IPAD_WINDOWING_OBSERVATION ', 1)[1]
                        for line in text.splitlines() if 'IPAD_WINDOWING_OBSERVATION ' in line]
        if not observations:
            # Some XCTest versions only include test stdout in xcresult.
            result_json = run('xcrun', 'xcresulttool', 'get', 'object', '--legacy',
                              '--path', str(out / (variant + '.xcresult')), '--format', 'json')
            print('XCTest finished; extracting activity logs separately if observations are unavailable.', flush=True)
            (out / (variant + '-result.json')).write_text(result_json)
        for observation in observations:
            print('::notice::' + variant + ' ' + observation.replace('%', '%25'), flush=True)
finally:
    subprocess.run(['xcrun', 'simctl', 'shutdown', device], check=False)
    subprocess.run(['xcrun', 'simctl', 'delete', device], check=False)
