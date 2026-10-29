'use strict';
const C=require('../release/files/console-cloud');
const defaults=()=>({enabled:0,algorithm:1,evidence_min:250,evidence_max:6000,boundary_gap:200,anchor_tolerance:40,center_margin:60,ramp_residual:30,surface_support:4,center_support:3});
function encodeCleanup(snapshot,config,generation,{status=config.enabled?2:1,flags,byteOrder='little'}={}){
 const n=snapshot.records.length/4;if(n>230400)throw new Error('Fixture exceeds producer capacity');
 const b=Buffer.alloc(128+n*9);b.write('CHCLD001');b.writeUInt32LE(128,8);b.writeUInt32LE(n,12);b.writeUInt32LE(n*8,16);b.writeUInt32LE(n,20);
 for(const [offset,value]of [[24,snapshot.frameId],[32,snapshot.revision],[40,snapshot.capturedAtMs],[48,generation]])b.writeBigUInt64LE(BigInt(value),offset);
 b.writeUInt32LE(config.algorithm,56);b.writeUInt32LE(status,60);b[64]=snapshot.units==='mm'?1:2;b[65]=snapshot.units==='mm'?1:0;b[66]=1;b[70]=config.enabled;b[71]=byteOrder==='little'?1:2;
 for(const [offset,name]of [[72,'evidence_min'],[74,'evidence_max'],[76,'boundary_gap'],[78,'anchor_tolerance'],[80,'center_margin'],[82,'ramp_residual']])b.writeUInt16LE(config[name],offset);
 b[84]=config.surface_support;b[85]=config.center_support;
 for(let i=0;i<snapshot.records.length;i++)b[byteOrder==='little'?'writeInt16LE':'writeInt16BE'](snapshot.records[i],128+i*2);
 const counts={0:0,1:0,2:0,4:0,8:0};
 for(let i=0;i<n;i++){const f=status===1?0:status!==2?4:(flags?.[i]??(i%3===0?8:1));b[128+n*8+i]=f;counts[f]++;}
 const gated=counts[8]+counts[2];for(const [o,v]of [[88,gated],[92,counts[8]],[96,counts[2]],[100,counts[4]],[104,counts[1]],[108,gated]])b.writeUInt32LE(v,o);
 return b;
}
function cleanupRoute(req,url,{source,control,json,send}){
 if(!control.cleanup||!url.pathname.startsWith('/api/cleanup/'))return false;
 const state=control.cleanup;
 if(!state.config){state.config=defaults();state.generation=1;}
 const settings=()=>({generation:state.generation,config:state.config,available:true,frame_generation:state.generation,status:state.config.enabled?2:1,constraints:{algorithm:state.algorithms||[1,2,3]}});
 if(url.pathname==='/api/cleanup/v1/config'){
  if(req.method==='POST'){
   const entries=[...url.searchParams];
   try{
    if(entries.length!==11||new Set(entries.map(x=>x[0])).size!==11||entries.some(([k,v])=>![...C.CLEANUP_FIELDS,'expected_generation'].includes(k)||!/^\d+$/.test(v)))throw new Error('fields');
    const c=C.validateCleanupConfig(Object.fromEntries(entries.map(([k,v])=>[k,Number(v)])));
    if(!(state.algorithms||[1,2,3]).includes(c.algorithm))throw new Error('unsupported algorithm');
    if(Number(url.searchParams.get('expected_generation'))!==state.generation){json({error:'stale'},409);return true;}
    state.config=c;state.generation++;state.posts=(state.posts||0)+1;
    if(state.failAck){json({error:'ack lost'},503);return true;}
   }catch(e){json({error:e.message},400);return true;}
  }else if(req.method!=='GET'){json({error:'method'},405);return true;}
  json(settings());return true;
 }
 if(url.pathname==='/api/cleanup/v1/cloud'){
  if(!control.capture){json({error:'disabled'},503);return true;}
  if(source.mode!==4){json({error:'mode'},409);return true;}
  const m=source.metadata(),records=state.records||source.cloudSnapshot().records.slice(0,4000);
  const snapshot={records,frameId:m.frame_id,revision:m.configuration_revision,capturedAtMs:Math.floor(m.captured_at_ms),units:m.units};
  let frame={config:{...state.config},generation:state.generation};
  if(state.freeze&&state.lastFrame)frame=state.lastFrame;else state.lastFrame=frame;
  const status=!frame.config.enabled?1:m.units!=='mm'?3:2;
  send('application/octet-stream',encodeCleanup(snapshot,frame.config,frame.generation,{status,flags:state.flags}));return true;
 }
 json({error:'unknown'},404);return true;
}
module.exports={defaults,encodeCleanup,cleanupRoute};
