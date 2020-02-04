// TODO come up with a cool name
// TODO clear handling of disambiguation
// TODO plot IDEs
// TODO plot photons
// TODO pre-upload textures
// TODO Use textures for hits at long distances so they're visible
// TODO make SaveAs and Print work
// TODO figure out z-order for objects in the plane

document.getElementById('runbox').value = run;
document.getElementById('subrunbox').value = subrun;
document.getElementById('evtbox').value = evt;

document.OnKeyDown = function(evt)
{
    if(evt.keyCode == 13) { // enter
        window.location.href = "seek/" + document.getElementById('runbox').value + '/' + document.getElementById('subrunbox').value + '/' + document.getElementById('evtbox').value;
    }
}

// TODO - probably we should ship our own (minified) copies of these
import * as THREE from "https://cdn.jsdelivr.net/npm/three@v0.110.0/build/three.module.js";
import {OrbitControls} from "https://cdn.jsdelivr.net/npm/three@v0.110.0/examples/jsm/controls/OrbitControls.js";

/*
// This is all confusing, but should be able to fetch a JSON and read with .json()
(async () => {
    let response = await fetch('coords.js');
    eval(response.text());
    console.log('done');
})();
*/

//fetch("coords.json")
//    .then(response => response.json())
//    .then(json => console.log(json));

let gAnimReentrant = false;

let scene = new THREE.Scene();

let camera = new THREE.PerspectiveCamera( 50, window.innerWidth / window.innerHeight, 0.1, 1e6 );

let renderer = new THREE.WebGLRenderer();
// I have no idea why these 4 pixels are necessary
renderer.setSize( window.innerWidth, window.innerHeight-4);

renderer.setClearColor('black');

renderer.alpha = true;
renderer.antialias = false;

// Disable depth buffer to get better handling for the transparent digits/wires
renderer.getContext().disable(renderer.getContext().DEPTH_TEST);

document.body.appendChild( renderer.domElement );

function AddLabel(pos, txt)
{
    var d = document.createElement('div');
    d.className = 'label';
    d.pos = pos; // stash position on the element
    d.appendChild(document.createTextNode(txt));
    document.body.appendChild(d);
    return d;
}

for(var delta of [100, 50, 10, 5, 1]){
    for(var z = 0; z <= cryos[0].max[2]; z += delta){
        AddLabel(new THREE.Vector3(0, 0, z), z.toString());
    }

    for(var x = 0; x <= cryos[0].max[0]; x += delta){
        AddLabel(new THREE.Vector3(+x, 0, 0), '+'+x.toString());
    }

    for(var x = 0; x >= cryos[0].min[0]; x -= delta){
        AddLabel(new THREE.Vector3(x, 0, 0), x.toString());
    }
}

function ArrToVec(arr)
{
    return new THREE.Vector3(arr[0], arr[1], arr[2]);
}

let mat_lin = new THREE.LineBasicMaterial({color: 'gray'});

let mat_geo = new THREE.LineBasicMaterial({color: 'darkred'});

let mat_hit = new THREE.MeshBasicMaterial({color: 'gray', side: THREE.DoubleSide});

let mat_sps = new THREE.MeshBasicMaterial({color: 'blue'});

let mat_vtxs = new THREE.MeshBasicMaterial({color: 'red'});

