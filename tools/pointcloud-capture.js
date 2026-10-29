/* Recorded-file forensic viewer. No device routes, polling, filters or mutations. */
'use strict';
(async()=>{
 const $=id=>document.getElementById(id),C=ChitonConsole,T=THREE;
 const response=await fetch('/capture');if(!response.ok)throw new Error('Capture unavailable');
 const b=await response.json(),m=b.manifest,n=m.record_count;
 const snapshot={records:new Int16Array(b.records.flat()),recordCount:n,frameId:m.frame_id,revision:m.revision,units:m.processed_units,unitsScope:m.units_scope,frameType:'depth',source:'recorded'};
 const v=new C.CloudView($('cloud'),{onUnavailable:message=>{$('status').textContent=message;}});
 v.update(snapshot,{min:0,max:6500});v.setOverlay(false);v.setProjection('orthographic');v.setOrientation('top');v.setPointStyle({shape:'circle',diameterCssPx:3,sizing:'screen'});
 const selected=new Set(),selectionAttribute=new T.BufferAttribute(new Float32Array(C.CLOUD_MAX),1);
 const ids=new Float32Array(C.CLOUD_MAX);for(let i=0;i<n;i++)ids[i]=i+1;
 v.geometry.setAttribute('recordId',new T.BufferAttribute(ids,1));v.geometry.setAttribute('selectionFlag',selectionAttribute);
 const anchor='vec4 viewPosition = modelViewMatrix * vec4(position, 1.0);';
 if(!v.material.vertexShader.includes(anchor))throw new Error('Recorded viewer shader contract changed');
 v.material.vertexShader='attribute float recordId;\nattribute float selectionFlag;\n'+v.material.vertexShader.replace(anchor,'if(selectionFlag > 0.5) pointColor=vec3(1.0,0.05,0.8);\n'+anchor);
 v.material.needsUpdate=true;
 const pickMaterial=v.material.clone();pickMaterial.vertexShader=pickMaterial.vertexShader.replace(anchor,
  'pointColor=vec3(mod(recordId,256.0),mod(floor(recordId/256.0),256.0),floor(recordId/65536.0))/255.0;\n'+anchor);
 let ranges={raw:[0,4095],processed:[0,6500]},exporting=false;
 function showSelection(){
  selectionAttribute.array.fill(0);for(const k of selected)selectionAttribute.array[k]=1;selectionAttribute.needsUpdate=true;v.dirty();
  const rows=[`RECORDED ${m.acquisition_backend} · frame ${m.frame_id}, process ${m.process_id}/${m.process_started_ns}`,`${selected.size} selected records; magenta in cloud and depth. Top-left sensor origin.`,
   `Diagnostic filter: ${b.report.filter_policy}; ${b.report.excluded_sample_count} excluded sample candidates.`,
   `Pixel status counts: ${JSON.stringify(b.report.status_counts)}`,
   `Projection mismatches: ${b.report.projection_mismatch_records.length}; processing mismatches: ${b.report.processing_mismatch_records.length}`];
  for(const k of [...selected].slice(0,32)){const i=b.indices[k];rows.push(`#${k} → (${i%640},${Math.floor(i/640)}) sensor=${b.raw[i]} SDK units; processed=${b.processed[i]} ${m.processed_units}; XYZD=${b.records[k].join(',')} flag=${b.flags[k]}`);}
  if(selected.size>32)rows.push('Additional selected rows are included in CSV.');$('details').textContent=rows.join('\n');
  drawDepth($('raw'),'raw',true);drawDepth($('processed'),'processed',true);
 }
 function drawDepth(canvas,kind,marked){
  const ctx=canvas.getContext('2d'),image=ctx.createImageData(640,480),values=b[kind],[lo,hi]=ranges[kind];
  for(let i=0;i<values.length;i++){
   const shade=Math.round(255*(1-Math.max(0,Math.min(1,(values[i]-lo)/(hi-lo))))),o=i*4;
   image.data[o]=image.data[o+1]=image.data[o+2]=shade;image.data[o+3]=255;
   if(b.pixel_status[i]>1&&(kind==='processed'||marked)){image.data[o]=130;image.data[o+1]=25;image.data[o+2]=30;}
   if(kind==='processed'&&!b.domain[i]){image.data[o]=image.data[o+1]=image.data[o+2]=((i%640+Math.floor(i/640))%8<4?22:40);}
  }
  ctx.putImageData(image,0,0);
  ctx.fillStyle='#101820';ctx.fillRect(0,480,640,80);ctx.fillStyle='white';ctx.font='12px monospace';
  ctx.fillText(`${kind==='raw'?'Original SDK depth':'Processed depth'} · ${m.acquisition_backend} · frame ${m.frame_id}`,8,498);
  ctx.fillText(`${lo} (white) … ${hi} (black) ${kind==='raw'?'unknown SDK depth units':m.processed_units}`,8,516);
  ctx.fillText('640×480 · origin top-left · magenta=selected source pixel centers',8,534);
  ctx.fillText(kind==='processed'?'Red=excluded; hatched=outside runlines; valid unsampled depth shown.':marked?'Red=excluded; original unmarked sensor PNG is preserved.':'All original sensor pixels shown; no cloud-based validity filtering.',8,552);
  if(marked){ctx.strokeStyle='#ff0dcc';ctx.fillStyle='#ff0dcc';let labels=0;
   for(const k of selected){const i=b.indices[k],x=i%640+.5,y=Math.floor(i/640)+.5;ctx.strokeRect(x-3,y-3,6,6);ctx.fillRect(Math.floor(x),Math.floor(y),1,1);
    if(labels++<32)ctx.fillText('#'+k,Math.min(590,x+5),Math.max(12,y-5));}}
  return canvas;
 }
 function pickRect(x0,y0,x1=x0,y1=y0){
  if(exporting)return [];
  v.render();const width=v.canvas.width,height=v.canvas.height;
  const minX=Math.max(0,Math.min(width-1,Math.floor(Math.min(x0,x1)))),maxX=Math.max(minX,Math.min(width-1,Math.floor(Math.max(x0,x1))));
  const minY=Math.max(0,Math.min(height-1,Math.floor(Math.min(y0,y1)))),maxY=Math.max(minY,Math.min(height-1,Math.floor(Math.max(y0,y1))));
  const target=new T.WebGLRenderTarget(width,height,{minFilter:T.NearestFilter,magFilter:T.NearestFilter,depthBuffer:true,stencilBuffer:false});
  const old=v.points.material,clear=v.renderer.getClearColor(new T.Color()),oldTarget=v.renderer.getRenderTarget();
  const pixels=new Uint8Array((maxX-minX+1)*(maxY-minY+1)*4),found=new Set();
  try{
   pickMaterial.uniforms=v.material.uniforms;v.points.material=pickMaterial;v.renderer.setClearColor(0);v.renderer.setRenderTarget(target);
   v.renderer.render(v.scene,v.camera);v.checkPrograms();
   v.renderer.readRenderTargetPixels(target,minX,height-1-maxY,maxX-minX+1,maxY-minY+1,pixels);
   for(let p=0;p<pixels.length;p+=4){const k=pixels[p]+pixels[p+1]*256+pixels[p+2]*65536-1;if(k>=0&&k<n)found.add(k);}
  }finally{v.points.material=old;v.renderer.setClearColor(clear);v.renderer.setRenderTarget(oldTarget);target.dispose();v.dirty();}
  for(const k of found)selected.add(k);showSelection();return [...found];
 }
 v.canvas.addEventListener('keydown',event=>{if(exporting){event.preventDefault();event.stopImmediatePropagation();}},true);
 let drag=null;
 function point(event){const r=v.canvas.getBoundingClientRect();return [(event.clientX-r.left)*v.canvas.width/r.width,(event.clientY-r.top)*v.canvas.height/r.height];}
 v.canvas.addEventListener('pointerdown',event=>{if(!$('select').checked||exporting)return;event.preventDefault();event.stopImmediatePropagation();drag=point(event);v.canvas.setPointerCapture(event.pointerId);},true);
 v.canvas.addEventListener('pointermove',event=>{if(drag){event.preventDefault();event.stopImmediatePropagation();}},true);
 v.canvas.addEventListener('pointerup',event=>{if(!drag)return;event.preventDefault();event.stopImmediatePropagation();const p=point(event),start=drag;drag=null;if(v.canvas.hasPointerCapture(event.pointerId))v.canvas.releasePointerCapture(event.pointerId);pickRect(...start,...p);},true);
 v.canvas.addEventListener('pointercancel',()=>{drag=null;},true);
 $('top').onclick=()=>v.setOrientation('top');$('front').onclick=()=>v.setOrientation('front');
 $('projection').onchange=()=>v.setProjection($('projection').value);
 $('diameter').onchange=()=>{try{v.setPointStyle({shape:'circle',diameterCssPx:Number($('diameter').value),sizing:'screen'});}catch(e){$('status').textContent=e.message;}};
 $('clear').onclick=()=>{selected.clear();showSelection();};
 $('ranges').onclick=()=>{try{const next={};for(const name of ['raw','processed']){const lo=Number($(name+'-min').value),hi=Number($(name+'-max').value);if(!Number.isInteger(lo)||!Number.isInteger(hi)||lo<0||hi>65535||hi<=lo)throw new Error('Display ranges require 0 ≤ min < max ≤ 65535');next[name]=[lo,hi];}ranges=next;v.setColorRange({min:ranges.processed[0],max:ranges.processed[1]});showSelection();}catch(e){$('status').textContent=e.message;}};
 function pose(){v.render();return {navigation:v.nav.pose(),projection:v.projection,view_matrix:v.camera.matrixWorldInverse.toArray(),projection_matrix:v.camera.projectionMatrix.toArray(),viewport:[v.canvas.width,v.canvas.height],dpr:v.effectivePixelRatio,style:{...v.pointStyle},clipping:'off',cleanup:'off',edl:false};}
 function cloudImage(label){
  v.render();const canvas=document.createElement('canvas');canvas.width=Math.max(640,v.canvas.width);let ctx=canvas.getContext('2d');ctx.font='14px monospace';
  const lines=[];
  for(const text of [`RECORDED ${m.acquisition_backend} · ${label} · frame ${m.frame_id} · ${n} retained records`,
   `API axes / ${m.processed_units} · depth colors ${ranges.processed.join('…')} · ${selected.size} selected · no hiding`,
   `Diagnostic exclusions: ${b.report.excluded_sample_count} sample candidates · ${b.report.filter_policy}`,
   'Labels identify projected selected centers, including centers occluded in this view.']){
   let line='';for(const word of text.split(' ')){if(line&&ctx.measureText(line+' '+word).width>canvas.width-16){lines.push(line);line=word;}else line+=(line?' ':'')+word;}lines.push(line);
  }
  canvas.height=v.canvas.height+20+lines.length*20;ctx=canvas.getContext('2d');ctx.fillStyle='#101820';ctx.fillRect(0,0,canvas.width,canvas.height);ctx.drawImage(v.canvas,0,0);
  ctx.font='14px monospace';ctx.fillStyle='white';lines.forEach((line,i)=>ctx.fillText(line,8,v.canvas.height+22+i*20));
  ctx.fillStyle='#ff0dcc';let count=0;
  for(const k of selected){if(count++>=32)break;const p=new T.Vector3(...b.records[k].slice(0,3)).project(v.camera);
   if(p.z>=-1&&p.z<=1&&Math.abs(p.x)<=1&&Math.abs(p.y)<=1)ctx.fillText('#'+k,(p.x+1)*v.canvas.width/2+4,(1-p.y)*v.canvas.height/2-4);}
  return canvas;
 }
 async function exportImages(){
  if(exporting)return;exporting=true;$('export').disabled=true;
  const original=pose(),images={},poses={current:original};
  const disabled=[];for(const e of document.querySelectorAll('button,input,select')){disabled.push([e,e.disabled]);e.disabled=true;}
  v.canvas.style.pointerEvents='none';
  try{
   images['cloud-view.png']=cloudImage('selected view');
   for(const [name,orientation] of [['cloud-top.png','top'],['cloud-front.png','front']]){v.setProjection('orthographic');v.setOrientation(orientation);poses[orientation]=pose();images[name]=cloudImage(orientation);}
   for(const [kind,prefix] of [['raw','sensor-depth'],['processed','processed-depth']])for(const marked of [false,true]){
    const c=document.createElement('canvas');c.width=640;c.height=560;images[prefix+(marked?'-marked':'')+'.png']=drawDepth(c,kind,marked);
   }
   const combined=document.createElement('canvas');combined.width=1920;combined.height=640;const ctx=combined.getContext('2d');ctx.fillStyle='#101820';ctx.fillRect(0,0,1920,640);
   for(const [column,name] of ['cloud-view.png','sensor-depth-marked.png','processed-depth-marked.png'].entries()){
    const c=images[name],scale=Math.min(640/c.width,550/c.height);ctx.drawImage(c,column*640,50,c.width*scale,c.height*scale);
    ctx.fillStyle='white';ctx.font='16px monospace';ctx.fillText(['Selected 3D view · '+m.processed_units,'Original SDK depth · unknown units','Processed depth · '+m.processed_units][column],column*640+8,22);
   }
   ctx.fillStyle='white';ctx.font='18px monospace';ctx.fillText(`Same capture frame ${m.frame_id}; magenta source correspondence; scales and units are independent and labeled.`,8,628);images['comparison.png']=combined;
   const encoded={};for(const [name,c] of Object.entries(images))encoded[name]=c.toDataURL('image/png').split(',')[1];
   const selection={record_ids:[...selected].sort((a,b)=>a-b),poses,depth_ranges:ranges,image_origin:'top-left',depth_pixel_dimensions:[640,480],selection_semantics:'visible point fragments; projected-center labels may be occluded in other views'};
   const r=await fetch('/export',{method:'POST',headers:{'Content-Type':'application/json','X-Capture-Token':window.CAPTURE_TOKEN},body:JSON.stringify({images:encoded,selection})});
   const result=await r.json();if(!r.ok)throw new Error(result.error||'Export failed');$('status').textContent='Written: '+result.directory;return result;
  }finally{Object.assign(v.nav,original.navigation);v.setProjection(original.projection);v.dirty();exporting=false;v.canvas.style.pointerEvents='';for(const [e,wasDisabled] of disabled)e.disabled=wasDisabled;$('export').disabled=false;}
 }
 $('export').onclick=()=>exportImages().catch(e=>{$('status').textContent=e.message;});
 window.CaptureViewer={bundle:b,view:v,selected,pickRect,exportImages,pose,showSelection};showSelection();
 window.CONSOLE_READY=true;
})().catch(error=>{document.getElementById('status').textContent=error.stack||error.message;console.error(error);});
