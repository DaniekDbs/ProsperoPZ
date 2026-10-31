"""Stage freshly built Java/native files while retaining repaired Wine modules."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parent.parent
title = root / 'dist/PPSA99777'
jar = root / 'artifacts/launcher/prosperopz-launcher.jar'
reports = [title / 'STAGE-REPORT.json',title / 'prosperopz/STAGE-REPORT.json']
stages = [json.loads(p.read_text()) for p in reports]
for stage in stages:
    assert stage['structurallyComplete'] and stage['titleId'] == 'PPSA99777'
    for name,key in [('ntdll','ntdllSha256'),('win32u','win32uSha256')]:
        module = title / ('win/wine/lib/wine/x86_64-unix/'+name+'.prx')
        assert hashlib.sha256(module.read_bytes()).hexdigest() == stage['wine'][key], 'Preserved Wine module changed unexpectedly'
assert jar.is_file() and (title / 'eboot.bin').is_file()
shutil.copy2(root/'THIRD_PARTY_NOTICES.md',title/'THIRD_PARTY_NOTICES.md')
for path in [title / 'launcher/prosperopz-launcher.jar',title / 'prosperopz/launcher/prosperopz-launcher.jar']:
    shutil.copy2(jar,path)
for path,stage in zip(reports,stages):
    stage['jarSha256'] = hashlib.sha256(jar.read_bytes()).hexdigest()
    stage.pop('zipfsBootstrapSha256', None)
    stage['ebootSha256'] = hashlib.sha256((title / 'eboot.bin').read_bytes()).hexdigest()
    path.write_text(json.dumps(stage,indent=2)+'\n')
spec = importlib.util.spec_from_file_location('deploy',root / 'scripts/deploy-ps5.py')
deploy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deploy)
deploy.validate_package(title,title / 'prosperopz')
print('STAGED_JAVA_NATIVE_UPDATE: existing Wine/OpenGL fixes retained')