function TextureLoadCallback(tex, mat, mipdim, texdim){
    console.log('Loaded callback', mipdim, texdim);

    // This is how you would create a data texture if necessary
    // let canvas = document.createElement('canvas');
    // canvas.width = tex.image.width;
    // canvas.height = tex.image.height;
    // let context = canvas.getContext('2d');
    // context.drawImage(tex.image, 0, 0);
    // let data = context.getImageData(0, 0, tex.image.width, tex.image.height).data;
    // let newtex = new THREE.DataTexture(data, tex.image.width, tex.image.height, THREE.RGBAFormat);

    // Until all the mipmaps are ready we stash them in the material
    if(mat.tmpmipmaps == undefined) mat.tmpmipmaps = {};
    mat.tmpmipmaps[mipdim] = tex.image;

    // Did we just succeed in loading the main texture?
    if(mipdim == texdim){
        tex.generateMipMaps = false;
        // Go pixely rather than blurry when zoomed right in
        tex.magFilter = THREE.NearestFilter;

        // Assign to the material and undo the temporary loading style
        mat.map = tex;
        mat.opacity = 1;
        mat.needsUpdate = true;
        requestAnimationFrame(animate);
    }

    // If the main texture is loaded
    if(mat.map != null){
        // And we have now loaded all of the mipmaps
        let ok = true;
        for(let i = 1; i <= texdim; i *= 2){
            if(!(i in mat.tmpmipmaps)) ok = false;
        }

        if(ok){
            // TODO - is it bad that the largest resolution image is in the
            // main map and also the first element in the mipmap list?
            for(let i = texdim; i >= 1; i /= 2){
                mat.map.mipmaps.push(mat.tmpmipmaps[i]);
            }
            delete mat.tmpmipmaps;

            mat.map.minFilter = THREE.LinearMipmapLinearFilter;
            // Necessary to not see block edges, but is super ugly...
            // Mostly because the wires are much wider than the ticks
            // mat.map.minFilter = THREE.NearestMipmapLinearFilter;

            mat.map.needsUpdate = true;
            mat.needsUpdate = true;
            requestAnimationFrame(animate);
        }
    }
}

// Cache so we can request the same texture material multiple times without
// duplication
let gtexmats = {};

function TextureMaterial(fname, texdim){
    if(fname in gtexmats) return gtexmats[fname];

    // Make the material a transparent solid colour until the texture loads
    let mat = new THREE.MeshBasicMaterial( { color: 'white', opacity: .1, side: THREE.DoubleSide, transparent: true, alphaTest: 1/512.} );
    gtexmats[fname] = mat;

    // Load all the mipmaps
    for(let d = texdim; d >= 1; d /= 2){
        new THREE.TextureLoader().load(fname+'_mip'+d+'.png',
                                       function(t){
                                           TextureLoadCallback(t, mat, d, texdim);
                                       },
                                       undefined,
                                       function(err){
                                           mat.opacity = 0;
                                           mat.needsUpdate;
                                           requestAnimationFrame(animate);
                                           console.log('error loading', fname, d, err);
                                       });
    }

    return mat;
}


let outlines = new THREE.Group();
let digs = {}; // Groups indexed by label
let wires = {};
let truth = new THREE.Group();

scene.add(outlines);
scene.add(truth);

let com = new THREE.Vector3();
let nplanes = 0;

let uperp = null;
let vperp = null;

function push_square_vtxs(c, du, dv, vtxs){
    let p1 = c.clone();
    let p2 = c.clone();
    let p3 = c.clone();
    let p4 = c.clone();

    // p1 = du-dv, p2 = du+dv, p3 = -du+dv, p4=-du-dv
    p1.add(du); p1.addScaledVector(dv, -1);
    p2.add(du); p2.add(dv);
    p3.addScaledVector(du, -1); p3.add(dv);
    p4.addScaledVector(du, -1); p4.addScaledVector(dv, -1);

    vtxs.push(p1.x, p1.y, p1.z,
              p2.x, p2.y, p2.z,
              p3.x, p3.y, p3.z,

              p1.x, p1.y, p1.z,
              p3.x, p3.y, p3.z,
              p4.x, p4.y, p4.z);
}

function push_icosahedron_vtxs(c, radius, vtxs, indices){
    const t = ( 1 + Math.sqrt( 5 ) ) / 2;

    const r0 = Math.sqrt(1+t*t);

    const vs = [
              -1, t, 0,
              1, t, 0,
              -1, -t, 0,
              1, -t, 0,
              0, -1, t,
              0, 1, t,
              0, -1, -t,
              0, 1, -t,
              t, 0, -1,
              t, 0, 1,
              -t, 0, -1,
              -t, 0, 1
              ];

    const is = [
              0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11,
              1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
              3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
              4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1
              ];

    for(let i = 0; i < is.length; ++i){
        indices.push(is[i] + vtxs.length/3);
    }

    for(let i = 0; i < vs.length; i += 3){
        vtxs.push(c.x + radius/r0*vs[i  ]);
        vtxs.push(c.y + radius/r0*vs[i+1]);
        vtxs.push(c.z + radius/r0*vs[i+2]);
    }
}

