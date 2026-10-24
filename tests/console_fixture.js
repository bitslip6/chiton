/* Local-only allowlisted API/JPEG fixture backed by the exact seeded numeric scene. */
'use strict';
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),crypto=require('node:crypto');
const {spawnSync}=require('node:child_process');
const {DemoSource}=require('../release/files/console-demo');
function assertCloudCount(count){if(!Number.isSafeInteger(count)||count<0||count>640*480)throw new Error('Fixture cloud exceeds bound');}
async function fixture(){
    const source=new DemoSource();const control={measurement:'ok',image:'ok',capture:true,mutationAck:'ok',ws:true,cloud:'ok',cloudOrder:'little',obj:'ok'};
    const counts={},active={},maximum={},sockets=new Set(),clients=new Set(),assets=path.resolve(__dirname,'../release/files');
    const jpegCache=new Map(),requests=[];
    function jpeg(m){
        const key=[m.frame_id,m.width,m.height,m.color_mode,m.frame_type,m.units].join(':');if(jpegCache.has(key))return jpegCache.get(key);
        const pixels=Buffer.alloc(m.width*m.height);for(let y=0;y<m.height;y++)for(let x=0;x<m.width;x++){let z=0;try{z=source.sample(x,y,m);}catch(_){}pixels[y*m.width+x]=z?Math.round(35+Math.max(0,Math.min(1,z/3000))*210):3;}
        const result=spawnSync(process.env.CHITON_CJPEG||'cjpeg',['-quality','75'],{input:Buffer.concat([Buffer.from(`P5\n${m.width} ${m.height}\n255\n`),pixels]),maxBuffer:3*1024*1024});
        if(result.status!==0)throw new Error('Install cjpeg (libjpeg tools) or set CHITON_CJPEG: '+result.error+' '+result.stderr);
        jpegCache.set(key,result.stdout);while(jpegCache.size>2)jpegCache.delete(jpegCache.keys().next().value);return result.stdout;
    }
    const server=http.createServer(async(req,res)=>{
        const url=new URL(req.url,'http://127.0.0.1');if(!req.url.startsWith('/')||url.host!=='127.0.0.1'){res.writeHead(403);res.end();return;}
        const p=url.pathname;requests.push(p);if(requests.length>10000)requests.shift();counts[p]=(counts[p]||0)+1;
        const measurement=p.startsWith('/get/value/client/'),kind=measurement?'measurement':p==='/image/camera'?'image':p==='/pointcloud/camera'?'cloud':'other';
        active[kind]=(active[kind]||0)+1;maximum[kind]=Math.max(maximum[kind]||0,active[kind]);res.on('close',()=>active[kind]--);
        const send=(type,body,status=200,headers={})=>{if(res.destroyed)return;const bytes=Buffer.isBuffer(body)?body:Buffer.from(body);res.writeHead(status,{'Content-Type':type,'Content-Length':bytes.length,'Cache-Control':'no-store',...headers});res.end(bytes);};
        const json=(data,status=200)=>send('application/json',JSON.stringify(data),status);
        const frameHeaders=(m,layout)=>({'X-Chiton-Frame-Id':String(m.frame_id),'X-Chiton-Frame-Age-Ms':String(m.frame_age_ms),'X-Chiton-Color-Mode':String(m.color_mode),
            'X-Chiton-Captured-At-Ms':String(m.captured_at_ms),'X-Chiton-Frame-Type':m.frame_type,'X-Chiton-Width':String(m.width),'X-Chiton-Height':String(m.height),
            'X-Chiton-Layout':layout,'X-Chiton-Units':layout==='jpeg'?'image_intensity':m.units,'X-Chiton-Source-Units':m.units,'X-Chiton-Units-Scope':m.units_scope,
            'X-Chiton-Configuration-Revision':String(m.configuration_revision),'X-Chiton-Invalid-Sample-Policy':'all_uint16_included','X-Chiton-Byte-Order':layout==='jpeg'?'not_applicable':'little'});
        try{
            if(measurement){
                if(control.measurement==='hang')return;
                if(control.measurement==='hanging-body'){res.writeHead(200,{'Content-Type':'application/json','Content-Length':10000});res.write('{');return;}
                if(control.measurement==='delay'){await new Promise(resolve=>setTimeout(resolve,250));if(res.destroyed)return;}
                if(!control.capture||control.measurement==='503')return json({error:'capture unavailable'},503);
                const parts=p.split('/'),clientId=parts[4],coords=parts.slice(5).map(Number);
                if(![2,4].includes(coords.length))return json({error:'selection'},400);
                const selection={kind:coords.length===2?'point':'roi',x:coords[0],y:coords[1],...coords.length===4?{x2:coords[2],y2:coords[3]}:{}};
                const payload=await source.readPayload({clientId,selection});
                if(control.measurement==='malformed')payload.stddev='NaN';
                if(control.measurement==='unsafe-id')payload.frame_id=9007199254740992;
                if(control.measurement==='missing-optionals')for(const k of ['captured_at_ms','frame_age_ms','min','max','first_captured_at_ms','last_captured_at_ms','smoothed','plugin_applied'])delete payload[k];
                if(control.measurement==='out-of-order')payload.frame_id=1;
                return json(payload);
            }
            if(p==='/colormode/setting')return json({value:source.mode,capture_enabled:control.capture,frame_id:source.metadata().frame_id});
            if(p==='/depth/setting')return json({value:source.frameType==='depth',frame_type:source.frameType});
            if(p==='/range/setting')return control.range==='unavailable'?json({error:'range unavailable'},503):json({...source.range});
            if(p==='/frame/camera')return control.capture?json(source.metadata()):json({error:'disabled'},503);
            if(p==='/dimension/camera')return json({inner_dist:85,outer_dist:232,center_x:320,center_y:240});
            if(p==='/temp/setting')return json({cputemp:42,vixeltemp:45});
            if(p==='/stream/metrics')return json({clients:clients.size,pipeline:{}});
            const mutation=/^\/(colormode|depth|smoothing|chitoncal|quality|range)\/setting\/(.+)$/.exec(p);
            if(mutation){
                const name={colormode:'mode',depth:'frameType',smoothing:'smoothing',chitoncal:'calibration',quality:'quality',range:'range'}[mutation[1]],args=mutation[2].split('/');
                const value=name==='mode'||name==='quality'?Number(args[0]):name==='smoothing'||name==='calibration'?args[0]==='on':name==='range'?{min:Number(args[0]),max:Number(args[1])}:args[0];
                const result=await source.applySetting({name,value});if(control.mutationAck==='fail-after-apply')return json({error:'ack lost'},503);
                return json(name==='range'?value:name==='frameType'?{value:value==='depth',frame_type:value}:{value:result.value});
            }
            if(p==='/image/camera'){
                if(!control.capture||control.image==='503')return json({error:'no JPEG'},503);
                if(source.mode===4)return json({error:'cloud mode',current_mode:4},409);
                const m=source.metadata();return send('image/jpeg',control.image==='invalid'?Buffer.from('bad image'):jpeg(m),200,frameHeaders(m,'jpeg'));
            }
            if(p==='/raw/camera'){
                if(!control.capture)return json({error:'disabled'},503);
                const m={...source.metadata(),width:640,height:480},raw=Buffer.alloc(614400);
                for(let y=0;y<480;y++)for(let x=0;x<640;x++)raw.writeUInt16LE(source.sample(x,y,m),(y*640+x)*2);
                return send('application/octet-stream',raw,200,{...frameHeaders(m,'processed_uint16_row_major'),'Content-Disposition':'attachment; filename="chiton-raw.bin"'});
            }
            if(p==='/pointcloud/camera'||p==='/pointcloud_obj/camera'){
                if(!control.capture)return json({error:'disabled'},503);
                if(source.mode!==4)return json({error:'select mode 4'},409);
                if(p==='/pointcloud_obj/camera'&&['409','503'].includes(control.obj))return json({error:'OBJ export unavailable'},Number(control.obj));
                if(p==='/pointcloud_obj/camera'&&control.obj==='json')return json({error:'Not point-cloud data'});
                if(control.cloud==='hang')return;
                if(['409','503','504'].includes(control.cloud))return json({error:'injected cloud error'},Number(control.cloud));
                if(control.cloud==='delay-header')await new Promise(resolve=>setTimeout(resolve,250));
                if(res.destroyed)return;
                const m=source.metadata(),snapshot=m.frame_type==='depth'?source.cloudSnapshot():{records:new Int16Array(),recordCount:0};
                const records=control.cloud==='empty'?new Int16Array():control.cloud==='single'?new Int16Array([0,0,0,0]):snapshot.records;
                const count=records.length/4;assertCloudCount(count);
                const headers={...frameHeaders({...m,width:count,height:1},'int16_xyzd'),'X-Chiton-Byte-Order':control.cloudOrder};
                if(control.cloud==='ir-header')headers['X-Chiton-Frame-Type']='ir';
                if(control.cloud==='missing-id')delete headers['X-Chiton-Frame-Id'];
                if(control.cloud==='unsafe-id')headers['X-Chiton-Frame-Id']='9007199254740992';
                if(control.cloud==='bad-count')headers['X-Chiton-Width']=String(count+1);
                if(control.cloud==='oversized'){headers['X-Chiton-Width']='307201';res.writeHead(200,{'Content-Type':'application/octet-stream','Content-Length':2457608,...headers});res.end();return;}
                if(p==='/pointcloud_obj/camera'){
                    const lines=[];for(let i=0;i<records.length;i+=4)lines.push('v '+[0,1,2].map(k=>(records[i+k]/6000).toFixed(4)).join(' '));
                    return send('text/plain; charset=utf-8',lines.join('\n')+'\n',200,{...headers,'X-Chiton-Layout':'obj_xyz_divided_by_6000','X-Chiton-Units':'normalized_6000_'+m.units,'X-Chiton-Byte-Order':'not_applicable'});
                }
                const body=Buffer.alloc(count*8);for(let i=0;i<records.length;i++)body[control.cloudOrder==='big'?'writeInt16BE':'writeInt16LE'](records[i],i*2);
                if(control.cloud==='hanging-body'){res.writeHead(200,{'Content-Type':'application/octet-stream','Content-Length':body.length,...headers});res.write(body.subarray(0,8));return;}
                if(control.cloud==='delay-body'){res.writeHead(200,{'Content-Type':'application/octet-stream','Content-Length':body.length,...headers});res.write(body.subarray(0,8));await new Promise(resolve=>setTimeout(resolve,250));if(!res.destroyed)res.end(body.subarray(8));return;}
                if(control.cloud==='truncated')return send('application/octet-stream',body.subarray(0,Math.max(0,body.length-1)),200,headers);
                return send('application/octet-stream',body,200,headers);
            }
            if(/^\/[A-Za-z0-9_.-]+\.(html|css|js|png|svg|jpg)$/.test(p)){
                const file=path.join(assets,p.slice(1));if(!fs.existsSync(file))return json({error:'asset missing'},404);
                return send(({'.html':'text/html','.css':'text/css','.js':'application/javascript','.png':'image/png','.jpg':'image/jpeg','.svg':'image/svg+xml'})[path.extname(file)],fs.readFileSync(file));
            }
            return json({error:'Route not allowlisted'},404);
        }catch(error){json({error:error.message},400);}
    });
    server.on('connection',socket=>{sockets.add(socket);socket.on('close',()=>sockets.delete(socket));});
    server.on('upgrade',(req,socket)=>{
        if(!control.ws||req.url!=='/stream/camera'){socket.end('HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n');return;}
        const accept=crypto.createHash('sha1').update(req.headers['sec-websocket-key']+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
        socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: '+accept+'\r\nSec-WebSocket-Protocol: chiton.jpeg.v1\r\n\r\n');clients.add(socket);
        let credit=true,rx=Buffer.alloc(0),last=0;
        socket.on('error',()=>{});socket.on('data',data=>{
            rx=Buffer.concat([rx,data]);while(rx.length>=6){const n=rx[1]&127;if(n>125||!(rx[1]&128)){socket.destroy();return;}if(rx.length<n+6)return;
                const opcode=rx[0]&15,payload=Buffer.from(rx.subarray(6,6+n));for(let i=0;i<n;i++)payload[i]^=rx[2+i%4];rx=rx.subarray(6+n);
                if(opcode===8){socket.end(Buffer.from([0x88,0]));return;}if(opcode===1 && payload.toString()===String(last))credit=true;
            }
        });
        const timer=setInterval(()=>{
            if(!credit||socket.destroyed)return;
            if(control.image!=='ok'){socket.destroy();return;}
            if(source.mode===4||!control.capture){const data=Buffer.from(JSON.stringify({mode:source.mode,capture_enabled:control.capture}));socket.write(Buffer.concat([Buffer.from([0x81,data.length]),data]));return;}
            const m=source.metadata();if(m.frame_id===last)return;last=m.frame_id;
            const meta=Buffer.from(JSON.stringify({version:1,frame_id:m.frame_id,mode:m.color_mode,width:m.width,height:m.height,age_ms:m.frame_age_ms,captured_at_ms:m.captured_at_ms,configuration_revision:m.configuration_revision,source_units:m.units,timestamp_kind:'host_receipt'}));
            const body=Buffer.alloc(4+meta.length);body.writeUInt32BE(meta.length);meta.copy(body,4);const payload=Buffer.concat([body,jpeg(m)]);
            const header=Buffer.alloc(payload.length<65536?4:10);header[0]=0x82;header[1]=payload.length<65536?126:127;if(payload.length<65536)header.writeUInt16BE(payload.length,2);else header.writeBigUInt64BE(BigInt(payload.length),2);
            credit=false;socket.write(Buffer.concat([header,payload]));
        },33);
        socket.once('close',()=>{clearInterval(timer);clients.delete(socket);});
    });
    await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
    return {source,control,counts,active,maximum,requests,clients,origin:'http://127.0.0.1:'+server.address().port,jpeg,
        close:async()=>{for(const s of sockets)s.destroy();await new Promise(resolve=>server.close(resolve));}};
}
module.exports={fixture};
