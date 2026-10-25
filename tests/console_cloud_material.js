/* Dependency-free material validation/reference math; actual r128/WebGL is tested separately. */
'use strict';
const assert=require('node:assert/strict');
const C=require('../release/files/console-cloud-material');
assert.deepEqual(C.DEFAULT_POINT_STYLE,{shape:'circle',diameterCssPx:2.5,sizing:'screen'});
assert(Object.isFrozen(C.DEFAULT_POINT_STYLE));
for(const shape of ['circle','square'])for(const sizing of ['screen','perspective'])for(const diameterCssPx of [1,2.5,10]){
    const style={shape,sizing,diameterCssPx},copy=C.validatePointStyle(style);assert.deepEqual(copy,style);assert.notEqual(copy,style);
}
for(const style of [null,{}, {shape:'circle'}, ...['triangle',null,1].map(shape=>({...C.DEFAULT_POINT_STYLE,shape})),
    ...['world',null,1].map(sizing=>({...C.DEFAULT_POINT_STYLE,sizing})),
    ...[0,.5,10.5,NaN,Infinity,-Infinity,'2.5',null].map(diameterCssPx=>({...C.DEFAULT_POINT_STYLE,diameterCssPx}))])assert.throws(()=>C.validatePointStyle(style));
for(const range of [null,undefined,{}, {min:NaN,max:2},{min:0,max:Infinity},{min:2,max:2},{min:3,max:2}])assert.equal(C.pointColorRange(range),null);
for(const range of [{min:0,max:1},{min:-32768,max:32767}]){assert.deepEqual(C.pointColorRange(range),range);assert.notEqual(C.pointColorRange(range),range);}
const screen=C.DEFAULT_POINT_STYLE,perspective={...screen,sizing:'perspective'};
for(const z of [-100,0,.001,1,50,100,1000])assert.equal(C.pointSizePixels(screen,2,[1,64],100,z),5);
for(const [z,size] of [[-1,10],[0,10],[1,10],[50,10],[100,5],[200,2.5],[1e9,2.5]])assert.equal(C.pointSizePixels(perspective,2,[1,64],100,z),size);
assert.equal(C.pointSizePixels(perspective,2,[1,6],100,0),6);
assert.equal(C.pointSizePixels(perspective,.1,[1,6],100,1000),1);
assert.equal(C.pointSizePixels({...perspective,diameterCssPx:10},2,[1,64],100,1),40);
assert.match(C.pointVertexShader,/attribute float depth/);assert.match(C.pointFragmentShader,/discard/);
console.log('console_cloud_material: defaults, complete-style copy/validation, invalid/neutral ranges, inverse-view-depth bounds and hardware clamps passed');