let hitvtxs = {}; // map indexed by reco algo

for(let key in planes){
    let plane = planes[key];
    let c = ArrToVec(plane.center);
    let a = ArrToVec(plane.across).multiplyScalar(plane.nwires*plane.pitch/2.);
    let d = ArrToVec(plane.normal).multiplyScalar(plane.nticks*Math.abs(plane.tick_pitch)/2.); // drift direction

    c.set(plane.tick_origin, c.y, c.z);

    c.add(d); // center in the drift direction too

    com.add(c);
    nplanes += 1; // javascript is silly and doesn't have any good size() method

    if(plane.view == 0){
        uperp = ArrToVec(plane.across).cross(ArrToVec(plane.normal));
    }
    if(plane.view == 1){
        vperp = ArrToVec(plane.across).cross(ArrToVec(plane.normal));
    }

    let vtxs = [];
    push_square_vtxs(c, a, d, vtxs);

    let geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(vtxs), 3));

    let edges = new THREE.EdgesGeometry( geom );
    let line = new THREE.LineSegments( edges, mat_lin );

    outlines.add(line);

    let uvlayer = 2;
    if(plane.view != 2){
        if(a.z/a.y > 0) uvlayer = 3; else uvlayer = 4;
    }

    line.layers.set(plane.view);
    line.layers.enable(uvlayer);

    for(let dws of [xdigs, xwires]){
        for(let label in dws){
            let dw = dws[label][key];
            if(dw == undefined) continue; // sometimes wires are missing

            for(let block of dw.blocks){
                // TODO - would want to combine all the ones with the same
                // texture file into a single geometry.
                let geom = new THREE.BufferGeometry();

                let blockc = c.clone();
                blockc.addScaledVector(ArrToVec(plane.across), (block.dx+32-plane.nwires/2)*plane.pitch);
                blockc.addScaledVector(ArrToVec(plane.normal), (block.dy+32-plane.nticks/2)*Math.abs(plane.tick_pitch));

                // TODO hardcoding in (half) block size isn't good
                let blocka = ArrToVec(plane.across).multiplyScalar(32*plane.pitch);
                let blockd = ArrToVec(plane.normal).multiplyScalar(32*Math.abs(plane.tick_pitch));

                vtxs = [];
                push_square_vtxs(blockc, blocka, blockd, vtxs);
                geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(vtxs), 3));

                // TODO ditto here
                let u0 =   (block.texdx   )/block.texdim;
                let v0 = 1-(block.texdy   )/block.texdim;
                let u1 =   (block.texdx+64)/block.texdim;
                let v1 = 1-(block.texdy+64)/block.texdim;

                let uvs = new Float32Array( [u1, v0,
                                             u1, v1,
                                             u0, v1,

                                             u1, v0,
                                             u0, v1,
                                             u0, v0] );

                geom.setAttribute('uv', new THREE.BufferAttribute(uvs, 2));

                let mat = TextureMaterial(block.fname, block.texdim);
                let dmesh = new THREE.Mesh(geom, mat);
                dmesh.layers.set(plane.view);
                if(dws === xdigs){
                    if(!(label in digs)){
                        digs[label] = new THREE.Group();
                        scene.add(digs[label]);
                    }
                    digs[label].add(dmesh);
                }
                else{
                    if(!(label in wires)){
                        wires[label] = new THREE.Group();
                        scene.add(wires[label]);
                    }
                    wires[label].add(dmesh);
                }

                dmesh.layers.enable(uvlayer);
            } // end for block
        } // end for label
    } // end for dws

    for(let label in xhits){
        if(!(label in hitvtxs)) hitvtxs[label] = {}; // indexed by pair of layers
        if(xhits[label][key] == undefined) continue; // not necessarily hits on all planes
        for(let hit of xhits[label][key]){
            let hc = c.clone();
            hc.addScaledVector(a, (2.*hit.wire+1)/plane.nwires-1);
            hc.addScaledVector(d, (2.*hit.tick+1)/plane.nticks-1);

            let du = ArrToVec(plane.across).multiplyScalar(plane.pitch*.45);
            let dv = ArrToVec(plane.normal).multiplyScalar(hit.rms*Math.abs(plane.tick_pitch));

            let views = (plane.view, uvlayer);
            if(!(views in hitvtxs[label])) hitvtxs[label][views] = [];
            push_square_vtxs(hc, du, dv, hitvtxs[label][views]);
        } // end for hit
    } // end for label
}

