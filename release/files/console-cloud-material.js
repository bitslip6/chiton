/* Three.js r128 point shader. Opaque hard-discard circles; no palette/output conversion. */
(function(root){
    'use strict';
    const C=root.ChitonConsole=root.ChitonConsole||{};
    const DEFAULT_POINT_STYLE=Object.freeze({shape:'circle',diameterCssPx:2.5,sizing:'screen'});
    function validatePointStyle(style){
        if(!style||!['circle','square'].includes(style.shape)||!['screen','perspective'].includes(style.sizing)||
            !Number.isFinite(style.diameterCssPx)||style.diameterCssPx<1||style.diameterCssPx>10)throw new Error('Invalid point style');
        return {shape:style.shape,diameterCssPx:style.diameterCssPx,sizing:style.sizing};
    }
    function pointColorRange(range){
        return range&&Number.isFinite(range.min)&&Number.isFinite(range.max)&&range.max>range.min?{min:range.min,max:range.max}:null;
    }
    // Reference for tests/telemetry, not a production per-record sizing loop.
    function pointSizePixels(style,pixelRatio,limits,referenceDistance,viewDepth){
        const scale=style.sizing==='perspective'?Math.max(.5,Math.min(2,referenceDistance/Math.max(.0001,viewDepth))):1;
        return Math.max(limits[0],Math.min(limits[1],style.diameterCssPx*pixelRatio*scale));
    }
    // r128 may downgrade material precision for fragment-limited WebGL1; signed depth still needs vertex highp.
    const pointVertexShader=`
        precision highp float;
        attribute float depth;
        uniform bool clipDepthEnabled;
        uniform vec2 clipDepthRange;
        uniform bool clipBoxEnabled;
        uniform vec3 clipBoxMin;
        uniform vec3 clipBoxMax;
        uniform bool rangeValid;
        uniform vec2 depthRange;
        uniform float diameterCssPx;
        uniform float effectivePixelRatio;
        uniform vec2 pointSizeRange;
        uniform bool perspectiveSizing;
        uniform float referenceDistance;
        varying mediump vec3 pointColor;
        void main() {
            pointColor = vec3(0.6, 0.67, 0.73);
            bool selected = (!clipDepthEnabled || (depth >= clipDepthRange.x && depth <= clipDepthRange.y))
                && (!clipBoxEnabled || (all(greaterThanEqual(position, clipBoxMin)) && all(lessThanEqual(position, clipBoxMax))));
            if (!selected) {
                // Finite center beyond the far clip plane: no fragments/depth, independent of sprite diameter.
                gl_PointSize = 1.0;
                gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
                return;
            }
            if (rangeValid) {
                float t = clamp((depth - depthRange.x) / (depthRange.y - depthRange.x), 0.0, 1.0);
                pointColor = max(vec3(0.0), vec3(1.0) - abs(vec3(t * 4.0) - vec3(3.0, 2.0, 1.0)));
            }
            vec4 viewPosition = modelViewMatrix * vec4(position, 1.0);
            float scale = perspectiveSizing ? clamp(referenceDistance / max(0.0001, -viewPosition.z), 0.5, 2.0) : 1.0;
            gl_PointSize = clamp(diameterCssPx * effectivePixelRatio * scale, pointSizeRange.x, pointSizeRange.y);
            gl_Position = projectionMatrix * viewPosition;
        }`;
    const pointFragmentShader=`
        uniform bool circleShape;
        varying mediump vec3 pointColor;
        void main() {
            vec2 p = gl_PointCoord - vec2(0.5);
            if (circleShape && dot(p, p) > 0.25) discard;
            gl_FragColor = vec4(pointColor, 1.0);
        }`;
    function createCloudMaterial(T,limits){
        return new T.ShaderMaterial({precision:'highp',uniforms:{
            clipDepthEnabled:{value:false},clipDepthRange:{value:new T.Vector2(-32768,32767)},
            clipBoxEnabled:{value:false},clipBoxMin:{value:new T.Vector3(-32768,-32768,-32768)},clipBoxMax:{value:new T.Vector3(32767,32767,32767)},
            rangeValid:{value:false},depthRange:{value:new T.Vector2(0,1)},
            diameterCssPx:{value:DEFAULT_POINT_STYLE.diameterCssPx},effectivePixelRatio:{value:1},
            pointSizeRange:{value:new T.Vector2(...limits)},perspectiveSizing:{value:false},
            referenceDistance:{value:1},circleShape:{value:true}
        },vertexShader:pointVertexShader,fragmentShader:pointFragmentShader,
        transparent:false,depthTest:true,depthWrite:true,blending:T.NoBlending,toneMapped:false});
    }
    // Optional screen-space EDL; requires a depth texture and fragment highp, not a new palette.
    function createCloudEDLMaterial(T){
        return new T.ShaderMaterial({precision:'highp',uniforms:{
            colorMap:{value:null},depthMap:{value:null},sampleStep:{value:new T.Vector2(1,1)},
            clipPlanes:{value:new T.Vector2(.1,100000)},orthographic:{value:false}
        },vertexShader:`varying vec2 uvEDL;
            void main(){uvEDL=uv;gl_Position=vec4(position.xy,0.0,1.0);}`,
        fragmentShader:`precision highp float;
            uniform sampler2D colorMap;
            uniform sampler2D depthMap;
            uniform vec2 sampleStep;
            uniform vec2 clipPlanes;
            uniform bool orthographic;
            varying vec2 uvEDL;
            float eyeDepth(float d){
                float n=clipPlanes.x,f=clipPlanes.y;
                return orthographic ? n+d*(f-n) : n*f/(f-d*(f-n));
            }
            float response(vec2 offset,float center){
                vec2 p=uvEDL+offset*sampleStep;
                if(any(lessThan(p,vec2(0.0)))||any(greaterThan(p,vec2(1.0))))return 0.0;
                float d=texture2D(depthMap,p).r;
                // A small fixed silhouette response for empty neighbors; never shade clear pixels.
                return d>=1.0 ? 0.12 : max(0.0,center-log2(max(0.0001,eyeDepth(d))));
            }
            void main(){
                vec4 color=texture2D(colorMap,uvEDL);
                float d=texture2D(depthMap,uvEDL).r;
                if(d>=1.0){gl_FragColor=color;return;}
                float center=log2(max(0.0001,eyeDepth(d)));
                float r=response(vec2(1.,0.),center)+response(vec2(-1.,0.),center)
                    +response(vec2(0.,1.),center)+response(vec2(0.,-1.),center)
                    +response(vec2(.707,.707),center)+response(vec2(-.707,.707),center)
                    +response(vec2(.707,-.707),center)+response(vec2(-.707,-.707),center);
                gl_FragColor=vec4(color.rgb*exp(-2.0*r/8.0),color.a);
            }`,depthTest:false,depthWrite:false,blending:T.NoBlending,toneMapped:false});
    }
    Object.assign(C,{createCloudEDLMaterial,DEFAULT_POINT_STYLE,validatePointStyle,pointColorRange,pointSizePixels,pointVertexShader,pointFragmentShader,createCloudMaterial});
    if(typeof module==='object')module.exports=C;
})(typeof window==='object'?window:globalThis);
