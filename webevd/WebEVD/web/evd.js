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

const AXES_NONE = 0;
const AXES_CMCM = 1;
const AXES_WIRECM = 2;
const AXES_WIRETICK = 3;

let gAxesType = AXES_NONE;

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
renderer.setSize( window.innerWidth, window.innerHeight);

renderer.setClearColor('black');

renderer.domElement.addEventListener("click", OnClick, true);

renderer.alpha = true;
renderer.antialias = false;

// Disable depth buffer to get better handling for the transparent digits/wires
renderer.getContext().disable(renderer.getContext().DEPTH_TEST);

document.body.appendChild( renderer.domElement );

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
    mat.tmpmipmaps[mipdim] = tex;

    // Longest run of mipmap levels
    let maxdim;
    for(maxdim = 1; maxdim <= texdim; maxdim *= 2){
        if(!(maxdim in mat.tmpmipmaps)) break;
    }
    if(maxdim == 1) return;
    maxdim /= 2;

    // We have something to show so undo the temporary loading style
    mat.opacity = 1;

    // TODO - is it bad that the largest resolution image is in the
    // main map and also the first element in the mipmap list?
    mat.map = mat.tmpmipmaps[maxdim];

    mat.map.mipmaps.length = 0;
    for(let i = maxdim; i >= 1; i /= 2){
        mat.map.mipmaps.push(mat.tmpmipmaps[i].image);
    }

    mat.generateMipMaps = false;
    if(maxdim == texdim){
        // Go pixely rather than blurry when zoomed right in
        mat.map.magFilter = THREE.NearestFilter;
    }
    else{
        mat.map.magFilter = THREE.LinearMipmapLinearFilter;
    }
    mat.map.minFilter = THREE.LinearMipmapLinearFilter;

    // No more need for the temporaries
    if(maxdim == texdim) delete mat.tmpmipmaps;

    requestAnimationFrame(animate);
}

// Cache so we can request the same texture material multiple times without
// duplication
let gtexmats = {};
let gmaxtexdim = 0;

function TextureMaterial(fname, texdim){
    if(fname in gtexmats) return gtexmats[fname].mat;

    // Make the material a transparent solid colour until the texture loads
    let mat = new THREE.MeshBasicMaterial( { color: 'white', opacity: .1, side: THREE.DoubleSide, transparent: true, alphaTest: 1/512.} );
    gtexmats[fname] = {mat: mat, texdim: texdim};

    if(texdim > gmaxtexdim) gmaxtexdim = texdim;

    return mat;
}

function FinalizeTextures(){
    // Load all the mipmaps
    for(let d = 1; d <= gmaxtexdim; d *= 2){
        for(let fname in gtexmats){
            let mat = gtexmats[fname].mat;
            let texdim = gtexmats[fname].texdim;
            if(texdim < d) continue;
            new THREE.TextureLoader().load(fname+'_'+d+'.png',
                                           function(t){
                                               TextureLoadCallback(t, mat, d, texdim);
                                           },
                                           undefined,
                                           function(err){
                                               mat.opacity = 0;
                                               mat.needsUpdate = true;
                                               requestAnimationFrame(animate);
                                               console.log('error loading', fname, d, err);
                                           });
        } // end for fname
    } // end for d
}


let outlines = new THREE.Group();
let digs = {}; // Groups indexed by label
let wires = {};
let truth = new THREE.Group();
let chargedTruth = new THREE.Group();

scene.add(outlines);
scene.add(truth);
scene.add(chargedTruth);

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

let tpclabels_div = document.getElementById('tpclabels_div');