com.divideScalar(nplanes);


for(let label in hitvtxs){
    let hits = new THREE.Group();
    scene.add(hits);

    for(let views in hitvtxs[label]){
        let hitgeom = new THREE.BufferGeometry();

        hitgeom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(hitvtxs[label][views]), 3));

        let h = new THREE.Mesh(hitgeom, mat_hit);
        h.layers.set(views[0]);
        h.layers.enable(views[1]); // uvlayer
        hits.add(h);
    }

    let btn = document.createElement('button');
    SetVisibility(hits, false, btn, label);

    btn.addEventListener('click', function(){
        SetVisibility(hits, !hits.visible, btn, label);
        requestAnimationFrame(animate);
    });

    document.getElementById('hits_dropdown').appendChild(btn);
}


for(let key in reco_vtxs){
    let vvtxs = [];
    let vidxs = [];
    for(let v of reco_vtxs[key]){
        push_icosahedron_vtxs(ArrToVec(v), .5, vvtxs, vidxs);
    }

    let vgeom = new THREE.BufferGeometry();
    vgeom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(vvtxs), 3));
    vgeom.setIndex(new THREE.BufferAttribute(new Uint16Array(vidxs), 1));
    let vtxs = new THREE.Mesh(vgeom, mat_vtxs);
    for(let i = 0; i <= 5; ++i) vtxs.layers.enable(i);
    scene.add(vtxs);


    let btn = document.createElement('button');
    SetVisibility(vtxs, false, btn, key);

    btn.addEventListener('click', function(){
            SetVisibility(vtxs, !vtxs.visible, btn, key);
            requestAnimationFrame(animate);
        });

    document.getElementById('vertices_dropdown').appendChild(btn);
}

// Physical cryostat
let cryogroup = new THREE.Group();

for(let key in cryos){
    let cryo = cryos[key];

    let r0 = ArrToVec(cryo.min);
    let r1 = ArrToVec(cryo.max);

    let boxgeom = new THREE.BoxBufferGeometry(r1.x-r0.x, r1.y-r0.y, r1.z-r0.z);

    let edges = new THREE.EdgesGeometry(boxgeom);
    let line = new THREE.LineSegments(edges, mat_geo);

    line.position.set((r0.x+r1.x)/2, (r0.y+r1.y)/2, (r0.z+r1.z)/2);

    for(let i = 0; i <= 5; ++i) line.layers.enable(i);

    cryogroup.add(line);
}

scene.add(cryogroup);

// Physical APAs
let apas = new THREE.Group();

for(let key in planes){
    let plane = planes[key];
    if(plane.view != 2) continue; // collection only

    let c = ArrToVec(plane.center);
    let a = ArrToVec(plane.across).multiplyScalar(plane.nwires*plane.pitch/2.);
    let d = ArrToVec(plane.wiredir).multiplyScalar(plane.depth/2.);

    let vtxs = [];
    push_square_vtxs(c, a, d, vtxs);

    let geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(vtxs), 3));

    let edges = new THREE.EdgesGeometry(geom);
    let line = new THREE.LineSegments(edges, mat_geo);

    for(let i = 0; i <= 5; ++i) line.layers.enable(i);

    apas.add(line);
}

scene.add(apas);

// Physical OpDets
let opdetgroup = new THREE.Group();

for(let opdet of opdets){
    let c = ArrToVec(opdet.center);
    let dy = new THREE.Vector3(0, opdet.height/2., 0);
    let dz = new THREE.Vector3(0, 0, opdet.length/2.);

    let vtxs = [];
    push_square_vtxs(c, dy, dz, vtxs);

    let geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(vtxs), 3));

    let edges = new THREE.EdgesGeometry(geom);
    let line = new THREE.LineSegments(edges, mat_geo);

    for(let i = 0; i <= 5; ++i) line.layers.enable(i);

    opdetgroup.add(line);
}

