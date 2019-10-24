// TODO learn how imports/modules work
//import { IcosahedronBufferGeometry } from './three.js-master/src/geometries/IcosahedronGeometry.js'

var scene = new THREE.Scene();

var camera = new THREE.PerspectiveCamera( 50, window.innerWidth / window.innerHeight, 0.1, 1e6 );

var renderer = new THREE.WebGLRenderer();
// I have no idea why these 4 pixels are necessary
renderer.setSize( window.innerWidth, window.innerHeight-4);

renderer.setClearColor('black');//'white');//0xa0a0a0);

renderer.alpha = true;
renderer.antialias = true;

document.body.appendChild( renderer.domElement );

function ArrToVec(arr)
{
    return new THREE.Vector3(arr[0], arr[1], arr[2]);
}

var mat_lin = new THREE.LineBasicMaterial({color: 'gray'});

var mat_hit = new THREE.MeshBasicMaterial( { color: 0xffffff, side: THREE.DoubleSide } );

function TextureMaterial(fname){
    var tex = new THREE.TextureLoader().load(fname,
                                             undefined,
                                             undefined,
                                             function(err){console.log('error loading', fname, err);});

  var mat = new THREE.MeshBasicMaterial( { color: 'white', side: THREE.DoubleSide, transparent: true, alphaTest: 1/512.} );
  tex.flipY = false; // some disagreement between conventions...
  tex.magFilter = THREE.NearestFilter;
  tex.minFilter = THREE.LinearFilter;
  tex.generateMipmaps = true;
  //  mat.alphaMap = tex;
  mat.map = tex;

  return mat;
}


var outlines = new THREE.Group();
var digs = new THREE.Group();
var wires = new THREE.Group();
var hits = new THREE.Group();
var reco_tracks = new THREE.Group();
var truth = new THREE.Group();

scene.add(outlines);
scene.add(digs);
scene.add(wires);
scene.add(hits);
scene.add(reco_tracks);
scene.add(truth);

var com = new THREE.Vector3();
var nplanes = 0;

var uperp = null;
var vperp = null;

for(key in planes){
    var plane = planes[key];
    var c = ArrToVec(plane.center);
    var a = ArrToVec(plane.across).multiplyScalar(plane.nwires*plane.pitch/2.);
    var d = ArrToVec(plane.normal).multiplyScalar(plane.nticks*Math.abs(plane.tick_pitch)/2.); // drift direction

    c.add(d); // center of the drift direction too

    com.add(c);
    nplanes += 1; // javascript is silly and doesn't have any good size() method

    if(plane.view == 0){
        uperp = ArrToVec(plane.across).cross(ArrToVec(plane.normal));
    }
    if(plane.view == 1){
        vperp = ArrToVec(plane.across).cross(ArrToVec(plane.normal));
    }

    var p1 = c.clone(); var p2 = c.clone(); var p3 = c.clone(); var p4 = c.clone();

    p1.add(a);
    p2.add(a);
    p2.add(d);
    p3.add(d);
    p3.addScaledVector(a, -1);
    p4.addScaledVector(a, -1);
    p1.addScaledVector(d, -1);
    p4.addScaledVector(d, -1);

    var geom = new THREE.BufferGeometry();
    var vertices = new Float32Array( [p1.x, p1.y, p1.z,
                                      p2.x, p2.y, p2.z,
                                      p3.x, p3.y, p3.z,

                                      p1.x, p1.y, p1.z,
                                      p3.x, p3.y, p3.z,
                                      p4.x, p4.y, p4.z]);

    // itemSize = 3 because there are 3 values (components) per vertex
    geom.addAttribute( 'position', new THREE.BufferAttribute( vertices, 3 ) );

    var edges = new THREE.EdgesGeometry( geom );
    var line = new THREE.LineSegments( edges, mat_lin );

    outlines.add(line);

    line.layers.set(plane.view);


    // Power-of-two fixup
    var fw = 1;//plane.nwires/THREE.Math.ceilPowerOfTwo(plane.nwires);
    var fh = 1;//plane.nticks/THREE.Math.ceilPowerOfTwo(plane.nticks);

    // TODO think carefully about geometry
    var uvs = new Float32Array( [fw,  0,
                                 fw, fh,
                                  0, fh,

                                 fw,  0,
                                  0, fh,
                                  0,  0] );

    geom.addAttribute( 'uv', new THREE.BufferAttribute( uvs, 2 ) );

    var mat = TextureMaterial("digits/"+key+".png");
    var d = new THREE.Mesh( geom, mat );
    d.layers.set(plane.view);
    digs.add(d);

    mat = TextureMaterial("wires/"+key+".png");
    var w = new THREE.Mesh( geom, mat );
    w.layers.set(plane.view);
    wires.add(w);
}

com.divideScalar(nplanes);

colors = ['red', 'blue', 'green', 'orange', 'purple', 'skyblue'];

function add_tracks(trajs, group){
    var i = 0;
    for(let track of trajs){
        col = colors[i%colors.length];
        i += 1;
        var mat_trk = new THREE.LineBasicMaterial({color: col, linewidth: 2});
        var trkgeom = new THREE.BufferGeometry();
        ptarr = [];
        for(let pt of track) ptarr = ptarr.concat(pt);
        trkgeom.addAttribute('position', new THREE.BufferAttribute(new Float32Array(ptarr), 3));

        var trkline = new THREE.Line(trkgeom, mat_trk);
        trkline.layers.enable(0); trkline.layers.enable(1); trkline.layers.enable(2);
        group.add(trkline);
    }
}