let wireaxes = [[], [], [], [], []];
let tickaxes = [[], [], [], [], []];

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
        // This ordering happens to give us beam left-to-right
        vperp = ArrToVec(plane.normal).cross(ArrToVec(plane.across));
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
                // TODO hardcoding in (half) block size isn't good
                blockc.addScaledVector(ArrToVec(plane.across), (block.x+block.dx/2-plane.nwires/2)*plane.pitch);
                blockc.addScaledVector(ArrToVec(plane.normal), (block.y+block.dy/2-plane.nticks/2)*Math.abs(plane.tick_pitch));

                let blocka = ArrToVec(plane.across).multiplyScalar(block.dx/2*plane.pitch);
                let blockd = ArrToVec(plane.normal).multiplyScalar(block.dy/2*Math.abs(plane.tick_pitch));

                vtxs = [];
                push_square_vtxs(blockc, blocka, blockd, vtxs);
                geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(vtxs), 3));

                // TODO ditto here
                let u0 =   (block.u         )/block.texdim;
                let v0 = 1-(block.v         )/block.texdim;
                let u1 =   (block.u+block.du)/block.texdim;
                let v1 = 1-(block.v+block.dv)/block.texdim;

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

    if(plane.view == 2){ // collection
        let div = document.createElement('div');
        div.className = 'label';
        // Cut off the plane number, we want the name of the whole APA/TPC
        div.appendChild(document.createTextNode(key.slice(0, key.lastIndexOf(' '))));
        div.pos = c; // stash the 3D position on the HTML element
        tpclabels_div.appendChild(div);
    }

    let r0 = c.clone();
    r0.addScaledVector(a, -1);
    r0.addScaledVector(d, -1);
    let r1 = c.clone();
    r1.addScaledVector(a, +1);
    r1.addScaledVector(d, -1);
    wireaxes[plane.view].push({r0: r0, r1: r1, w: plane.nwires});
    if(uvlayer != plane.view) wireaxes[uvlayer].push({r0: r0, r1: r1, w: plane.nwires});
    let r2 = c.clone();
    r2.addScaledVector(a, -1);
    r2.addScaledVector(d, +1);
    tickaxes[plane.view].push({r0: r0, r1: r2, w: plane.nticks});
    if(uvlayer != plane.view) tickaxes[uvlayer].push({r0: r0, r1: r2, w: plane.nticks});

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
} // end for planes

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

let opdetlabels_div = document.getElementById('opdetlabels_div');

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

    let d = document.createElement('div');
    d.className = 'label';
    d.appendChild(document.createTextNode(opdet.name));
    d.pos = c; // stash the 3D position on the HTML element
    opdetlabels_div.appendChild(d);
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

// Consistent colouring for each PDG.
// Declared outside the function to ensure consistency across the many times
// add_tracks is run.
let colour_map = {};
let colours = ['blue', 'skyblue', 'orange', 'magenta', 'green', 'purple', 'pink', 'red', 'violet', 'yellow']
let neutral_particles = [-1, 22, 111, 2112, 311]

function is_neutral(pdg) {
    if (neutral_particles.includes(pdg))
        return true;
    else if (pdg.length >= 10) // Nuclei
        return true;

    return false;
}

function add_tracks(trajs, group, must_be_charged){
    let i = 0;
    for(let track of trajs){
        let col = 'white';
        let track_pdg = 'pdg' in track ? track.pdg : -1;

        if (is_neutral(track_pdg) && must_be_charged)
            continue;

        if (track_pdg in colour_map)
            col = colour_map[track_pdg];
        else if (track_pdg != -1){
            if (is_neutral(track_pdg))
                col = 'grey';
            else
                col = colours[i % colours.length];

            colour_map[track_pdg] = col;
        } else {
            col = colours[i % colours.length];
        }

        i += 1;
        let ptarr = [];
        for(let pt of track.positions) ptarr = ptarr.concat(pt);

        let trkgeom = new THREE.BufferGeometry();
        trkgeom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(ptarr), 3));

        let mat_trk = new THREE.LineBasicMaterial({color: col, linewidth: 2});
        let trkline = new THREE.Line(trkgeom, mat_trk);

        for(let i = 0; i <= 5; ++i) trkline.layers.enable(i);
        group.add(trkline);
    }
}

for(let label in tracks){
    let reco_tracks = new THREE.Group();
    add_tracks(tracks[label], reco_tracks, false);
    scene.add(reco_tracks);

    AddDropdownToggle('tracks_dropdown', reco_tracks, label);
}

add_tracks(truth_trajs, truth, false);
add_tracks(truth_trajs, chargedTruth, true);


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

controls.update();

function ProjectToScreenPixels(r)
{
    const W = renderer.domElement.width;
    const H = renderer.domElement.height;

    r.project(camera);

    r.x = (1+r.x)*W/2.;
    r.y = (1-r.y)*H/2.;
}