scene.add(opdetgroup);


function AddDropdownToggle(dropdown_id, what, label, init = false)
{
    let btn = document.createElement('button');
    SetVisibility(what, init, btn, label);

    btn.addEventListener('click', function(){
        SetVisibility(what, !what.visible, btn, label);
        requestAnimationFrame(animate);
    });

    document.getElementById(dropdown_id).appendChild(btn);
}

for(let label in spacepoints){
    let spvtxs = [];
    let spidxs = [];
    for(let sp of spacepoints[label]){
        push_icosahedron_vtxs(ArrToVec(sp), .4, spvtxs, spidxs);
    }

    let spgeom = new THREE.BufferGeometry();
    spgeom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(spvtxs), 3));
    spgeom.setIndex(new THREE.BufferAttribute(new Uint32Array(spidxs), 1));
    let sps = new THREE.Mesh(spgeom, mat_sps);
    for(let i = 0; i <= 5; ++i) sps.layers.enable(i);
    scene.add(sps);

    AddDropdownToggle('spacepoints_dropdown', sps, label);
}


let colors = ['red', 'blue', 'green', 'orange', 'purple', 'skyblue'];

function add_tracks(trajs, group){
    let i = 0;
    for(let track of trajs){
        let col = colors[i%colors.length];
        i += 1;
        let mat_trk = new THREE.LineBasicMaterial({color: col, linewidth: 2});
        let trkgeom = new THREE.BufferGeometry();
        let ptarr = [];
        for(let pt of track) ptarr = ptarr.concat(pt);
        trkgeom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(ptarr), 3));

        let trkline = new THREE.Line(trkgeom, mat_trk);
        for(let i = 0; i <= 5; ++i) trkline.layers.enable(i);
        group.add(trkline);
    }
}

for(let label in tracks){
    let reco_tracks = new THREE.Group();
    add_tracks(tracks[label], reco_tracks);
    scene.add(reco_tracks);

    AddDropdownToggle('tracks_dropdown', reco_tracks, label);
}

add_tracks(truth_trajs, truth);


for(let label in xdigs){
    AddDropdownToggle('digs_dropdown', digs[label], label);
}

for(let label in xwires){
    AddDropdownToggle('wires_dropdown', wires[label], label);
}


let controls = new OrbitControls(camera, renderer.domElement);

controls.target = com;

camera.translateX(1000);
camera.lookAt(com);

//controls.autoRotate = true;
//controls.autoRotateSpeed *= 10;

controls.update();

function UpdateLabels()
{
    const W = renderer.domElement.width;
    const H = renderer.domElement.height;

    let ps = [];

    for(var label of document.getElementsByClassName('label')){
        // only show in collection view (1<<2) for now
        if(camera.layers.mask != 4){
            label.style.visibility = "hidden";
            continue;
        }

        var pos = label.pos.clone();
        pos.project(camera);

        let p = {x: (1+pos.x)*W/2.,
                 y: (1-pos.y)*H/2.};

        if(p.x < 30) p.x = 30;
        if(p.x > W-30) p.x = W-30;
        if(p.y < 30) p.y = 30;
        if(p.y > H-30) p.y = H-30;

        let dsqmin = 1e10;
        for(var q of ps){
            const dsq = (p.x-q.x)*(p.x-q.x) + (p.y-q.y)*(p.y-q.y);
            if(dsq < dsqmin) dsqmin = dsq;
        }

        if(dsqmin > 50*50){
            label.style.visibility = "visible";
            label.style.left = p.x + 'px';
            label.style.top  = p.y + 'px';
            ps.push(p);
        }
        else{
            label.style.visibility = "hidden";
        }
    }
}

function animate() {
    if(gAnimReentrant) return;
    gAnimReentrant = true;

    let now = performance.now(); // can be passed as an argument, but only for requestAnimationFrame callbacks
    if(animStart != null){
        let frac = (now-animStart)/1000.; // 1sec anim
        if(frac > 1){
            frac = 1;
            animStart = null;
        }

        animFunc(frac);
    }

    renderer.render( scene, camera );

    if(animStart != null) requestAnimationFrame(animate);

    gAnimReentrant = false;

    UpdateLabels();
}

