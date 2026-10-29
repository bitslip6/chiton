'use strict';
const assert=require('node:assert/strict');
const C=require('../release/files/console-cloud');Object.assign(C,require('../release/files/console-scheduler'));
const {DemoSource}=require('../release/files/console-demo'),{LiveSource}=require('../release/files/console-live'),{fixture}=require('./console_fixture');
function headers(count=1){return new Headers({'Content-Type':'application/octet-stream','X-Chiton-Layout':'int16_xyzd','X-Chiton-Color-Mode':'4','X-Chiton-Frame-Id':'1','X-Chiton-Configuration-Revision':'0','X-Chiton-Width':String(count),'X-Chiton-Height':'1','X-Chiton-Frame-Type':'depth','X-Chiton-Source-Units':'mm','X-Chiton-Units':'mm','X-Chiton-Units-Scope':'calibration_aoi','X-Chiton-Invalid-Sample-Policy':'all_uint16_included','X-Chiton-Byte-Order':'little'});}
(async()=>{
    let assertions=0;
    for(const order of ['little','big'])for(const count of [0,1,C.CLOUD_MAX]){
        const h=headers(count);h.set('X-Chiton-Byte-Order',order);const metadata=C.cloudHeaders(h),bytes=new ArrayBuffer(count*8),v=new DataView(bytes);
        for(let i=0;i<count;i++)[-32768,-1,0,32767].forEach((n,k)=>v.setInt16(i*8+k*2,n,order==='little'));
        const m=C.decodeCloud(bytes,metadata);assert.equal(m.recordCount,count);assert.deepEqual([...m.records.slice(0,4)],count?[-32768,-1,0,32767]:[]);
        const bounds=C.cloudBounds(m.records);assert(bounds.center.every(Number.isFinite));assert(bounds.radius>0);assertions++;
    }
    const required=['Layout','Color-Mode','Frame-Id','Configuration-Revision','Width','Height','Frame-Type','Source-Units','Units','Units-Scope','Invalid-Sample-Policy','Byte-Order'];
    for(const name of required){const h=headers();h.delete('X-Chiton-'+name);assert.throws(()=>C.cloudHeaders(h),name);assertions++;}
    for(const [key,value] of [['Frame-Id','9007199254740992'],['Frame-Id','0'],['Configuration-Revision','-1'],['Width','307201'],['Height','480'],['Width','1.5'],['Byte-Order','native'],['Units','normalized_6000_mm'],['Units-Scope','full_sensor'],['Frame-Type','ir'],['Frame-Age-Ms','NaN'],['Captured-At-Ms','-1']]){
        const h=headers();h.set('X-Chiton-'+key,value);assert.throws(()=>C.cloudHeaders(h));assertions++;
    }
    for(const rejected of ['status','oversized','missing-id']){
        let cancelled=false;const stream=new ReadableStream({start(c){c.enqueue(new Uint8Array(8));},cancel(){cancelled=true;}}),h=headers();
        if(rejected==='oversized')h.set('X-Chiton-Width','307201');if(rejected==='missing-id')h.delete('X-Chiton-Frame-Id');
        await assert.rejects(()=>C.readCloudResponse(new Response(stream,{headers:h,status:rejected==='status'?503:200})));
        assert.equal(cancelled,true,'Rejected '+rejected+' body cancelled');
    }
    const metadata=C.cloudHeaders(headers());assert.equal(metadata.frameAgeMs,null);assert.equal(metadata.capturedAtMs,null);
    for(const n of [0,1,7,9,16])assert.throws(()=>C.decodeCloud(new ArrayBuffer(n),metadata));
    assert.deepEqual(C.cloudColor(-1,{min:0,max:100}),C.cloudColor(0,{min:0,max:100}));assert.deepEqual(C.cloudColor(101,{min:0,max:100}),C.cloudColor(100,{min:0,max:100}));assert.deepEqual(C.cloudColor(12,null),[.6,.67,.73]);
    const extra=new ReadableStream({start(c){c.enqueue(new Uint8Array(16));c.close();}});await assert.rejects(()=>C.readCloudResponse(new Response(extra,{headers:headers()})),/Oversized/);
    await assert.rejects(()=>C.readCloudResponse(new Response(new Uint8Array(7),{headers:headers()})),/Truncated/);
    const nav=new C.CloudCamera();for(const records of [new Int16Array(),new Int16Array([0,0,0,0]),new Int16Array([-32768,-32768,-32768,-1,32767,32767,32767,0])]){
        nav.bounds=C.cloudBounds(records);for(const aspect of [.1,1,10]){nav.fit(aspect);for(let i=0;i<100;i++){nav.zoom(i%2?1e6:-1e6);nav.pan(1e6,-1e6);nav.orbit(1e6,-1e6);}assert(nav.position().every(Number.isFinite));assert(nav.radius>0);}}
    const mock=await fixture(),original=global.fetch;global.fetch=(route,options)=>original(mock.origin+route,options);
    try{
        const live=new LiveSource(),demo=new DemoSource({scenario:'cloud'});mock.source.mode=4;mock.source.seek(1000);demo.seek(1000);await live.readStatus({});
        const local=await demo.readCloud();assert.equal(local.records[(320*640+460)*4+3],(await demo.readMeasurement({clientId:'coherent',selection:{kind:'point',x:460,y:320}})).value);
        for(const order of ['little','big']){mock.control.cloudOrder=order;const m=await live.readCloud();assert.deepEqual(m.records,local.records);}
        for(const fault of ['missing-id','unsafe-id','bad-count','oversized','truncated','409','503','504','ir-header']){mock.control.cloud=fault;await assert.rejects(()=>live.readCloud(),fault);assertions++;}
        for(const fault of ['hanging-body','delay-header','delay-body']){mock.control.cloud=fault;await assert.rejects(()=>C.bounded(signal=>live.readCloud({signal}),null,50));}
        mock.control.cloud='empty';assert.equal((await live.readCloud()).recordCount,0);mock.control.cloud='single';assert.deepEqual([...(await live.readCloud()).records],[0,0,0,0]);
        mock.control.cloud='ok';const cloud=await live.readCloud(),obj=await live.exportFrame({format:'obj'}),lines=(await obj.blob.text()).trim().split('\n');assert.equal(lines.length,cloud.recordCount);
        for(const i of [0,12345,cloud.recordCount-1])lines[i].split(' ').slice(1).forEach((n,k)=>assert(Math.abs(Number(n)-cloud.records[i*4+k]/6000)<.000051));
        const bad=new DemoSource({scenario:'long-values'});bad.mode=4;await assert.rejects(()=>bad.readCloud(),/unsupported/);
        console.log(`console_cloud: ${assertions} contract groups + streamed caps, signed first/max records, endian, bounds/color/nav, demo/live coherence, faults/deadlines, OBJ numerical comparison passed`);
    }finally{global.fetch=original;await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
