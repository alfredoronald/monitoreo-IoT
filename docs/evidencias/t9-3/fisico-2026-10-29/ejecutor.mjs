import {createRequire} from 'node:module';
import {spawn,execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {mkdir,writeFile,appendFile} from 'node:fs/promises';
import {setTimeout as delay} from 'node:timers/promises';
import {createInterface} from 'node:readline';
import {openSync,closeSync} from 'node:fs';
const root='C:/Users/win/Develop/monitoreo-IoT';
const temp='C:/Users/win/AppData/Local/Temp/opencode';
const require=createRequire(root+'/backend/package.json');
require('dotenv').config({path:root+'/backend/.env'});
const {Pool}=require('pg');
const db=new Pool({host:process.env.PG_HOST,port:Number(process.env.PG_PORT),database:process.env.PG_DB,user:process.env.PG_USER,password:process.env.PG_PASS,connectionTimeoutMillis:5000});
const execute=promisify(execFile);
const directory=root+'/docs/evidencias/t9-3/fisico-2026-10-29/ensayo-'+Date.now();
await mkdir(directory,{recursive:true});
const serial=spawn('C:/Users/win/.platformio/penv/Scripts/python.exe',[temp+'/physical-serial.py'],{stdio:['pipe','pipe','pipe']});
let observations=[];
const lines=createInterface({input:serial.stdout});
lines.on('line',text=>{try{const value=JSON.parse(text);observations.push(value);appendFile(directory+'/serie.jsonl',text+'\n').catch(console.error);}catch{}});
serial.stderr.on('data',data=>console.error(data.toString()));
async function recent(){return (await db.query('SELECT id::text,tipo_variable,valor,fecha_hora FROM lecturas ORDER BY lecturas.id DESC LIMIT 1')).rows[0];}
async function waitFor(predicate,timeout=20000){const end=Date.now()+timeout;while(Date.now()<end){const result=await predicate();if(result)return result;await delay(100);}throw Error('Tiempo agotado esperando confirmación física');}
async function killPort(port){
 const query=port===1883?'Get-Process mosquitto -ErrorAction Stop | Select-Object -ExpandProperty Id':"Get-CimInstance Win32_Process -Filter \"name='node.exe'\" | Where-Object { $_.CommandLine -match 'physical-backend.mjs' } | Select-Object -ExpandProperty ProcessId";
 const {stdout}=await execute('powershell.exe',['-NoProfile','-Command',query]);
 const ids=stdout.trim().split(/\s+/).filter(id=>/^\d+$/.test(id));
 if(!ids.length)throw Error('No se encontró el proceso del componente');
 for(const id of ids)await execute('taskkill.exe',['/PID',id,'/F']);
 await waitFor(async()=>{for(const id of ids){try{process.kill(Number(id),0);return false;}catch{}}return true;},10000);
}
function startComponent(scenario){
 const command=scenario==='broker'?'C:/Program Files/mosquitto/mosquitto.exe':process.execPath;
 const args=scenario==='broker'?['-c','infra/mosquitto/mosquitto.conf','-v']:['--import','tsx',temp+'/physical-backend.mjs'];
 const log=openSync(directory+`/${scenario}.log`,'a');
 const child=spawn(command,args,{cwd:scenario==='broker'?root:root+'/backend',detached:true,stdio:['ignore',log,log]});
 closeSync(log);
 child.unref();
 child.on('error',console.error);
 return child;
}
try{
 const initial=await recent();
 if(!initial||Date.now()-new Date(initial.fecha_hora).getTime()>60000)throw Error('No hay lecturas físicas recientes; no se provocaron cortes');
 console.log('Physical baseline',JSON.stringify(initial));
 for(const scenario of ['broker','backend','wifi']){
  const baseline=await recent();
  const result={issue:149,scenario,started:'',ended:'',baselineId:baseline.id,seconds:null,recovered:null,
   source:'ESP32 físico en aula; interrupción automatizada confirmada por proceso/driver y recuperación hasta primera fila SQL',
   firmware:'7a0ac17',buildFlags:['WIFI_TEMPORARY_TEST=1'],samplingIntervalSeconds:15,passed:false,errors:[],operatorLocation:'Aula real confirmada por el usuario',outageHoldSeconds:3};
  let stopped=false;
  try{
   if(scenario==='wifi'){
    observations=[];serial.stdin.write('wifi prueba desconectar\n');
    const confirmed=await waitFor(()=>observations.find(row=>/desconexión confirmada/.test(row.line)),15000);
    result.interruptionConfirmation=confirmed;
   }else{await killPort(scenario==='broker'?1883:3000);}
   stopped=true;
   const started=Date.now();result.started=new Date(started).toISOString();
   await delay(3000);
   result.restoreRequested=new Date().toISOString();
   if(scenario==='wifi')serial.stdin.write('wifi prueba reconectar\n');else startComponent(scenario);
   stopped=false;
   while(Date.now()-started<120000){
    const row=(await db.query('SELECT id::text,tipo_variable,valor,fecha_hora FROM lecturas WHERE id>$1 AND fecha_hora >= $2 ORDER BY fecha_hora,id LIMIT 1',[baseline.id,result.started])).rows[0];
    if(row){result.ended=new Date().toISOString();result.seconds=(Date.parse(result.ended)-started)/1000;result.recovered=row;result.passed=result.seconds<60;break;}
    await delay(250);
   }
   if(result.seconds===null)result.errors.push('Sin fila nueva durante 120 s');
  }catch(error){result.errors.push(error.message);}
  finally{
   if(stopped){if(scenario==='wifi')serial.stdin.write('wifi prueba reconectar\n');else startComponent(scenario);}
   result.ended||=new Date().toISOString();
   await writeFile(directory+`/recuperacion-${scenario}.json`,JSON.stringify(result,null,2)+'\n');
  }
  console.log(JSON.stringify(result));
  if(!result.passed)throw Error('Recuperación no aprobada: '+scenario);
  // Wait for a fresh publication cycle before interrupting the next component.
  const recoveredId=(await recent()).id;
  await waitFor(async()=>Number((await recent()).id)>Number(recoveredId),45000);
 }
}finally{serial.kill();await db.end();}
