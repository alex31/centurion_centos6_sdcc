"""Build provenance for freshly compiled CPU6 artifacts and C runtimes."""
import hashlib,json,subprocess
from pathlib import Path

def sha256(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def compiler_info(path):
    path=Path(path).resolve()
    version=subprocess.run([path,'--version'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=False).stdout.strip()
    return {'path':str(path),'sha256':sha256(path),'version':version}

def input_hashes(paths):
    return {str(Path(p).resolve()):sha256(p) for p in paths if Path(p).is_file()}

def write_manifest(output,compiler,options,inputs,**details):
    output=Path(output)
    result={'format':'cpu6-build-v1','artifact':str(output.resolve()),'sha256':sha256(output),
            'compiler':compiler,'options':list(map(str,options)),'inputs':inputs,**details}
    output.with_suffix(output.suffix+'.build.json').write_text(json.dumps(result,indent=2)+'\n')
    return result