add_tracks(tracks, reco_tracks);
add_tracks(truth_trajs, truth);

var controls = new THREE.OrbitControls( camera, renderer.domElement );

controls.target = com;

camera.translateX(1000);
//console.log(camera.position);
camera.lookAt(com);

//controls.autoRotate = true;
//controls.autoRotateSpeed *= 10;

controls.update();

var animReentrant = false;

function animate() {
    if(animReentrant) return;
    animReentrant = true;

    var now = performance.now(); // can be passed as an argument, but only for requestAnimationFrame callbacks
    if(animStart != null){
        var frac = (now-animStart)/1000.; // 1sec anim
        if(frac > 1){
            frac = 1;
            animStart = null;
        }

        animFunc(frac);
    }

    renderer.render( scene, camera );

    if(animStart != null) requestAnimationFrame(animate);

    animReentrant = false;
}

function SetVisibility(col, state, id, str)
{
    col.visible = state;
    // Tick and Cross emojis respectively
    document.getElementById(id).innerHTML = (state ? '&#x2705 ' : '&#x274c ')+str;
}

function Toggle(col, id, str){
    SetVisibility(col, !col.visible, id, str);
    requestAnimationFrame(animate);
}

function ToggleRawDigits(){Toggle(digs, 'rawdigits', 'RawDigits');}
function ToggleWires(){Toggle(wires, 'wires', 'Wires');}
function ToggleHits(){Toggle(hits, 'hits', 'Hits');}
function ToggleSpacePoints(){Toggle(group, 'spacepoints', 'SpacePoints');}
function ToggleTracks(){Toggle(reco_tracks, 'tracks', 'Tracks');}
function ToggleTruth(){Toggle(truth, 'truth', 'Truth');}

AllViews();
ThreeDControls();

// Try to pre-load all textures - doesn't work
//renderer.compile(scene, camera);

SetVisibility(digs, false, 'rawdigits', 'RawDigits');
SetVisibility(wires, false, 'wires', 'Wires');
SetVisibility(hits, false, 'hits', 'Hits');
SetVisibility(reco_tracks, false, 'tracks', 'Tracks');
SetVisibility(truth, true, 'truth', 'Truth');

var animStart = null;

function ZView(){camera.layers.set(2); requestAnimationFrame(animate);}
function UView(){camera.layers.set(0); requestAnimationFrame(animate);}
function VView(){camera.layers.set(1); requestAnimationFrame(animate);}

function AllViews(){
    camera.layers.enable(0); camera.layers.enable(1); camera.layers.enable(2);
}

function ThreeDView(){
    AllViews();

    // A hack to make sure the Z->3D transition doesn't end up completely
    // degenerate for the camera (which leads to a strange 90 degree flip).
    var targetDiff = camera.position.clone();
    targetDiff.sub(controls.target);
    targetDiff.normalize();
    targetDiff.addScaledVector(camera.up, -1e-3);
    targetDiff.normalize();

    AnimateTo(targetDiff, new THREE.Vector3(0, 1, 0), 50, null);

    ThreeDControls();
}

// https://en.wikipedia.org/wiki/Slerp#Geometric_Slerp
function slerp(p0, p1, t){
    if(p0.equals(p1)) return p0;

    var omega = Math.acos(p0.dot(p1));
    var ret = p0.clone();
    ret.multiplyScalar(Math.sin((1-t)*omega)/Math.sin(omega));
    ret.addScaledVector(p1, Math.sin(t*omega)/Math.sin(omega));
    return ret;
}


function UpdateFOV(cam, newFOV)
{
    //    console.log('update fov ', cam, newFOV);

    var diff = cam.position.clone();
    diff.sub(controls.target);
    diff.multiplyScalar(cam.fov/newFOV);//Math.sin(cam.fov*3.14/180)/Math.sin(newFOV*3.14/180));
    diff.add(controls.target);
    cam.position.copy(diff);

    cam.near *= cam.fov/newFOV;
    cam.far *= cam.fov/newFOV;
    cam.fov = newFOV;
    cam.updateProjectionMatrix();
}

function AnimateTo(targetDiff, targetUp, targetFOV, endFunc){
    var initDiff = camera.position.clone();
    initDiff.sub(controls.target);
    initDiff.normalize();

    var initFOV = camera.fov;
    var initUp = camera.up.clone();

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
            var mag = camera.position.distanceTo(controls.target);

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
        RIGHT: null // THREE.MOUSE.ROTATE
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

function ZView2D(){
    camera.layers.enable(2);
    AnimateTo(new THREE.Vector3(0, 1, 0),
              new THREE.Vector3(1, 0, 0),
              1e-6, ZView);
    TwoDControls();
}

function UView2D(){
    camera.layers.enable(0);
    AnimateTo(uperp, new THREE.Vector3(1, 0, 0), 1e-6, UView);
    TwoDControls();
}

function VView2D(){
    camera.layers.enable(1);
    AnimateTo(vperp, new THREE.Vector3(1, 0, 0), 1e-6, VView);
    TwoDControls();
}

function Perspective()
{
    AnimateTo(null, null, 50, null);
}

function Ortho()
{
    AnimateTo(null, null, 1e-6, null);
}

function Theme(theme)
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

window.addEventListener('unload', function(event) {
    renderer.dispose();
//    console.log( THREE.WebGLRenderer.info);
      });

controls.addEventListener('change', animate);
window.addEventListener('resize', animate);
animate();

console.log(renderer.info);
