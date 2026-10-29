/* Capture-tool rendered provenance regression; loopback demo only, no camera APIs. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{capture}=require('../tools/capture_console');
const {validatePointStyle}=require('../release/files/console-cloud-material');
const evidence=path.resolve('_build/pointcloud-renderer-style'),square={shape:'square',diameterCssPx:2.5,sizing:'screen'};
function checkCapture(entry){
    assert.equal(entry.source,'demo');assert.equal(entry.seed,7);assert.equal(entry.renderReady,true);assert.equal(entry.layout.overflow,false);
    assert(Number.isSafeInteger(entry.frame)&&entry.frame>0,'source frame metadata remains separate');
    const cloud=entry.cloud;assert(cloud,'3D capture must retain structured cloud evidence');
    assert.deepEqual(validatePointStyle(cloud.pointStyle),entry.pointStyle||square);
    assert.equal(Object.hasOwn(cloud,'points'),false,'counters belong under cloud.renderer, not over rendered provenance');
    if(entry.ir||entry.noWebGL){
        assert.equal(cloud.frameId,null,'unavailable renderer must not borrow source frame identity');
        assert.equal(cloud.recordCount,null);assert.equal(cloud.range,null);assert.equal(cloud.renderer,null);assert.equal(entry.png,null);
        assert.match(entry.layout.imageState,entry.ir?/unavailable.*IR/i:/WebGL.*context/i);return {name:entry.name,available:false,...cloud};
    }
    assert.equal(cloud.frameId,entry.scenario==='frozen'?61:181);
    assert(Number.isSafeInteger(cloud.frameId)&&cloud.frameId>0);
    assert.equal(cloud.recordCount,entry.scenario==='cloud-empty'?0:307200);
    assert(Number.isSafeInteger(cloud.recordCount)&&cloud.recordCount>=0&&cloud.recordCount<=307200);
    assert.deepEqual(cloud.range,{min:0,max:3000});assert(Number.isFinite(cloud.range.min)&&Number.isFinite(cloud.range.max)&&cloud.range.min<cloud.range.max);
    const renderer=cloud.renderer;assert(renderer);assert.equal(renderer.points,cloud.recordCount);assert.deepEqual(renderer.pointStyle,cloud.pointStyle);
    assert(renderer.effectivePixelRatio>0&&renderer.effectivePixelRatio<=2);assert.equal(renderer.pointSizeRange.length,2);
    assert(renderer.pointSizeRange.every(Number.isFinite)&&renderer.pointSizeRange[0]>0&&renderer.pointSizeRange[1]>=renderer.pointSizeRange[0]);
    assert(renderer.pose.target.every(Number.isFinite)&&Number.isFinite(renderer.pose.radius)&&renderer.pose.radius>0);
    if(entry.png){const bytes=fs.readFileSync(entry.png.filename);assert.equal(bytes.subarray(0,8).toString('hex'),'89504e470d0a1a0a');assert.equal(bytes.length,entry.png.bytes);assert.equal(bytes.readUInt32BE(16),renderer.drawingBuffer[0]);assert(bytes.readUInt32BE(20)>renderer.drawingBuffer[1],'full viewport plus provenance footer');}
    return {name:entry.name,available:true,frameId:cloud.frameId,recordCount:cloud.recordCount,range:cloud.range,pointStyle:cloud.pointStyle,pose:renderer.pose,drawingBuffer:renderer.drawingBuffer};
}
(async()=>{
    const mock=await fixture(),reports=[];
    try{
        for(const [matrix,directory,count,options] of [['3d-style','qualification',22,{}],['3d','square-baseline-match',16,{pointStyle:square}]]){
            const out=path.join(evidence,'screenshots',directory),manifest=await capture({url:mock.origin+'/console.html',matrix,out,...options});
            assert.equal(manifest.captures.length,count);assert.deepEqual(manifest.errors,[]);assert.deepEqual(manifest.externalRequests,[]);
            assert.deepEqual(JSON.parse(fs.readFileSync(path.join(out,'manifest.json'),'utf8')),JSON.parse(JSON.stringify(manifest)));
            reports.push({matrix,directory,captures:manifest.captures.map(checkCapture),browser:manifest.browser});
            if(matrix==='3d-style')assert(manifest.captures.every(c=>c.png),'all22 nonempty style captures export actual PNGs');
            else {assert.equal(manifest.captures.filter(c=>c.cloud.renderer===null).length,2);assert.equal(manifest.captures.filter(c=>c.cloud.recordCount===0).length,1);}
        }
        assert.equal(mock.requests.some(p=>!/^\/[\w.-]+\.(html|js|css|png)$/.test(p)),false,'capture must not collect camera APIs');
        const out=path.join(evidence,'validation/capture-provenance-fix');fs.mkdirSync(out,{recursive:true});fs.writeFileSync(path.join(out,'report.json'),JSON.stringify({passed:true,reports,apiRequests:0},null,2));
        console.log('console_capture_provenance_browser: 22 style +16 square baseline-matrix captures retain rendered ID/count/normalized range/style, final nested renderer/pose/DPR/limits; empty and unavailable provenance,22 actual PNG exports and zero API requests passed');
    }finally{await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