function SetVisibility(col, state, elem, str)
{
    col.visible = state;
    // Tick and Cross emojis respectively
    elem.innerHTML = (state ? '&#x2705 ' : '&#x274c ')+str;
}

function SetVisibilityById(col, state, id, str)
{
    SetVisibility(col, state, document.getElementById(id), str);
}

function Toggle(col, id, str){
    SetVisibilityById(col, !col.visible, id, str);
    requestAnimationFrame(animate);
}

// TODO - would be better to have this javascript look up the buttons in the
// HTML and attach the handlers to them.
window.ToggleTruth = function(){Toggle(truth, 'truth', 'Truth');}
window.ToggleCryos = function(){Toggle(cryogroup, 'cryos', 'Cryostats');}
window.ToggleAPAs = function(){Toggle(apas, 'apas', 'APAs');}
window.ToggleOpDets = function(){Toggle(opdetgroup, 'opdets', 'OpDets');}


AllViews();
ThreeDControls();

// Try to pre-load all textures - doesn't work
//renderer.compile(scene, camera);

//SetVisibilityById(digs, false, 'rawdigits', 'RawDigits');
//SetVisibilityById(wires, false, 'wires', 'Wires');
SetVisibilityById(truth, true, 'truth', 'Truth');

SetVisibilityById(cryogroup, true, 'cryos', 'Cryostats');
SetVisibilityById(apas, true, 'apas', 'APAs');
SetVisibilityById(opdetgroup, false, 'opdets', 'OpDets');

let animStart = null;
let animFunc = null;

window.NoView = function(){camera.layers.set(5); requestAnimationFrame(animate);}

// TODO these only really do what you expect when already in 3D mode. May want
// to "re-diagonalize" this functionality.
window.ZView  = function(){camera.layers.set(2); requestAnimationFrame(animate);}
window.UView  = function(){camera.layers.set(0); requestAnimationFrame(animate);}
window.VView  = function(){camera.layers.set(1); requestAnimationFrame(animate);}
window.UVView = function(){camera.layers.set(3); requestAnimationFrame(animate);}
window.VUView = function(){camera.layers.set(4); requestAnimationFrame(animate);}

// Remains as a free function so others can call it
function AllViews(){
    camera.layers.enable(0); camera.layers.enable(1); camera.layers.enable(2);
}
window.AllViews = AllViews;

window.ThreeDView = function(){
    AllViews();

    // A hack to make sure the Z->3D transition doesn't end up completely
    // degenerate for the camera (which leads to a strange 90 degree flip).
    let targetDiff = camera.position.clone();
    targetDiff.sub(controls.target);
    targetDiff.normalize();
    targetDiff.addScaledVector(camera.up, -1e-3);
    targetDiff.normalize();

    AnimateTo(targetDiff, new THREE.Vector3(0, 1, 0), 50, null);

    ThreeDControls();
}

// https://en.wikipedia.org/wiki/Slerp#Geometric_Slerp
// p0 and p1 must be unit vectors
function slerp(p0, p1, t){
    let omega = Math.acos(THREE.Math.clamp(p0.dot(p1), -1, +1));
    if(omega == 0) return p0.clone();

    let ret = p0.clone();
    ret.multiplyScalar(Math.sin((1-t)*omega)/Math.sin(omega));
    ret.addScaledVector(p1, Math.sin(t*omega)/Math.sin(omega));
    return ret;
}

function lerpVec(v0, v1, t){
    return new THREE.Vector3(THREE.Math.lerp(v0.x, v1.x, t),
                             THREE.Math.lerp(v0.y, v1.y, t),
                             THREE.Math.lerp(v0.z, v1.z, t));
}

function UpdateFOV(cam, newFOV)
{
    let diff = cam.position.clone();
    diff.sub(controls.target);
    diff.multiplyScalar(cam.fov/newFOV);
    diff.add(controls.target);
    cam.position.copy(diff);

    cam.near *= cam.fov/newFOV;
    cam.far *= cam.fov/newFOV;
    cam.fov = newFOV;
    cam.updateProjectionMatrix();
}

