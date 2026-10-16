'use strict';
const assert=require('node:assert/strict');
const C=require('../release/files/console-model');Object.assign(C,require('../release/files/console-scheduler'));const {DemoSource,pixel}=require('../release/files/console-demo');const {LiveSource}=require('../release/files/console-live');const {fixture}=require('./console_fixture');
(async()=>{
    const mock=await fixture(),originalFetch=global.fetch;
    global.fetch=(route,options)=>{assert(String(route).startsWith('/'));return originalFetch(mock.origin+route,options);};
    try{
        const demo=new DemoSource(),live=new LiveSource();
        for(const [source,underlying] of [[demo,demo],[live,mock.source]]){
            underlying.seek(1000);await source.readStatus({signal:new AbortController().signal});
            const args={clientId:'test_client',selection:{kind:'point',x:460,y:320}};
            let m=await source.readMeasurement(args);assert.equal(m.value,pixel(460,320,m.frameId,{seed:7}));assert.equal(m.count,1);assert(m.historyReset);
            m=await source.readMeasurement(args);assert.equal(m.count,1);assert(!m.historyReset);
            underlying.seek(1050);m=await source.readMeasurement(args);assert.equal(m.count,2);
            const other=await source.readMeasurement({...args,clientId:'other_tab'});assert.equal(other.count,1);
            const roi=await source.readMeasurement({...args,selection:{kind:'roi',x:459,y:319,x2:462,y2:322}});
            assert.equal(roi.statisticsKind,'spatial');assert.equal(roi.count,9);assert.equal(roi.value,underlying.sample(460,320,underlying.metadata()));
            const expected=[];for(let y=319;y<322;y++)for(let x=459;x<462;x++)expected.push(underlying.sample(x,y,underlying.metadata()));assert.equal(roi.mean,C.stats(expected).mean);
            await source.applySetting({name:'frameType',value:'ir'});m=await source.readMeasurement(args);assert.equal(m.units,'ir_intensity_units');assert(m.historyReset);
            await source.applySetting({name:'frameType',value:'depth'});await source.applySetting({name:'calibration',value:false});m=await source.readMeasurement(args);assert.equal(m.units,'depth_sensor_units');
            await source.applySetting({name:'calibration',value:true});
            await source.applySetting({name:'range',value:{min:100,max:4000}});
            assert.deepEqual((await source.readStatus({signal:new AbortController().signal})).range,{min:100,max:4000});
            await assert.rejects(()=>source.applySetting({name:'range',value:{min:4000,max:100}}));
            for(let i=0;i<100;i++){underlying.seek(2000+i*50);m=await source.readMeasurement(args);}assert.equal(m.count,60);
            underlying.seek(0);m=await source.readMeasurement(args);assert(m.historyReset);assert.equal(m.count,1);
            for(let i=0;i<40;i++)await source.readMeasurement({...args,clientId:'viewer'+i});assert.equal(underlying.histories.size,32);
            underlying.seek(500000);m=await source.readMeasurement({...args,clientId:'viewer39'});assert(m.historyReset);assert.equal(m.count,1);
            source.stop();
        }
        await assert.rejects(()=>demo.applySetting({name:'quality',value:80}),/unavailable/);
        await live.readStatus({signal:new AbortController().signal});
        const args={clientId:'faults',selection:{kind:'point',x:460,y:320}};
        for(const mode of ['malformed','unsafe-id','503']){mock.control.measurement=mode;await assert.rejects(()=>live.readMeasurement(args));}
        mock.control.measurement='missing-optionals';const missing=await live.readMeasurement(args);assert.equal(missing.frameAgeMs,null);assert.equal(missing.min,null);assert.equal(missing.smoothed,null);
        mock.control.measurement='hanging-body';await assert.rejects(()=>C.bounded(signal=>live.readMeasurement({...args,signal}),null,50));
        mock.control.measurement='ok';const raw=await live.exportFrame({format:'raw'});assert.equal(raw.blob.size,614400);
        await assert.rejects(()=>live.exportFrame({format:'obj'}));await live.applySetting({name:'mode',value:4});
        const obj=await live.exportFrame({format:'obj'});assert.match(await obj.blob.text(),/^v /);
        mock.control.capture=false;await assert.rejects(()=>live.exportFrame({format:'raw'}));mock.control.capture=true;
        mock.control.mutationAck='fail-after-apply';await assert.rejects(()=>live.applySetting({name:'mode',value:3}));assert.equal(mock.source.mode,3);
        assert.equal(mock.counts['/colormode/setting/3'],1);
        console.log('console_source_contracts: demo/live coherence, units, ROI, histories, bounds, malformed/absent fields, deadlines, exports, lost-ack single attempt passed');
    }finally{global.fetch=originalFetch;await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
