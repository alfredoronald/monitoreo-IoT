// Captura real MQTT; no cambia el intervalo del ESP32 ni simula mensajes.
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { performance } from 'node:perf_hooks';
const args=Object.fromEntries(process.argv.slice(2).reduce((a,x,i,v)=>i%2?a:[...a,[x.replace(/^--/,''),v[i+1]]],[]));
const root=path.resolve(args.root||'.');
const interval=Number(args.interval),duration=Number(args.duration||600),settle=Number(args.settle||45);
if(![10,15,30].includes(interval)||!Number.isFinite(duration)||duration<600||!Number.isFinite(settle)||settle<0)throw Error('Usar --interval 10|15|30 --duration >=600 --settle >=0');
const out=path.resolve(args.output||`mqtt-${interval}s`);fs.mkdirSync(out,{recursive:true});
const require=createRequire(path.join(root,'backend/package.json'));
require('dotenv').config({path:path.join(root,'backend/.env'),quiet:true});
const mqtt=require('mqtt');
const topics=['ambiente/temperatura','ambiente/humedad','ambiente/co2'];
const rows=[];let started=null,timer,settleTimer,finished=false,reconnects=0,invalid=0,retained=0;
const client=mqtt.connect(process.env.MQTT_URL,{username:process.env.MQTT_USER,password:process.env.MQTT_PASS,clientId:`qa-interval-${process.pid}`,connectTimeout:10000,reconnectPeriod:1000});
const raw=fs.createWriteStream(path.join(out,'mensajes.jsonl'));
const connectLimit=setTimeout(()=>finish('No se pudo iniciar la suscripcion en 30 s'),30000);
client.on('reconnect',()=>{if(started!==null)reconnects++;});
client.on('error',()=>{}); // Error resumido sin URL ni credenciales.
client.on('connect',()=>client.subscribe('ambiente/#',{qos:0},err=>{
 if(err){finish('Fallo de suscripcion');return;}
 if(started!==null||settleTimer)return;
 clearTimeout(connectLimit);
 settleTimer=setTimeout(()=>{started=performance.now();console.log(`CAPTURANDO ${interval}s durante ${duration}s`);timer=setTimeout(()=>finish(),duration*1000);},settle*1000);
}));
client.on('message',(topic,payload,packet)=>{
 if(started===null||finished||!topics.includes(topic))return;
 if(packet.retain){retained++;return;}
 const text=payload.toString().trim();const value=text===''?NaN:Number(text);
 if(!Number.isFinite(value)){invalid++;return;}
 const row={fecha:new Date().toISOString(),elapsed_s:(performance.now()-started)/1000,topic,value};rows.push(row);raw.write(JSON.stringify(row)+'\n');
});
function finish(error){
 if(finished)return;finished=true;clearTimeout(connectLimit);clearTimeout(timer);clearTimeout(settleTimer);
 const elapsed=started===null?0:(performance.now()-started)/1000;
 const variables=Object.fromEntries(topics.map(topic=>{
  const a=rows.filter(x=>x.topic===topic),v=a.map(x=>x.value),gaps=a.slice(1).map((x,i)=>x.elapsed_s-a[i].elapsed_s);
  const mean=v.length?v.reduce((x,y)=>x+y,0)/v.length:null;
  const sd=v.length?Math.sqrt(v.reduce((x,y)=>x+(y-mean)**2,0)/v.length):null;
  const sorted=[...gaps].sort((a,b)=>a-b);
  return [topic,{mensajes:a.length,esperados_ventana:Math.floor(duration/interval),mensajes_hora_teoricos:3600/interval,min:v.length?Math.min(...v):null,max:v.length?Math.max(...v):null,media:mean,desviacion_estandar:sd,cv_porcentaje:mean?100*sd/Math.abs(mean):null,intervalo_mediano_s:sorted.length?sorted[Math.floor(sorted.length/2)]:null,max_gap_s:gaps.length?Math.max(...gaps):null,gaps_fuera_tolerancia:gaps.filter(x=>Math.abs(x-interval)>1).length}];
 }));
 const result={intervalo_configurado_s:interval,duracion_objetivo_s:duration,duracion_observada_s:elapsed,asentamiento_s:settle,mensajes_hora_total_teoricos:3*3600/interval,reconexiones_observadas:reconnects,invalidos:invalid,retenidos_excluidos:retained,variables,error:error||null,nota:'Conteos MQTT observados no prueban perdida extremo a extremo sin secuencias. CV refleja tambien cambios reales del ambiente; no es exactitud del sensor.'};
 fs.writeFileSync(path.join(out,'resumen.json'),JSON.stringify(result,null,2)+'\n');
 raw.end();client.end(true);console.log(JSON.stringify(result));if(error)process.exitCode=1;
}
process.on('SIGINT',()=>finish('Captura interrumpida'));