function AnimateTo(targetDiff, targetUp, targetFOV, endFunc){
    let initDiff = camera.position.clone();
    initDiff.sub(controls.target);
    initDiff.normalize();

    let initFOV = camera.fov;
    let initUp = camera.up.clone();

    console.log('Animate from ', initDiff, initUp, initFOV, ' to ', targetDiff, targetUp, targetFOV);

    // May be no need to animate
    if((targetDiff == null || targetDiff.equals(initDiff)) &&
       (targetUp == null || targetUp.equals(initUp)) &&
       (targetFOV == null || targetFOV == initFOV)){
        console.log('Requested animation is a no-op, skip');
        if(endFunc != null) endFunc();
        return;
    }

    animStart = performance.now();
    animFunc = function(frac){
        if(targetDiff != null){
            let mag = camera.position.distanceTo(controls.target);
            camera.position.copy(controls.target);
            camera.position.addScaledVector(slerp(initDiff, targetDiff, frac), mag);
        }

        if(targetUp != null) camera.up = slerp(initUp, targetUp, frac);

        if(targetFOV != null) UpdateFOV(camera, THREE.Math.lerp(initFOV, targetFOV, frac));

        // console.log('Anim: ', frac, camera.position, camera.up, camera.fov);

        if(frac == 1 && endFunc != null) endFunc();

        controls.update();
    }

    requestAnimationFrame(animate);
}

function TwoDControls(){
    controls.screenSpacePanning = true;

    controls.enableRotate = false;

    controls.mouseButtons = {
        LEFT: THREE.MOUSE.PAN,
        MIDDLE: THREE.MOUSE.DOLLY,
        RIGHT: null
    }

    // Seems to hang the touch controls entirely :(
    //controls.touches = {
    //    ONE: THREE.TOUCH.DOLLY_PAN,
    //    TWO: THREE.TOUCH.ROTATE
    //}

    controls.update();
}

function ThreeDControls(){
    controls.screenSpacePanning = true;

    controls.enableRotate = true;

    controls.mouseButtons = {
        LEFT: THREE.MOUSE.ROTATE,
        MIDDLE: THREE.MOUSE.DOLLY,
        RIGHT: THREE.MOUSE.PAN
    }

    controls.update();
}

window.ZView2D = function(){
    camera.layers.enable(2);
    AnimateTo(new THREE.Vector3(0, 1, 0),
              new THREE.Vector3(1, 0, 0),
              1e-6, ZView);
    TwoDControls();
}

window.UVView2D = function(){
    camera.layers.enable(3);
    AnimateTo(vperp, new THREE.Vector3(1, 0, 0), 1e-6, UVView);
    TwoDControls();
}

window.VUView2D = function(){
    camera.layers.enable(4);
    AnimateTo(uperp, new THREE.Vector3(1, 0, 0), 1e-6, VUView);
    TwoDControls();
}

window.Perspective = function(){AnimateTo(null, null,   50, null);}
window.Ortho       = function(){AnimateTo(null, null, 1e-6, null);}

window.Theme = function(theme)
{
    document.body.className = theme;

    // Doesn't work
    // renderer.setClearColor(window.getComputedStyle(document.body, null).getPropertyValue('backgroundColor'));

    if(theme == 'darktheme') renderer.setClearColor('black'); else renderer.setClearColor('white');

    requestAnimationFrame(animate);
}


window.addEventListener( 'resize', onWindowResize, false );

function onWindowResize() {
    renderer.setSize( window.innerWidth, window.innerHeight-4);

    // Keep aspect ratio correct
    camera.top = -512*window.innerHeight/window.innerWidth;
    camera.bottom = +512*window.innerHeight/window.innerWidth;

    // For 3D camera
    camera.aspect = window.innerWidth / window.innerHeight;

    camera.updateProjectionMatrix();
}

window.addEventListener('unload', function(event){renderer.dispose();});

controls.addEventListener('change', animate);
window.addEventListener('resize', animate);

animate();

console.log(renderer.info);
