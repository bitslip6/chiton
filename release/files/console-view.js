(function(root){
    'use strict';
    const C=root.ChitonConsole, $=id=>document.getElementById(id);
    const text=(id,v)=>{const node=$(id);if(node.textContent!==String(v))node.textContent=v;};
    class View {
        constructor(){this.sensor=$('sensor');this.overlay=$('overlay');this.graph=$('history');this.lastGraph=-Infinity;this.imageTimes=[];this.raf=null;this.lastImage=null;}
        drawFrame(image,metadata,state){
            const canvas=this.sensor,ctx=canvas.getContext('2d'),width=image.synthetic?image.metadata.width:image.width,height=image.synthetic?image.metadata.height:image.height;
            if(canvas.width!==width||canvas.height!==height){canvas.width=width;canvas.height=height;this.overlay.width=width;this.overlay.height=height;}
            if(image.synthetic){
                const m=image.metadata,source=image.source,pixels=ctx.createImageData(width,height),data=pixels.data;
                for(let y=0;y<height;y++)for(let x=0;x<width;x++){
                    let z=0;try{z=source.sample(x,y,m);}catch(_){/* Unmapped panorama background. */}
                    const i=(y*width+x)*4,t=Math.max(0,Math.min(1,z/3000));
                    if(source.mode===3||source.mode===6){const color=Math.max(0,Math.min(1,(z-source.range.min)/(source.range.max-source.range.min)));data[i]=z?Math.round(255*Math.max(0,1-Math.abs(color*4-3))):0;data[i+1]=z?Math.round(255*Math.max(0,1-Math.abs(color*4-2))):0;data[i+2]=z?Math.round(255*Math.max(0,1-Math.abs(color*4-1))):0;}
                    else{const intensity=z?Math.round(35+t*210):3;data[i]=data[i+1]=data[i+2]=intensity;}
                    data[i+3]=255;
                }ctx.putImageData(pixels,0,0);
            }else ctx.drawImage(image,0,0);
            const now=state.frozenNow??performance.now(),previous=this.lastImage;
            this.lastImage={...metadata,received:metadata.reviewReceived??now,distinctReceived:previous?.id===metadata.id&&previous?.mode===metadata.mode?previous.distinctReceived:(metadata.reviewReceived??now)};
            this.imageTimes.push(now);while(this.imageTimes.length>120||this.imageTimes[0]<now-2000)this.imageTimes.shift();
            this.drawOverlay(state);text('image-id','Image frame '+metadata.id);text('image-state',this.imageStale(now)?'Image delayed / stale':'Image live');
            const names=['SLAM','Grayscale intensity','Gain','Color intensity','3D · legacy','Panorama · grayscale','Panorama · color'];
            text('representation',names[metadata.mode]+' · '+width+' × '+height);return true;
        }
        imageStale(now){return this.lastImage && (now-this.lastImage.distinctReceived>1000 || (this.lastImage.age!=null && this.lastImage.age+now-this.lastImage.received>1000));}
        clearImage(){this.sensor.getContext('2d').clearRect(0,0,this.sensor.width,this.sensor.height);this.lastImage=null;this.drawOverlay({selection:null});}
        drawOverlay(s){
            const canvas=this.overlay,ctx=canvas.getContext('2d');ctx.clearRect(0,0,canvas.width,canvas.height);
            const selection=s.dragSelection||s.selection;if(s.device?.mode===4||!$('show-overlay').checked||!selection)return;
            const x=selection.x,y=selection.y,scale=canvas.width/640;
            ctx.lineWidth=2*scale;ctx.strokeStyle='#000';ctx.setLineDash([]);
            if(selection.kind==='roi'){ctx.strokeRect(x,y,selection.x2-x,selection.y2-y);ctx.strokeStyle='#8defff';ctx.setLineDash([6*scale,4*scale]);ctx.strokeRect(x,y,selection.x2-x,selection.y2-y);}
            else{
                const cross=()=>{ctx.beginPath();ctx.moveTo(x-14*scale,y);ctx.lineTo(x-4*scale,y);ctx.moveTo(x+4*scale,y);ctx.lineTo(x+14*scale,y);ctx.moveTo(x,y-14*scale);ctx.lineTo(x,y-4*scale);ctx.moveTo(x,y+4*scale);ctx.lineTo(x,y+14*scale);ctx.stroke();ctx.strokeRect(x-3*scale,y-3*scale,6*scale,6*scale);};
                ctx.lineWidth=4*scale;cross();ctx.strokeStyle='#98f5ff';ctx.lineWidth=1.5*scale;cross();
            }
            ctx.font=12*scale+'px ui-monospace,monospace';const label=selection.kind==='roi'?`${x},${y} → ${selection.x2},${selection.y2}`:`${x}, ${y}`;
            const w=ctx.measureText(label).width+12*scale,tx=Math.min(canvas.width-w,Math.max(0,x+16*scale)),ty=Math.max(24*scale,y-16*scale);
            ctx.save();ctx.strokeStyle='#03080d';ctx.lineWidth=3*scale;ctx.lineJoin='round';ctx.strokeText(label,tx+6*scale,ty);ctx.fillStyle='#b6f7ff';ctx.fillText(label,tx+6*scale,ty);ctx.restore();
        }
        schedule(state){if(this.raf!=null)return;this.raf=requestAnimationFrame(now=>{this.raf=null;this.render(state,now);});}
        stop(){if(this.raf!=null)cancelAnimationFrame(this.raf);this.raf=null;}
        render(s,now){
            now=s.frozenNow??now;
            const m=s.latest,fmt=v=>C.format(v,s.precision),unit=C.unitLabel(m?.units),health=C.health(m,now,s.hz,s.phase,s.device.capture);
            text('measurement-state',health);$('measurement-state').dataset.stale=String(/stale|Disconnected|disabled/.test(health));
            text('measured',fmt(m?.value));text('units',unit);text('estimate-units',unit);
            text('measured-label',s.selection.kind==='roi'?'Center pixel (processed)':m?.frameType==='ir'?'Measured IR (processed)':'Measured depth (processed)');
            text('measurement-scope',m?`${m.frameType==='ir'?'IR':'Depth'} · ${m.unitsScope==='calibration_aoi'?'calibration AOI':m.unitsScope==='full_sensor'?'full sensor':'scope unknown'}`:'No acknowledged data');
            text('measurement-frame',m?.frameId??'—');
            text('measurement-processing',`EMA ${m?.smoothed==null?'unknown':m.smoothed?'on':'off'} · plugin ${m?.plugin==null?'unknown':m.plugin?'on':'off'}`);
            for(const [id,key] of [['mean','mean'],['stddev','stddev'],['minimum','min'],['maximum','max'],['range','range']])text(id,fmt(m?.[key]));
            text('delta',m&&s.reference!=null?fmt(m.value-s.reference):'—');
            text('api-heading',s.selection.kind==='roi'?'API · spatial ROI statistics':'API · observed-frame statistics');
            text('api-count',C.format(m?.count,0));text('api-count-kind',s.selection.kind==='roi'?'pixels':'samples');
            text('api-span',m?.firstTime!=null&&m?.lastTime!=null?fmt((m.lastTime-m.firstTime)/1000):'unavailable');
            $('reference').disabled=!m;$('clear-reference').disabled=s.reference==null;
            const eligible=s.selection.kind==='point'&&m?.frameType==='depth';$('filter').disabled=!eligible;
            const filtering=eligible && s.filter!=='off';$('estimate-panel').hidden=!filtering;
            text('estimate',fmt(s.estimate));text('filter-state',s.kalman.reason);text('filter-preset-label',s.filter);
            text('filter-note',!eligible?'Unavailable for IR / spatial ROI':'Point-only · image unchanged · '+(s.filter==='off'?'Filter off':s.sourceName==='demo'?'demo tuning':'acknowledged local tuning'));
            const local=s.history.window(now),stats=C.stats(local.map(v=>v.value));
            const aligned=local.filter(v=>v.estimate!=null&&v.segment===s.segment),filtered=C.stats(aligned.map(v=>v.estimate)),measured=C.stats(aligned.map(v=>v.value));
            for(const id of ['median','mad','p05','p95'])text(id,fmt(stats?.[id]));
            text('local-sigma',fmt(measured?.stddev));text('filtered-sigma',fmt(filtered?.stddev));
            const span=local.length>1?(local.at(-1).received-local[0].received)/1000:0;
            text('local-count',local.length);text('local-span',fmt(span));
            text('local-ready',local.length>=10&&span>=1?'Ready (UI heuristic)':'Warming up');
            text('uncertainty-value',s.estimate==null?'—':fmt(Math.sqrt(s.kalman.P)));text('uncertainty-units',unit);
            const times=s.distinctTimes.filter(t=>t>=now-2000),elapsed=times.length>1?times.at(-1)-times[0]:0;
            const hz=elapsed>0?(times.length-1)*1000/Math.max(elapsed,now-times[0]):0;
            text('measurement-hz',C.format(hz,1)+' Hz');text('latency',C.format(s.latency,0)+' ms');
            text('age',m?.frameAgeMs==null?'unknown':C.format(m.frameAgeMs+now-m.received,0)+' ms');
            const it=this.imageTimes.filter(t=>t>=now-2000);text('image-fps',C.format(it.length>1?(it.length-1)*1000/Math.max(it.at(-1)-it[0],now-it[0]):0,1)+' FPS');text('skipped',s.skipped);
            text('provenance',m?`${s.sourceName==='demo'?'DEMO · SYNTHETIC':'Live API'} · measurement frame ${m.frameId} · revision ${m.revision} · ${m.units} / ${m.unitsScope||'scope unknown'} · EMA ${m.smoothed==null?'unknown':m.smoothed?'on':'off'} · plugin ${m.plugin==null?'unknown':m.plugin?'applied':'off'} · all uint16 included.`:'Metadata unavailable');
            if(s.device.mode===4)this.renderCloud(s,now);
            else if(this.imageStale(now))text('image-state','Image delayed / stale');
            if(s.phase==='paused')text('image-state','Viewer paused');
            $('image-fps').parentElement.hidden=s.device.mode===4;
            if(now-this.lastGraph>=50){this.drawGraph(s,now);this.lastGraph=now;}
        }
        syncCloudRenderingControls(s){
            const counters=this.cloud?.counters(),available=s.device.mode===4&&s.device.frameType==='depth'&&!!counters&&!s.cloudUnavailable&&s.device.capture!==false;
            for(const id of ['cloud-fit-all','cloud-fit-main','cloud-orientation'])$(id).disabled=!available||!counters.points;
            $('cloud-projection').disabled=!available;$('cloud-projection').value=s.cloudProjection;
            $('cloud-edl').disabled=!available||!counters.edlSupported;$('cloud-edl').checked=!!counters?.eyeDomeLighting;
            $('cloud-edl').title=counters?.edlUnavailable||'Viewer-only screen-space depth shading; additional GPU work. Included in PNGs.';
            const framing=counters?.framing;$('cloud-framing-status').hidden=s.device.mode!==4||framing?.mode!=='main';
            text('cloud-framing-status',framing?`Last main fit: ${framing.included} / ${framing.total} used for framing · all records retained`:'');
            text('cloud-framing-note',framing?`Last fit: ${framing.mode==='main'?'main cloud':'all records'} · ${framing.included} / ${framing.total} records used for framing. All records remain submitted; no validity filtering. ${counters.projection==='orthographic'?'Orthographic':'Perspective'} projection.`:'Fit all includes every returned record. Fit main cloud changes framing only.');
            text('cloud-edl-note',counters?.edlUnavailable?'Eye-dome lighting unavailable: '+counters.edlUnavailable+'. Plain points are still available.':`Eye-dome lighting ${counters?.eyeDomeLighting?'on':'off'} · optional local depth shading, not a measurement or palette change. Extra GPU work; also applies to PNGs.`);
        }
        renderCloud(s,now){
            this.syncCloudRenderingControls(s);
            const tracker=s.cloud,m=tracker.latest,counters=this.cloud?.counters(now),fmt=(n,d=0)=>C.format(n,d);
            const state=s.cloudUnavailable||(s.device.capture===false?'Cloud · capture disabled':tracker.health(now,s.cloudHz)+(tracker.error?' · '+tracker.error:''));
            text('image-state',state);text('image-id','Cloud frame '+(m?.frameId??'—'));
            text('representation','3D · depth points · API XYZ');
            text('cloud-target',s.cloudHz+' updates/s');text('cloud-hz',fmt(tracker.hz(now),1)+' Hz');text('cloud-fps',fmt(counters?.renderFPS,1)+' FPS');
            text('cloud-count',fmt(m?.recordCount)+' / '+fmt(counters?.points));text('cloud-transfer',fmt(tracker.latency)+' ms / '+fmt(m?.bytes)+' B');
            text('cloud-age',m?.frameAgeMs==null?'unknown':fmt(m.frameAgeMs+now-m.receivedMonoMs)+' ms');
            const range=this.cloud?.range;
            text('cloud-legend',range?`Depth ${range.min}…${range.max} · ${C.unitLabel(m?.units)}${m?.unitsScope==='calibration_aoi'?' (AOI)':''}`:'Depth color range unavailable · neutral points');
            text('cloud-provenance',(m?`${m.source==='demo'?'DEMO · synthetic projection':'Live API'} · cloud frame ${m.frameId} · revision ${m.revision} · ${m.units} / ${m.unitsScope} · API axes unchanged · all returned signed records included.`:'Cloud metadata unavailable')+(tracker.error?' Last cloud error: '+tracker.error:''));
            text('cloud-resources',counters?`Renderer objects: ${counters.geometries} geometries / ${counters.textures} textures · ${counters.calls} draw calls · CPU attribute capacity ${counters.bufferBytes} B · owned records ${m?.records.byteLength??0} B · geometry upload requests ${counters.geometryUploads} / ${counters.uploadedAttributeBytes} requested attribute B (not observed GL traffic) · changed uniform sets ${counters.uniformUpdates} · style ${counters.pointStyle.shape} / ${counters.pointStyle.diameterCssPx} CSS px / ${counters.pointStyle.sizing} · effective DPR ${fmt(counters.effectivePixelRatio,3)} · hardware point diameter ${counters.pointSizeRange.join('…')} physical px${counters.hardwareSizeLimited?(counters.pointStyle.sizing==='perspective'?' (may limit perspective sizes)':' (requested size limited)'):''} · ${counters.projection} · EDL ${counters.eyeDomeLighting?'on (2 passes)':'off'} · drawing buffer ${counters.drawingBuffer.join(' × ')} (not a GPU memory measurement)`:'Renderer unavailable');
        }
        drawGraph(s,now){
            const canvas=this.graph,dpr=Math.min(devicePixelRatio||1,3),rect=canvas.getBoundingClientRect();
            if(!rect.width||!rect.height)return;
            const width=Math.round(rect.width*dpr),height=Math.round(rect.height*dpr);
            if(canvas.width!==width||canvas.height!==height){canvas.width=width;canvas.height=height;}
            const ctx=canvas.getContext('2d');ctx.setTransform(dpr,0,0,dpr,0,0);const w=rect.width,h=rect.height;
            ctx.clearRect(0,0,w,h);
            const rows=s.held||s.history.items,end=s.held?rows.at(-1)?.received??now:now,ms=s.windowSeconds*1000;
            const visible=rows.filter(v=>v.received>=end-ms),values=visible.flatMap(v=>v.estimate==null?[v.value]:[v.value,v.estimate]);
            const low=values.length?Math.min(...values):0,high=values.length?Math.max(...values):1,pad=Math.max(5,(high-low)*.15),min=low-pad,max=high+pad;
            const x=t=>52+(t-end+ms)/ms*(w-62),y=v=>10+(max-v)/(max-min)*(h-30);
            ctx.font='11px system-ui';ctx.fillStyle='#9aaabd';ctx.lineWidth=1;ctx.setLineDash([]);
            for(let i=0;i<3;i++){const value=min+(max-min)*i/2,py=y(value);ctx.strokeStyle='#293849';ctx.beginPath();ctx.moveTo(50,py);ctx.lineTo(w,py);ctx.stroke();ctx.fillText(C.format(value,0),0,py+4);}
            for(let i=0;i<=4;i++)ctx.fillText((-(ms/1000)*(1-i/4)).toFixed(1),Math.min(w-24,52+(w-62)*i/4),h-2);
            for(const [key,color,dash] of [['value','#e8eff6',[]],['estimate','#48d4e8',[5,4]]]){
                ctx.strokeStyle=color;ctx.setLineDash(dash);ctx.lineWidth=1.7;ctx.beginPath();let previous=null;
                for(const row of visible){if(row[key]==null){previous=null;continue;}if(!previous||row.segment!==previous.segment||row.received-previous.received>Math.max(500,3000/s.hz))ctx.moveTo(x(row.received),y(row[key]));else ctx.lineTo(x(row.received),y(row[key]));previous=row;}ctx.stroke();
            }
        }
        async png(state,overlays){
            if(state.device.mode===4){
                if(!this.cloud||state.cloudUnavailable||state.device.capture===false)throw new Error('3D viewport unavailable');
                return this.cloud.png(state.cloud.latest,{source:state.sourceName,stale:state.cloud.health(performance.now(),state.cloudHz).includes('stale'),overlays,range:state.device.range});
            }
            if(!this.lastImage)throw new Error('No rendered image to export');
            const canvas=document.createElement('canvas');canvas.width=this.sensor.width;canvas.height=this.sensor.height;const ctx=canvas.getContext('2d');ctx.drawImage(this.sensor,0,0);
            if(overlays)ctx.drawImage(this.overlay,0,0);
            const label=(state.sourceName==='demo'?'DEMO · SYNTHETIC DATA':'CHITON · LIVE API')+' · image frame '+this.lastImage.id;
            ctx.fillStyle='#0b1118';ctx.fillRect(0,canvas.height-30,canvas.width,30);ctx.fillStyle='#e8eff6';ctx.font='14px system-ui';ctx.fillText(label,12,canvas.height-10);
            return new Promise((resolve,reject)=>canvas.toBlob(blob=>blob?resolve(blob):reject(new Error('PNG encoding failed')),'image/png'));
        }
    }
    Object.assign(C,{$,text,View});
})(window);