let axislabels_div = document.getElementById('axislabels_div');

// NB destroys r0 and r1. Returns new idx
function PaintAxis(ax, idx)
{
    if(ax.sign == undefined) ax.sign = '';

    const W = renderer.domElement.width;
    const H = renderer.domElement.height;

    let r0 = ax.r0.clone();
    let r1 = ax.r1.clone();
    ProjectToScreenPixels(r0);
    ProjectToScreenPixels(r1);

    if(Math.abs(r0.x-r1.x) < 1){ // vertical axis
        if(r0.x < 30) r0.x = r1.x = 30;
        if(r0.x > W-30) r0.x = r1.x = W-30;
    }

    if(Math.abs(r0.y-r1.y) < 1){ // horizontal axis
        if(r0.y < 30) r0.y = r1.y = 30;
        if(r0.y > H-30) r0.y = r1.y = H-30;
    }

    const Lsq = (r0.x-r1.x)*(r0.x-r1.x) + (r0.y-r1.y)*(r0.y-r1.y);

    // Find the smallest stride without clashes
    let stride = 0;
    for(stride of [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000]){
        let N = ax.w/stride;
        if(Lsq/(N*N) > 50*50) break; // enough spacing to not clash, accept
    }
    if(stride == 0) return idx;

    for(let w = 0; w <= ax.w; w += stride){
        // TODO with Vector2
        let r = new THREE.Vector3(0, 0, 0);
        r.addScaledVector(r0, 1-w/ax.w);
        r.addScaledVector(r1,   w/ax.w);

        // Don't paint off the screen
        if(r.x < 0 || r.y < 0 || r.x > W || r.y > H) continue;

        while(idx >= axislabels_div.children.length){
            let d = document.createElement('div');
            d.className = 'label';
            axislabels_div.appendChild(d);
        }

        let label = axislabels_div.children[idx];
        idx += 1;
        label.innerHTML = (w == 0) ? '0' : ax.sign + w;
        label.style.left = r.x + 'px';
        label.style.top  = r.y + 'px';
    }

    return idx;
}

let cmzaxes = [];
cmzaxes.push({r0: new THREE.Vector3(0, 0, 0),
              r1: new THREE.Vector3(0, 0, 1400),
              w: 1400});
let cmxaxes = [];
cmxaxes.push({r0: new THREE.Vector3(0, 0, 0),
              r1: new THREE.Vector3(360, 0, 0),
              w: 360, sign: '+'});
cmxaxes.push({r0: new THREE.Vector3(0, 0, 0),
              r1: new THREE.Vector3(-360, 0, 0),
              w: 360, sign: '-'});
let cmyaxes = [];
cmyaxes.push({r0: new THREE.Vector3(0, 0, 0),
              r1: new THREE.Vector3(0, 600, 0),
              w: 600, sign: '+'});
cmyaxes.push({r0: new THREE.Vector3(0, 0, 0),
              r1: new THREE.Vector3(0, -600, 0),
              w: 600, sign: '-'});

function PaintAxes()
{
    let idx = 0;
    if(gAxesType == AXES_CMCM){
        for(let ax of cmxaxes) idx = PaintAxis(ax, idx);
        for(let ax of cmyaxes) idx = PaintAxis(ax, idx);
        for(let ax of cmzaxes) idx = PaintAxis(ax, idx);
    }

    if(gAxesType == AXES_WIRECM){
        for(let ax of cmxaxes) idx = PaintAxis(ax, idx);
    }

    let layer = new THREE.Layers();
    for(let i = 0; i <= 4; ++i){
        layer.set(i);
        if(camera.layers.test(layer)){
            if(gAxesType == AXES_WIRECM){
                for(let ax of wireaxes[i]) idx = PaintAxis(ax, idx);
            }
            if(gAxesType == AXES_WIRETICK){
                for(let ax of wireaxes[i]) idx = PaintAxis(ax, idx);
                for(let ax of tickaxes[i]) idx = PaintAxis(ax, idx);
            }
        }
    }

    // TODO maybe just hide them unless there was a large change in length?
    for(let i = axislabels_div.children.length-1; i >= idx; --i){
        axislabels_div.removeChild(axislabels_div.children[i]);
    }
}

