"""Build provenance for freshly compiled CPU6 artifacts and C runtimes."""
import hashlib,json,subprocess
import shutil
from pathlib import Path

def sha256(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def compiler_info(path):
    path=Path(path).resolve()
    version=subprocess.run([path,'--version'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=False).stdout.strip()
    return {'path':str(path),'sha256':sha256(path),'version':version}

def input_hashes(paths):
    return {str(Path(p).resolve()):sha256(p) for p in paths if Path(p).is_file()}

def manifest_path(output):
    return output.with_suffix(output.suffix + '.build.json')


def check_outputs(outputs, inputs):
    # Resolve symlinks and compare inodes as well for existing hard links.
    paths = set()
    inodes = set()
    for path, is_output in [(p, False) for p in inputs] + [(p, True) for p in outputs]:
        resolved = path.resolve()
        try:
            stat = resolved.stat()
            inode = (stat.st_dev, stat.st_ino)
        except FileNotFoundError:
            inode = None
        if is_output and (resolved in paths or inode in inodes):
            raise ValueError(f'output would overwrite another build file: {path}')
        paths.add(resolved)
        if inode is not None:
            inodes.add(inode)


def save_temps(work, destination, protected):
    if destination is None:
        return
    files = [path for path in work.iterdir() if path.is_file()]
    check_outputs([destination / path.name for path in files], protected)
    destination.mkdir(parents=True, exist_ok=True)
    for path in files:
        shutil.copyfile(path, destination / path.name)


def write_manifest(output,compiler,options,inputs,**details):
    output=Path(output)
    result={'format':'cpu6-build-v1','artifact':str(output.resolve()),'sha256':sha256(output),
            'compiler':compiler,'options':list(map(str,options)),'inputs':inputs,**details}
    manifest_path(output).write_text(json.dumps(result,indent=2)+'\n')
    return result
