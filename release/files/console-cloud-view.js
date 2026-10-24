/* Three.js r128, local navigation, on-demand <=30 Hz paint. No transport or acquisition timers. */
(function(root){
    'use strict';
    const C=root.ChitonConsole;
    class CloudView {
        constructor(host,{onUnavailable=()=>{}}={}){
            this.host=host;this.onUnavailable=onUnavailable;this.nav=new C.CloudCamera();this.listeners=[];this.pointers=new Map();this.raf=null;this.lastPaint=-Infinity;this.paints=[];this.active=true;this.fitted=false;this.disposed=false;this.uploads=0;this.count=0;
            const T=root.THREE;if(!T||T.REVISION!=='128')throw new Error('Bundled Three.js r128 unavailable');
            this.renderer=new T.WebGLRenderer({antialias:false,alpha:false});
            this.renderer.setClearColor(0x03080d);this.canvas=this.renderer.domElement;this.canvas.id='cloud-canvas';this.canvas.tabIndex=0;
            this.canvas.setAttribute('aria-label','3D API coordinates. Drag to rotate; Shift or secondary drag to pan; wheel to zoom. Arrow keys rotate, Shift arrows pan, plus/minus zoom, F fit, R reset.');
            this.scene=new T.Scene();this.camera=new T.PerspectiveCamera(45,1,.1,1000000);
            this.geometry=new T.BufferGeometry();this.positions=new Float32Array(C.CLOUD_MAX*3);this.colors=new Float32Array(C.CLOUD_MAX*3);
            for(const [name,array] of [['position',this.positions],['color',this.colors]])this.geometry.setAttribute(name,new T.BufferAttribute(array,3).setUsage(T.DynamicDrawUsage));
            this.geometry.setDrawRange(0,0);this.material=new T.PointsMaterial({size:2.5,sizeAttenuation:false,vertexColors:true});
            this.points=new T.Points(this.geometry,this.material);this.points.frustumCulled=false;this.scene.add(this.points);
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
        }
        resize(){if(this.disposed||!this.active)return;const r=this.host.getBoundingClientRect();if(!r.width||!r.height)return;
            const dpr=Math.min(root.devicePixelRatio||1,2,4096/r.width,4096/r.height,Math.sqrt(8388608/(r.width*r.height)));
            this.renderer.setPixelRatio(dpr);this.renderer.setSize(r.width,r.height,false);this.camera.aspect=r.width/r.height;this.nav.aspect=this.camera.aspect;this.camera.updateProjectionMatrix();this.dirty();}
        update(snapshot,range){
            if(this.disposed)throw new Error('3D renderer disposed');const records=snapshot.records;this.count=snapshot.recordCount;
            for(let i=0;i<this.count;i++){const j=i*3,k=i*4;this.positions[j]=records[k];this.positions[j+1]=records[k+1];this.positions[j+2]=records[k+2];C.cloudColor(records[k+3],range,this.colors,j);}
            for(const attribute of Object.values(this.geometry.attributes)){attribute.updateRange.offset=0;attribute.updateRange.count=this.count*3;if(this.count)attribute.needsUpdate=true;}
            this.geometry.setDrawRange(0,this.count);this.uploads++;this.range=range?{min:range.min,max:range.max}:null;
            this.nav.bounds=C.cloudBounds(records);this.axes.position.fromArray(this.nav.bounds.center);this.axes.scale.setScalar(Math.max(1,this.nav.bounds.radius*.5));
            if(this.count&&!this.fitted){this.nav.home();this.fitted=true;}this.dirty();
        }
        recolor(snapshot,range){
            for(let i=0;i<this.count;i++)C.cloudColor(snapshot.records[i*4+3],range,this.colors,i*3);
            this.range=range?{min:range.min,max:range.max}:null;
            const attribute=this.geometry.attributes.color;attribute.updateRange.offset=0;attribute.updateRange.count=this.count*3;
            if(this.count)attribute.needsUpdate=true;this.uploads++;this.dirty();
        }
        fit(){this.nav.fit(this.camera.aspect);this.dirty();}
        reset(){this.nav.home();this.dirty();}
        setOverlay(visible){this.axes.visible=visible;this.dirty();}
        setActive(active){this.active=active;if(!active){if(this.raf!=null)cancelAnimationFrame(this.raf);this.raf=null;
                for(const id of this.pointers.keys())if(this.canvas.hasPointerCapture(id))this.canvas.releasePointerCapture(id);this.pointers.clear();
            }else{this.resize();this.dirty();}}
        dirty(){if(!this.active||this.disposed||this.raf!=null)return;this.raf=requestAnimationFrame(now=>{this.raf=null;if(now-this.lastPaint<1000/30){this.dirty();return;}this.render(now);});}
        render(now=performance.now()){
            if(!this.active||this.disposed)return;this.camera.position.fromArray(this.nav.position());this.camera.lookAt(...this.nav.target);
            this.camera.near=Math.max(.01,this.nav.radius/10000);this.camera.far=Math.max(this.camera.near*10,this.nav.radius+this.nav.base*10+this.nav.bounds.radius*2);this.camera.updateProjectionMatrix();
            this.renderer.render(this.scene,this.camera);this.lastPaint=now;this.paints.push(now);this.paints=this.paints.filter(t=>t>=now-2000).slice(-60);
        }
        counters(now=performance.now()){const t=this.paints.filter(v=>v>=now-2000),info=this.renderer.info;return {renderFPS:t.length>1?(t.length-1)*1000/Math.max(1,now-t[0]):0,points:this.count,uploads:this.uploads,
            geometries:info.memory.geometries,textures:info.memory.textures,calls:info.render.calls,bufferBytes:this.positions.byteLength+this.colors.byteLength,drawingBuffer:[this.canvas.width,this.canvas.height],pose:this.nav.pose(),pointers:this.pointers.size,raf:this.raf!=null};}
        async png(snapshot,{source,stale,overlays=true}={}){
            const range=this.range;
            if(!this.active||this.disposed||!snapshot||!this.count)throw new Error('No available nonempty 3D view to export');
            const canvas=document.createElement('canvas');canvas.width=this.canvas.width;canvas.height=this.canvas.height;const ctx=canvas.getContext('2d');
            const visible=this.axes.visible;this.axes.visible=overlays;this.render();ctx.drawImage(this.canvas,0,0);this.axes.visible=visible;this.dirty();
            const scale=Math.max(.5,Math.min(2,canvas.width/800)),line=22*scale;
            ctx.fillStyle='#0b1118';ctx.fillRect(0,canvas.height-line*3,canvas.width,line*3);ctx.fillStyle='#e8eff6';ctx.font=14*scale+'px system-ui';
            ctx.fillText(`${source==='demo'?'DEMO · SYNTHETIC DATA':'CHITON · LIVE API'} · cloud frame ${snapshot.frameId} · ${stale?'STALE':'snapshot'}`,10*scale,canvas.height-line*2.1);
            ctx.fillText(`${snapshot.units} / ${snapshot.unitsScope} · API coordinates · ${snapshot.recordCount} records`,10*scale,canvas.height-line*1.1);
            ctx.fillText(overlays?`Depth color: ${range?range.min+'…'+range.max+' '+snapshot.units:'unavailable (neutral)'} · X red / Y green / Z blue`:'Cloud and numeric measurement may be different frames',10*scale,canvas.height-line*.2);
            return new Promise((resolve,reject)=>canvas.toBlob(blob=>blob?resolve(blob):reject(new Error('Cloud PNG encoding failed')),'image/png'));
        }
        dispose(){if(this.disposed)return;this.setActive(false);this.disposed=true;this.observer.disconnect();for(const remove of this.listeners)remove();
            this.geometry.dispose();this.material.dispose();this.axes.geometry.dispose();this.axes.material.dispose();this.renderer.dispose();this.renderer.forceContextLoss();this.canvas.remove();}
    }
    C.CloudView=CloudView;
})(window);
