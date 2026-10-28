import subprocess, pathlib, re, json, os
main=pathlib.Path(os.environ.get('AUDIT_PRIVATE_ROOT', pathlib.Path.cwd()))
def git(*args): return subprocess.run(['git',*args],check=True,stdout=subprocess.PIPE).stdout
objects=git('rev-list','--objects','--all').splitlines()
oids=list(dict.fromkeys(line.split(b' ',1)[0] for line in objects))
result=subprocess.run(['git','cat-file','--batch'],input=b'\n'.join(oids)+b'\n',stdout=subprocess.PIPE,check=True).stdout
private=[]
for f in [main/'backend/.env',main/'frontend/.env']:
 if f.exists():
  for line in f.read_text(encoding='utf8').splitlines():
   if '=' in line and re.search(r'(PASS|SECRET|TOKEN|API_KEY)',line.split('=',1)[0],re.I):
    v=line.split('=',1)[1].strip().strip('"\'');
    if len(v)>=8: private.append((f.name+':'+line.split('=',1)[0],v.encode()))
f=main/'firmware/include/config.h'
if f.exists():
 for m in re.finditer(r'(?:wifi_password|mqtt_password)\s*\[\]\s*=\s*"([^"\r\n]+)"',f.read_text(encoding='utf8')):
  if len(m.group(1))>=8: private.append(('config.h:'+m.group(0).split('[')[0].strip(),m.group(1).encode()))
patterns={'private_key':rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----','github_token':rb'\b(?:ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9_]{30,}\b','aws_access_key':rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b','jwt':rb'\beyJ[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}\b'}
counts={k:0 for k in patterns}; matches=[]; count=0; position=0; candidates=[]
assignment=re.compile(rb'(?im)^\s*(?:export\s+|const\s+|constexpr\s+char\s+)?([\w.]*?(?:PASSWORD|PASSWD|MQTT_PASS|PG_PASS|API_KEY|SECRET|TOKEN)[\w.]*)\s*(?:\[\])?\s*[:=]\s*[\x22\x27]?([^\s\x22\x27;,#}]+)')
ignore=re.compile(rb'(?i)^(?:example|replace|placeholder|dummy|test|fake|mock|sample|incorrect|wrong|invalid|secreto|contrase|password|your|change|local|prueba|desconoc|not[-_])|^\$|^\{')
while position<len(result):
 end=result.find(b'\n',position); head=result[position:end].split(); position=end+1
 if len(head)!=3: continue
 oid,kind,size=head[0].decode(),head[1],int(head[2]); data=result[position:position+size]; position+=size+1
 if kind!=b'blob': continue
 count+=1
 for m in assignment.finditer(data):
  value=m.group(2)
  if len(value)>=8 and not ignore.search(value): candidates.append({'object':oid,'label':m.group(1).decode(errors='replace')})
 for name,pattern in patterns.items():
  if re.search(pattern,data): counts[name]+=1; matches.append({'object':oid,'rule':name})
 
 for label,secret in private:
  if secret in data: matches.append({'object':oid,'rule':'current_local_secret','label':label})
summary={'commits':len(git('rev-list','--all').splitlines()),'blobs':count,'rules':counts,'current_local_secret_objects':sum(x['rule']=='current_local_secret' for x in matches),'private_values_checked':len(private)}
report_path=pathlib.Path(os.environ.get('AUDIT_REPORT', 'backend/logs/auditoria-credenciales.json'))
report_path.parent.mkdir(parents=True, exist_ok=True)
report_path.write_text(json.dumps({'summary':summary,'matches':matches},indent=2))
report_path.with_name('auditoria-candidatos.json').write_text(json.dumps(candidates)); print(json.dumps({'summary':summary,'candidate_count':len(candidates)}))