function PaintLabels(div)
{
    for(let label of div.children){
        let r = label.pos.clone();
        ProjectToScreenPixels(r);
        label.style.left = r.x + 'px';
        label.style.top  = r.y + 'px';
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

    if(controls.autoRotate) controls.update();

    renderer.render( scene, camera );

    if(animStart != null || controls.autoRotate)
        requestAnimationFrame(animate);

    PaintAxes();
    if(opdetlabels_div.style.display != "none") PaintLabels(opdetlabels_div);
    if(tpclabels_div.style.display != "none") PaintLabels(tpclabels_div);

    gAnimReentrant = false;
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

function SetVisibilityLabel(col, state, elem, str)
{
    col.style.display = state ? "initial" : "none";
    // Tick and Cross emojis respectively
    elem.innerHTML = (state ? '&#x2705 ' : '&#x274c ')+str;
}

function SetVisibilityByIdLabel(col, state, id, str)
{
    SetVisibilityLabel(col, state, document.getElementById(id), str);
}

function ToggleLabel(col, id, str){
    SetVisibilityByIdLabel(col,
                           col.style.display == "none",
                           id, str);
    requestAnimationFrame(animate);
}

// TODO - would be better to have this javascript look up the buttons in the
// HTML and attach the handlers to them.
window.ToggleAllTruth = function(){Toggle(truth, 'allTruth', 'All');}
window.ToggleChargedTruth = function(){Toggle(chargedTruth, 'chargedTruth', 'Charged');}
window.ToggleCryos = function(){Toggle(cryogroup, 'cryos', 'Cryostats');}
window.ToggleAPAs = function(){Toggle(apas, 'apas', 'APAs');}
window.ToggleOpDets = function(){Toggle(opdetgroup, 'opdets', 'OpDets');}


AllViews();
ThreeDControls();

// Try to pre-load all textures - doesn't work
//renderer.compile(scene, camera);

//SetVisibilityById(digs, false, 'rawdigits', 'RawDigits');
//SetVisibilityById(wires, false, 'wires', 'Wires');
SetVisibilityById(truth, true, 'allTruth', 'All');
SetVisibilityById(chargedTruth, false, 'chargedTruth', 'Charged');

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
    controls.autoRotate = false;

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

window.Orbit = function()
{
    controls.autoRotate = !controls.autoRotate;
    requestAnimationFrame(animate);
}

window.NoAxes = function()
{
    gAxesType = AXES_NONE;
    requestAnimationFrame(animate);
}

window.PhysicalAxes = function()
{
    gAxesType = AXES_CMCM;
    requestAnimationFrame(animate);
}

window.WireCmAxes = function()
{
    gAxesType = AXES_WIRECM;
    requestAnimationFrame(animate);
}

window.WireTickAxes = function()
{
    gAxesType = AXES_WIRETICK;
    requestAnimationFrame(animate);
}

window.OpDetLabels = function()
{
    ToggleLabel(opdetlabels_div, 'opdetlabels', 'OpDets');
}
// default
SetVisibilityByIdLabel(opdetlabels_div, false, 'opdetlabels', 'OpDets')

window.TPCLabels = function()
{
    ToggleLabel(tpclabels_div, 'tpclabels', 'TPCs');
}
// default
SetVisibilityByIdLabel(tpclabels_div, false, 'tpclabels', 'TPCs');

function OnClick()
{
    controls.autoRotate = false;
}

window.Theme = function(theme)
{
    document.body.className = theme;

    // Doesn't work
    // renderer.setClearColor(window.getComputedStyle(document.body, null).getPropertyValue('backgroundColor'));

    if(theme == 'darktheme') renderer.setClearColor('black'); else renderer.setClearColor('white');
    if(theme == 'bluetheme') renderer.setClearColor(new THREE.Color(0, 0, 99./255));

    requestAnimationFrame(animate);
}


window.addEventListener( 'resize', onWindowResize, false );

function onWindowResize() {
    renderer.setSize( window.innerWidth, window.innerHeight);

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

FinalizeTextures();

window.Theme('bluetheme');

animate();

// Amazingly these dominate render times, and we never change any objects'
// position after the initial setup.
scene.matrixAutoUpdate = false;
scene.autoUpdate = false;

console.log(renderer.info);
