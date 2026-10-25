/* Three.js r128, local navigation, on-demand <=30 Hz paint. No transport or acquisition timers. */
(function(root){
    'use strict';
    const C=root.ChitonConsole;
    class CloudView {
        constructor(host,{onUnavailable=()=>{}}={}){
            this.host=host;this.onUnavailable=onUnavailable;this.nav=new C.CloudCamera();this.listeners=[];this.pointers=new Map();this.raf=null;this.lastPaint=-Infinity;this.paints=[];this.active=true;this.fitted=false;this.disposed=false;this.uploads=0;this.uploadedAttributeBytes=0;this.uniformUpdates=0;this.count=0;
            this.clipSettings=C.defaultClipSettings();this.clipUpdates=0;this.clipStats={matchingCount:0,clippedCount:0,submittedCount:0};this.clipStatsFrameId=null;this.clipStatsContext=null;
            this.pointStyle=C.validatePointStyle(C.DEFAULT_POINT_STYLE);this.range=null;this.referenceDistance=1;this.effectivePixelRatio=1;this.projection='perspective';this.framing={mode:'all',included:0,total:0};this.eyeDomeLighting=false;this.edl=null;this.edlError=null;
            const T=root.THREE;if(!T||T.REVISION!=='128')throw new Error('Bundled Three.js r128 unavailable');
            try{
            this.renderer=new T.WebGLRenderer({antialias:false,alpha:false});
            const gl=this.renderer.getContext();this.pointSizeRange=Array.from(gl.getParameter(gl.ALIASED_POINT_SIZE_RANGE));
            this.edlSupported=(this.renderer.capabilities.isWebGL2||this.renderer.extensions.has('WEBGL_depth_texture'))&&gl.getShaderPrecisionFormat(gl.FRAGMENT_SHADER,gl.HIGH_FLOAT)?.precision>0;
            this.renderer.setClearColor(0x03080d);this.canvas=this.renderer.domElement;this.canvas.id='cloud-canvas';this.canvas.tabIndex=0;
            this.canvas.setAttribute('aria-label','3D API coordinates. Drag to rotate; Shift or secondary drag to pan; wheel to zoom. Arrow keys rotate, Shift arrows pan, plus/minus zoom, F fit, R reset.');
            this.scene=new T.Scene();this.camera=new T.PerspectiveCamera(45,1,.1,1000000);this.perspectiveCamera=this.camera;
            this.orthographicCamera=new T.OrthographicCamera(-1,1,1,-1,.1,1000000);
            this.geometry=new T.BufferGeometry();this.records=new Int16Array(C.CLOUD_MAX*4);
            this.attributeBuffer=new T.InterleavedBuffer(this.records,4).setUsage(T.DynamicDrawUsage);
            this.geometry.setAttribute('position',new T.InterleavedBufferAttribute(this.attributeBuffer,3,0,false));
            this.geometry.setAttribute('depth',new T.InterleavedBufferAttribute(this.attributeBuffer,1,3,false));
            this.geometry.setDrawRange(0,0);this.material=C.createCloudMaterial(T,this.pointSizeRange);
            this.points=new T.Points(this.geometry,this.material);this.points.frustumCulled=false;this.points.visible=false;this.scene.add(this.points);
            this.axes=new T.AxesHelper(1);this.scene.add(this.axes);this.host.append(this.canvas);
            const on=(name,fn,options)=>{this.canvas.addEventListener(name,fn,options);this.listeners.push(()=>this.canvas.removeEventListener(name,fn,options));};
            on('contextmenu',e=>e.preventDefault());
            on('webglcontextlost',e=>{e.preventDefault();this.setActive(false);this.onUnavailable('WebGL context lost. Retry 3D explicitly.');});
            on('pointerdown',e=>{if(!this.active||e.button>2)return;e.preventDefault();this.canvas.focus();this.canvas.setPointerCapture(e.pointerId);this.pointers.set(e.pointerId,{x:e.clientX,y:e.clientY,pan:e.shiftKey||e.altKey||e.button!==0});});
            on('pointermove',e=>{
                const old=this.pointers.get(e.pointerId);if(!old||!this.active)return;e.preventDefault();
                const before=[...this.pointers.values()].map(p=>({...p}));this.pointers.set(e.pointerId,{...old,x:e.clientX,y:e.clientY});const after=[...this.pointers.values()];
                if(after.length>=2){const distance=p=>Math.hypot(p[0].x-p[1].x,p[0].y-p[1].y),centroid=p=>[(p[0].x+p[1].x)/2,(p[0].y+p[1].y)/2],a=centroid(before),b=centroid(after);
                    this.nav.pan(b[0]-a[0],b[1]-a[1]);this.nav.zoom(Math.log(Math.max(1,distance(before))/Math.max(1,distance(after))));
                }else if(old.pan||this.mode==='pan')this.nav.pan(e.clientX-old.x,e.clientY-old.y);
                else this.nav.orbit(-(e.clientX-old.x)*.008,-(e.clientY-old.y)*.008);
                this.dirty();
            });
            const release=e=>{this.pointers.delete(e.pointerId);if(this.canvas.hasPointerCapture(e.pointerId))this.canvas.releasePointerCapture(e.pointerId);};
            for(const event of ['pointerup','pointercancel','lostpointercapture'])on(event,release);
            on('wheel',e=>{if(!this.active)return;e.preventDefault();this.nav.zoom(e.deltaY*(e.deltaMode===1?.03:.001));this.dirty();},{passive:false});
            on('keydown',e=>{if(!this.active)return;const arrows={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]},d=arrows[e.key];
                if(d){if(e.shiftKey||this.mode==='pan')this.nav.pan(d[0]*12,d[1]*12);else this.nav.orbit(d[0]*.08,d[1]*.08);}
                else if(['+','=','-','_'].includes(e.key))this.nav.zoom(e.key==='-'||e.key==='_'?.15:-.15);
                else if(e.key.toLowerCase()==='f')this.fit();else if(e.key.toLowerCase()==='r')this.reset();else return;
                e.preventDefault();this.dirty();
            });
            this.observer=new ResizeObserver(()=>this.resize());this.observer.observe(host);this.resize();
            this.renderer.compile(this.scene,this.camera);this.checkPrograms();
            }catch(error){this.dispose();throw error;}
        }
        checkPrograms(){
            const gl=this.renderer.getContext();this.checkedPrograms=this.checkedPrograms||new WeakSet();
            for(const program of this.renderer.info.programs)if(!this.checkedPrograms.has(program)){
                if(!gl.getProgramParameter(program.program,gl.LINK_STATUS))throw new Error('Point renderer shader compilation/link failed');
                this.checkedPrograms.add(program);
            }
        }
        resize(){if(this.disposed||!this.active)return;const r=this.host.getBoundingClientRect();if(!r.width||!r.height)return;
            const dpr=Math.min(root.devicePixelRatio||1,2,4096/r.width,4096/r.height,Math.sqrt(8388608/(r.width*r.height)));
            if(this.effectivePixelRatio!==dpr){this.effectivePixelRatio=dpr;this.material.uniforms.effectivePixelRatio.value=dpr;this.uniformUpdates++;}
            this.renderer.setPixelRatio(dpr);this.renderer.setSize(r.width,r.height,false);this.camera.aspect=r.width/r.height;this.nav.aspect=this.camera.aspect;this.camera.updateProjectionMatrix();this.dirty();}
        update(snapshot,range){
            if(this.disposed)throw new Error('3D renderer disposed');
            const records=snapshot.records,count=snapshot.recordCount;
            if(!(records instanceof Int16Array)||!Number.isSafeInteger(count)||count<0||count>C.CLOUD_MAX||records.length!==count*4)throw new Error('Invalid signed cloud snapshot');
            const bounds=C.cloudBounds(records),context=C.cloudClipContext(snapshot);
            if(this.context!==context){this.context=context;this.fitted=false;}
            this.count=count;this.records.set(records);
            this.clipStats=C.summarizeCloudClipping(records,this.clipSettings);this.clipStatsFrameId=snapshot.frameId;this.clipStatsContext=context;
            this.attributeBuffer.updateRange.offset=0;this.attributeBuffer.updateRange.count=count*4;
            // Counts are application-requested populated-prefix uploads, not observed driver allocations.
            if(count){this.attributeBuffer.needsUpdate=true;this.uploads++;this.uploadedAttributeBytes+=count*8;}
            // Invisible empty Points also suppress a pending upload when updates coalesce before paint.
            this.points.visible=count>0;this.geometry.setDrawRange(0,count);this.setColorRange(range);
            this.nav.bounds=bounds;this.axes.position.fromArray(bounds.center);this.axes.scale.setScalar(Math.max(1,bounds.radius*.5));
            if(count&&!this.fitted){this.nav.home();this.framing={mode:'all',included:count,total:count};this.fitted=true;this.captureReferenceDistance();}this.dirty();
        }
        setClipSettings(settings){
            if(this.disposed)throw new Error('3D renderer disposed');
            const next=C.validateClipSettings(settings);
            if(JSON.stringify(next)===JSON.stringify(this.clipSettings))return;
            this.clipSettings=next;const u=this.material.uniforms,d=next.depth,b=next.box;
            u.clipDepthEnabled.value=d.enabled;u.clipDepthRange.value.set(d.min,d.max);
            u.clipBoxEnabled.value=b.enabled;u.clipBoxMin.value.fromArray(b.min);u.clipBoxMax.value.fromArray(b.max);
            this.clipStats=C.summarizeCloudClipping(this.records.subarray(0,this.count*4),next);
            // clipUpdates counts changed complete applied sets (including disabled-limit edits), not frames/paints.
            this.clipUpdates++;this.uniformUpdates++;this.dirty();
        }
        setPointStyle(style){
            if(this.disposed)throw new Error('3D renderer disposed');
            const next=C.validatePointStyle(style),old=this.pointStyle;
            if(next.shape===old.shape&&next.diameterCssPx===old.diameterCssPx&&next.sizing===old.sizing)return;
            this.pointStyle=next;const u=this.material.uniforms;
            u.circleShape.value=next.shape==='circle';u.diameterCssPx.value=next.diameterCssPx;u.perspectiveSizing.value=next.sizing==='perspective';
            this.uniformUpdates++;this.dirty();
        }
        setColorRange(range){
            if(this.disposed)throw new Error('3D renderer disposed');
            const next=C.pointColorRange(range);
            if(next?.min===this.range?.min&&next?.max===this.range?.max)return;
            this.range=next;const u=this.material.uniforms;u.rangeValid.value=!!next;u.depthRange.value.set(next?.min??0,next?.max??1);
            this.uniformUpdates++;this.dirty();
        }
        recolor(snapshot,range){this.setColorRange(range);}
        captureReferenceDistance(){
            const distance=this.nav.radius;
            if(Number.isFinite(distance)&&distance>0&&distance!==this.referenceDistance){
                this.referenceDistance=distance;this.material.uniforms.referenceDistance.value=distance;this.uniformUpdates++;
            }
        }
        fit(mode='all'){
            if(!['all','main'].includes(mode))throw new Error('Invalid framing mode');
            const bounds=mode==='main'?C.cloudMainBounds(this.records.subarray(0,this.count*4)):this.nav.bounds;
            this.nav.fit(this.nav.aspect,bounds);this.framing={mode,included:bounds.included??this.count,total:this.count};
            this.captureReferenceDistance();this.dirty();
        }
        reset(){this.nav.home();this.framing={mode:'all',included:this.count,total:this.count};this.captureReferenceDistance();this.dirty();}
        setOrientation(view){this.nav.orient(view);this.dirty();}
        setProjection(projection){
            if(!['perspective','orthographic'].includes(projection))throw new Error('Invalid projection');
            this.projection=projection;this.camera=projection==='orthographic'?this.orthographicCamera:this.perspectiveCamera;
            this.camera.aspect=this.nav.aspect||1;this.dirty();
        }
        setEyeDomeLighting(enabled){
            if(typeof enabled!=='boolean')throw new Error('Invalid eye-dome lighting setting');
            this.eyeDomeLighting=enabled&&this.edlSupported&&!this.edlError;this.dirty();
            return this.eyeDomeLighting;
        }
        disposeEDL(){
            if(!this.edl)return;this.edl.target.dispose();this.edl.geometry.dispose();this.edl.material.dispose();this.edl=null;
        }
        renderEDL(output){
            const T=root.THREE,r=this.renderer,w=output?output.width:this.canvas.width,h=output?output.height:this.canvas.height;
            if(!this.edl){
                const target=new T.WebGLRenderTarget(w,h,{minFilter:T.NearestFilter,magFilter:T.NearestFilter,depthBuffer:true,stencilBuffer:false});
                target.depthTexture=new T.DepthTexture(w,h,T.UnsignedIntType);
                const material=C.createCloudEDLMaterial(T),geometry=new T.PlaneBufferGeometry(2,2),scene=new T.Scene();
                scene.add(new T.Mesh(geometry,material));this.edl={target,material,geometry,scene,camera:new T.Camera()};
            }
            const e=this.edl;e.target.setSize(w,h);
            const u=e.material.uniforms;u.colorMap.value=e.target.texture;u.depthMap.value=e.target.depthTexture;
            u.sampleStep.value.set(1.5*this.effectivePixelRatio/w,1.5*this.effectivePixelRatio/h);
            u.clipPlanes.value.set(this.camera.near,this.camera.far);u.orthographic.value=this.projection==='orthographic';
            r.setRenderTarget(e.target);
            const gl=r.getContext();if(gl.checkFramebufferStatus(gl.FRAMEBUFFER)!==gl.FRAMEBUFFER_COMPLETE)throw new Error('Depth target unsupported');
            r.render(this.scene,this.camera);r.setRenderTarget(output);r.render(e.scene,e.camera);this.checkPrograms();
        }
        setOverlay(visible){this.axes.visible=visible;this.dirty();}
        setActive(active){this.active=active&&!this.failed&&!this.disposed;if(!this.active){if(this.raf!=null)cancelAnimationFrame(this.raf);this.raf=null;
                for(const id of this.pointers.keys())if(this.canvas.hasPointerCapture(id))this.canvas.releasePointerCapture(id);this.pointers.clear();
            }else{this.resize();this.dirty();}}
        dirty(){
            if(!this.active||this.disposed||this.raf!=null)return;
            this.raf=requestAnimationFrame(now=>{
                this.raf=null;const interval=1000/30,due=this.nextPaint??this.lastPaint+interval;
                if(now+.001<due){this.dirty();return;}
                this.render(now);
                // Retain fractional phase across RAF quantization/jitter, but never catch up
                // after a stall. One pending dirty frame, no idle animation loop.
                this.nextPaint=Number.isFinite(due)&&now-due<interval?due+interval:now+interval;
            });
        }
        render(now=performance.now()){
            if(!this.active||this.disposed)return;this.camera.position.fromArray(this.nav.position());this.camera.lookAt(...this.nav.target);
            this.camera.near=Math.max(.01,this.nav.radius/10000);this.camera.far=Math.max(this.camera.near*10,this.nav.radius+this.nav.base*10+this.nav.bounds.radius*2);this.camera.updateProjectionMatrix();
            if(this.projection==='orthographic'){
                const half=this.nav.radius*Math.tan(Math.PI/8);this.camera.left=-half*this.nav.aspect;this.camera.right=half*this.nav.aspect;
                this.camera.top=half;this.camera.bottom=-half;
            }
            if(this.eyeDomeLighting){
                // Tighten depth precision from ALL authoritative bounds, including off-framing outliers.
                const direction=this.nav.target.map((v,i)=>(v-this.camera.position.getComponent(i))/this.nav.radius),b=this.nav.bounds;
                let near=Infinity,far=0;
                for(let corner=0;corner<8;corner++){
                    let z=0;for(let i=0;i<3;i++)z+=((corner&(1<<i)?b.max[i]:b.min[i])-this.camera.position.getComponent(i))*direction[i];
                    near=Math.min(near,z);far=Math.max(far,z);
                }
                if(this.axes.visible){
                    const centerDepth=b.center.reduce((z,v,i)=>z+(v-this.camera.position.getComponent(i))*direction[i],0),length=Math.max(1,b.radius*.5);
                    near=Math.min(near,centerDepth-length);far=Math.max(far,centerDepth+length);
                }
                this.camera.near=Math.max(.01,near*.9);this.camera.far=Math.max(this.camera.near+1,far*1.1);
            }
            this.camera.updateProjectionMatrix();
            try{
                const output=this.renderer.getRenderTarget();
                if(this.eyeDomeLighting){
                    try{this.renderEDL(output);}catch(error){
                        this.renderer.setRenderTarget(output);this.edlError=error.message;this.eyeDomeLighting=false;this.disposeEDL();
                        this.renderer.render(this.scene,this.camera);
                    }
                }else this.renderer.render(this.scene,this.camera);
                this.checkPrograms();
            }
            catch(error){this.failed=true;this.setActive(false);this.onUnavailable('WebGL rendering failed: '+error.message);return;}
            this.lastPaint=now;this.nextPaint=now+1000/30;this.paints.push(now);this.paints=this.paints.filter(t=>t>=now-2000).slice(-64);
        }
        counters(now=performance.now()){const t=this.paints.filter(v=>v>=now-2000),info=this.renderer.info;return {renderFPS:t.length>1?(t.length-1)*1000/Math.max(1,now-t[0]):0,points:this.count,uploads:this.uploads,
            geometryUploads:this.uploads,uploadedAttributeBytes:this.uploadedAttributeBytes,uniformUpdates:this.uniformUpdates,
            clipSettings:C.validateClipSettings(this.clipSettings),clippingActive:C.clippingActive(this.clipSettings),...this.clipStats,
            clipStatsFrameId:this.clipStatsFrameId,clipStatsContext:this.clipStatsContext,clipUpdates:this.clipUpdates,
            projection:this.projection,framing:{...this.framing},eyeDomeLighting:this.eyeDomeLighting,
            edlSupported:!!this.edlSupported&&!this.edlError,edlUnavailable:this.edlError||(!this.edlSupported?'Depth textures and fragment high precision required':null),
            lightingPasses:this.eyeDomeLighting?2:1,edlTarget:this.edl?[this.edl.target.width,this.edl.target.height]:null,
            pointStyle:{...this.pointStyle},pointSizeRange:[...this.pointSizeRange],effectivePixelRatio:this.effectivePixelRatio,referenceDistance:this.referenceDistance,
            hardwareSizeLimited:this.pointStyle.diameterCssPx*this.effectivePixelRatio*(this.pointStyle.sizing==='perspective'?2:1)>this.pointSizeRange[1]||
                this.pointStyle.diameterCssPx*this.effectivePixelRatio*(this.pointStyle.sizing==='perspective'?.5:1)<this.pointSizeRange[0],
            geometries:info.memory.geometries,textures:info.memory.textures,calls:info.render.calls,bufferBytes:this.records.byteLength,drawingBuffer:[this.canvas.width,this.canvas.height],pose:this.nav.pose(),pointers:this.pointers.size,raf:this.raf!=null};}
        async png(snapshot,{source,stale,overlays=true}={}){
            const range=this.range;
            if(!this.active||this.disposed||!snapshot||!this.count)throw new Error('No available nonempty 3D view to export');
            const canvas=this.canvas,style=this.pointStyle,limits=this.pointSizeRange;
            const visible=this.axes.visible;this.axes.visible=overlays;this.render();this.axes.visible=visible;this.dirty();
            if(this.failed||this.disposed)throw new Error('3D render failed during PNG export');
            const limiting=this.counters().hardwareSizeLimited?(style.sizing==='perspective'?' · may limit':' · limited'):'';
            // Append bounded, wrapped provenance; never overpaint the viewport or truncate active intervals.
            const result=document.createElement('canvas'),out=result.getContext('2d'),font=Math.max(10,Math.min(16,canvas.width/45)),padding=8;
            out.font=font+'px system-ui';
            const provenance=[
                `${source==='demo'?'DEMO · SYNTHETIC DATA':'CHITON · LIVE API'} · cloud frame ${snapshot.frameId} · ${stale?'STALE':'snapshot'}`,
                `${snapshot.units} / ${snapshot.unitsScope} · API coordinates · ${snapshot.recordCount} records · ${this.projection} · ${this.framing.mode} framing`,
                `Depth color: ${range?range.min+'…'+range.max+' '+snapshot.units:'unavailable (neutral)'} · Cloud and numeric measurement may be different frames`,
                `Style: ${style.shape} ${style.diameterCssPx} CSS px · ${style.sizing} · DPR ${Number(this.effectivePixelRatio.toFixed(3))} · HW ${limits[0]}…${limits[1]} px${limiting} · EDL ${this.eyeDomeLighting?'on':'off'}`,
                ...C.clipDescription(this.clipSettings),
                `${this.clipStats.matchingCount} / ${this.count} records match clipping${this.clipStats.matchingCount===0?' · No records match clipping; axes/overlays may remain':''}`
            ];
            const lines=[];
            for(const paragraph of provenance){let current='';for(const word of paragraph.split(' ')){
                const next=current?current+' '+word:word;
                if(current&&out.measureText(next).width>canvas.width-padding*2){lines.push(current);current=word;}else current=next;
            }lines.push(current);}
            result.width=canvas.width;result.height=canvas.height+padding*2+Math.ceil(lines.length*font*1.4);
            out.drawImage(canvas,0,0);out.fillStyle='#0b1118';out.fillRect(0,canvas.height,result.width,result.height-canvas.height);
            out.font=font+'px system-ui';out.fillStyle='#e8eff6';
            lines.forEach((value,i)=>out.fillText(value,padding,canvas.height+padding+font*(1+i*1.4)));
            return new Promise((resolve,reject)=>result.toBlob(blob=>blob?resolve(blob):reject(new Error('Cloud PNG encoding failed')),'image/png'));
        }
        dispose(){if(this.disposed)return;this.setActive(false);this.disposed=true;this.observer?.disconnect();for(const remove of this.listeners)remove();
            this.disposeEDL();this.geometry?.dispose();this.material?.dispose();this.axes?.geometry.dispose();this.axes?.material.dispose();
            this.renderer?.dispose();this.renderer?.forceContextLoss();this.canvas?.remove();}
    }
    C.CloudView=CloudView;
})(window);
