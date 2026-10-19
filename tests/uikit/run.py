#!/usr/bin/env python3
"""Compile and run the real UIKit controllers on an iOS simulator, without game assets."""
import json
import os
from pathlib import Path
import platform
import plistlib
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[2]
out = root / 'build-uikit-smoke'
app = out / 'UIKitSmoke.app'
app.mkdir(parents=True, exist_ok=True)
def run(*args, **kwargs):
    return subprocess.check_output(args, text=True, **kwargs).strip()
sdk = run('xcrun', '--sdk', 'iphonesimulator', '--show-sdk-path')
subprocess.run(['xcrun','--sdk','iphonesimulator','clang++','-std=c++20','-fobjc-arc',
                '-target',platform.machine()+'-apple-ios16.3-simulator','-isysroot',sdk,
                '-D__IOS__','-I'+str(root/'src'),str(root/'tests/uikit/Host.mm'),
                '-framework','UIKit','-framework','Foundation','-framework','CoreGraphics','-o',str(app/'UIKitSmoke')],check=True)
with (app/'Info.plist').open('wb') as f:
    plistlib.dump({'CFBundleExecutable':'UIKitSmoke','CFBundleIdentifier':'org.paperreality.uikit-smoke',
                  'CFBundleName':'UIKitSmoke','CFBundlePackageType':'APPL','CFBundleVersion':'1',
                  'CFBundleShortVersionString':'1.0','MinimumOSVersion':'16.3','UIDeviceFamily':[1,2],
                  'UILaunchScreen':{},'CFBundleURLTypes':[{'CFBundleURLSchemes':['uikitsmoke']}],'UISupportedInterfaceOrientations':['UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight']},f)
subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
runtimes=json.loads(run('xcrun','simctl','list','runtimes','-j'))['runtimes']
runtime=next(r for r in reversed(runtimes) if r.get('isAvailable') and r['identifier'].startswith('com.apple.CoreSimulator.SimRuntime.iOS-'))
types=json.loads(run('xcrun','simctl','list','devicetypes','-j'))['devicetypes']
device_type=next(d for d in types if d['name']=='iPhone 15 Pro')
device=run('xcrun','simctl','create','PaperReality UIKit smoke',device_type['identifier'],runtime['identifier'])
try:
    run('xcrun','simctl','boot',device)
    subprocess.run(['xcrun','simctl','bootstatus',device,'-b'],check=True,timeout=240)
    run('xcrun','simctl','install',device,str(app))
    run('xcrun','simctl','launch',device,'org.paperreality.uikit-smoke')
    data=Path(run('xcrun','simctl','get_app_container',device,'org.paperreality.uikit-smoke','data'))
    result=data/'Documents/UIKitSmoke.json'
    for _ in range(100):
        if result.exists(): break
        time.sleep(1)
    subprocess.run(['xcrun','simctl','io',device,'screenshot',str(out/'settings.png')],check=False)
    if result.exists():
        report=json.loads(result.read_text()); (out/'result.json').write_text(json.dumps(report,indent=2))
    else:
        progress=data/'Documents/UIKitProgress.log'
        detail=progress.read_text() if progress.exists() else 'App did not reach launch callback'
        report={'status':'failed','message':'Simulator exited or timed out.\n'+detail}
    (out/'result.json').write_text(json.dumps(report,indent=2))
    logs=run('xcrun','simctl','spawn',device,'log','show','--last','3m','--style','compact','--predicate','process == "UIKitSmoke"')
    (out/'simulator.log').write_text(logs)
    if report['status']=='passed':
        run('xcrun','simctl','status_bar',device,'override','--time','9:41','--batteryState','charged','--batteryLevel','100')
        for page in ['general','audio','sidebar','audio-dark','general-dark']:
            (data/'Documents/UIKitCommand').write_text(page)
            source=data/('Documents/'+page+'.png')
            for _ in range(50):
                if source.exists():break
                time.sleep(.2)
            if not source.exists():raise RuntimeError('App view capture was not produced: '+page)
            screenshot=out/(page+'.png')
            screenshot.write_bytes(source.read_bytes())
    print(json.dumps(report,indent=2))
    if report['status']!='passed':
        print(logs[-10000:])
        for line in report['message'].splitlines()[:35]: print('::error::'+line.replace('%','%25').replace('\r','%0D'))
        sys.exit(1)
finally:
    subprocess.run(['xcrun','simctl','shutdown',device],check=False)
    subprocess.run(['xcrun','simctl','delete',device],check=False)
